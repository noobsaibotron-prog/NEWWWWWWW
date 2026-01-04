param(
    [string]$TorchUrl = "https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-2.2.0%2Bcpu.zip",
    [string]$TorchDest = "../third_party/libtorch",
    [string]$GloveUrl = "https://nlp.stanford.edu/data/glove.6B.zip",
    [string]$GloveDest = "../glove.6B.50d.txt",
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function New-DirSafe {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Invoke-DownloadIfNeeded {
    param(
        [string]$Url,
        [string]$OutFile
    )
    if (-not (Test-Path $OutFile) -or $Force) {
        Write-Host "Downloading $Url -> $OutFile"
        Invoke-WebRequest -Uri $Url -OutFile $OutFile
    } else {
        Write-Host "Already present: $OutFile (use -Force to re-download)"
    }
}

# Resolve destinations relative to script
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$TorchDestFull = Resolve-Path -Path (Join-Path $ScriptDir $TorchDest) -ErrorAction SilentlyContinue
if (-not $TorchDestFull) {
    $TorchDestFull = Join-Path $ScriptDir $TorchDest
}

$GloveDestFull = Resolve-Path -Path (Join-Path $ScriptDir $GloveDest) -ErrorAction SilentlyContinue
if (-not $GloveDestFull) {
    $GloveDestFull = Join-Path $ScriptDir $GloveDest
}

# Torch download and extract
New-DirSafe (Split-Path -Parent $TorchDestFull)
$TorchZip = Join-Path $ScriptDir "libtorch_tmp.zip"
Invoke-DownloadIfNeeded -Url $TorchUrl -OutFile $TorchZip

if (Test-Path $TorchDestFull -and $Force) {
    Write-Host "Removing existing Torch destination: $TorchDestFull"
    Remove-Item -Recurse -Force $TorchDestFull
}
if (-not (Test-Path $TorchDestFull)) {
    Write-Host "Extracting Torch to $TorchDestFull"
    Expand-Archive -Path $TorchZip -DestinationPath (Split-Path -Parent $TorchDestFull) -Force
    if (-not (Test-Path $TorchDestFull) -and (Test-Path (Join-Path (Split-Path -Parent $TorchDestFull) "libtorch"))) {
        Rename-Item -Path (Join-Path (Split-Path -Parent $TorchDestFull) "libtorch") -NewName (Split-Path -Leaf $TorchDestFull)
    }
}

# GloVe download and extract 50d
$GloveZip = Join-Path $ScriptDir "glove_tmp.zip"
Download-IfNeeded -Url $GloveUrl -OutFile $GloveZip

$GloveExtractDir = Join-Path $ScriptDir "glove_tmp"
if (Test-Path $GloveExtractDir) {
    Remove-Item -Recurse -Force $GloveExtractDir
}
New-DirSafe $GloveExtractDir
Write-Host "Extracting GloVe to temp dir"
Expand-Archive -Path $GloveZip -DestinationPath $GloveExtractDir -Force

$GloveSource = Join-Path $GloveExtractDir "glove.6B.50d.txt"
if (-not (Test-Path $GloveSource)) {
    throw "glove.6B.50d.txt not found in archive."
}

if (Test-Path $GloveDestFull -and $Force) {
    Remove-Item -Force $GloveDestFull
}
Copy-Item -Path $GloveSource -Destination $GloveDestFull -Force
Write-Host "Placed glove.6B.50d.txt at $GloveDestFull"

Write-Host "Done. Set -DAIEQ_ENABLE_TORCH=ON and point AIEQ_TORCH_ROOT to $TorchDestFull"
