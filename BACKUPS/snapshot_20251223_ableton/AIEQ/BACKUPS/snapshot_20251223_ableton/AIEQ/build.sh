#!/bin/bash

echo ""
echo "============================================"
echo "   AI EQUALIZER PRO V2 - Build Script"
echo "============================================"
echo ""

# Configuration
PROJECT_NAME="AIEqualizerPro"
BUILD_TYPE="Release"
BUILD_DIR="build"

# Find JUCE
if [ -z "$JUCE_PATH" ]; then
    if [ -d "/Applications/JUCE" ]; then
        JUCE_PATH="/Applications/JUCE"
    elif [ -d "$HOME/JUCE" ]; then
        JUCE_PATH="$HOME/JUCE"
    elif [ -d "../JUCE" ]; then
        JUCE_PATH="../JUCE"
    elif [ -d "../../JUCE" ]; then
        JUCE_PATH="../../JUCE"
    else
        echo "[ERROR] JUCE not found! Please set JUCE_PATH environment variable."
        echo "        Example: export JUCE_PATH=/Applications/JUCE"
        exit 1
    fi
fi

echo "[INFO] Using JUCE from: $JUCE_PATH"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure
echo "[STEP 1/3] Configuring CMake..."
echo ""

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DJUCE_PATH="$JUCE_PATH"

if [ $? -ne 0 ]; then
    echo ""
    echo "[ERROR] CMake configuration failed!"
    exit 1
fi

# Build
echo ""
echo "[STEP 2/3] Building $BUILD_TYPE..."
echo ""

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel

if [ $? -ne 0 ]; then
    echo ""
    echo "[ERROR] Build failed!"
    exit 1
fi

# Find outputs
echo ""
echo "[STEP 3/3] Locating built plugins..."
echo ""

# macOS
if [ "$(uname)" == "Darwin" ]; then
    VST3_PATH=$(find "$BUILD_DIR" -name "*.vst3" -type d 2>/dev/null | head -1)
    AU_PATH=$(find "$BUILD_DIR" -name "*.component" -type d 2>/dev/null | head -1)
    APP_PATH=$(find "$BUILD_DIR" -name "*.app" -type d 2>/dev/null | head -1)
    
    if [ -n "$VST3_PATH" ]; then
        echo "[SUCCESS] VST3 Plugin: $VST3_PATH"
    fi
    
    if [ -n "$AU_PATH" ]; then
        echo "[SUCCESS] AU Plugin: $AU_PATH"
    fi
    
    if [ -n "$APP_PATH" ]; then
        echo "[SUCCESS] Standalone App: $APP_PATH"
    fi
    
    echo ""
    echo "To install plugins, copy to:"
    echo "  VST3: ~/Library/Audio/Plug-Ins/VST3/"
    echo "  AU:   ~/Library/Audio/Plug-Ins/Components/"
# Linux
else
    VST3_PATH=$(find "$BUILD_DIR" -name "*.vst3" -type d 2>/dev/null | head -1)
    STANDALONE_PATH=$(find "$BUILD_DIR" -type f -executable -name "*Standalone*" 2>/dev/null | head -1)
    
    if [ -n "$VST3_PATH" ]; then
        echo "[SUCCESS] VST3 Plugin: $VST3_PATH"
    fi
    
    if [ -n "$STANDALONE_PATH" ]; then
        echo "[SUCCESS] Standalone: $STANDALONE_PATH"
    fi
    
    echo ""
    echo "To install VST3 plugin, copy to:"
    echo "  ~/.vst3/"
fi

echo ""
echo "============================================"
echo "   BUILD COMPLETE!"
echo "============================================"
echo ""
