#!/usr/bin/env python3
"""Hook 安装脚本 - 合并 hook 配置到 ~/.claude/settings.json

将 4 个 hook（UserPromptSubmit / PreToolUse / PostToolUse / Stop）合并进
用户现有的 settings.json，保留其他已有字段（如 env、enabledPlugins）。

Usage:
  python install_hooks.py           # 安装 hooks
  python install_hooks.py --uninstall  # 卸载 hooks
"""
import sys
import json
import shutil
from pathlib import Path

SETTINGS_PATH = Path.home() / ".claude" / "settings.json"

# === PyInstaller 兼容：永久化 hook_client.py ===
def get_hook_client_path() -> str:
    """获取 hook_client.py 的永久路径

    在 PyInstaller 打包环境 (onefile) 下，__file__ 指向临时目录 _MEIXXXXX，
    exe 退出后临时目录被删除，导致 Claude Code 找不到 hook 脚本。

    解决方案：
    1. 检测是否在 frozen 环境（sys.frozen 或 sys._MEIPASS 存在）
    2. 如果是，将 hook_client.py 复制到永久目录 ~/.codebuddy_bridge/
    3. 返回永久路径供写入 settings.json

    Returns:
        hook_client.py 的绝对路径（正斜杠格式，适配 settings.json）
    """
    # 非打包环境：直接使用当前目录的 hook_client.py
    if not getattr(sys, 'frozen', False):
        pc_tools_dir = Path(__file__).parent.absolute()
        hook_client = pc_tools_dir / "hook_client.py"
        return str(hook_client).replace("\\", "/")

    # PyInstaller 打包环境：从临时目录复制到永久位置
    permanent_dir = Path.home() / ".codebuddy_bridge"
    permanent_dir.mkdir(parents=True, exist_ok=True)
    permanent_hook = permanent_dir / "hook_client.py"

    # 源文件在 _MEIPASS 临时目录（PyInstaller 解压位置）
    if hasattr(sys, '_MEIPASS'):
        temp_hook = Path(sys._MEIPASS) / "hook_client.py"
    else:
        # 备用方案：从当前 exe 目录查找（不应该走到这里）
        temp_hook = Path(__file__).parent / "hook_client.py"

    # 复制到永久位置（覆盖旧版本）
    if temp_hook.exists():
        shutil.copy2(temp_hook, permanent_hook)
        print(f"[复制] {temp_hook} -> {permanent_hook}")
    else:
        # 如果源文件不存在，检查永久位置是否已有旧版本
        if not permanent_hook.exists():
            raise FileNotFoundError(f"无法找到 hook_client.py（源: {temp_hook}）")
        print(f"[警告] 源文件不存在，使用现有永久副本: {permanent_hook}")

    return str(permanent_hook).replace("\\", "/")

# 获取 hook_client.py 路径（打包环境下会触发复制）
HOOK_CLIENT = get_hook_client_path()

# Hook 配置
HOOKS_CONFIG = {
    "UserPromptSubmit": [
        {
            "hooks": [
                {"type": "command", "command": f"python {HOOK_CLIENT} user_prompt"}
            ]
        }
    ],
    "PreToolUse": [
        {
            "matcher": "Write|Edit|NotebookEdit",
            "hooks": [
                {"type": "command", "command": f"python {HOOK_CLIENT} pre_tool"}
            ]
        }
    ],
    "PostToolUse": [
        {
            "matcher": "*",
            "hooks": [
                {"type": "command", "command": f"python {HOOK_CLIENT} post_tool"}
            ]
        }
    ],
    "Stop": [
        {
            "hooks": [
                {"type": "command", "command": f"python {HOOK_CLIENT} stop"}
            ]
        }
    ]
}


def load_settings() -> dict:
    """加载现有 settings.json"""
    if SETTINGS_PATH.exists():
        with open(SETTINGS_PATH, 'r', encoding='utf-8') as f:
            return json.load(f)
    return {}


def backup_settings():
    """备份 settings.json"""
    if SETTINGS_PATH.exists():
        backup_path = SETTINGS_PATH.with_suffix('.json.backup')
        shutil.copy2(SETTINGS_PATH, backup_path)
        print(f"[备份] {backup_path}")


def save_settings(settings: dict):
    """保存 settings.json"""
    SETTINGS_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(SETTINGS_PATH, 'w', encoding='utf-8') as f:
        json.dump(settings, f, indent=2, ensure_ascii=False)


def install():
    """安装 hooks"""
    settings = load_settings()

    # 备份
    backup_settings()

    # 合并 hooks（保留现有其他 hook 事件）
    if "hooks" not in settings:
        settings["hooks"] = {}

    for event, config in HOOKS_CONFIG.items():
        settings["hooks"][event] = config

    save_settings(settings)

    print(f"[OK] Hooks 已安装到 {SETTINGS_PATH}")
    print(f"     Hook 客户端: {HOOK_CLIENT}")
    print(f"     已注册事件: {', '.join(HOOKS_CONFIG.keys())}")
    print()
    print("下一步:")
    print("  1. 启动 daemon: python atkbox_daemon.py --port COM6")
    print("  2. 重启 Claude Code 使 hooks 生效")


def uninstall():
    """卸载 hooks"""
    settings = load_settings()

    if "hooks" not in settings:
        print("[跳过] 未找到 hooks 配置")
        return

    # 备份
    backup_settings()

    # 移除我们安装的 hook 事件
    for event in HOOKS_CONFIG.keys():
        if event in settings["hooks"]:
            del settings["hooks"][event]

    # 如果 hooks 为空，删除整个键
    if not settings["hooks"]:
        del settings["hooks"]

    save_settings(settings)

    print(f"[OK] Hooks 已从 {SETTINGS_PATH} 卸载")


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--uninstall":
        uninstall()
    else:
        install()


if __name__ == "__main__":
    main()
