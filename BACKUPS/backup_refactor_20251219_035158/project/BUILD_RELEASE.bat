@echo off
cd /d "%~dp0"

echo === AI EQUALIZER RELEASE BUILD ===
echo.

REM Setup Visual Studio
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul 2>&1

cd build

REM Configure
if not exist CMakeCache.txt (
    echo [1/3] Configuring...
    cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="C:\AIEQ\JUCE" ..
    if errorlevel 1 exit /b 1
) else (
    echo [1/3] Already configured
)

REM Build
echo [2/3] Building...
nmake
if errorlevel 1 exit /b 1

echo [3/3] Installing...

REM Find VST3
set VST3=
for /r "AIEqualizerPro_artefacts\Release\VST3" %%f in (*.vst3) do set VST3=%%f
if "%VST3%"=="" (
    if exist "Release\lib\AI Equalizer Pro.vst3" set VST3=Release\lib\AI Equalizer Pro.vst3
)

if "%VST3%"=="" (
    echo [ERROR] VST3 not found!
    pause
    exit /b 1
)

echo   Found: %VST3%

REM Copy to LocalAppData
set LOCAL=%LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3
if exist "%LOCAL%" rmdir /s /q "%LOCAL%" 2>nul
xcopy /E /I /Y "%VST3%" "%LOCAL%\" >nul
echo   [OK] LocalAppData: %LOCAL%

REM Find and copy JSON
for /r "AIEqualizerPro_artefacts\Release\VST3" %%f in (moduleinfo.json) do (
    copy /Y "%%f" "%LOCAL%\moduleinfo.json" >nul 2>&1
    echo   [OK] moduleinfo.json copied
    goto :json_done
)
:json_done

REM Try Common Files (may need admin)
set COMMON=C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3
xcopy /E /I /Y "%VST3%" "%COMMON%\" >nul 2>&1
if not errorlevel 1 (
    echo   [OK] Common Files: %COMMON%
    for /r "AIEqualizerPro_artefacts\Release\VST3" %%f in (moduleinfo.json) do (
        copy /Y "%%f" "%COMMON%\moduleinfo.json" >nul 2>&1
    )
) else (
    echo   [WARNING] Common Files requires admin
)

echo.
echo === DONE ===
echo Plugin: %LOCAL%
echo.
pause
