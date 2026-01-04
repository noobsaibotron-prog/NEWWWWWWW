@echo off
setlocal enabledelayedexpansion

cd /d C:\AIEQ
echo === AI EQUALIZER BUILD ===
echo.

REM Trova Visual Studio
set "VSDEV="
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" (
    set "VSDEV=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
    set "VSDEV=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
)
if "%VSDEV%"=="" (
    echo [ERRORE] Visual Studio non trovato!
    pause
    exit /b 1
)

echo [1/4] Setup Visual Studio...
call "%VSDEV%" -arch=x64
if errorlevel 1 (
    echo [ERRORE] VsDevCmd fallito!
    pause
    exit /b 1
)

REM Verifica nmake
where nmake >nul 2>&1
if errorlevel 1 (
    echo [ERRORE] nmake non trovato nel PATH!
    pause
    exit /b 1
)

REM Crea directory build se non esiste
if not exist build mkdir build
cd build

REM Configure CMake
echo [2/4] Configurazione CMake...
if not exist CMakeCache.txt (
    cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="C:\AIEQ\JUCE" ..
    if errorlevel 1 (
        echo [ERRORE] CMake fallito!
        pause
        exit /b 1
    )
)

REM Build
echo [3/4] Compilazione...
nmake
if errorlevel 1 (
    echo [ERRORE] Compilazione fallita!
    pause
    exit /b 1
)

REM Install
echo [4/4] Installazione...
set "VST3SRC=Release\lib\AI Equalizer Pro.vst3"
set "VST3DST=%LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3"

if not exist "%VST3SRC%" (
    echo [ERRORE] VST3 non trovato in %VST3SRC%
    dir /s *.vst3 2>nul
    pause
    exit /b 1
)

if exist "%VST3DST%" rmdir /s /q "%VST3DST%" 2>nul
xcopy /E /I /Y "%VST3SRC%" "%VST3DST%\"
if errorlevel 1 (
    echo [ERRORE] Copia fallita!
    pause
    exit /b 1
)

echo.
echo === COMPLETATO ===
echo VST3 installato in: %VST3DST%
echo.
echo Apri la DAW e fai Rescan dei plugin VST3.
echo.
pause

