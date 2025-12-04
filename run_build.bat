@echo off
echo === AI EQUALIZER PRO BUILD ===
echo.

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"

echo Initializing VS2022 environment...
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if %ERRORLEVEL% neq 0 (
    echo Failed to initialize VS environment
    exit /b 1
)

set "CMAKE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

cd /d C:\AIEQ

echo Cleaning build...
if exist build rmdir /s /q build
mkdir build
cd build

echo.
echo Configuring CMake...
"%CMAKE%" .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo CMake configuration FAILED
    exit /b 1
)

echo.
echo Building project...
nmake
if %ERRORLEVEL% neq 0 (
    echo Build FAILED
    exit /b 1
)

echo.
echo === BUILD COMPLETED ===
echo.

echo Looking for VST3...
dir /s /b *.vst3

echo.
echo Copying to VST3 folders...

set "VST3_SRC=C:\AIEQ\build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
set "VST3_LOCAL=%LOCALAPPDATA%\VST3"
set "VST3_COMMON=C:\Program Files\Common Files\VST3"

if exist "%VST3_SRC%" (
    echo Source: %VST3_SRC%
    
    if not exist "%VST3_LOCAL%" mkdir "%VST3_LOCAL%"
    xcopy /E /I /Y "%VST3_SRC%" "%VST3_LOCAL%\AI Equalizer Pro.vst3"
    echo Copied to %VST3_LOCAL%
    
    xcopy /E /I /Y "%VST3_SRC%" "%VST3_COMMON%\AI Equalizer Pro.vst3"
    echo Copied to %VST3_COMMON%
) else (
    echo VST3 not found at expected path
    echo Searching...
    for /r C:\AIEQ\build %%F in (*.vst3) do (
        echo Found: %%F
    )
)

echo.
echo === INSTALLATION COMPLETE ===
echo Rescan plugins in Ableton to find AI Equalizer Pro
