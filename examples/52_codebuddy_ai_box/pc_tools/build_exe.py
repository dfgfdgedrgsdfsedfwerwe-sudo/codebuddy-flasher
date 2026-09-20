#!/usr/bin/env python3
"""CodeBuddy Bridge - PyInstaller 打包脚本

生成独立的 Windows exe 文件，包含所有依赖。

运行方式:
    python build_exe.py

输出:
    dist/CodeBuddyBridge.exe - 单文件可执行程序
"""
import PyInstaller.__main__
import shutil
from pathlib import Path

# 清理旧的构建目录（若目录被占用则跳过，PyInstaller 会覆盖写入）
build_dir = Path("build")
dist_dir = Path("dist")
if build_dir.exists():
    try:
        shutil.rmtree(build_dir)
    except OSError as e:
        print(f"[警告] 无法删除 build/（被占用），继续: {e}")
if dist_dir.exists():
    try:
        shutil.rmtree(dist_dir)
    except OSError as e:
        print(f"[警告] 无法删除 dist/（被占用），继续: {e}")

print("Building CodeBuddy Bridge executable...")

# PyInstaller 参数
PyInstaller.__main__.run([
    'codebuddy_bridge_gui.py',

    # 输出设置
    '--onefile',                    # 打包成单个 exe
    '--windowed',                   # 无控制台窗口（GUI 应用）
    '--name=CodeBuddyBridge',       # 输出文件名

    # 图标（如果有的话）
    # '--icon=icon.ico',

    # 隐藏导入（PyInstaller 可能检测不到的模块）
    '--hidden-import=serial',
    '--hidden-import=serial.tools',
    '--hidden-import=serial.tools.list_ports',

    # 打包 hook_client.py 到 exe 内（安装 hooks 时会复制到 ~/.codebuddy_bridge/）
    # Windows 用 ; 分隔，语法: 源路径;目标目录(. = 根)
    '--add-data=hook_client.py;.',

    # 打包 mcp_atkbox_server.py 到 exe 内（安装 MCP 时会复制到 ~/.codebuddy_bridge/）
    '--add-data=mcp_atkbox_server.py;.',

    # 优化
    '--clean',                      # 清理临时文件

    # 调试（首次打包建议保留，成功后可删除）
    # '--debug=all',                # 输出详细调试信息
])

print("\n" + "="*60)
print("Build complete!")
print(f"Output: {dist_dir / 'CodeBuddyBridge.exe'}")
print("="*60)

# 复制到发布目录（可选）
release_dir = Path("../releases")
if release_dir.exists():
    shutil.copy(dist_dir / "CodeBuddyBridge.exe", release_dir)
    print(f"Copied to: {release_dir / 'CodeBuddyBridge.exe'}")
