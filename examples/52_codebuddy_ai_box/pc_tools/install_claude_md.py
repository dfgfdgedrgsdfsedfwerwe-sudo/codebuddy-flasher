#!/usr/bin/env python3
"""安装/卸载 ATK BOX 全局 CLAUDE.md 指令

自动配置 Claude Code 使其在多选决策时同步显示到硬件。

用法:
    python install_claude_md.py           # 安装
    python install_claude_md.py --uninstall  # 卸载
"""
import sys
from pathlib import Path

CLAUDE_MD_PATH = Path.home() / ".claude" / "CLAUDE.md"

ATKBOX_SECTION = """
## ATK BOX Hardware Integration

When you need user input via `AskUserQuestion` with **2-4 discrete options** (multiple-choice decisions), **also call** `mcp__atkbox__ask_on_atkbox` with the same question to display it on the ATK BOX hardware device in parallel.

**Trigger condition**: Only when the question has 2-4 predefined options (suitable for touch screen selection).

**Do NOT use for**:
- Open-ended text input questions
- Yes/No questions with 1 option
- Questions requiring more than 4 options

**Usage pattern**:
```
When asking "Which database?" with options ["MySQL", "PostgreSQL", "MongoDB"]:
1. Call AskUserQuestion(...) for PC dialog
2. Simultaneously call mcp__atkbox__ask_on_atkbox(title="Which database?", options=["MySQL", "PostgreSQL", "MongoDB"])
3. User can respond via either PC or ATK BOX hardware
```

**Error handling**: If MCP tool fails (daemon not running, hardware disconnected), gracefully fall back to PC-only input without blocking the conversation.

**Character limits**:
- Title: max 31 characters (English recommended for display clarity)
- Each option: max 23 characters

This creates a dual-interface experience where technical decisions appear both in the terminal and on the physical ATK BOX touch screen.
"""

SECTION_MARKER_START = "## ATK BOX Hardware Integration"
SECTION_MARKER_END = "This creates a dual-interface experience"


def install():
    """安装 ATK BOX 指令到全局 CLAUDE.md"""
    # 读取现有内容（如果文件存在）
    if CLAUDE_MD_PATH.exists():
        with open(CLAUDE_MD_PATH, 'r', encoding='utf-8') as f:
            content = f.read()

        # 检查是否已存在 ATK BOX 配置段
        if SECTION_MARKER_START in content:
            print("[跳过] ATK BOX 配置段已存在于 ~/.claude/CLAUDE.md")
            return

        # 在文件末尾追加
        with open(CLAUDE_MD_PATH, 'a', encoding='utf-8') as f:
            f.write("\n" + ATKBOX_SECTION)
        print("[追加] ATK BOX 配置段已添加到现有 CLAUDE.md")
    else:
        # 创建新文件
        CLAUDE_MD_PATH.parent.mkdir(parents=True, exist_ok=True)
        with open(CLAUDE_MD_PATH, 'w', encoding='utf-8') as f:
            f.write("# Global Claude Code Configuration\n")
            f.write(ATKBOX_SECTION)
        print("[创建] ~/.claude/CLAUDE.md 已创建")

    print(f"     配置文件: {CLAUDE_MD_PATH}")
    print()
    print("下一步:")
    print("  1. [重要] 必须完全重启 Claude Code（exit 退出所有窗口，再 claude 启动）")
    print("  2. 确保 CodeBuddyBridge.exe 已启动（daemon 和 MCP 都要运行）")
    print("  3. 在新会话中问一个 2-4 选项的问题，观察是否双重调用")
    print("  4. 示例提示词: '数据库选 MySQL、PostgreSQL 还是 MongoDB？'")


def uninstall():
    """从 CLAUDE.md 移除 ATK BOX 配置段"""
    if not CLAUDE_MD_PATH.exists():
        print("[跳过] ~/.claude/CLAUDE.md 不存在")
        return

    with open(CLAUDE_MD_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    # 查找并移除 ATK BOX 配置段
    if SECTION_MARKER_START not in content:
        print("[跳过] 未找到 ATK BOX 配置段")
        return

    # 简单策略：移除从 marker_start 到文件末尾的内容
    # （假设 ATK BOX 段在最后，如果用户手动编辑过需谨慎）
    lines = content.split('\n')
    new_lines = []
    skip = False

    for line in lines:
        if SECTION_MARKER_START in line:
            skip = True
            continue
        if not skip:
            new_lines.append(line)

    # 写回文件
    with open(CLAUDE_MD_PATH, 'w', encoding='utf-8') as f:
        f.write('\n'.join(new_lines).rstrip() + '\n')

    print(f"[OK] ATK BOX 配置段已从 {CLAUDE_MD_PATH} 移除")


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--uninstall":
        uninstall()
    else:
        install()


if __name__ == "__main__":
    main()
