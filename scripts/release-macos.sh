#!/usr/bin/env bash

# Developer-ID sign, validate, package and notarize a macOS release. There is
# no unsigned fallback: missing credentials or any failed validation aborts.

set -euo pipefail

BUILD_DIR="${1:-build-release}"
CONFIG="${2:-Release}"
ARTIFACT_ROOT="${BUILD_DIR}/AIEqualizerPro_artefacts/${CONFIG}"
DIST_DIR="${AIEQ_DIST_DIR:-${BUILD_DIR}/dist}"
REPORT_DIR="${AIEQ_VALIDATION_REPORT_DIR:-${BUILD_DIR}/validation-reports}"

fail() {
    printf 'release failed: %s\n' "$*" >&2
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

[[ "$(uname -s)" == "Darwin" ]] || fail "macOS release requires a Darwin host"
[[ -n "${AIEQ_SIGNING_IDENTITY:-}" ]] || fail "AIEQ_SIGNING_IDENTITY is required"
[[ -n "${AIEQ_NOTARY_PROFILE:-}" ]] || fail "AIEQ_NOTARY_PROFILE is required"
[[ -n "${PLUGINVAL_BIN:-}" ]] || fail "PLUGINVAL_BIN is required"

VST3_BUNDLE="$(find_one_bundle "vst3" "${BUILD_DIR}/${CONFIG}/lib" "${ARTIFACT_ROOT}/VST3")"
AU_BUNDLE="$(find_one_bundle "component" "${BUILD_DIR}/${CONFIG}/lib" "${ARTIFACT_ROOT}/AU")"
APP_BUNDLE="$(find_one_bundle "app" "${BUILD_DIR}/${CONFIG}/bin" "${ARTIFACT_ROOT}/Standalone")"

for bundle in "$VST3_BUNDLE" "$AU_BUNDLE" "$APP_BUNDLE"; do
    codesign --force --deep --options runtime --timestamp \
        --sign "$AIEQ_SIGNING_IDENTITY" "$bundle"
    codesign --verify --deep --strict --verbose=2 "$bundle"
done

AIEQ_REQUIRE_SIGNED=1 \
AIEQ_VALIDATION_REPORT_DIR="$REPORT_DIR" \
"$(dirname "$0")/validate-macos-artifacts.sh" "$BUILD_DIR" "$CONFIG"

mkdir -p "$DIST_DIR" "$REPORT_DIR"
STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/aieq-release.XXXXXX")"
trap '/bin/rm -rf "$STAGE_DIR"' EXIT
ditto "$VST3_BUNDLE" "$STAGE_DIR/$(basename "$VST3_BUNDLE")"
ditto "$AU_BUNDLE" "$STAGE_DIR/$(basename "$AU_BUNDLE")"
ditto "$APP_BUNDLE" "$STAGE_DIR/$(basename "$APP_BUNDLE")"

RELEASE_LABEL="${AIEQ_RELEASE_LABEL:-${GITHUB_REF_NAME:-local}}"
DMG_PATH="${DIST_DIR}/AI-Equalizer-Pro-${RELEASE_LABEL}.dmg"
hdiutil create -ov -format UDZO -volname "AI Equalizer Pro" \
    -srcfolder "$STAGE_DIR" "$DMG_PATH"

codesign --force --timestamp --sign "$AIEQ_SIGNING_IDENTITY" "$DMG_PATH"
codesign --verify --verbose=2 "$DMG_PATH"

xcrun notarytool submit "$DMG_PATH" \
    --keychain-profile "$AIEQ_NOTARY_PROFILE" \
    --wait --output-format json | tee "$REPORT_DIR/notary-result.json"
xcrun stapler staple -v "$DMG_PATH"
xcrun stapler validate -v "$DMG_PATH"
spctl --assess --type open --context context:primary-signature -vv "$DMG_PATH"

shasum -a 256 "$DMG_PATH" > "$DIST_DIR/SHA256SUMS"
codesign -dv --verbose=4 "$VST3_BUNDLE" 2> "$REPORT_DIR/vst3-signature.txt"
codesign -dv --verbose=4 "$AU_BUNDLE" 2> "$REPORT_DIR/au-signature.txt"
codesign -dv --verbose=4 "$APP_BUNDLE" 2> "$REPORT_DIR/app-signature.txt"

printf 'signed and notarized release created: %s\n' "$DMG_PATH"
