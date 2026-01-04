# Quick Build Guide

## Building AI Equalizer Pro V2

### ⚠️ IMPORTANT: Run from Project Directory

**You must be in the project directory to run the build script!**

```batch
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
BUILD_FINAL.bat
```

### Option 1: Automatic Build (Recommended)
```batch
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
BUILD_FINAL.bat
```

This will:
- Auto-detect JUCE in the project directory
- Auto-detect Visual Studio
- Build in Release mode
- Use the `build` directory

### Option 2: Custom Build Type
```batch
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
BUILD_FINAL.bat Debug
BUILD_FINAL.bat Release
```

### Option 3: Custom Build Directory
```batch
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
BUILD_FINAL.bat Release my_build_dir
```

### Option 4: Specify JUCE Path (if needed)
```batch
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
BUILD_FINAL.bat Release build "C:\Path\To\JUCE"
```

**Note**: If JUCE is in the project directory (which it is), you don't need to specify the path!

## Quick Start (One Command)

Open Command Prompt or PowerShell and run:

```batch
cd /d C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2 && BUILD_FINAL.bat
```

Or in PowerShell:
```powershell
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2; .\BUILD_FINAL.bat
```

## JUCE Detection

The build script automatically searches for JUCE in these locations (in order):
1. `.\JUCE\CMakeLists.txt` (project directory) ✅ **This is where yours is!**
2. `..\JUCE\CMakeLists.txt` (parent directory)
3. `..\..\JUCE\CMakeLists.txt` (grandparent directory)
4. `C:\JUCE\CMakeLists.txt`
5. `%USERPROFILE%\JUCE\CMakeLists.txt`
6. `%JUCE_PATH%` environment variable

If JUCE is found in the project directory (most common case), no configuration needed!

## Troubleshooting

### Error: "BUILD_FINAL.bat non è riconosciuto"
**Solution**: You're in the wrong directory!
```batch
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
BUILD_FINAL.bat
```

### Error: "JUCE not found"
**Solution**: JUCE is in the project directory, so this should not happen. If it does:
1. Verify `JUCE\CMakeLists.txt` exists
2. Or set environment variable: `set JUCE_PATH=C:\Path\To\JUCE`
3. Or pass as 3rd argument: `BUILD_FINAL.bat Release build "C:\Path\To\JUCE"`

### Error: "CMake not found"
**Solution**: 
- Install CMake from https://cmake.org/
- Or use Visual Studio's bundled CMake (script will find it automatically)

### Error: "Visual Studio not found"
**Solution**:
- Install Visual Studio 2022 or 2024 with C++ workload
- The script will auto-detect it using vswhere.exe

## Build Output

After successful build, find your plugin at:
```
build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3
```

## Testing Build Configuration

Run the test script to verify everything is set up correctly:
```powershell
cd C:\Users\noobs\Downloads\AIEqualizer_V2_NOVA\AIEqualizer_V2
powershell -ExecutionPolicy Bypass -File test_build.ps1
```
