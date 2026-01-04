# ============================================================================
# AI EQUALIZER PRO - List Backups
# ============================================================================

$backupDir = "C:\AIEQ\BACKUPS"

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " AI EQUALIZER PRO - BACKUP LIST" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

$backups = Get-ChildItem $backupDir -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending

if ($backups.Count -eq 0) {
    Write-Host "❌ No backups found." -ForegroundColor Yellow
    Write-Host "   Run .\backup_build.ps1 to create one." -ForegroundColor DarkGray
    exit
}

Write-Host "📦 Found $($backups.Count) backup(s):" -ForegroundColor White
Write-Host ""

$totalSize = 0

foreach ($backup in $backups) {
    $vst = Get-ChildItem $backup.FullName -Filter "*.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
    $sizeMB = if ($vst) { [math]::Round($vst.Length / 1MB, 2) } else { 0 }
    $totalSize += $sizeMB
    
    # Read note from INFO.txt
    $infoPath = Join-Path $backup.FullName "INFO.txt"
    $note = ""
    if (Test-Path $infoPath) {
        $content = Get-Content $infoPath -Raw
        if ($content -match "## Note\s*\n(.+?)(\n##|\z)") {
            $note = $matches[1].Trim()
        }
    }
    
    # Format date
    $date = $backup.LastWriteTime.ToString("dd MMM yyyy HH:mm")
    
    Write-Host "┌─────────────────────────────────────────────" -ForegroundColor DarkGray
    Write-Host "│ 📁 $($backup.Name)" -ForegroundColor White
    Write-Host "│    Size: $sizeMB MB | Created: $date" -ForegroundColor DarkGray
    if ($note) {
        Write-Host "│    Note: $note" -ForegroundColor DarkCyan
    }
}

Write-Host "└─────────────────────────────────────────────" -ForegroundColor DarkGray
Write-Host ""
Write-Host "Total backup size: $([math]::Round($totalSize, 2)) MB" -ForegroundColor Cyan
Write-Host ""
Write-Host "Commands:" -ForegroundColor Yellow
Write-Host "  .\restore_backup.ps1          # Interactive restore" -ForegroundColor White
Write-Host "  .\restore_backup.ps1 -BackupName 'v2.0.0_...'  # Direct restore" -ForegroundColor White
Write-Host ""

