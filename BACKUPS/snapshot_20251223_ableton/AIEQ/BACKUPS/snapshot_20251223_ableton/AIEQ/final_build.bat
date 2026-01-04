@echo off
setlocal enabledelayedexpansion
echo ========================================
echo    AI EQUALIZER PRO - FINAL BUILD
echo ========================================
echo.

:: Setup VS2022
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "CMAKE_EXE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

:: Check VS exists
if not exist "%VS_PATH%" (
    echo ERROR: Visual Studio 2022 not found at %VS_PATH%
    pause
    exit /b 1
)

:: Initialize environment
echo [1/5] Initializing Visual Studio 2022 environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if %ERRORLEVEL% neq 0 (
    echo WARNING: vcvarsall.bat returned error, continuing anyway...
)

:: Change to project directory  
cd /d "C:\AIEQ"

:: Force clean build folder
echo [2/5] Cleaning build folder...
if exist build (
    del /f /q /s build\* >nul 2>&1
    rmdir /s /q build 2>nul
    timeout /t 2 /nobreak >nul
    rmdir /s /q build 2>nul
)
if not exist build mkdir build
cd build

:: Configure CMake
echo [3/5] Configuring CMake...
"%CMAKE_EXE%" .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo FAILED: CMake configuration
    pause
    exit /b 1
)
echo OK: CMake configured

:: Build
echo [4/5] Building (this takes a few minutes)...
nmake
if %ERRORLEVEL% neq 0 (
    echo FAILED: Build
    pause  
    exit /b 1
)
echo OK: Build completed

:: Copy VST3
echo [5/5] Installing VST3...
set "VST3_SRC="
for /r "C:\AIEQ\build" %%F in (*.vst3) do (
    if exist "%%F\*" set "VST3_SRC=%%F"
)

if defined VST3_SRC (
    echo Found: %VST3_SRC%
    
    :: Local user folder
    set "LOCAL_VST3=%LOCALAPPDATA%\VST3"
    if not exist "!LOCAL_VST3!" mkdir "!LOCAL_VST3!"
    xcopy /E /I /Y "%VST3_SRC%" "!LOCAL_VST3!\AI Equalizer Pro.vst3" >nul
    echo Installed to: !LOCAL_VST3!\AI Equalizer Pro.vst3
    
    :: Common Files (system-wide)
    set "COMMON_VST3=C:\Program Files\Common Files\VST3"
    xcopy /E /I /Y "%VST3_SRC%" "!COMMON_VST3!\AI Equalizer Pro.vst3" >nul
    echo Installed to: !COMMON_VST3!\AI Equalizer Pro.vst3
    
) else (
    echo WARNING: VST3 not found
    dir /s /b "C:\AIEQ\build\*.vst3" 2>nul
)

echo.
echo ========================================
echo    BUILD AND INSTALLATION COMPLETE!
echo ========================================
echo.
echo In Ableton:
echo   1. Go to Preferences ^> Plug-ins
echo   2. Click "Rescan"
echo   3. Search for "AI Equalizer Pro"
echo.
pause
