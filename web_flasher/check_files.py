#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证打包所需文件是否齐全
"""
import os
import sys

# 修复 Windows 控制台编码问题
if sys.platform == 'win32':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

def check_file(path, min_size_kb=0):
    """检查文件是否存在且大小合理"""
    if not os.path.exists(path):
        print(f"[X] 缺失: {path}")
        return False

    size = os.path.getsize(path)
    size_kb = size / 1024

    if min_size_kb > 0 and size_kb < min_size_kb:
        print(f"[X] 文件过小: {path} ({size_kb:.1f} KB, 期望 >{min_size_kb} KB)")
        return False

    print(f"[OK] {path} ({size_kb:.1f} KB)")
    return True

def check_dir(path, required=True):
    """检查目录是否存在"""
    if not os.path.isdir(path):
        if required:
            print(f"[X] 目录缺失: {path}")
            return False
        else:
            print(f"[!] 可选目录不存在: {path}")
            return True

    files = os.listdir(path)
    print(f"[OK] {path}/ ({len(files)} 个文件)")
    return True

def main():
    print("=" * 60)
    print("  CodeBuddy Web Flasher - 打包文件检查")
    print("=" * 60)
    print()

    all_ok = True

    print("核心文件:")
    all_ok &= check_file("index.html", min_size_kb=20)
    all_ok &= check_file("serve.py", min_size_kb=1)
    all_ok &= check_file("launcher.py", min_size_kb=2)
    all_ok &= check_file("build_launcher.py", min_size_kb=2)
    print()

    print("Vendor 库 (离线支持):")
    all_ok &= check_file("vendor/esptool-bundle.js", min_size_kb=100)
    print()

    print("固件目录:")
    all_ok &= check_dir("firmware", required=True)
    all_ok &= check_dir("firmware/k10", required=True)
    all_ok &= check_dir("firmware/dongle", required=True)
    print()

    print("资源目录:")
    all_ok &= check_dir("images", required=True)
    print()

    print("Electron 文件:")
    electron_ok = check_file("electron-main.js", min_size_kb=2)
    electron_ok &= check_file("package.json", min_size_kb=0.5)
    if not electron_ok:
        print("  (如果不打 Electron 包可忽略)")
    print()

    print("=" * 60)
    if all_ok:
        print("[OK] 所有必需文件就绪，可以开始打包")
        print()
        print("PyInstaller 打包:")
        print("  python build_launcher.py")
        print()
        print("Electron 打包:")
        print("  npm install")
        print("  npm run build:electron")
    else:
        print("[X] 有文件缺失，请先完成以下步骤:")
        print()
        if not os.path.exists("vendor/esptool-bundle.js"):
            print("1. 下载 esptool-js:")
            print("   node download_vendor.js")
        print()
        print("2. 确认固件文件已放在 firmware/ 目录")

    print("=" * 60)
    sys.exit(0 if all_ok else 1)

if __name__ == "__main__":
    main()
