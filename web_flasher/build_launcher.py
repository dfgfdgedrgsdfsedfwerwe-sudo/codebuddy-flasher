#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PyInstaller 打包脚本
用法: python build_launcher.py
生成: dist/CodeBuddy_Flasher.exe (单文件，约 10-15MB)
"""
import os
import sys
import subprocess
import shutil

# 修复 Windows 控制台编码问题
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

def check_pyinstaller():
    """检查 PyInstaller 是否安装"""
    try:
        import PyInstaller
        print(f"[OK] PyInstaller {PyInstaller.__version__} 已安装")
        return True
    except ImportError:
        print("[X] PyInstaller 未安装")
        print("\n请运行以下命令安装:")
        print("    pip install pyinstaller")
        return False

def build():
    """执行 PyInstaller 打包"""
    if not check_pyinstaller():
        sys.exit(1)

    print("\n开始打包 CodeBuddy Web Flasher...")
    print("=" * 60)

    # PyInstaller 命令（用 python -m 方式调用，避免 PATH 问题）
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",                    # 打包为单个 exe
        "--console",                    # 显示控制台窗口（用户可看服务器状态，关窗即停止）
        "--name=CodeBuddy_Flasher",     # exe 名称
        "--add-data=index.html;.",      # 添加 HTML 文件
        "--add-data=vendor;vendor",     # 添加 vendor 目录（esptool-js）
        "--add-data=firmware;firmware", # 添加固件目录
        "--add-data=images;images",     # 添加图片目录
        "--clean",                      # 清理缓存
        "launcher.py"
    ]

    # 若存在 .ico 图标则使用
    if os.path.exists("images/icon.ico"):
        cmd.insert(4, "--icon=images/icon.ico")

    print("执行命令:")
    print(" ".join(cmd))
    print()

    result = subprocess.run(cmd)

    if result.returncode == 0:
        print("\n" + "=" * 60)
        print("[OK] 打包成功!")
        print(f"[OK] 输出文件: dist/CodeBuddy_Flasher.exe")

        # 显示文件大小
        exe_path = "dist/CodeBuddy_Flasher.exe"
        if os.path.exists(exe_path):
            size_mb = os.path.getsize(exe_path) / (1024 * 1024)
            print(f"[OK] 文件大小: {size_mb:.1f} MB")
        print("=" * 60)
    else:
        print("\n[X] 打包失败，请检查错误信息")
        sys.exit(1)

def clean():
    """清理构建文件"""
    dirs_to_remove = ["build", "dist", "__pycache__"]
    files_to_remove = ["CodeBuddy_Flasher.spec"]

    print("清理构建文件...")
    for d in dirs_to_remove:
        if os.path.exists(d):
            shutil.rmtree(d)
            print(f"  删除目录: {d}")

    for f in files_to_remove:
        if os.path.exists(f):
            os.remove(f)
            print(f"  删除文件: {f}")

    print("清理完成")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="CodeBuddy Flasher 打包工具")
    parser.add_argument("--clean", action="store_true", help="清理构建文件")
    args = parser.parse_args()

    if args.clean:
        clean()
    else:
        build()
