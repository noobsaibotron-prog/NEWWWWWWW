@echo off
cd /d C:\AIEQ
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul 2>&1
cd build
if not exist CMakeCache.txt cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="C:\AIEQ\JUCE" ..
nmake
cd ..
if exist "build\Release\lib\AI Equalizer Pro.vst3" (
    if exist "%LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3" rmdir /s /q "%LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3" 2>nul
    xcopy /E /I /Y "build\Release\lib\AI Equalizer Pro.vst3" "%LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3\" >nul
    echo OK: Installato in %LOCALAPPDATA%\VST3
) else (
    echo ERRORE: VST3 non trovato
)
pause

