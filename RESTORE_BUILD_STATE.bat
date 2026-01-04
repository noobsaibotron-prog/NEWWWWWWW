@echo off
REM ============================================
REM Script per ripristinare backup dello stato build
REM ============================================

setlocal enabledelayedexpansion

echo.
echo ========================================
echo RESTORE dello stato di build da backup
echo ========================================
echo.

REM Determina quale backup ripristinare
if "%1"=="" (
    REM Nessun parametro - usa l'ultimo backup
    if exist "LAST_BACKUP.txt" (
        set /p BACKUP_DIR=<LAST_BACKUP.txt
        echo [INFO] Usando ultimo backup: !BACKUP_DIR!
    ) else (
        echo [ERRORE] Nessun backup specificato e LAST_BACKUP.txt non trovato
        echo.
        echo Uso: RESTORE_BUILD_STATE.bat [cartella_backup]
        echo.
        echo Backup disponibili:
        dir /B /AD build_backup_* 2>nul
        echo.
        pause
        exit /b 1
    )
) else (
    set BACKUP_DIR=%1
)

REM Verifica che il backup esista
if not exist "!BACKUP_DIR!" (
    echo [ERRORE] Backup non trovato: !BACKUP_DIR!
    echo.
    pause
    exit /b 1
)

echo.
echo ⚠️  ATTENZIONE ⚠️
echo.
echo Questo sovrascriverà lo stato di build corrente con:
echo   !BACKUP_DIR!
echo.
echo Vuoi continuare? (S/N)
choice /C SN /N /M "Premi S per Si, N per No: "
if errorlevel 2 (
    echo.
    echo Restore annullato dall'utente
    pause
    exit /b 0
)

echo.
echo Ripristino in corso...
echo.

REM Rimuovi le directory attuali
if exist "build\tools" (
    echo [1/3] Rimozione build\tools corrente...
    rmdir /S /Q "build\tools"
    echo       [OK] Rimossa
)

if exist "build\CMakeCache.txt" (
    echo [2/3] Rimozione build\CMakeCache.txt corrente...
    del /F /Q "build\CMakeCache.txt"
    echo       [OK] Rimossa
)

REM Ripristina dal backup
if exist "!BACKUP_DIR!\tools" (
    echo [3/3] Ripristino build\tools...
    xcopy "!BACKUP_DIR!\tools" "build\tools\" /E /I /Q /H /Y >nul
    if %errorlevel% equ 0 (
        echo       [OK] build\tools ripristinata
    ) else (
        echo       [ERRORE] Problema nel ripristino di build\tools
    )
)

if exist "!BACKUP_DIR!\CMakeCache.txt" (
    echo       Ripristino build\CMakeCache.txt...
    copy "!BACKUP_DIR!\CMakeCache.txt" "build\" >nul
    if %errorlevel% equ 0 (
        echo       [OK] CMakeCache.txt ripristinata
    ) else (
        echo       [ERRORE] Problema nel ripristino di CMakeCache.txt
    )
)

if exist "!BACKUP_DIR!\CMakeFiles" (
    echo       Ripristino build\CMakeFiles...
    xcopy "!BACKUP_DIR!\CMakeFiles" "build\CMakeFiles\" /E /I /Q /H /Y >nul
    if %errorlevel% equ 0 (
        echo       [OK] CMakeFiles ripristinata
    ) else (
        echo       [ERRORE] Problema nel ripristino di CMakeFiles
    )
)

echo.
echo ========================================
echo RESTORE COMPLETATO!
echo ========================================
echo.
echo Stato build ripristinato da: !BACKUP_DIR!
echo.
pause

