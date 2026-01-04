@echo off
echo === BUILD RELEASE ===

cd C:\AIEQ\build

REM Aggiungi Windows SDK al PATH per rc.exe
set "PATH=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;%PATH%"

REM Compila
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . --config Release

if errorlevel 1 (
    echo [ERRORE] Compilazione fallita!
    pause
    exit /b 1
)

echo.
echo === SUCCESSO! ===
if exist "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3" (
    echo [OK] Plugin RELEASE compilato!
    dir "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
)

pause

