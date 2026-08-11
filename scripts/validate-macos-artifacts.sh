#!/usr/bin/env bash

# Blocking validation for built macOS artifacts. This script deliberately
# rejects missing or ambiguous bundles: a green release must prove that the
# expected VST3, AU and Standalone products all exist and load.

set -euo pipefail

BUILD_DIR="${1:-build-mac}"
CONFIG="${2:-Release}"
ARTIFACT_ROOT="${BUILD_DIR}/AIEqualizerPro_artefacts/${CONFIG}"
REPORT_DIR="${AIEQ_VALIDATION_REPORT_DIR:-${BUILD_DIR}/validation-reports}"
PLUGINVAL_STRICTNESS="${PLUGINVAL_STRICTNESS:-10}"
REQUIRE_SIGNED="${AIEQ_REQUIRE_SIGNED:-0}"
EXPECTED_BUNDLE_ID="${AIEQ_EXPECTED_BUNDLE_ID:-com.AIAudio.AIEqualizerPro}"
AU_SUBTYPE="${AIEQ_AU_SUBTYPE:-AIEQ}"
EXPECTED_MODEL_PATH="${AIEQ_EXPECTED_MODEL_PATH:-Resources/Models/ml_weights.bin}"

fail() {
    printf 'release validation failed: %s\n' "$*" >&2
    exit 1
}

find_one_bundle() {
    local suffix="$1"
    shift
    local root
    local count
    local result

    for root in "$@"; do
        [[ -d "$root" ]] || continue
        count="$(find "$root" -maxdepth 1 -type d -name "*.${suffix}" | wc -l | tr -d ' ')"
        [[ "$count" != "0" ]] || continue
        [[ "$count" == "1" ]] || fail "expected at most one .${suffix} in $root, found $count"
        result="$(find "$root" -maxdepth 1 -type d -name "*.${suffix}" -print | head -n 1)"
        printf '%s\n' "$result"
        return 0
    done

    fail "could not resolve one .${suffix} bundle in any expected artifact directory: $*"
}

validate_bundle() {
    local bundle="$1"
    local plist="${bundle}/Contents/Info.plist"
    local executable_name
    local executable_path
    local bundle_id
    local architectures

    [[ -f "$plist" ]] || fail "missing Info.plist: $plist"
    bundle_id="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$plist")"
    [[ "$bundle_id" == "$EXPECTED_BUNDLE_ID" ]] \
        || fail "unexpected bundle identifier '$bundle_id' in $bundle"

    executable_name="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$plist")"
    executable_path="${bundle}/Contents/MacOS/${executable_name}"
    [[ -f "$executable_path" ]] || fail "missing bundle executable: $executable_path"
    local packaged_model="${bundle}/Contents/Resources/models/ml_weights.bin"
    [[ -s "$packaged_model" ]] \
        || fail "missing packaged ML weights in Contents/Resources/models: $bundle"
    [[ -s "$EXPECTED_MODEL_PATH" ]] || fail "expected source ML weights do not exist: $EXPECTED_MODEL_PATH"
    cmp -s "$EXPECTED_MODEL_PATH" "$packaged_model" \
        || fail "packaged ML weights differ from $EXPECTED_MODEL_PATH: $bundle"

    architectures="$(lipo -archs "$executable_path")"
    [[ " $architectures " == *" arm64 "* ]] || fail "$bundle is missing arm64"
    [[ " $architectures " == *" x86_64 "* ]] || fail "$bundle is missing x86_64"

    if [[ "$REQUIRE_SIGNED" == "1" ]]; then
        codesign --verify --deep --strict --verbose=2 "$bundle"
    fi
}

[[ "$(uname -s)" == "Darwin" ]] || fail "this validator requires macOS"
[[ -n "${PLUGINVAL_BIN:-}" ]] || fail "PLUGINVAL_BIN must point to the pinned pluginval executable"
[[ -x "$PLUGINVAL_BIN" ]] || fail "pluginval is not executable: $PLUGINVAL_BIN"

VST3_BUNDLE="$(find_one_bundle "vst3" "${BUILD_DIR}/${CONFIG}/lib" "${ARTIFACT_ROOT}/VST3")"
AU_BUNDLE="$(find_one_bundle "component" "${BUILD_DIR}/${CONFIG}/lib" "${ARTIFACT_ROOT}/AU")"
APP_BUNDLE="$(find_one_bundle "app" "${BUILD_DIR}/${CONFIG}/bin" "${ARTIFACT_ROOT}/Standalone")"

mkdir -p "$REPORT_DIR/pluginval"
validate_bundle "$VST3_BUNDLE"
validate_bundle "$AU_BUNDLE"
validate_bundle "$APP_BUNDLE"

"$PLUGINVAL_BIN" \
    --strictness-level "$PLUGINVAL_STRICTNESS" \
    --output-dir "$REPORT_DIR/pluginval" \
    "$VST3_BUNDLE"

# AU discovery is registry based. CI uses an isolated runner home, so installing
# the just-built component here cannot mask a different system installation.
AU_INSTALL_ROOT="${AIEQ_AU_INSTALL_ROOT:-${HOME}/Library/Audio/Plug-Ins/Components}"
mkdir -p "$AU_INSTALL_ROOT"
AU_INSTALL_PATH="${AU_INSTALL_ROOT}/$(basename "$AU_BUNDLE")"
AU_BACKUP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/aieq-au-backup.XXXXXX")"
AU_HAD_PREVIOUS=0

restore_previous_au() {
    /bin/rm -rf "$AU_INSTALL_PATH"
    if [[ "$AU_HAD_PREVIOUS" == "1" ]]; then
        ditto "${AU_BACKUP_DIR}/$(basename "$AU_BUNDLE")" "$AU_INSTALL_PATH"
    fi
    /bin/rm -rf "$AU_BACKUP_DIR"
    killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
}
trap restore_previous_au EXIT

if [[ -d "$AU_INSTALL_PATH" ]]; then
    ditto "$AU_INSTALL_PATH" "${AU_BACKUP_DIR}/$(basename "$AU_BUNDLE")"
    AU_HAD_PREVIOUS=1
fi
/bin/rm -rf "$AU_INSTALL_PATH"
ditto "$AU_BUNDLE" "$AU_INSTALL_PATH"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

auval -v aufx "$AU_SUBTYPE" AIAU | tee "$REPORT_DIR/auval.txt"

{
    printf 'VST3=%s\n' "$VST3_BUNDLE"
    printf 'AU=%s\n' "$AU_BUNDLE"
    printf 'Standalone=%s\n' "$APP_BUNDLE"
    printf 'pluginval_strictness=%s\n' "$PLUGINVAL_STRICTNESS"
    printf 'signed_required=%s\n' "$REQUIRE_SIGNED"
    printf 'au_subtype=%s\n' "$AU_SUBTYPE"
} > "$REPORT_DIR/artifacts.txt"

printf 'macOS artifact validation passed\n'
