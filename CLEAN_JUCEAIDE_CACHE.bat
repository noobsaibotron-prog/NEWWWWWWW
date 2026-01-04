@echo off
REM ============================================
REM Script per pulire la cache di juceaide
REM Risolve errori di generatore CMake mismatch
REM ============================================

echo.
echo ========================================
echo Pulizia cache juceaide...
echo ========================================
echo.

REM Rimuovi la directory tools dalla build
if exist "build\tools" (
    echo Rimozione di build\tools...
    rmdir /S /Q "build\tools"
    echo [OK] build\tools rimossa
) else (
    echo [INFO] build\tools non trovata, niente da pulire
)

REM Rimuovi anche CMakeCache.txt dalla directory principale se esiste
if exist "build\CMakeCache.txt" (
    echo.
    echo [INFO] Trovato build\CMakeCache.txt principale
    echo Vuoi rimuovere anche questo? (S/N)
    choice /C SN /N /M "Premi S per Si, N per No: "
    if errorlevel 2 (
        echo Cache principale mantenuta
    ) else (
        del /F /Q "build\CMakeCache.txt"
        echo [OK] Cache principale rimossa
    )
)

echo.
echo ========================================
echo Pulizia completata!
echo Ora puoi eseguire BUILD_NOW.bat
echo ========================================
echo.
pause

