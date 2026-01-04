@echo off
setlocal enabledelayedexpansion
title AI Equalizer Pro - Building with Semantic Engine

echo ============================================================
echo    AI EQUALIZER PRO - FULL BUILD WITH SEMANTIC ENGINE
echo ============================================================
echo.

:: Find VS
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
if not exist "%VS_PATH%" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"

if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo ERROR: Visual Studio not found!
    pause
    exit /b 1
)

echo [1/5] Found VS at: %VS_PATH%
echo.

:: Init environment
echo [2/5] Initializing build environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
echo       Done.
echo.

:: Set cmake path
set "CMAKE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

:: Go to project
cd /d C:\AIEQ

:: Verify Semantic Engine files exist
echo [3/5] Verifying Semantic Engine files...
if exist "Source\AI\SemanticEQEngine.cpp" (
    echo       SemanticEQEngine.cpp - OK
) else (
    echo       SemanticEQEngine.cpp - MISSING!
    pause
    exit /b 1
)
if exist "Source\AI\SemanticEQEngine.h" (
    echo       SemanticEQEngine.h - OK
) else (
    echo       SemanticEQEngine.h - MISSING!
    pause
    exit /b 1
)
if exist "Source\GUI\SemanticControlPanel.h" (
    echo       SemanticControlPanel.h - OK
) else (
    echo       SemanticControlPanel.h - MISSING!
    pause
    exit /b 1
)
echo.

:: Clean and rebuild
echo [4/5] Building project...
if exist build rmdir /s /q build 2>nul
mkdir build
cd build

echo       Configuring CMake...
"%CMAKE%" .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release >nul
if %ERRORLEVEL% neq 0 (
    echo       CMake FAILED!
    pause
    exit /b 1
)
echo       CMake configured.

echo       Compiling (this takes 2-3 minutes)...
nmake 2>&1 | findstr /C:"Building" /C:"Linking" /C:"error" /C:"%"
if %ERRORLEVEL% neq 0 (
    echo       Build may have failed. Check above for errors.
)

:: Check output
echo.
echo [5/5] Checking output...
if exist "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3" (
    echo       VST3 bundle created successfully!
) else (
    echo       VST3 not found at expected location!
    dir /s /b *.vst3 2>nul
    pause
    exit /b 1
)

:: Install
echo.
echo Installing to VST3 folders...

set "SRC=C:\AIEQ\build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
set "DLL=C:\AIEQ\build\Release\lib\AI Equalizer Pro.vst3"

:: Local
set "LOCAL_VST3=%LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3"
if exist "%LOCAL_VST3%" rmdir /s /q "%LOCAL_VST3%"
xcopy /E /I /Y "%SRC%" "%LOCAL_VST3%" >nul
if exist "%DLL%" copy /Y "%DLL%" "%LOCAL_VST3%\Contents\x86_64-win\" >nul
echo       Installed to Local AppData

:: Common Files (may need admin)
set "COMMON_VST3=C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3"
if exist "%COMMON_VST3%" rmdir /s /q "%COMMON_VST3%" 2>nul
xcopy /E /I /Y "%SRC%" "%COMMON_VST3%" >nul 2>&1
if exist "%DLL%" copy /Y "%DLL%" "%COMMON_VST3%\Contents\x86_64-win\" >nul 2>&1
echo       Installed to Common Files

echo.
echo ============================================================
echo    BUILD COMPLETE!
echo ============================================================
echo.
echo Plugin includes:
echo   - Semantic EQ Engine (Air, Warmth, Punch, Clarity...)
echo   - Natural Language Control
echo   - Enhanced AI Detection
echo   - Dynamic EQ per band
echo.
echo In Ableton:
echo   1. Preferences - Plug-ins - Rescan
echo   2. Search "AI Equalizer Pro"
echo.
pause
