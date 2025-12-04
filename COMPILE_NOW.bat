@echo off
setlocal enabledelayedexpansion

echo ============================================
echo    AI EQUALIZER PRO - FULL REBUILD
echo ============================================
echo.

:: Find VS2022
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
) else (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
)

echo VS Path: %VS_PATH%

:: Check if VS exists
if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo ERROR: Cannot find Visual Studio at %VS_PATH%
    pause
    exit /b 1
)

:: Initialize environment
echo.
echo [1/6] Initializing Visual Studio environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
echo      Done.

:: Set paths
set "CMAKE_EXE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "PROJECT_DIR=C:\AIEQ"

:: Go to project
cd /d "%PROJECT_DIR%"

:: Clean build completely
echo.
echo [2/6] Cleaning build folder...
if exist build (
    rmdir /s /q build 2>nul
    ping -n 2 127.0.0.1 >nul
    rmdir /s /q build 2>nul
)
mkdir build
cd build
echo      Done.

:: Configure
echo.
echo [3/6] Configuring CMake (NMake)...
"%CMAKE_EXE%" .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo      FAILED!
    pause
    exit /b 1
)
echo      Done.

:: Build
echo.
echo [4/6] Building project (this takes 2-5 minutes)...
nmake
if %ERRORLEVEL% neq 0 (
    echo      FAILED!
    pause
    exit /b 1
)
echo      Done.

:: Find VST3
echo.
echo [5/6] Locating VST3 output...
set "VST3_FOUND="
for /r "%PROJECT_DIR%\build" %%F in (*.vst3) do (
    if exist "%%F\Contents" (
        set "VST3_SRC=%%F"
        set "VST3_FOUND=1"
    )
)

if not defined VST3_FOUND (
    echo      WARNING: VST3 bundle not found!
    dir /s /b "%PROJECT_DIR%\build\*.vst3" 2>nul
    pause
    exit /b 1
)

echo      Found: %VST3_SRC%

:: Install
echo.
echo [6/6] Installing to VST3 folders...

set "LOCAL_VST3=%LOCALAPPDATA%\VST3"
set "COMMON_VST3=C:\Program Files\Common Files\VST3"

:: Remove old versions
if exist "%LOCAL_VST3%\AI Equalizer Pro.vst3" rmdir /s /q "%LOCAL_VST3%\AI Equalizer Pro.vst3"
if exist "%COMMON_VST3%\AI Equalizer Pro.vst3" rmdir /s /q "%COMMON_VST3%\AI Equalizer Pro.vst3"

:: Copy new version
if not exist "%LOCAL_VST3%" mkdir "%LOCAL_VST3%"
xcopy /E /I /Y "%VST3_SRC%" "%LOCAL_VST3%\AI Equalizer Pro.vst3" >nul
echo      Installed to: %LOCAL_VST3%

xcopy /E /I /Y "%VST3_SRC%" "%COMMON_VST3%\AI Equalizer Pro.vst3" >nul
echo      Installed to: %COMMON_VST3%

echo.
echo ============================================
echo    BUILD AND INSTALLATION COMPLETE!
echo ============================================
echo.
echo The following features are now included:
echo   - Semantic EQ Engine (Air, Warmth, Punch...)
echo   - Natural Language Control
echo   - Enhanced AI Detection
echo   - Dynamic EQ
echo.
echo To use in Ableton:
echo   1. Close Ableton
echo   2. Reopen Ableton
echo   3. Preferences - Plug-ins - Rescan
echo   4. Search "AI Equalizer Pro"
echo.
pause
