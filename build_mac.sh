#!/usr/bin/env bash
set -euo pipefail

# Simple macOS build helper for AIEqualizerPro
# Usage: ./build_mac.sh [Debug|Release]   (default: Release)
# You can override defaults via env:
#   BUILD_DIR=build-mac
#   ARCHS="x86_64;arm64"
#   DEPLOY_TARGET=11.0
#   JUCE_PATH=/path/to/JUCE    (optional, if not auto-detected)

CONFIG="${1:-Release}"
BUILD_DIR="${BUILD_DIR:-build-mac}"
ARCHS="${ARCHS:-x86_64;arm64}"
DEPLOY_TARGET="${DEPLOY_TARGET:-11.0}"

JUCE_ARG=()
if [[ -n "${JUCE_PATH:-}" ]]; then
  JUCE_ARG+=("-DJUCE_PATH=${JUCE_PATH}")
fi

echo "Configuring CMake project..."
cmake -S . -B "${BUILD_DIR}" -G "Ninja" \
  -DCMAKE_BUILD_TYPE="${CONFIG}" \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_OSX_ARCHITECTURES="${ARCHS}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOY_TARGET}" \
  ${JUCE_ARG[@]+"${JUCE_ARG[@]}"}

echo "Building (${CONFIG})..."
cmake --build "${BUILD_DIR}" --config "${CONFIG}"

cat <<'EOF'
Build complete.
Artefacts are in:
  VST3:        <build>/AIEqualizerPro_artefacts/<cfg>/VST3/AI Equalizer Pro.vst3
  AU:          <build>/AIEqualizerPro_artefacts/<cfg>/AU/AI Equalizer Pro.component
  Standalone:  <build>/AIEqualizerPro_artefacts/<cfg>/Standalone/AI Equalizer Pro.app
Where <build> = BUILD_DIR (default: build-mac) and <cfg> = Debug/Release.
EOF

