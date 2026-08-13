#!/usr/bin/env bash
# Reproducible sanitizer gate for the blocking thread-safety regression target.
#
# Usage:
#   scripts/build_sanitize.sh thread
#   scripts/build_sanitize.sh address,undefined
#
# Optional environment:
#   BUILD_DIR=/absolute/or/relative/path  (defaults to build-tsan or build-san)
#   JOBS=8
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SANITIZER="${1:-thread}"
JOBS="${JOBS:-8}"

case "$SANITIZER" in
  thread)
    DEFAULT_BUILD_DIR="$ROOT/build-tsan"
    export TSAN_OPTIONS="${TSAN_OPTIONS:-halt_on_error=1:abort_on_error=1}"
    ;;
  address,undefined)
    DEFAULT_BUILD_DIR="$ROOT/build-san"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:abort_on_error=1:detect_leaks=1}"
    export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:abort_on_error=1:print_stacktrace=1}"
    ;;
  *)
    printf 'unsupported sanitizer set: %s\n' "$SANITIZER" >&2
    printf 'expected: thread | address,undefined\n' >&2
    exit 2
    ;;
esac

BUILD_DIR="${BUILD_DIR:-$DEFAULT_BUILD_DIR}"

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAIEQ_SANITIZE="$SANITIZER"

cmake --build "$BUILD_DIR" \
  --target AIEqualizerPro_ThreadSafetyTests \
  --parallel "$JOBS"

ctest --test-dir "$BUILD_DIR" \
  -R '^aieq_thread_safety_regression$' \
  --output-on-failure \
  --timeout 180
