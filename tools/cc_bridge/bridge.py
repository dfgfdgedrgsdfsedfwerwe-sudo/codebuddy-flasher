"""cc_bridge - 用 Claude Agent SDK 的 can_use_tool 回调, 把决策点经串口
推到 Dongle→K10, 阻塞等 K10 触摸回执后再放行 Claude 的下一步。

运行: python bridge.py <UART0_COM> "给 Claude 的任务"
依赖: claude-agent-sdk, pyserial (见 requirements.txt)。

注: SDK 确切类名 / system_prompt append 结构 / 多选答案回填字段, 以安装版本的
    claude-agent-sdk 文档为准 (见设计文档 2.1 / 6.1)。
"""
import asyncio
import sys

from serial_link import SerialLink
from claude_agent_sdk import query, ClaudeAgentOptions
from claude_agent_sdk import PermissionResultAllow, PermissionResultDeny

APPEND_PROMPT = "遇到有多个合理方案的决策点时，优先用 AskUserQuestion 让用户选，而不是直接选定。"
PERM_OPTS = ["允许", "拒绝", "总是允许"]
DECISION_TIMEOUT = 120.0


def _summarize(tool_name, tool_input):
    if tool_name == "Bash":
        return f"运行: {tool_input.get('command', '')}"[:31]
    if tool_name in ("Write", "Edit"):
        return f"{tool_name}: {tool_input.get('file_path', '')}"[:31]
    return f"{tool_name}"[:31]


class Bridge:
    def __init__(self, port):
        self.link = SerialLink(port)

    def open(self):
        self.link.open()

    async def can_use_tool(self, tool_name, tool_input, context):
        # AskUserQuestion: Claude 主动多选题
        if tool_name == "AskUserQuestion":
            q = tool_input.get("questions", [{}])[0]
            title = q.get("question", "请选择")[:31]
            opts = [o.get("label", str(i)) for i, o in enumerate(q.get("options", []))][:4]
            did = self.link.send_decision(1, title, opts)
            try:
                idx = await asyncio.to_thread(self.link.wait_reply, did, DECISION_TIMEOUT)
            except TimeoutError:
                return PermissionResultDeny(message="K10 未选择 (超时)")
            if idx == 0xFF:
                return PermissionResultDeny(message="K10 取消/离线")
            # 把选择作为答案回填 (SDK 具体回填多选答案的字段实现时核实)
            chosen = opts[idx] if idx < len(opts) else opts[0]
            return PermissionResultAllow(updated_input={**tool_input, "_k10_choice": chosen})
        # 其它工具: 权限确认
        title = _summarize(tool_name, tool_input)
        did = self.link.send_decision(0, title, PERM_OPTS)
        try:
            idx = await asyncio.to_thread(self.link.wait_reply, did, DECISION_TIMEOUT)
        except TimeoutError:
            return PermissionResultDeny(message="K10 未确认 (超时)")
        if idx == 0:   # 允许
            return PermissionResultAllow()
        if idx == 2:   # 总是允许
            return PermissionResultAllow()
        return PermissionResultDeny(message="用户在 K10 上拒绝")


async def main():
    if len(sys.argv) < 3:
        print("用法: python bridge.py <COM口> <给Claude的任务>")
        return
    port, prompt = sys.argv[1], sys.argv[2]
    b = Bridge(port)
    b.open()
    opts = ClaudeAgentOptions(
        system_prompt={"type": "preset", "preset": "claude_code", "append": APPEND_PROMPT},
        can_use_tool=b.can_use_tool,
    )
    async for msg in query(prompt=prompt, options=opts):
        print(msg)


if __name__ == "__main__":
    asyncio.run(main())
