@echo off
setlocal

rem ------------------------------------------------------------
rem Configurazione: VS 2022 (stabile) + SDK 10.0.22621.0
rem Se vuoi provare VS 2026, sostituisci i tre set qui sotto con i percorsi VS 18 e GENERATOR=Visual Studio 18 2026
rem ------------------------------------------------------------
set "VS_CMAKE=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "GENERATOR=Visual Studio 17 2022"

set "SDK_VERSION=10.0.22621.0"
set "SDK_BASE=C:\Program Files (x86)\Windows Kits\10\Lib\%SDK_VERSION%"

rem ------------------------------------------------------------
rem Inizializza ambiente VS
rem ------------------------------------------------------------
if not exist "%VS_VCVARS%" (
    echo [ERRORE] vcvarsall non trovato: %VS_VCVARS%
    exit /b 1
)
call "%VS_VCVARS%" x64
if errorlevel 1 (
    echo [ERRORE] vcvarsall ha fallito
    exit /b 1
)

rem ------------------------------------------------------------
rem Forza variabili SDK e percorsi LIB/LIBPATH
rem ------------------------------------------------------------
set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10\"
set "WindowsSDKVersion=%SDK_VERSION%\"
set "WindowsSdkLibVersion=%SDK_VERSION%\\"
set "LIB=%SDK_BASE%\ucrt\x64;%SDK_BASE%\um\x64;%LIB%"
set "LIBPATH=%SDK_BASE%\ucrt\x64;%SDK_BASE%\um\x64;%LIBPATH%"

rem ------------------------------------------------------------
rem Pulisce e configura
rem ------------------------------------------------------------
cd /d "%~dp0"
rmdir /s /q build 2>nul
mkdir build 2>nul

"%VS_CMAKE%" -S . -B build -G "%GENERATOR%" -A x64 ^
    -DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION=%SDK_VERSION% ^
    -DCMAKE_SYSTEM_VERSION=%SDK_VERSION%
if errorlevel 1 (
    echo [ERRORE] Configurazione CMake fallita
    exit /b 1
)

rem ------------------------------------------------------------
rem Build
rem ------------------------------------------------------------
"%VS_CMAKE%" --build build --config Release
if errorlevel 1 (
    echo [ERRORE] Build fallita
    exit /b 1
)

rem ------------------------------------------------------------
rem Verifica artefatto
rem ------------------------------------------------------------
if exist "build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3" (
    echo [OK] Plugin generato in build\AIEqualizerPro_artefacts\Release\VST3
) else (
    echo [AVVISO] Plugin non trovato dopo la build (controlla log)
)

endlocal
