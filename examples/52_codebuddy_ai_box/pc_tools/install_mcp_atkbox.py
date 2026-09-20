#!/usr/bin/env python3
"""快速安装 ATK BOX MCP Server 到 Claude Code

自动将 mcp_atkbox_server.py 配置到 ~/.claude/mcp_servers.json

用法:
    python install_mcp_atkbox.py           # 安装
    python install_mcp_atkbox.py --uninstall  # 卸载
"""
import sys
import json
import shutil
from pathlib import Path

MCP_CONFIG_FILE = Path.home() / ".claude.json"  # Claude Code 2.1.206+ 实际读取的配置文件


def get_mcp_server_path() -> str:
    """获取 mcp_atkbox_server.py 的永久路径

    在 PyInstaller 打包环境 (onefile) 下，__file__ 指向临时目录 _MEIXXXXX，
    exe 退出后临时目录被删除，导致 Claude Code 找不到 MCP server 脚本。

    解决方案（对齐 install_hooks.py）：
    1. 检测是否在 frozen 环境（sys.frozen 存在）
    2. 如果是，将 mcp_atkbox_server.py 复制到永久目录 ~/.codebuddy_bridge/
    3. 返回永久路径供写入 .claude.json

    Returns:
        mcp_atkbox_server.py 的绝对路径（正斜杠格式，适配 JSON）
    """
    # 非打包环境：直接使用当前目录的 mcp_atkbox_server.py
    if not getattr(sys, 'frozen', False):
        pc_tools_dir = Path(__file__).parent.absolute()
        server_script = pc_tools_dir / "mcp_atkbox_server.py"
        return str(server_script).replace("\\", "/")

    # PyInstaller 打包环境：从临时目录复制到永久位置
    permanent_dir = Path.home() / ".codebuddy_bridge"
    permanent_dir.mkdir(parents=True, exist_ok=True)
    permanent_server = permanent_dir / "mcp_atkbox_server.py"

    # 源文件在 _MEIPASS 临时目录（PyInstaller 解压位置）
    if hasattr(sys, '_MEIPASS'):
        temp_server = Path(sys._MEIPASS) / "mcp_atkbox_server.py"
    else:
        # 备用方案：从当前 exe 目录查找（不应该走到这里）
        temp_server = Path(__file__).parent / "mcp_atkbox_server.py"

    # 复制到永久位置（覆盖旧版本）
    if temp_server.exists():
        shutil.copy2(temp_server, permanent_server)
        print(f"[复制] {temp_server} -> {permanent_server}")
    else:
        # 如果源文件不存在，检查永久位置是否已有旧版本
        if not permanent_server.exists():
            raise FileNotFoundError(f"无法找到 mcp_atkbox_server.py（源: {temp_server}）")
        print(f"[警告] 源文件不存在，使用现有永久副本: {permanent_server}")

    return str(permanent_server).replace("\\", "/")


# 获取 MCP server 路径（打包环境下会触发复制）
SERVER_PATH = get_mcp_server_path()

SERVER_NAME = "atkbox"


def load_config() -> dict:
    """读取现有 MCP 配置"""
    if MCP_CONFIG_FILE.exists():
        try:
            with open(MCP_CONFIG_FILE, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception as e:
            print(f"[警告] 读取现有配置失败: {e}，将创建新配置")
            return {}
    return {}


def save_config(config: dict):
    """保存 MCP 配置"""
    MCP_CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(MCP_CONFIG_FILE, 'w', encoding='utf-8') as f:
        json.dump(config, f, indent=2, ensure_ascii=False)


def install():
    """安装 MCP server"""
    config = load_config()

    # 确保 mcpServers 字段存在（Claude Code 2.1.206+ 要求）
    if "mcpServers" not in config:
        config["mcpServers"] = {}

    # 添加 atkbox server 到 mcpServers 字段
    config["mcpServers"][SERVER_NAME] = {
        "command": "python",
        "args": [SERVER_PATH],
        "type": "stdio"
    }

    save_config(config)

    print("[OK] ATK BOX MCP Server 已安装")
    print(f"     配置文件: {MCP_CONFIG_FILE}")
    print(f"     Server 脚本: {SERVER_PATH}")
    print(f"     已写入 mcpServers 字段")
    print()
    print("下一步:")
    print("  1. 确保 CodeBuddyBridge.exe 正在运行（点'启动服务'）")
    print("  2. [重要] 必须完全重启 Claude Code CLI（exit 退出所有窗口，再 claude 启动）")
    print("  3. 输入 /mcp 或 /tools 验证 ask_on_atkbox 工具出现")


def uninstall():
    """卸载 MCP server"""
    config = load_config()

    removed = False
    if "mcpServers" in config and SERVER_NAME in config["mcpServers"]:
        del config["mcpServers"][SERVER_NAME]
        removed = True

    if removed:
        save_config(config)
        print(f"[OK] ATK BOX MCP Server 已卸载")
    else:
        print("[跳过] 未找到 atkbox server 配置")


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--uninstall":
        uninstall()
    else:
        install()


if __name__ == "__main__":
    main()
