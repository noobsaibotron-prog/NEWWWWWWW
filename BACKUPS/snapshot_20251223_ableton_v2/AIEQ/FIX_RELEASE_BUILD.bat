@echo off
echo === FIX RELEASE BUILD ===
echo.

cd C:\AIEQ\build

echo [1/3] Pulizia cache CMake...
if exist CMakeCache.txt del CMakeCache.txt
if exist CMakeFiles rd /s /q CMakeFiles 2>nul
echo [OK] Cache pulito

echo.
echo [2/3] Riconfigurazione CMake in RELEASE...
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe" -DCMAKE_CXX_COMPILER="C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe" -DJUCE_PATH="C:\AIEQ\JUCE" ..

if errorlevel 1 (
    echo [ERRORE] Configurazione fallita!
    pause
    exit /b 1
)

echo.
echo [3/3] Compilazione RELEASE...
echo [INFO] Aggiunta Windows SDK al PATH per rc.exe...
set "PATH=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;%PATH%"
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build .

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
) else (
    echo [AVVISO] Controlla la directory Release
    dir /s /b AIEqualizerPro_artefacts\Release\*.vst3 2>nul
)

pause

