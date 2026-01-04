@echo off
setlocal enabledelayedexpansion

echo === AI EQUALIZER RELEASE BUILD AND INSTALL ===
echo.

cd /d "%~dp0"

REM Setup Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul 2>&1

REM Clean JUCE build directory to avoid permission issues
if exist "build\JUCE" (
    echo [INFO] Cleaning JUCE build directory...
    rmdir /s /q "build\JUCE" 2>nul
)

cd build

REM Configure CMake
echo [INFO] Configuring CMake...
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="C:\AIEQ\JUCE" .. >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake configuration failed!
    echo [INFO] Retrying with verbose output...
    cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="C:\AIEQ\JUCE" ..
    pause
    exit /b 1
)

REM Build Release
echo [INFO] Building Release (this may take several minutes)...
nmake
if errorlevel 1 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo [INFO] Build completed successfully!
echo.

REM Find VST3 file
set VST3_FOUND=0
set VST3_PATH=

REM Check multiple possible locations
if exist "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win\AI Equalizer Pro.vst3" (
    set VST3_PATH=AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3
    set VST3_FOUND=1
) else if exist "AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3" (
    set VST3_PATH=AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3
    set VST3_FOUND=1
)

if %VST3_FOUND%==0 (
    echo [ERROR] VST3 file not found!
    echo [INFO] Searching in build directory...
    dir /s /b "*.vst3" 2>nul
    pause
    exit /b 1
)

echo [INFO] Found VST3: %VST3_PATH%
echo.

REM Find moduleinfo.json
set JSON_FOUND=0
set JSON_PATH=

for /r "AIEqualizerPro_artefacts\Release\VST3" %%f in (moduleinfo.json) do (
    set JSON_PATH=%%f
    set JSON_FOUND=1
    goto :found_json
)
:found_json

if %JSON_FOUND%==1 (
    echo [INFO] Found moduleinfo.json: %JSON_PATH%
) else (
    echo [WARNING] moduleinfo.json not found - will create placeholder
)
echo.

REM Copy to LocalAppData
set LOCAL_APP_DATA=%LOCALAPPDATA%\VST3
if not exist "%LOCAL_APP_DATA%" mkdir "%LOCAL_APP_DATA%"

set TARGET_VST3=%LOCAL_APP_DATA%\AI Equalizer Pro.vst3

echo [INFO] Copying to LocalAppData: %TARGET_VST3%
if exist "%TARGET_VST3%" rmdir /s /q "%TARGET_VST3%"
xcopy /E /I /Y "%VST3_PATH%" "%TARGET_VST3%\" >nul
if errorlevel 1 (
    echo [ERROR] Failed to copy VST3 to LocalAppData!
    pause
    exit /b 1
)
echo [OK] VST3 copied to LocalAppData

if %JSON_FOUND%==1 (
    copy /Y "%JSON_PATH%" "%TARGET_VST3%\moduleinfo.json" >nul
    if errorlevel 1 (
        echo [WARNING] Failed to copy JSON to LocalAppData
    ) else (
        echo [OK] moduleinfo.json copied to LocalAppData
    )
) else (
    echo [INFO] Creating placeholder moduleinfo.json...
    (
        echo {^
        echo   "VST3": {^
        echo     "Vendor": "AI Equalizer Pro",^
        echo     "Name": "AI Equalizer Pro",^
        echo     "Version": "2.1.0"^
        echo   }^
        echo }
    ) > "%TARGET_VST3%\moduleinfo.json"
    echo [OK] Created placeholder moduleinfo.json
)

REM Copy to Common Files (requires admin)
set COMMON_FILES=C:\Program Files\Common Files\VST3
set TARGET_COMMON=%COMMON_FILES%\AI Equalizer Pro.vst3

echo.
echo [INFO] Copying to Common Files: %TARGET_COMMON%
echo [INFO] This may require administrator privileges...

if exist "%COMMON_FILES%" (
    if exist "%TARGET_COMMON%" rmdir /s /q "%TARGET_COMMON%" 2>nul
    xcopy /E /I /Y "%VST3_PATH%" "%TARGET_COMMON%\" >nul
    if errorlevel 1 (
        echo [WARNING] Failed to copy to Common Files (may need admin rights)
        echo [INFO] Try running this script as Administrator
    ) else (
        echo [OK] VST3 copied to Common Files
        
        if %JSON_FOUND%==1 (
            copy /Y "%JSON_PATH%" "%TARGET_COMMON%\moduleinfo.json" >nul
        ) else (
            copy /Y "%TARGET_VST3%\moduleinfo.json" "%TARGET_COMMON%\moduleinfo.json" >nul
        )
        echo [OK] moduleinfo.json copied to Common Files
    )
) else (
    echo [WARNING] Common Files directory not found or not accessible
)

echo.
echo === INSTALLATION COMPLETED ===
echo.
echo Plugin installed in:
echo   LocalAppData: %TARGET_VST3%
echo   Common Files: %TARGET_COMMON%
echo.
echo File sizes:
for %%f in ("%TARGET_VST3%") do echo   LocalAppData: %%~zf bytes
if exist "%TARGET_COMMON%" (
    for %%f in ("%TARGET_COMMON%") do echo   Common Files: %%~zf bytes
)
echo.
pause
