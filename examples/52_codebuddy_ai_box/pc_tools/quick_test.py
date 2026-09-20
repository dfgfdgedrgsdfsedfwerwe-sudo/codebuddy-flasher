#!/usr/bin/env python3
"""快速验证测试 - 检查系统是否正常工作"""
import socket
import json
import sys
import time

def check_daemon():
    """检查守护进程是否运行"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2.0)
        sock.connect(("127.0.0.1", 47100))
        sock.close()
        return True
    except:
        return False

def send_test_decision():
    """发送测试决策请求"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(15.0)
        sock.connect(("127.0.0.1", 47100))

        request = json.dumps({
            "type": "decision",
            "title": "Quick test:",
            "options": ["OK", "Cancel"]
        }) + "\n"

        sock.sendall(request.encode("utf-8"))

        response_data = b""
        while b"\n" not in response_data:
            response_data += sock.recv(1024)

        sock.close()
        return json.loads(response_data.decode("utf-8"))
    except Exception as e:
        return {"ok": False, "error": str(e)}

def main():
    print("="*60)
    print("  CodeBuddy AI BOX 快速验证测试")
    print("="*60)

    # 检查守护进程
    print("\n[1/2] 检查守护进程...")
    if not check_daemon():
        print("❌ 守护进程未运行")
        print("\n请在另一个终端运行:")
        print("  python atkbox_daemon.py --port COM6")
        return 1

    print("✅ 守护进程运行中")

    # 发送测试请求
    print("\n[2/2] 发送测试决策请求...")
    print("\n请选择输入方式:")
    print("  - ATK BOX: 触摸屏幕上的选项 + confirm")
    print("  - PC 键盘: 在守护进程终端输入 1 或 2")
    print("\n⏱️  等待响应（15 秒超时）...\n")

    result = send_test_decision()

    if result.get("ok"):
        chosen = result.get("chosen")
        source = result.get("source", "unknown")
        options = ["OK", "Cancel"]

        print("\n" + "="*60)
        print("  ✅ 测试通过！")
        print("="*60)
        print(f"\n选择: [{chosen}] {options[chosen]}")
        print(f"来源: {source.upper()}")

        if source == "box":
            print("\n✓ BOX 触摸功能正常")
            print("✓ BOX 显示格式应为: '1. OK' (数字+文本)")
        elif source == "keyboard":
            print("\n✓ PC 键盘输入功能正常")
            print("✓ 双向输入支持工作中")

        print("\n系统已就绪，可以运行完整测试套件:")
        print("  python test_wizard.py")
        return 0
    else:
        print("\n" + "="*60)
        print("  ❌ 测试失败")
        print("="*60)
        print(f"\n错误: {result.get('error')}")
        print("\n可能原因:")
        print("  - Dongle 未连接到 COM6")
        print("  - ATK BOX 未正常启动")
        print("  - ESP-NOW 通信失败")
        print("\n请检查:")
        print("  1. 硬件连接: python -m serial.tools.list_ports")
        print("  2. BOX 串口输出: python -m platformio device monitor -p COM11")
        print("  3. 守护进程日志: type %USERPROFILE%\\.claude\\atkbox_daemon.log")
        return 1

if __name__ == "__main__":
    sys.exit(main())
