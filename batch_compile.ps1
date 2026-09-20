# 批量编译所有 examples 并更新固件管理平台
# 执行时间：约 2-3 小时

$ErrorActionPreference = "Continue"
$baseDir = "C:\Users\4090\Desktop\dfk10_arduino_demo-master"
$examplesDir = "$baseDir\examples"
$webFlasherDir = "$baseDir\web_flasher"
$firmwareDir = "$webFlasherDir\firmware"
$logFile = "$baseDir\batch_compile_log.txt"

# 初始化日志
"=== 批量编译开始: $(Get-Date) ===" | Tee-Object -FilePath $logFile

# 获取所有 example 目录
$examples = Get-ChildItem -Path $examplesDir -Directory | Where-Object { $_.Name -ne "51_mic_wifi" }

$total = $examples.Count
$current = 0
$succeeded = @()
$failed = @()

foreach ($example in $examples) {
    $current++
    $exampleName = $example.Name
    $examplePath = $example.FullName

    "[$current/$total] 编译 $exampleName ..." | Tee-Object -FilePath $logFile -Append

    # 检查是否有 platformio.ini
    if (-not (Test-Path "$examplePath\platformio.ini")) {
        "  ⚠ 跳过（无 platformio.ini）" | Tee-Object -FilePath $logFile -Append
        $failed += @{name=$exampleName; reason="无platformio.ini"}
        continue
    }

    # 编译
    Push-Location $examplePath
    $buildOutput = pio run 2>&1 | Out-String
    $buildSuccess = $LASTEXITCODE -eq 0
    Pop-Location

    if ($buildSuccess) {
        "  ✓ 编译成功" | Tee-Object -FilePath $logFile -Append

        # 查找生成的 bin 文件
        $buildDir = "$baseDir\.pio\build"
        $envDirs = Get-ChildItem -Path $buildDir -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne "project.checksum" }

        if ($envDirs.Count -eq 0) {
            "  ⚠ 未找到编译输出" | Tee-Object -FilePath $logFile -Append
            $failed += @{name=$exampleName; reason="未找到bin"}
            continue
        }

        # 使用第一个环境的输出
        $envDir = $envDirs[0].FullName
        $binFiles = Get-ChildItem -Path $envDir -Filter "*.bin" -File

        if ($binFiles.Count -eq 0) {
            "  ⚠ 未找到 .bin 文件" | Tee-Object -FilePath $logFile -Append
            $failed += @{name=$exampleName; reason="无bin文件"}
            continue
        }

        # 创建固件目录
        $targetDir = "$firmwareDir\$exampleName"
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

        # 复制 bin 文件
        foreach ($bin in $binFiles) {
            Copy-Item -Path $bin.FullName -Destination $targetDir -Force
            "    复制: $($bin.Name)" | Tee-Object -FilePath $logFile -Append
        }

        $succeeded += $exampleName
    }
    else {
        "  ✗ 编译失败" | Tee-Object -FilePath $logFile -Append
        $buildOutput | Select-String "error:" | ForEach-Object { "    $_" } | Tee-Object -FilePath $logFile -Append
        $failed += @{name=$exampleName; reason="编译失败"}
    }
}

# 统计
"" | Tee-Object -FilePath $logFile -Append
"=== 编译完成: $(Get-Date) ===" | Tee-Object -FilePath $logFile -Append
"总计: $total 个项目" | Tee-Object -FilePath $logFile -Append
"成功: $($succeeded.Count) 个" | Tee-Object -FilePath $logFile -Append
"失败: $($failed.Count) 个" | Tee-Object -FilePath $logFile -Append
"" | Tee-Object -FilePath $logFile -Append

if ($succeeded.Count -gt 0) {
    "成功编译的项目:" | Tee-Object -FilePath $logFile -Append
    $succeeded | ForEach-Object { "  - $_" } | Tee-Object -FilePath $logFile -Append
}

if ($failed.Count -gt 0) {
    "" | Tee-Object -FilePath $logFile -Append
    "失败的项目:" | Tee-Object -FilePath $logFile -Append
    $failed | ForEach-Object { "  - $($_.name): $($_.reason)" } | Tee-Object -FilePath $logFile -Append
}

"" | Tee-Object -FilePath $logFile -Append
"下一步：运行 update_platform.ps1 更新固件管理平台" | Tee-Object -FilePath $logFile -Append
