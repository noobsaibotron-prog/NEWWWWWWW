@echo off
echo === COMPILAZIONE RELEASE ===
echo.

cd C:\AIEQ\build

echo [INFO] Pulizia cache CMake per forzare Release...
if exist CMakeCache.txt del CMakeCache.txt
if exist CMakeFiles rd /s /q CMakeFiles 2>nul

echo [INFO] Riconfigurazione CMake in Release...
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -DCMAKE_BUILD_TYPE=Release ..

if errorlevel 1 (
    echo [ERRORE] Configurazione CMake fallita!
    pause
    exit /b 1
)

echo [INFO] Compilazione in Release...
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . --config Release

if errorlevel 1 (
    echo [ERRORE] Compilazione fallita!
    pause
    exit /b 1
)

echo.
echo === BUILD RELEASE COMPLETATA! ===
if exist "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3" (
    echo [OK] Plugin VST3 Release trovato!
    dir "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
) else (
    echo [AVVISO] Plugin Release non trovato nella posizione attesa
)

pause

