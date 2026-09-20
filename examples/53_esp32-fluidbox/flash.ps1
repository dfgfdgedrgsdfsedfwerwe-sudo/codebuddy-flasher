# FluidBox 自动烧录脚本
# 用法: .\flash.ps1 [端口号]
# 示例: .\flash.ps1 COM11

param(
    [string]$Port = ""
)

Write-Host "=== FluidBox 烧录脚本 ===" -ForegroundColor Cyan
Write-Host ""

# 检测可用端口
$availablePorts = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object

if ($availablePorts.Count -eq 0) {
    Write-Host "ERROR: 未检测到串口设备！" -ForegroundColor Red
    Write-Host ""
    Write-Host "请检查:" -ForegroundColor Yellow
    Write-Host "  1. ATK BOX USB线是否连接"
    Write-Host "  2. 驱动是否安装 (设备管理器 > 端口)"
    Write-Host "  3. 尝试拔插USB重新枚举"
    Write-Host ""
    exit 1
}

Write-Host "可用端口:" -ForegroundColor Green
foreach ($p in $availablePorts) {
    Write-Host "  - $p"
}
Write-Host ""

# 如果未指定端口，使用第一个
if ($Port -eq "") {
    $Port = $availablePorts[0]
    Write-Host "自动选择端口: $Port" -ForegroundColor Yellow
} else {
    Write-Host "使用指定端口: $Port" -ForegroundColor Yellow
}
Write-Host ""

# 提示进入下载模式
Write-Host "准备烧录..." -ForegroundColor Cyan
Write-Host "如果连接失败，请手动进入下载模式:" -ForegroundColor Yellow
Write-Host "  1. 按住 BOOT 按钮" -ForegroundColor White
Write-Host "  2. 按一下 RESET 按钮" -ForegroundColor White
Write-Host "  3. 松开 BOOT 按钮" -ForegroundColor White
Write-Host ""
Write-Host "10秒后开始烧录..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

# 切换到项目根目录
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $projectRoot

Write-Host "开始烧录固件到 $Port ..." -ForegroundColor Cyan
Write-Host ""

# 执行烧录
python -m platformio run -t upload -e 53_esp32-fluidbox --upload-port $Port

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "✅ 烧录成功！" -ForegroundColor Green
    Write-Host ""
    Write-Host "预期效果:" -ForegroundColor Cyan
    Write-Host "  - LCD显示彩色流体场（蓝→绿→红渐变）"
    Write-Host "  - 重力场自动旋转，粒子随之流动"
    Write-Host ""
    Write-Host "查看串口输出:" -ForegroundColor Yellow
    Write-Host "  python -m platformio device monitor -p $Port -b 115200"
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "❌ 烧录失败！" -ForegroundColor Red
    Write-Host ""
    Write-Host "故障排查:" -ForegroundColor Yellow
    Write-Host "  1. 检查 $Port 是否正确"
    Write-Host "  2. 手动进入下载模式后重试"
    Write-Host "  3. 拔插USB重新枚举端口"
    Write-Host "  4. 查看完整指南: FLASHING_GUIDE.md"
    Write-Host ""
    exit 1
}
