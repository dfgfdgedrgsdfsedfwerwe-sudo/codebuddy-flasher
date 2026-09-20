#!/usr/bin/env python3
"""MCP Server - ATK BOX 决策推送工具

提供 MCP 工具让 Claude 主动推送决策选项到 AI BOX，用户在硬件上选择后返回结果。

工具:
- ask_on_atkbox: 在 AI BOX 上显示决策选项，等待用户触摸选择

运行:
    python mcp_atkbox_server.py

配置到 ~/.claude/mcp_servers.json:
{
  "atkbox": {
    "command": "python",
    "args": ["C:/Users/4090/Desktop/.../mcp_atkbox_server.py"],
    "type": "stdio"
  }
}
"""
import json
import sys
import socket
from typing import Any

# MCP 协议版本
MCP_VERSION = "2024-11-05"

DAEMON_HOST = "127.0.0.1"
DAEMON_PORT = 47100
TIMEOUT = 40.0


def send_decision_to_daemon(title: str, options: list) -> dict:
    """通过 daemon IPC 发送决策请求到 AI BOX

    Returns:
        {"ok": True, "chosen": 0-3} 或 {"ok": False, "error": "..."}
    """
    if len(options) < 1 or len(options) > 4:
        return {"ok": False, "error": "Options must be 1-4 items"}

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(TIMEOUT)
        sock.connect((DAEMON_HOST, DAEMON_PORT))

        request = {
            "type": "decision",
            "title": title[:31],
            "options": [opt[:23] for opt in options]
        }

        sock.sendall(json.dumps(request).encode('utf-8') + b'\n')

        # 阻塞等待用户选择
        data = b""
        while b"\n" not in data:
            chunk = sock.recv(1024)
            if not chunk:
                break
            data += chunk

        sock.close()

        if data:
            response = json.loads(data.decode('utf-8').strip())
            return response
        else:
            return {"ok": False, "error": "No response from daemon"}

    except socket.timeout:
        return {"ok": False, "error": "Timeout waiting for user decision"}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def handle_initialize(request_id: int, params: dict):
    """处理 initialize 请求"""
    response = {
        "jsonrpc": "2.0",
        "id": request_id,
        "result": {
            "protocolVersion": MCP_VERSION,
            "serverInfo": {
                "name": "atkbox",
                "version": "1.0.0"
            },
            "capabilities": {
                "tools": {}
            }
        }
    }
    print(json.dumps(response), flush=True)


def handle_list_tools(request_id: int):
    """列出可用工具"""
    response = {
        "jsonrpc": "2.0",
        "id": request_id,
        "result": {
            "tools": [
                {
                    "name": "ask_on_atkbox",
                    "description": "Display a decision prompt on the ATK BOX hardware device and wait for user's touch selection. Returns the index of chosen option (0-3). Supports 1-4 options, each max 23 chars. Title max 31 chars. User interacts via physical touch screen on the device. **Use this in parallel with AskUserQuestion when asking multiple-choice questions (2-4 options)** to create a dual-interface experience where the user can respond via either PC terminal or physical hardware.",
                    "inputSchema": {
                        "type": "object",
                        "properties": {
                            "title": {
                                "type": "string",
                                "description": "Question or prompt title (max 31 chars, English recommended)"
                            },
                            "options": {
                                "type": "array",
                                "items": {"type": "string"},
                                "minItems": 1,
                                "maxItems": 4,
                                "description": "1-4 options for user to choose from (each max 23 chars)"
                            }
                        },
                        "required": ["title", "options"]
                    }
                }
            ]
        }
    }
    print(json.dumps(response), flush=True)


def handle_call_tool(request_id: int, params: dict):
    """执行工具调用"""
    tool_name = params.get("name")
    arguments = params.get("arguments", {})

    if tool_name == "ask_on_atkbox":
        title = arguments.get("title", "Choose:")
        options = arguments.get("options", [])

        # 调用 daemon
        result = send_decision_to_daemon(title, options)

        if result.get("ok"):
            chosen = result.get("chosen", 255)
            if chosen == 255:
                content = "User cancelled or timeout"
                is_error = False
            else:
                content = f"User chose option {chosen}: {options[chosen] if chosen < len(options) else 'N/A'}"
                is_error = False
        else:
            content = f"Error: {result.get('error', 'Unknown error')}"
            is_error = True

        response = {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": {
                "content": [
                    {
                        "type": "text",
                        "text": content
                    }
                ],
                "isError": is_error
            }
        }
    else:
        response = {
            "jsonrpc": "2.0",
            "id": request_id,
            "error": {
                "code": -32601,
                "message": f"Unknown tool: {tool_name}"
            }
        }

    print(json.dumps(response), flush=True)


def main():
    """MCP stdio server 主循环"""
    # 写入欢迎日志到 stderr（不影响 MCP 协议）
    print("[MCP] ATK BOX Decision Server started", file=sys.stderr, flush=True)

    for line in sys.stdin:
        try:
            request = json.loads(line)
            method = request.get("method")
            request_id = request.get("id")
            params = request.get("params", {})

            if method == "initialize":
                handle_initialize(request_id, params)
            elif method == "tools/list":
                handle_list_tools(request_id)
            elif method == "tools/call":
                handle_call_tool(request_id, params)
            else:
                # 未知方法，返回错误
                response = {
                    "jsonrpc": "2.0",
                    "id": request_id,
                    "error": {
                        "code": -32601,
                        "message": f"Method not found: {method}"
                    }
                }
                print(json.dumps(response), flush=True)

        except json.JSONDecodeError:
            # 忽略无效 JSON
            continue
        except Exception as e:
            print(f"[MCP] Error: {e}", file=sys.stderr, flush=True)


if __name__ == "__main__":
    main()
