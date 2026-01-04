$ErrorActionPreference = "Stop"
$LogFile = "C:\AIEQ\build_output.txt"

Write-Host "Starting build..." -ForegroundColor Green
Write-Host "Log file: $LogFile" -ForegroundColor Yellow

# Change to build directory
Set-Location "C:\AIEQ\build"

# Execute build and capture all output
$buildCmd = "cmake --build . --config Release --parallel"
Write-Host "Executing: $buildCmd" -ForegroundColor Cyan

& cmd /c "$buildCmd 2>&1" | Tee-Object -FilePath $LogFile

$exitCode = $LASTEXITCODE
Write-Host "`nBuild completed with exit code: $exitCode" -ForegroundColor $(if ($exitCode -eq 0) { "Green" } else { "Red" })

# Check if plugin was created
$pluginPath = "C:\AIEQ\build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
if (Test-Path $pluginPath) {
    Write-Host "`n[SUCCESS] Plugin found at: $pluginPath" -ForegroundColor Green
    Get-Item $pluginPath | Select-Object FullName, LastWriteTime, Length
} else {
    Write-Host "`n[WARNING] Plugin not found at: $pluginPath" -ForegroundColor Yellow
}

# Show last 20 lines of log
Write-Host "`n=== Last 20 lines of build log ===" -ForegroundColor Cyan
Get-Content $LogFile -Tail 20