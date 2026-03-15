# ============================================================
# AIEQ Test & Deploy Script
# Usage: .\test-and-deploy.ps1 [-SkipTests] [-Verbose]
#
# 1. Build Debug (tests)
# 2. Run full test suite
# 3. If all pass → build Release VST3
# 4. Copy VST3 to system folder
# ============================================================

param(
    [switch]$SkipTests,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$CMAKE     = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$BUILD_DIR = "C:\AIEQ\build"
$VST3_SRC  = "$BUILD_DIR\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
$VST3_DST  = "C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3"
$TESTS_EXE = "$BUILD_DIR\Release\bin\AIEqualizerPro_Tests.exe"

function Write-Step { param($msg) Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok   { param($msg) Write-Host "    [OK] $msg" -ForegroundColor Green }
function Write-Fail { param($msg) Write-Host "    [FAIL] $msg" -ForegroundColor Red }

# ── 1. Build Debug (compila i test) ──────────────────────────────────────────
Write-Step "Building Debug (tests)..."
$testBuildArgs = @("--build", $BUILD_DIR, "--config", "Release", "--target", "AIEqualizerPro_Tests")
if ($Verbose) { $testBuildArgs += "--verbose" }

& $CMAKE @testBuildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Fail "Test build failed. Aborting."
    exit 1
}
Write-Ok "Test build OK"

# ── 2. Run tests ─────────────────────────────────────────────────────────────
if (-not $SkipTests) {
    Write-Step "Running test suite..."

    if (-not (Test-Path $TESTS_EXE)) {
        Write-Fail "Test executable not found: $TESTS_EXE"
        exit 1
    }

    $testArgs = @()
    if ($Verbose) { $testArgs += "--verbose" }

    & $TESTS_EXE @testArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "Tests FAILED. Release build blocked."
        Write-Host ""
        Write-Host "  Fix the failing tests before deploying." -ForegroundColor Yellow
        exit 1
    }
    Write-Ok "All tests passed"
} else {
    Write-Host "    [SKIP] Tests skipped (--SkipTests)" -ForegroundColor Yellow
}

# ── 3. Build Release VST3 ────────────────────────────────────────────────────
Write-Step "Building Release VST3..."
$releaseArgs = @("--build", $BUILD_DIR, "--config", "Release", "--target", "AIEqualizerPro_VST3")
if ($Verbose) { $releaseArgs += "--verbose" }

& $CMAKE @releaseArgs
if ($LASTEXITCODE -ne 0) {
    Write-Fail "Release build failed."
    exit 1
}
Write-Ok "Release build OK"

# ── 4. Deploy ────────────────────────────────────────────────────────────────
Write-Step "Deploying VST3..."

if (-not (Test-Path $VST3_SRC)) {
    Write-Fail "VST3 artifact not found: $VST3_SRC"
    exit 1
}

Copy-Item -Recurse -Force $VST3_SRC $VST3_DST
Write-Ok "Deployed to: $VST3_DST"

# ── Summary ──────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  AIEQ deploy complete" -ForegroundColor Green
Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
