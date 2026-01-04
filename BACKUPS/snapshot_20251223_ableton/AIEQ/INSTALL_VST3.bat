@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   INSTALLAZIONE AI EQUALIZER PRO VST3
echo ============================================
echo.

:: Verifica privilegi amministratore
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERRORE] Questo script richiede privilegi amministratore!
    echo.
    echo Per eseguirlo:
    echo 1. Tasto destro su questo file
    echo 2. Seleziona "Esegui come amministratore"
    echo.
    pause
    exit /b 1
)

echo [OK] Privilegi amministratore verificati
echo.

:: Percorsi
set "SOURCE=%~dp0build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3"
set "DEST=C:\Program Files\Common Files\VST3"

:: Verifica sorgente
if not exist "%SOURCE%" (
    echo [ERRORE] Plugin sorgente non trovato!
    echo Percorso atteso: %SOURCE%
    echo.
    echo Assicurati di aver compilato il progetto con BUILD_NOW.bat
    echo.
    pause
    exit /b 1
)

echo [INFO] Plugin sorgente trovato
echo.

:: Rimuovi versione precedente se esiste
if exist "%DEST%\AI Equalizer Pro.vst3" (
    echo [INFO] Rimozione versione precedente...
    rmdir /s /q "%DEST%\AI Equalizer Pro.vst3" 2>nul
)

if exist "%DEST%\AI Equalizer Pros.vst3" (
    echo [INFO] Rimozione versione incompleta...
    rmdir /s /q "%DEST%\AI Equalizer Pros.vst3" 2>nul
)

:: Copia plugin
echo [INFO] Copia plugin in directory VST3...
xcopy /E /I /Y "%SOURCE%" "%DEST%\AI Equalizer Pro.vst3\" >nul

if %errorlevel% neq 0 (
    echo [ERRORE] Copia fallita!
    pause
    exit /b 1
)

:: Verifica installazione
if exist "%DEST%\AI Equalizer Pro.vst3\Contents\x86_64-win\AI Equalizer Pro.vst3" (
    echo.
    echo ============================================
    echo   INSTALLAZIONE COMPLETATA CON SUCCESSO!
    echo ============================================
    echo.
    echo Plugin installato in:
    echo   %DEST%\AI Equalizer Pro.vst3
    echo.
    echo Prossimi passi:
    echo 1. Riavvia il tuo DAW
    echo 2. Il plugin dovrebbe apparire nella lista VST3
    echo 3. Cerca "AI Equalizer Pro" nel tuo DAW
    echo.
) else (
    echo [ERRORE] Installazione incompleta - file VST3 non trovato!
    echo.
    pause
    exit /b 1
)

pause

