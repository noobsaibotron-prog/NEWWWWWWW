# ============================================================================
# AI EQUALIZER PRO - Build Backup Script
# ============================================================================
# Usage: .\backup_build.ps1 [-Version "2.0.1"] [-Note "Fixed resonance bug"]
#
# Creates a timestamped backup of the current VST3 build
# ============================================================================

param(
    [string]$Version = "2.0.0",
    [string]$Note = ""
)

$ErrorActionPreference = "Stop"

# Paths
$projectRoot = "C:\AIEQ"
$backupDir = "$projectRoot\BACKUPS"
$vst3Source = "$projectRoot\build\Release\lib\AI Equalizer Pro.vst3"
$sourceDir = "$projectRoot\Source"

# Timestamp
$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$backupName = "v${Version}_${timestamp}"
$backupPath = "$backupDir\$backupName"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " AI EQUALIZER PRO - BACKUP SYSTEM" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Verify VST3 exists
if (-not (Test-Path $vst3Source)) {
    Write-Host "❌ ERROR: VST3 not found at: $vst3Source" -ForegroundColor Red
    Write-Host "   Run a build first!" -ForegroundColor Yellow
    exit 1
}

# Create backup directory
New-Item -Path $backupPath -ItemType Directory -Force | Out-Null
Write-Host "📁 Creating backup: $backupName" -ForegroundColor Green

# Copy VST3
Copy-Item $vst3Source "$backupPath\AI Equalizer Pro.vst3" -Force
$vst3Size = [math]::Round((Get-Item "$backupPath\AI Equalizer Pro.vst3").Length / 1MB, 2)
Write-Host "   ✅ VST3 copied ($vst3Size MB)" -ForegroundColor White

# Copy Source files (zip)
$sourceZip = "$backupPath\Source.zip"
Compress-Archive -Path $sourceDir -DestinationPath $sourceZip -Force
$srcSize = [math]::Round((Get-Item $sourceZip).Length / 1KB, 1)
Write-Host "   ✅ Source archived ($srcSize KB)" -ForegroundColor White

# Create info file
$infoContent = @"
# AI EQUALIZER PRO - Backup Info
# ================================

Version: $Version
Created: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Backup Name: $backupName

## Contents
- AI Equalizer Pro.vst3 ($vst3Size MB)
- Source.zip ($srcSize KB)

## Note
$Note

## Restore Instructions
1. Close your DAW
2. Copy 'AI Equalizer Pro.vst3' to:
   - C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win\
   OR
   - C:\Users\<username>\AppData\Local\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win\
3. Restart DAW and rescan plugins
"@

$infoContent | Out-File -FilePath "$backupPath\INFO.txt" -Encoding UTF8
Write-Host "   ✅ Info file created" -ForegroundColor White

Write-Host ""
Write-Host "✅ BACKUP COMPLETE!" -ForegroundColor Green
Write-Host "   Location: $backupPath" -ForegroundColor White
Write-Host ""

# List all backups
Write-Host "📦 ALL BACKUPS:" -ForegroundColor Cyan
Write-Host "-------------------------------------------" -ForegroundColor DarkGray
Get-ChildItem $backupDir -Directory | Sort-Object LastWriteTime -Descending | ForEach-Object {
    $vst = Get-ChildItem $_.FullName -Filter "*.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
    $sizeMB = if ($vst) { [math]::Round($vst.Length / 1MB, 2) } else { "N/A" }
    $info = Get-Content "$($_.FullName)\INFO.txt" -ErrorAction SilentlyContinue | Select-String "^Note"
    $noteStr = if ($info) { $info -replace "^## Note\s*", "" } else { "" }
    
    $marker = if ($_.Name -eq $backupName) { "→ " } else { "  " }
    Write-Host "$marker $($_.Name)  [$sizeMB MB]" -ForegroundColor $(if ($_.Name -eq $backupName) { "Yellow" } else { "White" })
}
Write-Host ""

