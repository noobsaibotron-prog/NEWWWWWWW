<#
.SYNOPSIS
  Crea uno zip dei sorgenti necessari per buildare su macOS (senza artefatti Windows).

.USAGE
  powershell -ExecutionPolicy Bypass -File package_mac.ps1 [-Output AIEQ-mac.zip] [-IncludeJUCE] [-IncludeThirdParty]

.PARAMETER Output
  Nome/percorsi dello zip di output. Default: AIEQ-mac.zip

.PARAMETER IncludeJUCE
  Includi la cartella JUCE se è presente nel repo (default: non inclusa).

.PARAMETER IncludeThirdParty
  Includi la cartella ThirdParty se presente (default: non inclusa).
#>

param(
    [string]$Output = "AIEQ-mac.zip",
    [switch]$IncludeJUCE = $false,
    [switch]$IncludeThirdParty = $false
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$items = @(
    "CMakeLists.txt",
    "build_mac.sh",
    "Source"
)

if ($IncludeJUCE -and (Test-Path "JUCE")) {
    $items += "JUCE"
}

if ($IncludeThirdParty -and (Test-Path "ThirdParty")) {
    $items += "ThirdParty"
}

Write-Host "Packaging for macOS..." -ForegroundColor Cyan
Write-Host "Output: $Output"
Write-Host "Items: $($items -join ', ')"

# Rimuovi zip precedente
if (Test-Path $Output) {
    Remove-Item $Output -Force
}

Compress-Archive -Path $items -DestinationPath $Output -Force

Write-Host "Done. Copy $Output to your Mac, unzip, then run build_mac.sh" -ForegroundColor Green

