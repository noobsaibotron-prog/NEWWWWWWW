param(
    [string]$BuildRoot = "C:\AIEQ\build",
    [string]$UserVst3 = "$env:LOCALAPPDATA\VST3\AI Equalizer Pro.vst3",
    [string]$CommonVst3 = "$env:ProgramFiles\Common Files\VST3\AI Equalizer Pro.vst3"
)

function Show-PathInfo {
    param([string]$PathLabel, [string]$PathValue)
    if (Test-Path $PathValue) {
        $item = Get-Item $PathValue
        $sizeMB = "{0:N2}" -f (($item | Get-ChildItem -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB)
        Write-Host "[OK] $PathLabel: $($item.FullName)" -ForegroundColor Green
        Write-Host "     Modified: $($item.LastWriteTime) | Size: $sizeMB MB"
    } else {
        Write-Host "[MISSING] $PathLabel: $PathValue" -ForegroundColor Yellow
    }
}

Write-Host "=== Checking VST3 artefacts ===" -ForegroundColor Cyan

$buildVst3 = Join-Path $BuildRoot "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
Show-PathInfo -PathLabel "Build artefact" -PathValue $buildVst3
Show-PathInfo -PathLabel "User VST3" -PathValue $UserVst3
Show-PathInfo -PathLabel "Common VST3" -PathValue $CommonVst3

Write-Host "`nDone." -ForegroundColor Cyan
