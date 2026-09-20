#!/bin/bash
# CodeBuddy K10 开发环境初始化脚本 (Git Bash)
# 项目: 51_K10_wifi (CodeBuddy Wireless)
# 设备型号: K10-UNIHIKER
# 芯片方案: ESP32-S3
# 生成时间: 2026-09-17

set -e

echo "=== CodeBuddy K10 开发环境初始化 ==="
echo ""

# ==================== 1. 系统必备工具检查 ====================
echo "[1/6] 检查系统必备工具..."

tools_ok=true

# Python 检查
if command -v python &>/dev/null; then
    py_version=$(python --version 2>&1)
    echo "  ✓ Python: $py_version"
else
    echo "  ✗ Python 未安装"
    echo "    请从 https://www.python.org/downloads/ 下载安装 Python 3.11+"
    tools_ok=false
fi

# Git 检查
if command -v git &>/dev/null; then
    git_version=$(git --version 2>&1)
    echo "  ✓ Git: $git_version"
else
    echo "  ✗ Git 未安装"
    echo "    请从 https://git-scm.com/downloads 下载安装"
    tools_ok=false
fi

if [ "$tools_ok" = false ]; then
    echo ""
    echo "❌ 系统工具缺失，请先安装后重新运行此脚本"
    exit 1
fi

# ==================== 2. Python 环境与依赖 ====================
echo ""
echo "[2/6] 检查 Python 依赖..."

required_packages=("platformio" "esptool" "pyserial" "pyyaml")
pip_list=$(pip list 2>&1)
missing_packages=()

for pkg in "${required_packages[@]}"; do
    if echo "$pip_list" | grep -q "^$pkg "; then
        echo "  ✓ $pkg 已安装"
    else
        echo "  ✗ $pkg 未安装"
        missing_packages+=("$pkg")
    fi
done

if [ ${#missing_packages[@]} -gt 0 ]; then
    echo ""
    echo "  正在安装缺失的 Python 包..."
    for pkg in "${missing_packages[@]}"; do
        pip install "$pkg"
    done
fi

# ==================== 3. PlatformIO 验证 ====================
echo ""
echo "[3/6] 验证 PlatformIO..."

if pio_version=$(python -m platformio --version 2>&1); then
    echo "  ✓ PlatformIO: $pio_version"
else
    echo "  ✗ PlatformIO 不可用"
    exit 1
fi

# ==================== 4. Git 子模块初始化 ====================
echo ""
echo "[4/6] 初始化 Git 子模块 (lvgl, LovyanGFX)..."

submodules_ok=true
if [ -d "lib/lvgl/src" ]; then
    echo "  ✓ lvgl 子模块已初始化"
else
    echo "  ! lvgl 子模块未初始化，正在初始化..."
    if git submodule update --init --recursive lib/lvgl; then
        echo "  ✓ lvgl 初始化完成"
    else
        echo "  ✗ lvgl 初始化失败"
        submodules_ok=false
    fi
fi

if [ -d "lib/LovyanGFX/src" ]; then
    echo "  ✓ LovyanGFX 子模块已初始化"
else
    echo "  ! LovyanGFX 子模块未初始化，正在初始化..."
    if git submodule update --init --recursive lib/LovyanGFX; then
        echo "  ✓ LovyanGFX 初始化完成"
    else
        echo "  ✗ LovyanGFX 初始化失败"
        submodules_ok=false
    fi
fi

# ==================== 5. 依赖库检查 ====================
echo ""
echo "[5/6] 检查依赖库完整性..."

libs_ok=true
if [ -d "lib/ESP32_JPEG/src" ]; then
    echo "  ✓ ESP32_JPEG 库存在"
else
    echo "  ! ESP32_JPEG 库缺失"
    echo "    如果 lib.zip 存在，请解压到 lib/ 目录"
    libs_ok=false
fi

if [ -d "lib/Arduino_DriveBus/src" ]; then
    echo "  ✓ Arduino_DriveBus 库存在"
else
    echo "  ! Arduino_DriveBus 库缺失"
    echo "    如果 lib.zip 存在，请解压到 lib/ 目录"
    libs_ok=false
fi

# ==================== 6. 首次编译验证 ====================
echo ""
echo "[6/6] 首次编译验证..."

if [ "$submodules_ok" = false ] || [ "$libs_ok" = false ]; then
    echo "  ⚠ 依赖不完整，跳过编译验证"
else
    echo "  正在编译 51_mic_wifi..."
    if python -m platformio run -e 51_mic_wifi; then
        echo "  ✓ 编译成功"
    else
        echo "  ✗ 编译失败，请检查错误信息"
        exit 1
    fi
fi

# ==================== 总结 ====================
echo ""
echo "=== 环境初始化完成 ==="
echo ""
echo "下一步操作:"
echo "  1. 连接 K10 设备到 USB 端口"
echo "  2. 烧录固件: python -m platformio run -t upload -e 51_mic_wifi --upload-port COM4"
echo "  3. 查看串口输出: python -m platformio device monitor -p COM4 -b 115200"
echo ""
echo "Dongle 固件编译 (需要 ESP-IDF):"
echo "  cd dongle_firmware"
echo "  idf.py build"
echo "  idf.py -p COM6 flash monitor"
echo ""
