# Batch compile LVGL demo + Snake game, output for firmware platform
# Estimated time: 10-15 minutes

$ErrorActionPreference = "Continue"
$baseDir = "C:\Users\4090\Desktop\dfk10_arduino_demo-master"
$webFlasherDir = "$baseDir\web_flasher"
$firmwareDir = "$webFlasherDir\firmware"
$logFile = "$baseDir\compile_examples_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

# Project list to compile
$targets = @(
    @{name="30_lvgl_Gif"; env="30_lvgl_Gif"; displayName="LVGL Gif Demo"},
    @{name="40_Snake_Game"; env="40_Snake_Game"; displayName="Snake Game"}
)

"=== Build Start: $(Get-Date) ===" | Tee-Object -FilePath $logFile
"Working Directory: $baseDir" | Tee-Object -FilePath $logFile -Append
"" | Tee-Object -FilePath $logFile -Append

$succeeded = @()
$failed = @()

foreach ($target in $targets) {
    $name = $target.name
    $env = $target.env
    $displayName = $target.displayName

    "=== Building $displayName ($name) ===" | Tee-Object -FilePath $logFile -Append

    # Enter project root (where platformio.ini is)
    Push-Location $baseDir

    # Run build (set PLATFORMIO_SRC_DIR to override src_dir per env)
    $srcDir = "examples/$name"
    "Command: PLATFORMIO_SRC_DIR=$srcDir python -m platformio run -e $env" | Tee-Object -FilePath $logFile -Append
    $buildStart = Get-Date
    $env:PLATFORMIO_SRC_DIR = $srcDir
    $buildOutput = python -m platformio run -e $env 2>&1 | Out-String
    Remove-Item Env:\PLATFORMIO_SRC_DIR
    $buildSuccess = $LASTEXITCODE -eq 0
    $buildDuration = ((Get-Date) - $buildStart).TotalSeconds

    Pop-Location

    if ($buildSuccess) {
        "[OK] Build succeeded ($([math]::Round($buildDuration, 1))s)" | Tee-Object -FilePath $logFile -Append

        # Find generated bin files
        $buildEnvDir = "$baseDir\.pio\build\$env"

        if (-not (Test-Path $buildEnvDir)) {
            "[WARN] Build output dir not found: $buildEnvDir" | Tee-Object -FilePath $logFile -Append
            $failed += @{name=$displayName; reason="no output dir"}
            continue
        }

        $binFiles = Get-ChildItem -Path $buildEnvDir -Filter "*.bin" -File

        if ($binFiles.Count -eq 0) {
            "[WARN] No .bin files found" | Tee-Object -FilePath $logFile -Append
            $failed += @{name=$displayName; reason="no bin"}
            continue
        }

        # Create firmware target dir
        $targetDir = "$firmwareDir\$name"
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

        # Copy bin files and record info
        $binInfo = @()
        foreach ($bin in $binFiles) {
            Copy-Item -Path $bin.FullName -Destination $targetDir -Force
            $sizeKB = [math]::Round($bin.Length / 1KB, 1)
            $binInfo += @{file=$bin.Name; size=$sizeKB}
            "  Copied: $($bin.Name) ($sizeKB KB)" | Tee-Object -FilePath $logFile -Append
        }

        $succeeded += @{
            name=$displayName
            folder=$name
            env=$env
            bins=$binInfo
        }
    }
    else {
        "[FAIL] Build failed ($([math]::Round($buildDuration, 1))s)" | Tee-Object -FilePath $logFile -Append

        # Extract error lines
        $errors = $buildOutput | Select-String -Pattern "error:" | Select-Object -First 5
        if ($errors) {
            "Error summary:" | Tee-Object -FilePath $logFile -Append
            $errors | ForEach-Object { "  $_" } | Tee-Object -FilePath $logFile -Append
        }

        # Save full output
        $errorLogFile = "$baseDir\compile_error_$name.log"
        $buildOutput | Out-File -FilePath $errorLogFile -Encoding utf8
        "Full error log: $errorLogFile" | Tee-Object -FilePath $logFile -Append

        $failed += @{name=$displayName; reason="build failed"}
    }

    "" | Tee-Object -FilePath $logFile -Append
}

# Summary
"=== Build Complete: $(Get-Date) ===" | Tee-Object -FilePath $logFile -Append
"Succeeded: $($succeeded.Count) / $($targets.Count)" | Tee-Object -FilePath $logFile -Append
"" | Tee-Object -FilePath $logFile -Append

if ($succeeded.Count -gt 0) {
    "[OK] Successful projects:" | Tee-Object -FilePath $logFile -Append
    foreach ($item in $succeeded) {
        "  - $($item.name)" | Tee-Object -FilePath $logFile -Append
        "    folder: firmware\$($item.folder)" | Tee-Object -FilePath $logFile -Append
        foreach ($bin in $item.bins) {
            "    bin: $($bin.file) ($($bin.size) KB)" | Tee-Object -FilePath $logFile -Append
        }
    }
}

if ($failed.Count -gt 0) {
    "" | Tee-Object -FilePath $logFile -Append
    "[FAIL] Failed projects:" | Tee-Object -FilePath $logFile -Append
    foreach ($item in $failed) {
        "  - $($item.name): $($item.reason)" | Tee-Object -FilePath $logFile -Append
    }
}

"" | Tee-Object -FilePath $logFile -Append
"Log file: $logFile" | Tee-Object -FilePath $logFile -Append
