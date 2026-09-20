#!/usr/bin/env python3
"""
CodeBuddy Web Flasher 启动器
用 PyInstaller 打包后，双击 exe 即可启动本地服务器并自动打开浏览器
"""
import http.server
import socketserver
import webbrowser
import os
import sys
import threading
import time

PORT = 8000

def get_resource_path():
    """获取资源目录（PyInstaller 打包后是临时目录 _MEIPASS）"""
    if getattr(sys, 'frozen', False):
        # 打包后运行
        return sys._MEIPASS
    else:
        # 开发环境运行
        return os.path.dirname(os.path.abspath(__file__))

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        # 切换到资源目录，让 HTTP 服务器从这里提供文件
        os.chdir(get_resource_path())
        super().__init__(*args, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()

    def log_message(self, format, *args):
        # 静默日志，避免终端刷屏
        pass

def open_browser_delayed():
    """延迟 1 秒后打开浏览器（等服务器启动完成）"""
    time.sleep(1)
    url = f"http://localhost:{PORT}"
    try:
        webbrowser.open(url)
        print(f"已在浏览器中打开: {url}")
    except Exception as e:
        print(f"无法自动打开浏览器: {e}")
        print(f"请手动打开: {url}")

def main():
    print("=" * 60)
    print("  CodeBuddy Web Flasher - 启动器")
    print("=" * 60)
    print(f"资源目录: {get_resource_path()}")
    print(f"监听端口: {PORT}")
    print("浏览器即将自动打开...")
    print("按 Ctrl+C 停止服务器")
    print("=" * 60)

    # 启动浏览器打开线程
    browser_thread = threading.Thread(target=open_browser_delayed, daemon=True)
    browser_thread.start()

    # 启动 HTTP 服务器
    socketserver.TCPServer.allow_reuse_address = True
    try:
        with socketserver.TCPServer(("127.0.0.1", PORT), Handler) as httpd:
            print(f"\n✓ 服务器运行中: http://localhost:{PORT}")
            print("✓ 请在浏览器中操作，不要关闭此窗口\n")
            httpd.serve_forever()
    except OSError as e:
        if "address already in use" in str(e).lower() or "只允许使用一次" in str(e):
            print(f"\n✗ 错误: 端口 {PORT} 已被占用")
            print("请关闭占用该端口的程序后重试")
        else:
            print(f"\n✗ 启动失败: {e}")
        input("\n按回车键退出...")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n服务器已停止")
        sys.exit(0)

if __name__ == "__main__":
    main()
