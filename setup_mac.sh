#!/usr/bin/env bash
set -euo pipefail

# Simple mac setup + build helper for AIEqualizerPro
# - Verifica Xcode Command Line Tools
# - Installa Homebrew se manca
# - Installa CMake via brew se manca
# - Lancia build_mac.sh (Release di default)
#
# Uso:
#   ./setup_mac.sh [Debug|Release]
# Opzioni via env:
#   JUCE_PATH=/percorso/JUCE        # se vuoi usare una copia JUCE esterna
#   ARCHS="x86_64;arm64"            # override arch
#   DEPLOY_TARGET=11.0              # override deployment target
#   BUILD_DIR=build-mac             # override build dir

CONFIG="${1:-Release}"

need_xcode() {
    if ! xcode-select -p >/dev/null 2>&1; then
        echo "⚠️  Xcode Command Line Tools non rilevati."
        echo "    Esegui: xcode-select --install"
        echo "    Poi rilancia questo script."
        exit 1
    fi
}

ensure_brew() {
    if ! command -v brew >/dev/null 2>&1; then
        echo "Homebrew non trovato: lo installo..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        # Aggiungi brew al PATH per la sessione corrente
        eval "$(/opt/homebrew/bin/brew shellenv 2>/dev/null || true)"
        eval "$(/usr/local/bin/brew shellenv 2>/dev/null || true)"
    fi
}

ensure_cmake() {
    if ! command -v cmake >/dev/null 2>&1; then
        echo "CMake non trovato: installo con brew..."
        brew install cmake
    fi
}

run_build() {
    echo "Avvio build (${CONFIG})..."
    ./build_mac.sh "${CONFIG}"
}

need_xcode
ensure_brew
ensure_cmake
run_build

