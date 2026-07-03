#!/usr/bin/env bash
# ---------------------------------------------------------------------------------------------------
# Local sanitizer CORRECTNESS gate (validation net, item #1).
#
# Builds + runs the FIXTURE-FREE product test categories under AddressSanitizer + UndefinedBehaviorSanitizer
# (or ThreadSanitizer). Any UB / use-after-free / OOB (or, under TSan, data race) in PRODUCT code (Source/*)
# fails the gate (halt_on_error=1). JUCE compiles its OWN unit tests into the binary (gated by JUCE_UNIT_TESTS,
# which we need for our own test hooks) and registers its SIMD test in category "DSP" — so --category=DSP
# also runs juce::dsp::SIMDRegisterUnitTests, which does deliberate integer overflow. That JUCE test noise is
# suppressed via sanitize/ubsan_suppressions.txt; PRODUCT code is never suppressed.
#
# COVERAGE (honest — this is NOT a full-surface gate):
#   Covered: DSP, Core, AI, Integration, Regression (fixture-free product categories).
#   NOT covered here (documented follow-ups):
#     - fixture/env-dependent AI categories: AI-Corpus (WAV), AI-Sweep (shipped model), RealData
#       (AIEQ_REALDATA_DIR), AI-Diag / AI-Front / AI-Knobs / AI-Calibration.
#     - Perceptual (some tests load the model); Performance (wall-clock/flaky; its target isn't built here).
#     - the clean removal of JUCE's own bundled unit tests (would drop the suppressions entirely).
#
# ThreadSanitizer (the CONCURRENCY gate) is a SEPARATE run in its OWN build dir — TSan is mutually
# exclusive with ASan:   SAN=thread ./build_sanitize.sh
#
# Separate from the gate, this script also runs the QUARANTINED thread-safety DETECTOR
# (AIEqualizerPro_ThreadSafetyTests — pluginval param-storm reproducer, expected RED until the
# known race is fixed) when SAN=thread or RUN_THREADSAFETY=1. See the block at the bottom.
#
# Usage:  ./build_sanitize.sh            # -fsanitize=address,undefined   -> build-san
#         SAN=thread ./build_sanitize.sh # -fsanitize=thread              -> build-tsan
# ---------------------------------------------------------------------------------------------------
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SAN="${SAN:-address,undefined}"
if [[ "$SAN" == *thread* ]]; then DEFAULT_DIR="build-tsan"; else DEFAULT_DIR="build-san"; fi
BUILD_DIR="${BUILD_DIR:-$DEFAULT_DIR}"
BIN="$ROOT/$BUILD_DIR/Debug/bin"

echo ">> Configuring $BUILD_DIR  (Debug, native arch, -fsanitize=$SAN)"
cmake -S "$ROOT" -B "$ROOT/$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DAIEQ_SANITIZE="$SAN"

echo ">> Building correctness test targets under -fsanitize=$SAN"
cmake --build "$ROOT/$BUILD_DIR" --target \
  AIEqualizerPro_Tests AIEqualizerPro_AI_Tests AIEqualizerPro_IntegrationTests \
  AIEqualizerPro_ThreadSafetyTests

export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:suppressions=$ROOT/sanitize/ubsan_suppressions.txt"
export ASAN_OPTIONS="detect_leaks=0"   # LSan is not enabled by default on macOS; UAF/OOB still caught
export TSAN_OPTIONS="halt_on_error=0:second_deadlock_stack=1"  # collect ALL race reports, don't stop at the first

fail=0
run() {  # <binary> <category>
    echo "=============== $1 --category=$2 ==============="
    if ! "$BIN/$1" --category="$2"; then
        echo "!! SANITIZER FAILURE: $1 --category=$2"
        fail=1
    fi
}

# Fixture-free product categories (see COVERAGE note above for what is deliberately not run here).
run AIEqualizerPro_Tests            DSP
run AIEqualizerPro_Tests            Core
run AIEqualizerPro_Tests            Regression
run AIEqualizerPro_AI_Tests         AI
run AIEqualizerPro_IntegrationTests Integration

if [ "$fail" -eq 0 ]; then
    echo "=================== SANITIZER GATE: PASS (-fsanitize=$SAN) ==================="
else
    echo "=================== SANITIZER GATE: FAIL (-fsanitize=$SAN) ==================="
fi

# ---------------------------------------------------------------------------------------------------
# Thread-safety DETECTOR (quarantined — NOT part of the correctness gate above; separate verdict,
# does NOT change this script's exit code). In-process reproducer of pluginval's "Parameter thread
# safety" crash (REPORTS/KNOWN_ISSUE_pluginval_s10_param_thread_safety.md): EXPECTED RED (crash or
# sanitizer report) while that race is unfixed. Runs under SAN=thread (TSan names the racing write,
# both stacks) or with RUN_THREADSAFETY=1 (ASan catches the OOB variant at the faulting
# instruction). AIEQ_STORM_MS lengthens the storm (default 2000 ms) for deeper hunts.
# A quiet run is interleaving-dependent luck, NOT proof of fix — the end-to-end judge remains
# pluginval with pinned seed 0x782104d at strictness 8.
# ---------------------------------------------------------------------------------------------------
if [[ "$SAN" == *thread* || "${RUN_THREADSAFETY:-0}" == "1" ]]; then
    echo ""
    echo "=============== DETECTOR: AIEqualizerPro_ThreadSafetyTests --category=ThreadSafety ==============="
    echo "(quarantined reproducer — expected RED until the pluginval param race is fixed)"
    if "$BIN/AIEqualizerPro_ThreadSafetyTests" --category=ThreadSafety; then
        echo ">> DETECTOR: quiet this run (no crash / no sanitizer report / no failed assertion)."
        echo ">> NOTE: interleaving-dependent — a quiet run is NOT proof the race is fixed."
    else
        echo ">> DETECTOR: REPRODUCED (crash / sanitizer report / failed assertion) — see output above."
    fi
fi

[ "$fail" -eq 0 ] || exit 1
