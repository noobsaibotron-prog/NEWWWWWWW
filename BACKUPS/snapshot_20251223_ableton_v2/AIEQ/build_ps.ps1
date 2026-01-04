# AI Equalizer Pro Build Script
Write-Host "=== AI EQUALIZER PRO BUILD ===" -ForegroundColor Cyan

$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$cmakePath = "$vsPath\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

# Check paths
if (-not (Test-Path $vsPath)) {
    Write-Host "ERROR: Visual Studio 2022 not found!" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $cmakePath)) {
    Write-Host "ERROR: CMake not found!" -ForegroundColor Red
    exit 1
}

Write-Host "VS2022 found at: $vsPath" -ForegroundColor Green
Write-Host "CMake found at: $cmakePath" -ForegroundColor Green

# Find MSVC
$msvcDir = Get-ChildItem "$vsPath\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$clPath = "$vsPath\VC\Tools\MSVC\$($msvcDir.Name)\bin\Hostx64\x64\cl.exe"

if (-not (Test-Path $clPath)) {
    Write-Host "ERROR: cl.exe not found!" -ForegroundColor Red
    exit 1
}

Write-Host "MSVC Compiler: $clPath" -ForegroundColor Green

# Setup environment
$env:PATH = "$vsPath\VC\Tools\MSVC\$($msvcDir.Name)\bin\Hostx64\x64;$env:PATH"
$env:PATH = "$vsPath\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:PATH"

# Find Windows SDK
$sdkVersions = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Directory | 
    Where-Object { $_.Name -match "^\d+\." } | Sort-Object Name -Descending
$sdkVersion = $sdkVersions[0].Name
Write-Host "Windows SDK: $sdkVersion" -ForegroundColor Green

$env:PATH = "C:\Program Files (x86)\Windows Kits\10\bin\$sdkVersion\x64;$env:PATH"

# Set include paths
$includeBase = "$vsPath\VC\Tools\MSVC\$($msvcDir.Name)\include"
$sdkInclude = "C:\Program Files (x86)\Windows Kits\10\Include\$sdkVersion"
$env:INCLUDE = "$includeBase;$sdkInclude\ucrt;$sdkInclude\um;$sdkInclude\shared"

# Set lib paths
$libBase = "$vsPath\VC\Tools\MSVC\$($msvcDir.Name)\lib\x64"
$sdkLib = "C:\Program Files (x86)\Windows Kits\10\Lib\$sdkVersion"
$env:LIB = "$libBase;$sdkLib\ucrt\x64;$sdkLib\um\x64"

Write-Host "Environment configured" -ForegroundColor Green

# Clean and create build folder
Set-Location C:\AIEQ
if (Test-Path "build") {
    Remove-Item -Recurse -Force "build"
    Write-Host "Cleaned build folder" -ForegroundColor Yellow
}
New-Item -ItemType Directory -Path "build" | Out-Null
Set-Location "build"

Write-Host "`nConfiguring CMake..." -ForegroundColor Cyan

# Configure CMake
$cmakeArgs = @(
    "..",
    "-G", "NMake Makefiles",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_C_COMPILER=$clPath",
    "-DCMAKE_CXX_COMPILER=$clPath"
)

& $cmakePath $cmakeArgs 2>&1 | ForEach-Object { Write-Host $_ }

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`nBuilding..." -ForegroundColor Cyan

# Build
& $cmakePath --build . --config Release --parallel 2>&1 | ForEach-Object { Write-Host $_ }

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`n=== BUILD COMPLETED ===" -ForegroundColor Green

# Find VST3
$vst3Files = Get-ChildItem -Path "C:\AIEQ\build" -Recurse -Filter "*.vst3" -Directory
foreach ($vst3 in $vst3Files) {
    Write-Host "Found: $($vst3.FullName)" -ForegroundColor Green
}

# Copy to standard locations
$vst3Source = "C:\AIEQ\build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
$localVst3 = "$env:LOCALAPPDATA\VST3"
$commonVst3 = "C:\Program Files\Common Files\VST3"

if (Test-Path $vst3Source) {
    Write-Host "`nCopying VST3..." -ForegroundColor Cyan
    
    # Local
    if (-not (Test-Path $localVst3)) { New-Item -ItemType Directory -Path $localVst3 | Out-Null }
    Copy-Item -Path $vst3Source -Destination "$localVst3\AI Equalizer Pro.vst3" -Recurse -Force
    Write-Host "Copied to: $localVst3" -ForegroundColor Green
    
    # Common Files (needs admin)
    try {
        Copy-Item -Path $vst3Source -Destination "$commonVst3\AI Equalizer Pro.vst3" -Recurse -Force
        Write-Host "Copied to: $commonVst3" -ForegroundColor Green
    } catch {
        Write-Host "Could not copy to Common Files (needs admin)" -ForegroundColor Yellow
    }
}

Write-Host "`n=== DONE ===" -ForegroundColor Green
Write-Host "Rescan plugins in Ableton to find AI Equalizer Pro" -ForegroundColor Cyan
