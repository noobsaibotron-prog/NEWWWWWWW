@echo off
REM ============================================
REM Script per creare backup dello stato build
REM ============================================

echo.
echo ========================================
echo BACKUP dello stato di build corrente
echo ========================================
echo.

REM Crea una cartella backup con timestamp
for /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set mydate=%%c%%b%%a)
for /f "tokens=1-2 delims=/:" %%a in ('time /t') do (set mytime=%%a%%b)
set BACKUP_DIR=build_backup_%mydate%_%mytime%

echo Creazione backup in: %BACKUP_DIR%
echo.

REM Crea directory di backup
if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%"

REM Backup di build/tools (la parte critica)
if exist "build\tools" (
    echo [1/4] Backup di build\tools...
    xcopy "build\tools" "%BACKUP_DIR%\tools\" /E /I /Q /H /Y >nul
    if %errorlevel% equ 0 (
        echo       [OK] build\tools salvata
    ) else (
        echo       [WARN] Errore nel backup di build\tools
    )
) else (
    echo [1/4] build\tools non esiste, skip
)

REM Backup di CMakeCache.txt principale
if exist "build\CMakeCache.txt" (
    echo [2/4] Backup di build\CMakeCache.txt...
    copy "build\CMakeCache.txt" "%BACKUP_DIR%\" >nul
    if %errorlevel% equ 0 (
        echo       [OK] CMakeCache.txt salvata
    ) else (
        echo       [WARN] Errore nel backup di CMakeCache.txt
    )
) else (
    echo [2/4] build\CMakeCache.txt non esiste, skip
)

REM Backup di CMakeFiles
if exist "build\CMakeFiles" (
    echo [3/4] Backup di build\CMakeFiles...
    xcopy "build\CMakeFiles" "%BACKUP_DIR%\CMakeFiles\" /E /I /Q /H /Y >nul
    if %errorlevel% equ 0 (
        echo       [OK] CMakeFiles salvata
    ) else (
        echo       [WARN] Errore nel backup di CMakeFiles
    )
) else (
    echo [3/4] build\CMakeFiles non esiste, skip
)

REM Salva informazioni sul backup
echo [4/4] Creazione info backup...
echo BACKUP CREATO: %date% %time% > "%BACKUP_DIR%\BACKUP_INFO.txt"
echo ================================ >> "%BACKUP_DIR%\BACKUP_INFO.txt"
echo. >> "%BACKUP_DIR%\BACKUP_INFO.txt"
echo Contenuto backup: >> "%BACKUP_DIR%\BACKUP_INFO.txt"
echo - build\tools >> "%BACKUP_DIR%\BACKUP_INFO.txt"
echo - build\CMakeCache.txt >> "%BACKUP_DIR%\BACKUP_INFO.txt"
echo - build\CMakeFiles >> "%BACKUP_DIR%\BACKUP_INFO.txt"
echo. >> "%BACKUP_DIR%\BACKUP_INFO.txt"
echo Per ripristinare usa: RESTORE_BUILD_STATE.bat %BACKUP_DIR% >> "%BACKUP_DIR%\BACKUP_INFO.txt"
echo       [OK] Info salvate

REM Salva il nome del backup in un file per restore facile
echo %BACKUP_DIR% > LAST_BACKUP.txt

echo.
echo ========================================
echo BACKUP COMPLETATO!
echo ========================================
echo.
echo Posizione: %CD%\%BACKUP_DIR%
echo.
echo Per ripristinare questo backup usa:
echo   RESTORE_BUILD_STATE.bat %BACKUP_DIR%
echo.
echo Oppure semplicemente:
echo   RESTORE_BUILD_STATE.bat
echo   (ripristina l'ultimo backup automaticamente)
echo.
pause

