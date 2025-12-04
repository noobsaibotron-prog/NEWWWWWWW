# ============================================================================
# AI EQUALIZER PRO - Build & Backup Script
# ============================================================================
# Usage: .\build_and_backup.ps1 -Version "2.0.1" -Note "Added feature X"
#
# Compiles the project AND creates a backup automatically
# ============================================================================

param(
    [string]$Version = "2.0.0",
    [string]$Note = "",
    [switch]$SkipBackup
)

$ErrorActionPreference = "Stop"

$projectRoot = "C:\AIEQ"
$buildDir = "$projectRoot\build"
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

Write-Host ""
Write-Host "============================================" -ForegroundColor Magenta
Write-Host " AI EQUALIZER PRO - BUILD & BACKUP" -ForegroundColor Magenta
Write-Host "============================================" -ForegroundColor Magenta
Write-Host " Version: $Version" -ForegroundColor White
Write-Host " Note: $Note" -ForegroundColor DarkGray
Write-Host "============================================" -ForegroundColor Magenta
Write-Host ""

# Step 1: Build
Write-Host "🔨 STEP 1: BUILDING..." -ForegroundColor Yellow
Write-Host ""

Push-Location $buildDir

try {
    & $cmake --build . --config Release --parallel
    $buildSuccess = $LASTEXITCODE -eq 0
} catch {
    $buildSuccess = $false
}

Pop-Location

if (-not $buildSuccess) {
    Write-Host ""
    Write-Host "❌ BUILD FAILED!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "✅ BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host ""

# Step 2: Backup
if (-not $SkipBackup) {
    Write-Host "💾 STEP 2: CREATING BACKUP..." -ForegroundColor Yellow
    & "$projectRoot\scripts\backup_build.ps1" -Version $Version -Note $Note
}

# Step 3: Install
Write-Host ""
Write-Host "📦 STEP 3: INSTALLING..." -ForegroundColor Yellow

$vst3Source = "$projectRoot\build\Release\lib\AI Equalizer Pro.vst3"
$localVST = "C:\Users\$env:USERNAME\AppData\Local\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win"

New-Item -Path $localVST -ItemType Directory -Force | Out-Null
Copy-Item $vst3Source "$localVST\AI Equalizer Pro.vst3" -Force
Write-Host "   ✅ Installed to Local VST3" -ForegroundColor Green

# System install (admin)
Start-Process powershell -Verb RunAs -ArgumentList "-Command New-Item 'C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win' -ItemType Directory -Force | Out-Null; Copy-Item '$vst3Source' 'C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win\AI Equalizer Pro.vst3' -Force" -Wait -ErrorAction SilentlyContinue
Write-Host "   ✅ Installed to System VST3" -ForegroundColor Green

Write-Host ""
Write-Host "============================================" -ForegroundColor Magenta
Write-Host " ✅ ALL DONE!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Magenta
Write-Host ""
Write-Host "🎹 Restart your DAW and rescan plugins!" -ForegroundColor Cyan
Write-Host ""

