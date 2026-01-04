@echo off
setlocal enabledelayedexpansion

echo === AI EQUALIZER RELEASE BUILD ===
echo.

cd /d "%~dp0"

REM Setup Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul 2>&1

cd build

REM Configure if needed
if not exist CMakeCache.txt (
    echo [1/4] Configuring CMake...
    cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="C:\AIEQ\JUCE" ..
    if errorlevel 1 (
        echo [ERROR] CMake failed!
        pause
        exit /b 1
    )
) else (
    echo [1/4] CMake already configured
)

REM Build
echo [2/4] Building Release...
nmake
if errorlevel 1 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo [3/4] Finding VST3 and JSON files...

REM Find VST3
set VST3_PATH=
for /r "AIEqualizerPro_artefacts\Release\VST3" %%f in (AI Equalizer Pro.vst3) do (
    set VST3_PATH=%%f
    goto :found_vst3
)
:found_vst3

if "!VST3_PATH!"=="" (
    echo [ERROR] VST3 not found!
    pause
    exit /b 1
)

REM Find JSON
set JSON_PATH=
for /r "AIEqualizerPro_artefacts\Release\VST3" %%f in (moduleinfo.json) do (
    set JSON_PATH=%%f
    goto :found_json
)
:found_json

echo   VST3: !VST3_PATH!
if not "!JSON_PATH!"=="" echo   JSON: !JSON_PATH!

echo [4/4] Installing...

REM LocalAppData
set LOCAL_VST3=%LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3
if exist "!LOCAL_VST3!" rmdir /s /q "!LOCAL_VST3!"
xcopy /E /I /Y "!VST3_PATH!" "!LOCAL_VST3!\" >nul
if errorlevel 1 (
    echo [ERROR] Failed to copy to LocalAppData!
    pause
    exit /b 1
)
echo   [OK] LocalAppData: !LOCAL_VST3!

if not "!JSON_PATH!"=="" (
    copy /Y "!JSON_PATH!" "!LOCAL_VST3!\moduleinfo.json" >nul
    echo   [OK] JSON copied to LocalAppData
)

REM Common Files
set COMMON_VST3=C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3
if exist "!COMMON_VST3!" rmdir /s /q "!COMMON_VST3!" 2>nul
xcopy /E /I /Y "!VST3_PATH!" "!COMMON_VST3!\" >nul 2>&1
if errorlevel 1 (
    echo   [WARNING] Common Files copy failed (may need admin)
) else (
    echo   [OK] Common Files: !COMMON_VST3!
    if not "!JSON_PATH!"=="" (
        copy /Y "!JSON_PATH!" "!COMMON_VST3!\moduleinfo.json" >nul 2>&1
    )
)

echo.
echo === COMPLETATO ===
echo Plugin installato in:
echo   !LOCAL_VST3!
echo   !COMMON_VST3!
echo.
pause

