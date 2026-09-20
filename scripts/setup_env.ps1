# CodeBuddy K10 开发环境初始化脚本 (Windows PowerShell)
# 项目: 51_K10_wifi (CodeBuddy Wireless)
# 设备型号: K10-UNIHIKER
# 芯片方案: ESP32-S3
# 生成时间: 2026-09-17

Write-Host "=== CodeBuddy K10 开发环境初始化 ===" -ForegroundColor Cyan
Write-Host ""

# ==================== 1. 系统必备工具检查 ====================
Write-Host "[1/6] 检查系统必备工具..." -ForegroundColor Yellow

function Test-CommandExists {
    param($Command)
    try {
        Get-Command $Command -ErrorAction Stop | Out-Null
        return $true
    } catch {
        return $false
    }
}

$tools_ok = $true

# Python 检查
if (Test-CommandExists "python") {
    $py_version = python --version 2>&1
    Write-Host "  ✓ Python: $py_version" -ForegroundColor Green
} else {
    Write-Host "  ✗ Python 未安装" -ForegroundColor Red
    Write-Host "    请从 https://www.python.org/downloads/ 下载安装 Python 3.11+" -ForegroundColor Yellow
    $tools_ok = $false
}

# Git 检查
if (Test-CommandExists "git") {
    $git_version = git --version 2>&1
    Write-Host "  ✓ Git: $git_version" -ForegroundColor Green
} else {
    Write-Host "  ✗ Git 未安装" -ForegroundColor Red
    Write-Host "    请从 https://git-scm.com/downloads 下载安装" -ForegroundColor Yellow
    $tools_ok = $false
}

if (-not $tools_ok) {
    Write-Host ""
    Write-Host "❌ 系统工具缺失，请先安装后重新运行此脚本" -ForegroundColor Red
    exit 1
}

# ==================== 2. Python 环境与依赖 ====================
Write-Host ""
Write-Host "[2/6] 检查 Python 依赖..." -ForegroundColor Yellow

$required_packages = @{
    "platformio" = "6.1.0"
    "esptool" = "4.0"
    "pyserial" = "3.5"
    "pyyaml" = "6.0"
}

$pip_list = pip list 2>&1 | Out-String
$missing_packages = @()

foreach ($pkg in $required_packages.Keys) {
    if ($pip_list -match $pkg) {
        Write-Host "  ✓ $pkg 已安装" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $pkg 未安装" -ForegroundColor Red
        $missing_packages += $pkg
    }
}

if ($missing_packages.Count -gt 0) {
    Write-Host ""
    Write-Host "  正在安装缺失的 Python 包..." -ForegroundColor Yellow
    foreach ($pkg in $missing_packages) {
        pip install $pkg
    }
}

# ==================== 3. PlatformIO 验证 ====================
Write-Host ""
Write-Host "[3/6] 验证 PlatformIO..." -ForegroundColor Yellow

try {
    $pio_version = python -m platformio --version 2>&1
    Write-Host "  ✓ PlatformIO: $pio_version" -ForegroundColor Green
} catch {
    Write-Host "  ✗ PlatformIO 不可用" -ForegroundColor Red
    exit 1
}

# ==================== 4. Git 子模块初始化 ====================
Write-Host ""
Write-Host "[4/6] 初始化 Git 子模块 (lvgl, LovyanGFX)..." -ForegroundColor Yellow

$submodules_ok = $true
if (Test-Path "lib/lvgl/src") {
    Write-Host "  ✓ lvgl 子模块已初始化" -ForegroundColor Green
} else {
    Write-Host "  ! lvgl 子模块未初始化，正在初始化..." -ForegroundColor Yellow
    git submodule update --init --recursive lib/lvgl
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ lvgl 初始化完成" -ForegroundColor Green
    } else {
        Write-Host "  ✗ lvgl 初始化失败" -ForegroundColor Red
        $submodules_ok = $false
    }
}

if (Test-Path "lib/LovyanGFX/src") {
    Write-Host "  ✓ LovyanGFX 子模块已初始化" -ForegroundColor Green
} else {
    Write-Host "  ! LovyanGFX 子模块未初始化，正在初始化..." -ForegroundColor Yellow
    git submodule update --init --recursive lib/LovyanGFX
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ LovyanGFX 初始化完成" -ForegroundColor Green
    } else {
        Write-Host "  ✗ LovyanGFX 初始化失败" -ForegroundColor Red
        $submodules_ok = $false
    }
}

# ==================== 5. 依赖库检查 ====================
Write-Host ""
Write-Host "[5/6] 检查依赖库完整性..." -ForegroundColor Yellow

$libs_ok = $true
if (Test-Path "lib/ESP32_JPEG/src") {
    Write-Host "  ✓ ESP32_JPEG 库存在" -ForegroundColor Green
} else {
    Write-Host "  ! ESP32_JPEG 库缺失" -ForegroundColor Yellow
    Write-Host "    如果 lib.zip 存在，请解压到 lib/ 目录" -ForegroundColor Yellow
    $libs_ok = $false
}

if (Test-Path "lib/Arduino_DriveBus/src") {
    Write-Host "  ✓ Arduino_DriveBus 库存在" -ForegroundColor Green
} else {
    Write-Host "  ! Arduino_DriveBus 库缺失" -ForegroundColor Yellow
    Write-Host "    如果 lib.zip 存在，请解压到 lib/ 目录" -ForegroundColor Yellow
    $libs_ok = $false
}

# ==================== 6. 首次编译验证 ====================
Write-Host ""
Write-Host "[6/6] 首次编译验证..." -ForegroundColor Yellow

if (-not $submodules_ok -or -not $libs_ok) {
    Write-Host "  ⚠ 依赖不完整，跳过编译验证" -ForegroundColor Yellow
} else {
    Write-Host "  正在编译 51_mic_wifi..." -ForegroundColor Yellow
    python -m platformio run -e 51_mic_wifi
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ 编译成功" -ForegroundColor Green
    } else {
        Write-Host "  ✗ 编译失败，请检查错误信息" -ForegroundColor Red
        exit 1
    }
}

# ==================== 总结 ====================
Write-Host ""
Write-Host "=== 环境初始化完成 ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "下一步操作:" -ForegroundColor Yellow
Write-Host "  1. 连接 K10 设备到 USB 端口" -ForegroundColor White
Write-Host "  2. 烧录固件: python -m platformio run -t upload -e 51_mic_wifi --upload-port COM4" -ForegroundColor White
Write-Host "  3. 查看串口输出: python -m platformio device monitor -p COM4 -b 115200" -ForegroundColor White
Write-Host ""
Write-Host "Dongle 固件编译 (需要 ESP-IDF):" -ForegroundColor Yellow
Write-Host "  cd dongle_firmware" -ForegroundColor White
Write-Host "  idf.py build" -ForegroundColor White
Write-Host "  idf.py -p COM6 flash monitor" -ForegroundColor White
Write-Host ""
