param(
    [string]$SourceRoot = "C:\AIEQ\Source",
    [string]$BuildArtefacts = "C:\AIEQ\build\AIEqualizerPro_artefacts",
    [string]$UserVst3 = "$env:LOCALAPPDATA\VST3\AI Equalizer Pro.vst3",
    [string]$CommonVst3 = "$env:ProgramFiles\Common Files\VST3\AI Equalizer Pro.vst3",
    [string]$DestRoot = "C:\AIEQ\BACKUPS"
)

$ErrorActionPreference = "Stop"

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$zipPath = Join-Path $DestRoot "auto_backup_$timestamp.zip"
$tempDir = Join-Path $DestRoot "auto_backup_$timestamp"

$paths = @($SourceRoot, $BuildArtefacts, $UserVst3, $CommonVst3)
$existing = @()
foreach ($p in $paths) {
    if (Test-Path $p) { $existing += $p }
    else { Write-Host "[WARN] Missing path: $p" -ForegroundColor Yellow }
}

if ($existing.Count -eq 0) {
    Write-Host "[ERROR] No existing paths to backup." -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $DestRoot)) {
    New-Item -ItemType Directory -Path $DestRoot | Out-Null
}

try {
    Write-Host "[INFO] Creating ZIP backup: $zipPath" -ForegroundColor Cyan
    Compress-Archive -Path $existing -DestinationPath $zipPath -Force -CompressionLevel Optimal
    Write-Host "[OK] Backup ZIP created: $zipPath" -ForegroundColor Green
}
catch {
    Write-Host "[WARN] ZIP failed, falling back to folder copy: $tempDir" -ForegroundColor Yellow
    if (-not (Test-Path $tempDir)) { New-Item -ItemType Directory -Path $tempDir | Out-Null }
    foreach ($p in $existing) {
        $name = Split-Path $p -Leaf
        $dest = Join-Path $tempDir $name
        Write-Host "[INFO] Copying $p -> $dest"
        robocopy $p $dest /MIR /NJH /NJS /NP /NFL /NDL | Out-Null
    }
    Write-Host "[OK] Folder backup created at: $tempDir" -ForegroundColor Green
}
