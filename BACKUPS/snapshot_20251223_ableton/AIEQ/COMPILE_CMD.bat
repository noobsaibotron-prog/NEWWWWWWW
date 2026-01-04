@echo off
REM Comando per compilare AI Equalizer Pro (VST3 + Standalone)
REM Usa questo file o esegui direttamente BUILD_NOW.bat

echo ========================================
echo AI EQUALIZER PRO - BUILD RELEASE
echo ========================================
echo.

REM Opzione 1: Usa BUILD_NOW.bat (piu semplice)
echo [OPZIONE 1] Usa BUILD_NOW.bat:
echo cd C:\AIEQ
echo BUILD_NOW.bat
echo.

REM Opzione 2: Comando manuale completo
echo [OPZIONE 2] Comando manuale completo:
echo "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
echo cd /d C:\AIEQ\build
echo cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="C:\AIEQ\JUCE" ..
echo nmake
echo.

echo ========================================
echo MODIFICHE APPLICATE:
echo - Standalone aggiunto ai FORMATS
echo - VST3 + Standalone verranno compilati
echo ========================================
pause

