#!/usr/bin/env python3
"""Claude Code Hook Client - 瘦客户端，连 atkbox_daemon 发 IPC 请求

Usage: python hook_client.py <event_type>
  event_type: user_prompt | pre_tool | post_tool | stop

Hook 事件处理：
- user_prompt: 用户提交 prompt → 发送 THINKING 状态
- pre_tool: 工具执行前 → 拦截 Write/Edit/NotebookEdit，发送审批请求（阻塞）
- post_tool: 工具执行后 → 发送 RUNNING 状态 + 工具名
- stop: 任务完成 → 发送 DONE 状态
"""
import sys
import json
import socket

DAEMON_HOST = "127.0.0.1"
DAEMON_PORT = 47100
TIMEOUT = 5.0  # 连接超时（审批请求会阻塞更久，但 socket 本身不超时）


def send_request(request: dict) -> dict:
    """发送 JSON 请求到 daemon，返回响应

    Args:
        request: JSON 请求字典

    Returns:
        响应字典 或 {"ok": False, "error": "..."}
    """
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(TIMEOUT)
        sock.connect((DAEMON_HOST, DAEMON_PORT))
        sock.sendall((json.dumps(request) + "\n").encode("utf-8"))
        response = sock.recv(4096).decode("utf-8").strip()
        sock.close()
        return json.loads(response)
    except Exception as e:
        return {"ok": False, "error": str(e)}


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "Missing event_type argument"}), file=sys.stderr)
        sys.exit(1)

    event_type = sys.argv[1]
    hook_input = json.load(sys.stdin)  # Claude Code 通过 stdin 传 JSON

    if event_type == "user_prompt":
        # 用户提交 prompt → THINKING 状态
        prompt = hook_input.get("prompt", "")[:50]
        resp = send_request({"type": "status", "state": "THINKING", "detail": prompt})
        # 非阻塞，不需要输出 JSON，静默退出
        sys.exit(0)

    elif event_type == "pre_tool":
        # 工具执行前 → 只拦截 Write/Edit/NotebookEdit
        tool_name = hook_input["tool_name"]
        tool_input = hook_input.get("tool_input", {})

        # === 监测：AskUserQuestion 工具调用 ===
        if tool_name == "AskUserQuestion":
            print(f"[MONITOR] AskUserQuestion detected!", file=sys.stderr)
            questions = tool_input.get("questions", [])
            print(f"[MONITOR] Number of questions: {len(questions)}", file=sys.stderr)
            if questions:
                print(f"[MONITOR] First question: {questions[0].get('question', 'N/A')}", file=sys.stderr)

            # 推送通知到 daemon（记录到日志）
            send_request({
                "type": "status",
                "state": "RUNNING",
                "tool": "AskUserQuestion",
                "detail": f"{len(questions)} Q"
            })

            # 不拦截，让 Claude Code 正常处理（不输出 JSON = allow）
            sys.exit(0)

        # 只拦截写操作工具
        if tool_name in ["Write", "Edit", "NotebookEdit"]:
            file_path = tool_input.get("file_path", tool_input.get("notebook_path", "unknown"))
            preview = tool_input.get("content", tool_input.get("new_source", ""))[:200]

            resp = send_request({
                "type": "approval",
                "tool": tool_name,
                "file": file_path,
                "preview": preview
            })

            if resp.get("ok") and resp.get("decision") == "allow":
                print(json.dumps({"permissionDecision": "allow"}))
            else:
                print(json.dumps({"permissionDecision": "deny"}))
        # 其他工具：不输出 JSON，默认 allow
        sys.exit(0)

    elif event_type == "post_tool":
        # 工具执行后 → RUNNING 状态 + 工具名
        tool_name = hook_input["tool_name"]
        tool_input = hook_input.get("tool_input", {})

        # 提取目标文件/参数（优先级：file_path > path > pattern > command）
        detail = (tool_input.get("file_path") or
                 tool_input.get("path") or
                 tool_input.get("pattern") or
                 tool_input.get("command", ""))[:30]

        resp = send_request({"type": "status", "state": "RUNNING", "tool": tool_name, "detail": detail})
        sys.exit(0)

    elif event_type == "stop":
        # 任务完成 → DONE 状态
        resp = send_request({"type": "status", "state": "DONE"})
        sys.exit(0)

    # ==========================================================
    # 实验事件监测（探测 AskUserQuestion 触发哪个 hook）
    # 所有实验 handler 把完整 hook_input 记录到 daemon 日志
    # ==========================================================
    elif event_type in (
        "notify_agent_input", "notify_permission", "notify_elicitation",
        "notify_other", "elicitation", "message_display", "permission_request"
    ):
        # 提取关键字段用于日志
        keys = list(hook_input.keys())
        # 尝试提取问题/选项相关字段
        detail_parts = [f"event={event_type}", f"keys={keys}"]

        # 常见字段探测
        for field in ("notification_type", "message", "question", "options",
                      "prompt", "tool_name", "questions", "title"):
            if field in hook_input:
                val = str(hook_input[field])[:100]
                detail_parts.append(f"{field}={val}")

        detail = " | ".join(detail_parts)[:200]

        # 推送到 daemon 记录日志（复用 status 通道）
        send_request({
            "type": "status",
            "state": "RUNNING",
            "tool": f"[EXP:{event_type}]",
            "detail": detail[:30]
        })

        # 同时用 stderr 输出完整信息（Claude Code 会记录）
        print(f"[EXPERIMENT] {event_type}: {json.dumps(hook_input)[:500]}", file=sys.stderr)

        # 不干预流程，静默放行
        sys.exit(0)

    else:
        print(json.dumps({"error": f"Unknown event_type: {event_type}"}), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
