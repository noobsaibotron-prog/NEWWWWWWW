@echo off
setlocal enabledelayedexpansion

echo === QUICK BUILD ===

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "CMAKE_PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

echo Initializing VS environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

cd /d C:\AIEQ

echo Cleaning build folder...
if exist build rmdir /s /q build
mkdir build
cd build

echo Configuring CMake...
"%CMAKE_PATH%" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo Building...
"%CMAKE_PATH%" --build . --config Release --parallel
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo === BUILD COMPLETED ===
dir /s /b *.vst3 2>nul

echo.
echo Copying VST3 to standard locations...

set "VST3_SOURCE=C:\AIEQ\build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
set "VST3_LOCAL=%LOCALAPPDATA%\VST3"
set "VST3_COMMON=C:\Program Files\Common Files\VST3"

if exist "%VST3_SOURCE%" (
    echo Found: %VST3_SOURCE%
    
    if not exist "%VST3_LOCAL%" mkdir "%VST3_LOCAL%"
    xcopy /E /I /Y "%VST3_SOURCE%" "%VST3_LOCAL%\AI Equalizer Pro.vst3"
    echo Copied to: %VST3_LOCAL%
    
    if exist "%VST3_COMMON%" (
        xcopy /E /I /Y "%VST3_SOURCE%" "%VST3_COMMON%\AI Equalizer Pro.vst3"
        echo Copied to: %VST3_COMMON%
    )
) else (
    echo VST3 not found at expected location
    dir /s /b C:\AIEQ\build\*.vst3 2>nul
)

echo.
echo === DONE ===
pause
