@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   CORREZIONE NOME FILE VST3
echo ============================================
echo.

:: Verifica privilegi amministratore
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERRORE] Questo script richiede privilegi amministratore!
    echo.
    echo Tasto destro su questo file -^> "Esegui come amministratore"
    echo.
    pause
    exit /b 1
)

set "VST3_DIR=C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win"
set "OLD_FILE=%VST3_DIR%\AI Equalizer Pro.dll"
set "NEW_FILE=%VST3_DIR%\AI Equalizer Pro.vst3"

if not exist "%OLD_FILE%" (
    echo [INFO] File gia' corretto o non trovato
    if exist "%NEW_FILE%" (
        echo [OK] Il file ha gia' il nome corretto: AI Equalizer Pro.vst3
    ) else (
        echo [ERRORE] Nessun file trovato in %VST3_DIR%
    )
    pause
    exit /b 1
)

echo [INFO] Rinomino il file...
ren "%OLD_FILE%" "AI Equalizer Pro.vst3"

if %errorlevel% equ 0 (
    echo.
    echo ============================================
    echo   CORREZIONE COMPLETATA!
    echo ============================================
    echo.
    echo Il file e' stato rinominato correttamente.
    echo.
    echo Ora fai un nuovo rescan dei plugin VST3 in Ableton.
    echo.
) else (
    echo [ERRORE] Rinomina fallita!
)

pause

