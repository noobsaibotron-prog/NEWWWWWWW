@echo off
setlocal enabledelayedexpansion
echo === AI EQUALIZER BUILD ===

:: IMPORTANTE: Inizializza ambiente VS PRIMA di tutto
:: Prova diverse versioni di Visual Studio (2026 = versione 19)
set "VS_GENERATOR="
set "VS_PATH="
set "MSVC_BIN="

:: Trova Visual Studio e il compilatore
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"
    set "VS_GENERATOR=Visual Studio 18 2026"
) else if exist "C:\Program Files\Microsoft Visual Studio\2024\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2024\Community"
    set "VS_GENERATOR=Visual Studio 17 2022"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
    set "VS_GENERATOR=Visual Studio 17 2022"
) else if exist "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2019\Community"
    set "VS_GENERATOR=Visual Studio 16 2019"
)

if "!VS_PATH!"=="" (
    echo [ERRORE] Visual Studio non trovato!
    pause
    exit /b 1
)

:: Trova MSVC e cl.exe direttamente
if exist "!VS_PATH!\VC\Tools\MSVC" (
    for /f "delims=" %%M in ('dir /b /ad /o-n "!VS_PATH!\VC\Tools\MSVC" 2^>nul') do (
        set "MSVC_TEST_BIN=!VS_PATH!\VC\Tools\MSVC\%%M\bin\Hostx64\x64"
        if exist "!MSVC_TEST_BIN!\cl.exe" (
            set "MSVC_BIN=!MSVC_TEST_BIN!"
            goto :msvc_found
        )
    )
)

:msvc_found
if "!MSVC_BIN!"=="" (
    echo [ERRORE] Compilatore MSVC non trovato in !VS_PATH!
    pause
    exit /b 1
)

echo [INFO] Trovato MSVC in: !MSVC_BIN!

:: Chiama vcvarsall.bat per inizializzare l'ambiente
echo [INFO] Inizializzazione ambiente Visual Studio...
call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64
set "VCVARS_ERROR=!ERRORLEVEL!"
if !VCVARS_ERROR! neq 0 (
    echo [AVVISO] vcvarsall.bat ha restituito errore !VCVARS_ERROR!, ma continuo...
    echo [INFO] I percorsi verranno configurati manualmente
) else (
    echo [OK] vcvarsall.bat eseguito con successo
)

:: Aggiungi MSVC al PATH se non già presente (fallback)
echo !PATH! | findstr /C:"!MSVC_BIN!" >nul 2>&1
if errorlevel 1 (
    set "PATH=!MSVC_BIN!;!PATH!"
    echo [INFO] Aggiunto MSVC al PATH: !MSVC_BIN!
)

:: Aggiungi MSBuild al PATH (necessario per Visual Studio generator)
set "MSBUILD_PATH=!VS_PATH!\MSBuild\Current\Bin"
if exist "!MSBUILD_PATH!\MSBuild.exe" (
    echo !PATH! | findstr /C:"!MSBUILD_PATH!" >nul 2>&1
    if errorlevel 1 (
        set "PATH=!MSBUILD_PATH!;!PATH!"
        echo [INFO] Aggiunto MSBuild al PATH: !MSBUILD_PATH!
    )
) else (
    :: Fallback: cerca MSBuild in altri percorsi comuni
    set "MSBUILD_PATH=!VS_PATH!\MSBuild\17.0\Bin"
    if exist "!MSBUILD_PATH!\MSBuild.exe" (
        echo !PATH! | findstr /C:"!MSBUILD_PATH!" >nul 2>&1
        if errorlevel 1 (
            set "PATH=!MSBUILD_PATH!;!PATH!"
            echo [INFO] Aggiunto MSBuild al PATH: !MSBUILD_PATH!
        )
    )
)

:: vcvarsall.bat dovrebbe già aver impostato PATH, LIB, INCLUDE
:: Ma aggiungiamo Windows SDK al PATH per rc.exe se necessario
set "WINSDK_VER="
if exist "C:\Program Files (x86)\Windows Kits\10\bin" (
    for /f "delims=" %%W in ('dir /b /ad /o-n "C:\Program Files (x86)\Windows Kits\10\bin" 2^>nul') do (
        if exist "C:\Program Files (x86)\Windows Kits\10\bin\%%W\x64\rc.exe" (
            set "WINSDK_VER=%%W"
            set "PATH=C:\Program Files (x86)\Windows Kits\10\bin\%%W\x64;!PATH!"
            goto :sdk_found
        )
    )
)
:sdk_found

:: Aggiungi Windows SDK libs a LIB se non già presente
:: Cerca Windows SDK anche se WINSDK_VER non è stato trovato prima
if not defined WINSDK_VER (
    if exist "C:\Program Files (x86)\Windows Kits\10\Lib" (
        for /f "delims=" %%W in ('dir /b /ad /o-n "C:\Program Files (x86)\Windows Kits\10\Lib" 2^>nul') do (
            if exist "C:\Program Files (x86)\Windows Kits\10\Lib\%%W\ucrt\x64\libucrt.lib" (
                set "WINSDK_VER=%%W"
                goto :sdk_version_found
            )
        )
    )
)
:sdk_version_found

if defined WINSDK_VER (
    set "WINSDK_UM_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\!WINSDK_VER!\um\x64"
    set "WINSDK_UCRT_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\!WINSDK_VER!\ucrt\x64"
    
    if exist "!WINSDK_UM_LIB!" (
        echo !LIB! | findstr /C:"!WINSDK_UM_LIB!" >nul 2>&1
        if errorlevel 1 (
            set "LIB=!WINSDK_UM_LIB!;!LIB!"
            echo [INFO] Added Windows SDK libs to LIB: !WINSDK_UM_LIB!
        )
    )
    
    if exist "!WINSDK_UCRT_LIB!" (
        echo !LIB! | findstr /C:"!WINSDK_UCRT_LIB!" >nul 2>&1
        if errorlevel 1 (
            set "LIB=!WINSDK_UCRT_LIB!;!LIB!"
            echo [INFO] Added UCRT libs to LIB: !WINSDK_UCRT_LIB!
        )
    )
) else (
    echo [AVVISO] Windows SDK version non trovata, provo a cercare manualmente...
    if exist "C:\Program Files (x86)\Windows Kits\10\Lib" (
        for /f "delims=" %%W in ('dir /b /ad /o-n "C:\Program Files (x86)\Windows Kits\10\Lib" 2^>nul') do (
            set "WINSDK_UCRT_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\%%W\ucrt\x64"
            if exist "!WINSDK_UCRT_LIB!\libucrt.lib" (
                echo !LIB! | findstr /C:"!WINSDK_UCRT_LIB!" >nul 2>&1
                if errorlevel 1 (
                    set "LIB=!WINSDK_UCRT_LIB!;!LIB!"
                    echo [INFO] Added UCRT libs to LIB: !WINSDK_UCRT_LIB!
                )
            )
            set "WINSDK_UM_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\%%W\um\x64"
            if exist "!WINSDK_UM_LIB!" (
                echo !LIB! | findstr /C:"!WINSDK_UM_LIB!" >nul 2>&1
                if errorlevel 1 (
                    set "LIB=!WINSDK_UM_LIB!;!LIB!"
                    echo [INFO] Added Windows SDK libs to LIB: !WINSDK_UM_LIB!
                )
            )
        )
    )
)

:: Aggiungi anche le librerie MSVC a LIB (sempre, anche se vcvarsall.bat potrebbe averle già aggiunte)
if defined MSVC_BIN (
    :: Costruisci il percorso delle librerie MSVC
    :: MSVC_BIN è: ...\MSVC\version\bin\Hostx64\x64
    :: MSVC_LIB_DIR dovrebbe essere: ...\MSVC\version\lib\x64
    :: Usa for per estrarre il percorso base senza sostituzione di stringa
    for %%P in ("!MSVC_BIN!") do (
        set "MSVC_BASE=%%~dpP"
        set "MSVC_BASE=!MSVC_BASE:~0,-1!"
    )
    for %%P in ("!MSVC_BASE!") do (
        set "MSVC_BASE=%%~dpP"
        set "MSVC_BASE=!MSVC_BASE:~0,-1!"
    )
    for %%P in ("!MSVC_BASE!") do (
        set "MSVC_BASE=%%~dpP"
        set "MSVC_BASE=!MSVC_BASE:~0,-1!"
    )
    set "MSVC_LIB_DIR=!MSVC_BASE!\lib\x64"
    
    :: Verifica se il percorso esiste
    if exist "!MSVC_LIB_DIR!" (
        :: Controlla se è già in LIB (confronto case-insensitive)
        echo !LIB! | findstr /I /C:"!MSVC_LIB_DIR!" >nul 2>&1
        if errorlevel 1 (
            :: Aggiungi all'inizio di LIB (priorità massima)
            set "LIB=!MSVC_LIB_DIR!;!LIB!"
            echo [INFO] Added MSVC libs to LIB: !MSVC_LIB_DIR!
        ) else (
            echo [INFO] MSVC libs già presenti in LIB: !MSVC_LIB_DIR!
        )
    ) else (
        echo [AVVISO] Percorso librerie MSVC non trovato: !MSVC_LIB_DIR!
        echo [INFO] MSVC_BIN era: !MSVC_BIN!
        echo [INFO] MSVC_BASE era: !MSVC_BASE!
        :: Prova metodo alternativo: vcvarsall dovrebbe averlo già aggiunto
        echo [INFO] vcvarsall.bat potrebbe aver già aggiunto le librerie MSVC a LIB
    )
) else (
    echo [AVVISO] MSVC_BIN non definito, non posso aggiungere librerie MSVC a LIB
)

echo.

:: Aggiungi anche i percorsi INCLUDE per gli header files (se non già presenti)
if defined MSVC_BIN (
    :: Costruisci il percorso degli include MSVC
    for %%P in ("!MSVC_BIN!") do (
        set "MSVC_BASE=%%~dpP"
        set "MSVC_BASE=!MSVC_BASE:~0,-1!"
    )
    for %%P in ("!MSVC_BASE!") do (
        set "MSVC_BASE=%%~dpP"
        set "MSVC_BASE=!MSVC_BASE:~0,-1!"
    )
    for %%P in ("!MSVC_BASE!") do (
        set "MSVC_BASE=%%~dpP"
        set "MSVC_BASE=!MSVC_BASE:~0,-1!"
    )
    set "MSVC_INCLUDE_DIR=!MSVC_BASE!\include"
    
    if exist "!MSVC_INCLUDE_DIR!" (
        echo !INCLUDE! | findstr /I /C:"!MSVC_INCLUDE_DIR!" >nul 2>&1
        if errorlevel 1 (
            set "INCLUDE=!MSVC_INCLUDE_DIR!;!INCLUDE!"
            echo [INFO] Added MSVC include to INCLUDE: !MSVC_INCLUDE_DIR!
        )
    )
)

:: Aggiungi Windows SDK include paths
if defined WINSDK_VER (
    set "WINSDK_UCRT_INCLUDE=C:\Program Files (x86)\Windows Kits\10\Include\!WINSDK_VER!\ucrt"
    set "WINSDK_UM_INCLUDE=C:\Program Files (x86)\Windows Kits\10\Include\!WINSDK_VER!\um"
    set "WINSDK_SHARED_INCLUDE=C:\Program Files (x86)\Windows Kits\10\Include\!WINSDK_VER!\shared"
    
    if exist "!WINSDK_UCRT_INCLUDE!" (
        echo !INCLUDE! | findstr /I /C:"!WINSDK_UCRT_INCLUDE!" >nul 2>&1
        if errorlevel 1 (
            set "INCLUDE=!WINSDK_UCRT_INCLUDE!;!INCLUDE!"
        )
    )
    if exist "!WINSDK_UM_INCLUDE!" (
        echo !INCLUDE! | findstr /I /C:"!WINSDK_UM_INCLUDE!" >nul 2>&1
        if errorlevel 1 (
            set "INCLUDE=!WINSDK_UM_INCLUDE!;!INCLUDE!"
        )
    )
    if exist "!WINSDK_SHARED_INCLUDE!" (
        echo !INCLUDE! | findstr /I /C:"!WINSDK_SHARED_INCLUDE!" >nul 2>&1
        if errorlevel 1 (
            set "INCLUDE=!WINSDK_SHARED_INCLUDE!;!INCLUDE!"
        )
    )
)

:: Mostra la configurazione finale di LIB e INCLUDE per debug
echo.
echo [INFO] Configurazione ambiente LIB per il linker:
echo !LIB!
echo.
echo [INFO] Configurazione ambiente INCLUDE per il compilatore:
echo !INCLUDE!
echo.

echo [INFO] Verifica ambiente compilatore...
where cl.exe >nul 2>&1
if !ERRORLEVEL! neq 0 (
    :: Fallback: verifica se cl.exe esiste nel percorso MSVC trovato
    if exist "!MSVC_BIN!\cl.exe" (
        echo [INFO] cl.exe trovato direttamente, aggiungo al PATH...
        set "PATH=!MSVC_BIN!;!PATH!"
        where cl.exe >nul 2>&1
        if !ERRORLEVEL! neq 0 (
            echo [ERRORE] cl.exe ancora non trovato dopo aggiunta al PATH!
            echo [INFO] Percorso MSVC: !MSVC_BIN!
            pause
            exit /b 1
        )
    ) else (
        echo [ERRORE] cl.exe non trovato nel PATH e non esiste in !MSVC_BIN!
        echo [INFO] Verifica che Visual Studio sia installato correttamente
        echo [INFO] Percorso Visual Studio: !VS_PATH!
        pause
        exit /b 1
    )
)
where cl.exe
echo [OK] Compilatore trovato: 
for /f "delims=" %%C in ('where cl.exe') do echo   %%C
echo.

:: Vai nella cartella del progetto
cd /d "%~dp0"

:: Pulisci build (con gestione errori se bloccata)
echo [INFO] Pulizia directory build...
if exist build (
    :: Pulisci sempre CMakeCache.txt e CMakeFiles per evitare conflitti di generatori
    if exist build\CMakeCache.txt del /Q /F build\CMakeCache.txt 2>nul
    if exist build\CMakeFiles rd /S /Q build\CMakeFiles 2>nul
    if exist build\CMakeScratch rd /S /Q build\CMakeScratch 2>nul
    
    :: Prova a rimuovere completamente, ma continua anche se fallisce
    rmdir /s /q build 2>nul
    if exist build (
        echo [AVVISO] Directory build potrebbe essere in uso, verrà riutilizzata
        echo [INFO] CMakeCache.txt e CMakeFiles sono stati rimossi
        timeout /t 2 /nobreak >nul 2>&1
        :: Riprova una volta
        rmdir /s /q build 2>nul
    )
)
if not exist build mkdir build
cd build

:: Configura CMake
:: Per Visual Studio generator, CMake trova automaticamente il compilatore
:: Non serve il toolchain file se vcvarsall.bat è stato chiamato correttamente
echo [INFO] Configuring CMake with generator: !VS_GENERATOR!
echo [INFO] VS Path: !VS_PATH!
echo [INFO] JUCE Path: %~dp0JUCE
echo [INFO] Current directory: %CD%
echo.

:: Verifica che CMake esista
if not exist "!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    echo [ERRORE] CMake non trovato in: !VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
    pause
    exit /b 1
)

:: Verifica che JUCE esista
if not exist "%~dp0JUCE\CMakeLists.txt" (
    echo [ERRORE] JUCE non trovato in: %~dp0JUCE
    echo [INFO] Assicurati che la cartella JUCE esista nella directory del progetto
    pause
    exit /b 1
)

:: Salva il percorso completo del compilatore per passarlo a CMake
for /f "delims=" %%C in ('where cl.exe') do set "CL_FULL_PATH=%%C"

:: Imposta variabili d'ambiente per CMake e processi figli (inclusi sottoprocessi JUCE)
set "CC=!CL_FULL_PATH!"
set "CXX=!CL_FULL_PATH!"
set "CMAKE_C_COMPILER=!CL_FULL_PATH!"
set "CMAKE_CXX_COMPILER=!CL_FULL_PATH!"

echo [INFO] Compilatore per CMake: !CL_FULL_PATH!
echo [INFO] Variabili d'ambiente impostate:
echo   CC=!CC!
echo   CXX=!CXX!
echo   CMAKE_C_COMPILER=!CMAKE_C_COMPILER!
echo   CMAKE_CXX_COMPILER=!CMAKE_CXX_COMPILER!
echo.

:: Imposta anche CMAKE_GENERATOR come variabile d'ambiente per i sottoprocessi
set "CMAKE_GENERATOR=!VS_GENERATOR!"

:: NOTA: Con Visual Studio generator, NON usiamo il toolchain file
:: CMake dovrebbe trovare automaticamente tutto se l'ambiente è configurato correttamente
:: Il toolchain file verrà usato solo con NMake generator se Visual Studio fallisce
:: Usa direttamente NMake Makefiles generator (più affidabile)
echo [INFO] Usando NMake Makefiles generator (più affidabile per questo ambiente)...
echo [INFO] Le variabili d'ambiente (INCLUDE, LIB, PATH) sono già impostate correttamente

"!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
    -G "NMake Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER="!CL_FULL_PATH!" ^
    -DCMAKE_CXX_COMPILER="!CL_FULL_PATH!" ^
    -DJUCE_PATH="%~dp0JUCE" ^
    ..

if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERRORE] Configurazione CMake fallita!
    echo [INFO] Verifica che:
    echo   - Visual Studio 2026 sia installato correttamente
    echo   - Il compilatore C++ sia installato (Desktop development with C++)
    echo   - vcvarsall.bat abbia configurato correttamente l'ambiente
    echo   - Le librerie Windows SDK siano installate
    echo.
    pause
    exit /b 1
)

echo [OK] Configurazione CMake completata con successo!
echo.

:: Compila
"!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . --config Release --parallel

if !ERRORLEVEL! neq 0 (
    echo ERRORE nella compilazione!
    pause
    exit /b 1
)

echo.
echo === BUILD COMPLETATA! ===
dir /s /b *.vst3 2>nul
dir /s /b *.exe 2>nul

echo.
echo [INFO] Copiando plugin VST3 nella directory standard per Ableton...
"!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . --config Release --target copy_vst3_to_common_files

if !ERRORLEVEL! equ 0 (
    echo [OK] Plugin VST3 copiato!
    echo [INFO] Percorsi:
    echo   - Directory utente: %LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3
    echo   - Directory sistema: %ProgramFiles%\Common Files\VST3\AI Equalizer Pro.vst3 (se disponibile)
    echo.
    echo [INFO] Ableton scansionerà automaticamente queste directory!
    echo [INFO] Apri Ableton e clicca Rescan per trovare il plugin aggiornato!
) else (
    echo [AVVISO] Impossibile copiare automaticamente il plugin.
    echo [INFO] Puoi copiare manualmente da:
    echo   build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3
    echo   a: %LOCALAPPDATA%\VST3\ oppure %ProgramFiles%\Common Files\VST3\
)
pause