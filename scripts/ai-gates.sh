#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# ai-gates.sh — the "Assicurati di non rompere nulla" ritual, in one command.
#
# Encodes the standing AI-work gate protocol so it is deterministic and not
# re-typed by hand each time:
#   1. BUILD   — the 4 test binaries (or skip with --no-build)
#   2. FLOOR   — AI-Sweep clean false-positive floor (the SACRED 0/18 gate)
#   3. GREEN   — all 4 binaries no-arg = every category EXCEPT KnownDebt, 0 fail
#   4. DIAG    — print the P4 held-out diagnostic tables (AI-Diag), honest metrics
#
# Verdict is by EXIT CODE of each binary (TestMain returns 0 pass / 1 fail), not
# by text parsing. Overall exit: 0 only if BUILD + FLOOR + GREEN all pass.
# DIAG is informational (it already runs inside the GREEN no-arg pass).
#
# Usage:
#   scripts/ai-gates.sh                 # full: build + floor + green + diag
#   scripts/ai-gates.sh --no-build      # skip the build, run gates on current binaries
#   scripts/ai-gates.sh --no-diag       # skip printing the diagnostic tables
#   BUILD_DIR=build-release scripts/ai-gates.sh   # override build dir (default build-mac)
# ─────────────────────────────────────────────────────────────────────────────
set -u
set -o pipefail

BUILD_DIR="${BUILD_DIR:-build-mac}"
JOBS="${JOBS:-8}"
DO_BUILD=1
DO_DIAG=1
for a in "$@"; do
  case "$a" in
    --no-build) DO_BUILD=0 ;;
    --no-diag)  DO_DIAG=0 ;;
    -h|--help)  sed -n '2,22p' "$0"; exit 0 ;;
    *) echo "unknown arg: $a" >&2; exit 2 ;;
  esac
done

# Resolve repo root from this script's location, so it runs from anywhere.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BIN="$BUILD_DIR/Release/bin"

BINARIES=(AIEqualizerPro_AI_Tests AIEqualizerPro_IntegrationTests \
          AIEqualizerPro_PerformanceTests AIEqualizerPro_Tests)

fail=0
note() { printf '\n\033[1m== %s ==\033[0m\n' "$1"; }
ok()   { printf '  \033[32mPASS\033[0m  %s\n' "$1"; }
bad()  { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; fail=1; }

# ── 1. BUILD ────────────────────────────────────────────────────────────────
if [[ $DO_BUILD -eq 1 ]]; then
  note "BUILD ($BUILD_DIR, -j$JOBS)"
  if cmake --build "$BUILD_DIR" --target "${BINARIES[@]}" -j"$JOBS" 2>&1 \
       | grep -E "error:|FAILED|ninja: build stopped" ; then
    bad "build had errors"
    echo; echo "BUILD failed — aborting before gates."; exit 1
  fi
  ok "4 test binaries built"
else
  note "BUILD skipped (--no-build)"
fi

for b in "${BINARIES[@]}"; do
  [[ -x "$BIN/$b" ]] || { echo "missing binary: $BIN/$b (build first)"; exit 1; }
done

# ── 2. FLOOR — sacred AI-Sweep clean false-positive floor ────────────────────
note "FLOOR — AI-Sweep clean false-positive floor (must be 0/18)"
if "$BIN/AIEqualizerPro_AI_Tests" --category=AI-Sweep >/tmp/ai_sweep.log 2>&1; then
  ok "AI-Sweep floor intact"
else
  bad "AI-Sweep floor BREACHED — see /tmp/ai_sweep.log"
fi

# ── 3. GREEN — 4 binaries no-arg (all categories except KnownDebt) ───────────
note "GREEN — 4 binaries no-arg (every category except KnownDebt)"
for b in "${BINARIES[@]}"; do
  if out="$("$BIN/$b" 2>&1)"; then
    line="$(echo "$out" | grep -E "Passed:|Failed:" | tr '\n' ' ')"
    ok "$b  [$line]"
  else
    line="$(echo "$out" | grep -E "Failed:|FAIL" | head -3 | tr '\n' ' ')"
    bad "$b  [$line]"
    echo "$out" | tail -25 > "/tmp/${b}.log"
    echo "        full tail: /tmp/${b}.log"
  fi
done

# ── 4. DIAG — honest P4 held-out diagnostic tables (informational) ───────────
if [[ $DO_DIAG -eq 1 ]]; then
  note "DIAG — P4 held-out diagnostic (AI-Diag, informational)"
  "$BIN/AIEqualizerPro_IntegrationTests" --category=AI-Diag --verbose 2>&1 \
    | grep -E "class|oracle|Resonance|Harshness|Muddiness|Sibilance|Boominess|Boxyness|Thinness|clean FP|HELD-OUT|----" \
    || echo "  (no AI-Diag output captured)"
fi

# ── Verdict ──────────────────────────────────────────────────────────────────
note "VERDICT"
if [[ $fail -eq 0 ]]; then
  printf '  \033[32mALL GATES GREEN\033[0m — floor intact, 4 binaries no-arg clean.\n'
  exit 0
else
  printf '  \033[31mGATE FAILURE\033[0m — do NOT proceed. Inspect /tmp/*.log above.\n'
  exit 1
fi
