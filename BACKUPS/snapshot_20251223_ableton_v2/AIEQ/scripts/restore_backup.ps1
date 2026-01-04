# ============================================================================
# AI EQUALIZER PRO - Restore Backup Script
# ============================================================================
# Usage: .\restore_backup.ps1 [-BackupName "v2.0.0_2025-12-07_14-38-14"]
#
# Restores a previous VST3 build from backup
# ============================================================================

param(
    [string]$BackupName = ""
)

$ErrorActionPreference = "Stop"

# Paths
$backupDir = "C:\AIEQ\BACKUPS"
$localVST = "C:\Users\$env:USERNAME\AppData\Local\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win"
$systemVST = "C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " AI EQUALIZER PRO - RESTORE SYSTEM" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# List available backups
$backups = Get-ChildItem $backupDir -Directory | Sort-Object LastWriteTime -Descending

if ($backups.Count -eq 0) {
    Write-Host "❌ No backups found in: $backupDir" -ForegroundColor Red
    exit 1
}

Write-Host "📦 AVAILABLE BACKUPS:" -ForegroundColor Cyan
Write-Host "-------------------------------------------" -ForegroundColor DarkGray

$index = 1
$backups | ForEach-Object {
    $vst = Get-ChildItem $_.FullName -Filter "*.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
    $sizeMB = if ($vst) { [math]::Round($vst.Length / 1MB, 2) } else { "N/A" }
    Write-Host "  [$index] $($_.Name)  ($sizeMB MB)" -ForegroundColor White
    $index++
}
Write-Host ""

# If no backup specified, ask for selection
if ([string]::IsNullOrEmpty($BackupName)) {
    $selection = Read-Host "Enter backup number to restore (or 'q' to quit)"
    
    if ($selection -eq 'q') {
        Write-Host "Cancelled." -ForegroundColor Yellow
        exit 0
    }
    
    $selIndex = [int]$selection - 1
    if ($selIndex -lt 0 -or $selIndex -ge $backups.Count) {
        Write-Host "❌ Invalid selection" -ForegroundColor Red
        exit 1
    }
    
    $BackupName = $backups[$selIndex].Name
}

# Find backup
$backupPath = "$backupDir\$BackupName"
if (-not (Test-Path $backupPath)) {
    Write-Host "❌ Backup not found: $BackupName" -ForegroundColor Red
    exit 1
}

$vst3Source = "$backupPath\AI Equalizer Pro.vst3"
if (-not (Test-Path $vst3Source)) {
    Write-Host "❌ VST3 not found in backup" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "🔄 RESTORING: $BackupName" -ForegroundColor Yellow
Write-Host ""

# Show backup info
$infoFile = "$backupPath\INFO.txt"
if (Test-Path $infoFile) {
    Write-Host "📋 Backup Info:" -ForegroundColor Cyan
    Get-Content $infoFile | Select-Object -First 15 | ForEach-Object {
        Write-Host "   $_" -ForegroundColor DarkGray
    }
    Write-Host ""
}

# Confirm
$confirm = Read-Host "Restore this backup? (y/n)"
if ($confirm -ne 'y') {
    Write-Host "Cancelled." -ForegroundColor Yellow
    exit 0
}

# Restore to Local VST folder
Write-Host ""
Write-Host "📂 Installing to Local VST3 folder..." -ForegroundColor White
New-Item -Path $localVST -ItemType Directory -Force | Out-Null
Copy-Item $vst3Source "$localVST\AI Equalizer Pro.vst3" -Force
Write-Host "   ✅ Installed to: $localVST" -ForegroundColor Green

# Try to restore to System VST folder (needs admin)
Write-Host ""
Write-Host "📂 Installing to System VST3 folder (requires admin)..." -ForegroundColor White
try {
    Start-Process powershell -Verb RunAs -ArgumentList "-Command New-Item '$systemVST' -ItemType Directory -Force | Out-Null; Copy-Item '$vst3Source' '$systemVST\AI Equalizer Pro.vst3' -Force" -Wait
    Write-Host "   ✅ Installed to: $systemVST" -ForegroundColor Green
} catch {
    Write-Host "   ⚠️ Could not install to system folder (run as admin)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " ✅ RESTORE COMPLETE!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Close your DAW" -ForegroundColor White
Write-Host "  2. Reopen your DAW" -ForegroundColor White
Write-Host "  3. Rescan plugins" -ForegroundColor White
Write-Host ""

