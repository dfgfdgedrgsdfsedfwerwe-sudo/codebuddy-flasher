# 烧录固件到 COM6 dongle
# 使用方法: 在 PowerShell 中运行 .\flash_to_com6.ps1

Write-Host "=== 烧录 CodeBuddy Dongle 固件到 COM6 ===" -ForegroundColor Green

# 激活 ESP-IDF 环境
. C:\Users\4090\esp\esp-idf\export.ps1

# 切换到固件目录
Set-Location C:\Users\4090\Desktop\dfk10_arduino_demo-master\dongle_firmware

# 烧录到 COM6
Write-Host "`n开始烧录到 COM6..." -ForegroundColor Yellow
idf.py -p COM6 flash

Write-Host "`n=== 烧录完成 ===" -ForegroundColor Green
Write-Host "请将 USB 线从 COM 口拔出，插入 USB 口以启动 dongle" -ForegroundColor Cyan
Write-Host "LED 状态指示:" -ForegroundColor Cyan
Write-Host "  - 慢闪 (1Hz): 初始化中" -ForegroundColor White
Write-Host "  - 心跳 (双闪): 正常工作" -ForegroundColor White
Write-Host "  - 快闪 (5Hz): 初始化失败" -ForegroundColor White
Write-Host "  - 快速闪烁: 数据传输中" -ForegroundColor White
