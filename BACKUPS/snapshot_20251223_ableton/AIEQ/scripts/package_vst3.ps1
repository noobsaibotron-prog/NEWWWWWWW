param(
    [string]$BuildRoot = "C:\AIEQ\build",
    [string]$OutputDir = "C:\AIEQ\dist"
)

$ErrorActionPreference = "Stop"

$artifact = Join-Path $BuildRoot "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
if (-not (Test-Path $artifact)) {
    Write-Host "[ERROR] Artefact not found: $artifact" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

$zipPath = Join-Path $OutputDir "AIEqualizerPro_VST3.zip"

Write-Host "[INFO] Packaging VST3 to $zipPath" -ForegroundColor Cyan
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

Compress-Archive -Path $artifact -DestinationPath $zipPath

Write-Host "[OK] Package created: $zipPath" -ForegroundColor Green
