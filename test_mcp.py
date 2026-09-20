#!/usr/bin/env python3
"""快速测试 MCP 服务器响应"""
import subprocess
import json
import sys

def test_initialize():
    """测试 initialize 请求"""
    request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {"protocolVersion": "2024-11-05"}
    }

    proc = subprocess.Popen(
        ["python", "examples/52_codebuddy_ai_box/pc_tools/mcp_atkbox_server.py"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    stdout, stderr = proc.communicate(input=json.dumps(request) + "\n", timeout=2)

    print("=== STDERR (日志) ===")
    print(stderr)
    print("\n=== STDOUT (MCP响应) ===")
    print(stdout)

    if stdout.strip():
        response = json.loads(stdout.strip())
        print("\n=== 解析后的响应 ===")
        print(json.dumps(response, indent=2, ensure_ascii=False))
        return response
    return None


def test_list_tools():
    """测试 tools/list 请求"""
    request = {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "tools/list",
        "params": {}
    }

    proc = subprocess.Popen(
        ["python", "examples/52_codebuddy_ai_box/pc_tools/mcp_atkbox_server.py"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    stdout, stderr = proc.communicate(input=json.dumps(request) + "\n", timeout=2)

    print("=== tools/list 响应 ===")
    if stdout.strip():
        response = json.loads(stdout.strip())
        print(json.dumps(response, indent=2, ensure_ascii=False))
        return response
    return None


if __name__ == "__main__":
    import sys
    import io
    # 强制 UTF-8 输出
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

    print("[TEST] ATK BOX MCP Server\n")

    print("[1] 测试 initialize...")
    init_resp = test_initialize()

    if init_resp and init_resp.get("result"):
        print("[OK] Initialize 成功\n")
    else:
        print("[FAIL] Initialize 失败\n")
        sys.exit(1)

    print("\n[2] 测试 tools/list...")
    tools_resp = test_list_tools()

    if tools_resp and tools_resp.get("result", {}).get("tools"):
        tools = tools_resp["result"]["tools"]
        print(f"\n[OK] 找到 {len(tools)} 个工具:")
        for tool in tools:
            print(f"   - {tool['name']}: {tool['description'][:80]}...")
    else:
        print("[FAIL] tools/list 失败")
        sys.exit(1)

    print("\n[SUCCESS] MCP 服务器测试通过！")
    print("\n下一步: 重启 Claude Code CLI 后输入 /mcp 验证")
