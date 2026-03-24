cmake_minimum_required(VERSION 3.22)

# CRITICAL FIX for CMake 4.1: Prevent using Visual Studio project files with nmake
# When using NMake Makefiles generator, CMake 4.1 may incorrectly try to use .vcxproj files
# Force CMake to use STATIC_LIBRARY for compiler tests, which uses simple Makefiles
# IMPORTANT: Only apply this fix for NMake Makefiles generator, NOT for Visual Studio generators
# This must be set BEFORE project() call
if(NOT DEFINED CMAKE_TRY_COMPILE_TARGET_TYPE)
    # Before project(), CMAKE_GENERATOR might not be available yet
    # Check multiple sources to detect if NMake generator is being used
    set(IS_NMAKE_GENERATOR FALSE)

    # Method 1: Check environment variable (set by scripts like BUILD_NOW.bat)
    if(DEFINED ENV{CMAKE_GENERATOR} AND "$ENV{CMAKE_GENERATOR}" MATCHES "NMake")
        set(IS_NMAKE_GENERATOR TRUE)
        message(STATUS "Detected NMake generator from environment variable: $ENV{CMAKE_GENERATOR}")
    endif()

    # Method 2: Check cache variable (may be set via -G flag on command line)
    get_property(GENERATOR_FROM_CACHE CACHE CMAKE_GENERATOR PROPERTY VALUE)
    if(NOT IS_NMAKE_GENERATOR AND GENERATOR_FROM_CACHE AND "${GENERATOR_FROM_CACHE}" MATCHES "NMake")
        set(IS_NMAKE_GENERATOR TRUE)
        message(STATUS "Detected NMake generator from cache: ${GENERATOR_FROM_CACHE}")
    endif()

    # Only set CMAKE_TRY_COMPILE_TARGET_TYPE for NMake Makefiles generator
    if(IS_NMAKE_GENERATOR)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE INTERNAL "")
        message(STATUS "Setting CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY for NMake generator")
    else()
        message(STATUS "Non-NMake generator detected - CMAKE_TRY_COMPILE_TARGET_TYPE not modified (CMake will use default)")
    endif()
endif()

# Try to detect and set C++ compiler before project() call
# This helps when CMake is run directly without BUILD_NOW.bat
if(NOT DEFINED CMAKE_CXX_COMPILER OR CMAKE_CXX_COMPILER STREQUAL "")
    # Try to find Visual Studio compiler first
    # Note: VS 18/2026 is included for compiler detection, but may have generator issues.
    # The generator fallback logic (lines 135-140) handles cases where VS 18 generator isn't registered.
    set(VS_PATHS
        "C:/Program Files/Microsoft Visual Studio/2022/Community"
        "C:/Program Files/Microsoft Visual Studio/2019/Community"
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"
        "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community"
        # VS 18/2026: Include for compiler detection, but generator may need fallback
        "C:/Program Files/Microsoft Visual Studio/18/Community"
        "C:/Program Files/Microsoft Visual Studio/2026/Community"
        "C:/Program Files/Microsoft Visual Studio/2024/Community"
    )

    foreach(VS_PATH ${VS_PATHS})
        if(EXISTS "${VS_PATH}/VC/Tools/MSVC")
            file(GLOB MSVC_PATHS "${VS_PATH}/VC/Tools/MSVC/*")
            if(MSVC_PATHS)
                list(SORT MSVC_PATHS)
                list(REVERSE MSVC_PATHS)
                list(GET MSVC_PATHS 0 MSVC_PATH)
                set(MSVC_BIN "${MSVC_PATH}/bin/Hostx64/x64")
                if(EXISTS "${MSVC_BIN}/cl.exe")
                    set(CMAKE_CXX_COMPILER "${MSVC_BIN}/cl.exe" CACHE FILEPATH "C++ compiler" FORCE)
                    set(CMAKE_C_COMPILER "${MSVC_BIN}/cl.exe" CACHE FILEPATH "C compiler" FORCE)
                    message(STATUS "Found MSVC compiler: ${CMAKE_CXX_COMPILER}")
                    # Warn if VS 18/2026 is detected - generator may need fallback
                    if("${VS_PATH}" MATCHES "(/18/|/2026/|\\\\18\\\\|\\\\2026\\\\)")
                        message(STATUS "Note: VS 18/2026 detected. If using Visual Studio generator, it may fall back to VS 2022 if not registered.")
                    endif()
                    break()
                endif()
            endif()
        endif()
    endforeach()

    # Fallback: Try MinGW if Visual Studio not found
    if(NOT CMAKE_CXX_COMPILER)
        find_program(MINGW_GXX
            NAMES g++ x86_64-w64-mingw32-g++ mingw32-g++
            PATHS
                "C:/mingw64/bin"
                "C:/msys64/mingw64/bin"
                "C:/msys64/ucrt64/bin"
                "C:/Program Files/mingw-w64/x86_64-8.1.0-posix-seh-rt_v6-rev0/mingw64/bin"
        )
        if(MINGW_GXX)
            set(CMAKE_CXX_COMPILER "${MINGW_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
            message(STATUS "Found MinGW compiler: ${CMAKE_CXX_COMPILER}")
        endif()
    endif()

    # Fallback: Try Clang if available
    if(NOT CMAKE_CXX_COMPILER)
        find_program(CLANG_XX
            NAMES clang++ clang-cl
            PATHS
                "C:/Program Files/LLVM/bin"
                "C:/Program Files (x86)/LLVM/bin"
        )
        if(CLANG_XX)
            set(CMAKE_CXX_COMPILER "${CLANG_XX}" CACHE FILEPATH "C++ compiler" FORCE)
            message(STATUS "Found Clang compiler: ${CMAKE_CXX_COMPILER}")
        endif()
    endif()
endif()

# Detect whether a Visual Studio installation is available; if not, fall back to a portable generator
set(VS_INSTALL_FOUND FALSE)
set(VS_INSTALL_DIR "")
set(_VS_HINT_DIRS
    "C:/Program Files/Microsoft Visual Studio/2022/Community"
    "C:/Program Files/Microsoft Visual Studio/2022/Professional"
    "C:/Program Files/Microsoft Visual Studio/2022/Enterprise"
    "C:/Program Files/Microsoft Visual Studio/2022/BuildTools"
    "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools"
    "C:/Program Files/Microsoft Visual Studio/2019/Community"
    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"
    # VS 18/2026: Include for detection, but generator fallback may be needed
    "C:/Program Files/Microsoft Visual Studio/18/Community"
    "C:/Program Files/Microsoft Visual Studio/2026/Community"
    "C:/Program Files/Microsoft Visual Studio/2024/Community"
)
foreach(_VS_DIR ${_VS_HINT_DIRS})
    if(EXISTS "${_VS_DIR}/VC/Tools/MSVC")
        set(VS_INSTALL_FOUND TRUE)
        set(VS_INSTALL_DIR "${_VS_DIR}")
        break()
    endif()
endforeach()

# IMPORTANT: Check for VS 18 generator BEFORE checking if VS installation is found.
# If user specifies "Visual Studio 18 2026" but VS 2022 is not installed, we need to
# convert to "Visual Studio 17 2022" first, then check if that installation exists.
# If we check for missing installation first, we'll try Ninja/MinGW fallback before
# the VS 18 -> VS 2022 conversion happens.
if(CMAKE_GENERATOR MATCHES "Visual Studio 18")
    message(WARNING "Generator ${CMAKE_GENERATOR} not registered. Falling back to Visual Studio 17 2022.")
    set(CMAKE_GENERATOR "Visual Studio 17 2022" CACHE INTERNAL "Forced generator fallback" FORCE)
    # After converting to VS 2022, re-check if VS installation exists (in case VS 18 was installed)
    # The _VS_HINT_DIRS already includes VS 2022 paths, so VS_INSTALL_FOUND should already be correct
    # But if VS 18 was the only installation, we need to check if it exists as fallback
    if(NOT VS_INSTALL_FOUND)
        # Check if VS 18 installation exists even though generator was converted
        if(EXISTS "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC")
            set(VS_INSTALL_FOUND TRUE)
            set(VS_INSTALL_DIR "C:/Program Files/Microsoft Visual Studio/18/Community")
            message(STATUS "Found VS 18 installation (generator converted to VS 2022 for compatibility)")
        elseif(EXISTS "C:/Program Files/Microsoft Visual Studio/2026/Community/VC/Tools/MSVC")
            set(VS_INSTALL_FOUND TRUE)
            set(VS_INSTALL_DIR "C:/Program Files/Microsoft Visual Studio/2026/Community")
            message(STATUS "Found VS 2026 installation (generator converted to VS 2022 for compatibility)")
        endif()
    endif()
endif()

# Now check if the (possibly adjusted) Visual Studio generator has an installation available
if(CMAKE_GENERATOR MATCHES "^Visual Studio" AND NOT VS_INSTALL_FOUND)
    message(WARNING "Generator '${CMAKE_GENERATOR}' requires Visual Studio, but no installation was detected. Trying a portable generator fallback.")
    find_program(NINJA_EXE ninja)
    if(NINJA_EXE)
        set(CMAKE_MAKE_PROGRAM "${NINJA_EXE}" CACHE FILEPATH "Build tool" FORCE)
        set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "Fallback generator" FORCE)
        message(STATUS "Using Ninja generator via ${NINJA_EXE}")
    else()
        find_program(MINGW_MAKE_EXE NAMES mingw32-make make)
        if(MINGW_MAKE_EXE)
            set(CMAKE_MAKE_PROGRAM "${MINGW_MAKE_EXE}" CACHE FILEPATH "Build tool" FORCE)
            set(CMAKE_GENERATOR "MinGW Makefiles" CACHE INTERNAL "Fallback generator" FORCE)
            message(STATUS "Using MinGW Makefiles generator via ${MINGW_MAKE_EXE}")
        else()
            message(FATAL_ERROR "Visual Studio generator requested but no Visual Studio installation found. Install VS 2022 Build Tools (Desktop C++) or install Ninja/MinGW and re-run with -G Ninja/MinGW Makefiles.")
        endif()
    endif()
endif()
# Ensure a sane default Windows SDK if none provided
if(NOT DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.22621.0" CACHE STRING "WinSDK fallback" FORCE)
endif()

# Configura i percorsi delle librerie Windows SDK PRIMA della chiamata project()
# Questo assicura che il test del compilatore di CMake possa trovare libucrt.lib e altre librerie Windows SDK
if(CMAKE_CXX_COMPILER MATCHES "cl\\.exe$" OR CMAKE_C_COMPILER MATCHES "cl\\.exe$")
    # Trova Windows SDK
    set(WINSDK_LIB_PATHS "")
    if(EXISTS "C:/Program Files (x86)/Windows Kits/10/Lib")
        file(GLOB WINSDK_LIB_VERSIONS "C:/Program Files (x86)/Windows Kits/10/Lib/*")
        if(WINSDK_LIB_VERSIONS)
            list(SORT WINSDK_LIB_VERSIONS)
            list(REVERSE WINSDK_LIB_VERSIONS)
            # Get the highest version (first after reverse sort) for SDK version detection
            list(GET WINSDK_LIB_VERSIONS 0 WINSDK_LIB_VER_HIGHEST)
            # Extract Windows SDK version name from the path (e.g., "10.0.22621.0")
            get_filename_component(WINSDK_VER_NAME_DETECTED "${WINSDK_LIB_VER_HIGHEST}" NAME)

            # Build library paths list (use all versions found)
            foreach(WINSDK_LIB_VER ${WINSDK_LIB_VERSIONS})
                if(EXISTS "${WINSDK_LIB_VER}/ucrt/x64")
                    list(APPEND WINSDK_LIB_PATHS "${WINSDK_LIB_VER}/ucrt/x64")
                endif()
                if(EXISTS "${WINSDK_LIB_VER}/um/x64")
                    list(APPEND WINSDK_LIB_PATHS "${WINSDK_LIB_VER}/um/x64")
                endif()
            endforeach()

            # CRITICAL: Set CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION immediately after detection,
            # BEFORE project() call (which is at line 424), so it's available during compiler tests
            if(WINSDK_VER_NAME_DETECTED)
                set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "${WINSDK_VER_NAME_DETECTED}" CACHE STRING "Windows SDK version detected from Lib directory" FORCE)
                set(CMAKE_SYSTEM_VERSION "${WINSDK_VER_NAME_DETECTED}" CACHE STRING "System version" FORCE)
                message(STATUS "Detected Windows SDK version: ${WINSDK_VER_NAME_DETECTED} (configured before project() call)")
            endif()
        endif()
    endif()

    # Trova il percorso delle librerie MSVC
    # Il compilatore è in: .../MSVC/version/bin/Hostx64/x64/cl.exe
    # Le librerie sono in: .../MSVC/version/lib/x64
    if(CMAKE_CXX_COMPILER)
        # Estrai la directory del compilatore
        get_filename_component(MSVC_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        # Risali di 3 livelli: x64 -> Hostx64 -> bin -> version
        get_filename_component(MSVC_HOST_DIR "${MSVC_COMPILER_DIR}" DIRECTORY)
        get_filename_component(MSVC_BIN_DIR "${MSVC_HOST_DIR}" DIRECTORY)
        get_filename_component(MSVC_VERSION_DIR "${MSVC_BIN_DIR}" DIRECTORY)

        # Costruisci il percorso delle librerie
        set(MSVC_LIB_DIR "${MSVC_VERSION_DIR}/lib/x64")

        if(EXISTS "${MSVC_LIB_DIR}")
            list(APPEND WINSDK_LIB_PATHS "${MSVC_LIB_DIR}")
            message(STATUS "Found MSVC libs at: ${MSVC_LIB_DIR}")
        else()
            message(WARNING "MSVC lib directory not found: ${MSVC_LIB_DIR}")
            message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER}")
            message(STATUS "  Version dir: ${MSVC_VERSION_DIR}")
        endif()
    endif()

    # Imposta CMAKE_SYSTEM_LIBRARY_PATH per aiutare CMake a trovare le librerie durante il test del compilatore
    if(WINSDK_LIB_PATHS)
        set(CMAKE_SYSTEM_LIBRARY_PATH ${WINSDK_LIB_PATHS} CACHE STRING "Percorsi delle librerie di sistema" FORCE)
        message(STATUS "Impostazione percorsi librerie Windows SDK per il test del compilatore: ${CMAKE_SYSTEM_LIBRARY_PATH}")

        # Imposta anche i flag iniziali del linker per includere i percorsi delle librerie
        # Questo aiuta durante la fase di test del compilatore
        # Preserva eventuali flag già impostati (ad esempio da un toolchain file)
        if(NOT DEFINED CMAKE_EXE_LINKER_FLAGS_INIT)
            set(CMAKE_EXE_LINKER_FLAGS_INIT "")
        endif()
        if(NOT DEFINED CMAKE_SHARED_LINKER_FLAGS_INIT)
            set(CMAKE_SHARED_LINKER_FLAGS_INIT "")
        endif()
        if(NOT DEFINED CMAKE_MODULE_LINKER_FLAGS_INIT)
            set(CMAKE_MODULE_LINKER_FLAGS_INIT "")
        endif()
        foreach(LIB_PATH ${WINSDK_LIB_PATHS})
            set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} /LIBPATH:\"${LIB_PATH}\"")
            set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} /LIBPATH:\"${LIB_PATH}\"")
            set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_MODULE_LINKER_FLAGS_INIT} /LIBPATH:\"${LIB_PATH}\"")
        endforeach()
    endif()
endif()

# Imposta variabili d'ambiente per i compilatori prima della chiamata project()
# Questo aiuta JUCE a trovare i compilatori quando configura juceaide
if(CMAKE_C_COMPILER)
    set(ENV{CC} "${CMAKE_C_COMPILER}")
    set(ENV{CMAKE_C_COMPILER} "${CMAKE_C_COMPILER}")

    # Workaround per CMake 4.1: imposta CMAKE_C_COMPILE_OBJECT nella cache
    # prima di project() per evitare errori durante la generazione
    # Usa il formato standard di CMake per MSVC che gestisce correttamente le virgolette
    if(CMAKE_C_COMPILER MATCHES "cl\\.exe$" AND NOT DEFINED CMAKE_C_COMPILE_OBJECT)
        # Formato standard per MSVC con NMake - usa @<< per gestire correttamente le virgolette
        # Questo formato è quello che CMake usa normalmente per MSVC
        set(CMAKE_C_COMPILE_OBJECT "<CMAKE_C_COMPILER> <DEFINES> <INCLUDES> <FLAGS> /c <SOURCE> /Fo<OBJECT>" CACHE INTERNAL "C compile object command" FORCE)
        set(CMAKE_CXX_COMPILE_OBJECT "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> /c <SOURCE> /Fo<OBJECT>" CACHE INTERNAL "CXX compile object command" FORCE)
        message(STATUS "Workaround: Impostato CMAKE_C_COMPILE_OBJECT nella cache prima di project()")
    endif()
endif()
if(CMAKE_CXX_COMPILER)
    set(ENV{CXX} "${CMAKE_CXX_COMPILER}")
    set(ENV{CMAKE_CXX_COMPILER} "${CMAKE_CXX_COMPILER}")
endif()

# Assicurati che le variabili d'ambiente LIB e INCLUDE siano ereditate
# (vcvarsall.bat dovrebbe averle già impostate, ma le rivediamo per sicurezza)
if(CMAKE_CXX_COMPILER MATCHES "cl\\.exe$")
    # Estrai il percorso MSVC per costruire i percorsi include
    get_filename_component(MSVC_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(MSVC_HOST_DIR "${MSVC_COMPILER_DIR}" DIRECTORY)
    get_filename_component(MSVC_BIN_PARENT "${MSVC_HOST_DIR}" DIRECTORY)
    get_filename_component(MSVC_VERSION_DIR "${MSVC_BIN_PARENT}" DIRECTORY)

    set(MSVC_INCLUDE_DIR "${MSVC_VERSION_DIR}/include")
    set(INCLUDE_PATHS_TO_SET "")

    if(EXISTS "${MSVC_INCLUDE_DIR}")
        list(APPEND INCLUDE_PATHS_TO_SET "${MSVC_INCLUDE_DIR}")
    endif()

    # Trova Windows SDK include paths
    if(EXISTS "C:/Program Files (x86)/Windows Kits/10/Include")
        file(GLOB WINSDK_INCLUDE_VERSIONS "C:/Program Files (x86)/Windows Kits/10/Include/*")
        if(WINSDK_INCLUDE_VERSIONS)
            list(SORT WINSDK_INCLUDE_VERSIONS)
            list(REVERSE WINSDK_INCLUDE_VERSIONS)
            list(GET WINSDK_INCLUDE_VERSIONS 0 WINSDK_INCLUDE_VER)
            get_filename_component(WINSDK_VER_NAME "${WINSDK_INCLUDE_VER}" NAME)

            if(EXISTS "${WINSDK_INCLUDE_VER}/ucrt")
                list(APPEND INCLUDE_PATHS_TO_SET "${WINSDK_INCLUDE_VER}/ucrt")
            endif()
            if(EXISTS "${WINSDK_INCLUDE_VER}/um")
                list(APPEND INCLUDE_PATHS_TO_SET "${WINSDK_INCLUDE_VER}/um")
            endif()
            if(EXISTS "${WINSDK_INCLUDE_VER}/shared")
                list(APPEND INCLUDE_PATHS_TO_SET "${WINSDK_INCLUDE_VER}/shared")
            endif()

            # Note: CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION is already set in the LIB paths section
            # (lines 186-204, immediately after Windows SDK version detection from Lib directory).
            # Setting it here would be redundant and ineffective because it must be set BEFORE project() call.
            # The SDK version detection happens in the LIB paths section where it's most appropriate.
        endif()
    endif()

    # Imposta INCLUDE environment variable per i subprocessi CMake (incluso juceaide)
    if(INCLUDE_PATHS_TO_SET)
        string(JOIN ";" INCLUDE_STRING ${INCLUDE_PATHS_TO_SET})
        if(DEFINED ENV{INCLUDE})
            # Aggiungi solo se non già presente
            string(FIND "$ENV{INCLUDE}" "${MSVC_INCLUDE_DIR}" FOUND_MSVC)
            if(FOUND_MSVC EQUAL -1)
                set(ENV{INCLUDE} "${INCLUDE_STRING};$ENV{INCLUDE}")
                message(STATUS "Added MSVC and Windows SDK includes to INCLUDE environment variable")
            endif()
        else()
            set(ENV{INCLUDE} "${INCLUDE_STRING}")
            message(STATUS "Set INCLUDE environment variable: $ENV{INCLUDE}")
        endif()

        # Aggiungi anche i percorsi INCLUDE ai flag del compilatore
        # Questo assicura che siano inclusi nei Makefile generati
        foreach(INCLUDE_PATH ${INCLUDE_PATHS_TO_SET})
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /I\"${INCLUDE_PATH}\"")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /I\"${INCLUDE_PATH}\"")
        endforeach()
        message(STATUS "Added INCLUDE paths to compiler flags")
    endif()

    # ---------------------------------------------------------------------------------------------
    # LIB environment: aggiungi percorsi MSVC e Windows SDK così i test del compilatore trovano libucrt.lib
    # ---------------------------------------------------------------------------------------------
    set(LIB_PATHS_TO_SET "")
    if(EXISTS "${MSVC_LIB_DIR}")
        list(APPEND LIB_PATHS_TO_SET "${MSVC_LIB_DIR}")
    endif()
    if(EXISTS "C:/Program Files (x86)/Windows Kits/10/Lib")
        file(GLOB WINSDK_LIB_VERSIONS "C:/Program Files (x86)/Windows Kits/10/Lib/*")
        if(WINSDK_LIB_VERSIONS)
            list(SORT WINSDK_LIB_VERSIONS)
            list(REVERSE WINSDK_LIB_VERSIONS)
            list(GET WINSDK_LIB_VERSIONS 0 WINSDK_LIB_VER)
            if(EXISTS "${WINSDK_LIB_VER}/ucrt/x64")
                list(APPEND LIB_PATHS_TO_SET "${WINSDK_LIB_VER}/ucrt/x64")
            endif()
            if(EXISTS "${WINSDK_LIB_VER}/um/x64")
                list(APPEND LIB_PATHS_TO_SET "${WINSDK_LIB_VER}/um/x64")
            endif()
        endif()
    endif()

    if(LIB_PATHS_TO_SET)
        string(JOIN ";" LIB_STRING ${LIB_PATHS_TO_SET})
        if(DEFINED ENV{LIB})
            # Aggiungi solo se non già presente
            string(FIND "$ENV{LIB}" "${MSVC_LIB_DIR}" FOUND_MSVC_LIB)
            if(FOUND_MSVC_LIB EQUAL -1)
                set(ENV{LIB} "${LIB_STRING};$ENV{LIB}")
                message(STATUS "Added MSVC and Windows SDK libs to LIB environment variable")
            endif()
        else()
            set(ENV{LIB} "${LIB_STRING}")
            message(STATUS "Set LIB environment variable: $ENV{LIB}")
        endif()

        # Aggiungi percorsi lib anche ai flag del linker (utile per generatori che non ereditano l'env)
        foreach(LIB_PATH ${LIB_PATHS_TO_SET})
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LIBPATH:\"${LIB_PATH}\"")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LIBPATH:\"${LIB_PATH}\"")
            set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /LIBPATH:\"${LIB_PATH}\"")
        endforeach()
        message(STATUS "Added LIB paths to linker flags")
    endif()

    # Use nmake override ONLY when the chosen generator is NMake Makefiles.
    if(CMAKE_GENERATOR MATCHES "NMake")
        # Find nmake.exe and add it to PATH if not already there
        # Common locations: VC/Tools/MSVC/version/bin/Hostx64/x64/nmake.exe or VC/Auxiliary/Build/nmake.exe
        set(NMAKE_FOUND FALSE)
        set(NMAKE_PATHS
            "${MSVC_COMPILER_DIR}/nmake.exe"  # Same directory as cl.exe
            "${MSVC_HOST_DIR}/nmake.exe"      # Host directory
            "${MSVC_BIN_PARENT}/nmake.exe"    # bin directory
        )

        get_filename_component(MSVC_BIN_PARENT_PARENT "${MSVC_BIN_PARENT}" DIRECTORY)
        get_filename_component(VS_VC_DIR "${MSVC_BIN_PARENT_PARENT}" DIRECTORY)
        if(EXISTS "${VS_VC_DIR}/Auxiliary/Build/nmake.exe")
            list(APPEND NMAKE_PATHS "${VS_VC_DIR}/Auxiliary/Build/nmake.exe")
        endif()

        foreach(NMAKE_PATH ${NMAKE_PATHS})
            if(EXISTS "${NMAKE_PATH}")
                get_filename_component(NMAKE_DIR "${NMAKE_PATH}" DIRECTORY)
                if(NOT "$ENV{PATH}" MATCHES "${NMAKE_DIR}")
                    set(ENV{PATH} "${NMAKE_DIR};$ENV{PATH}")
                    message(STATUS "Added nmake.exe directory to PATH: ${NMAKE_DIR}")
                endif()
                set(CMAKE_MAKE_PROGRAM "${NMAKE_PATH}" CACHE FILEPATH "Build tool" FORCE)
                message(STATUS "Set CMAKE_MAKE_PROGRAM to nmake: ${NMAKE_PATH}")
                set(NMAKE_FOUND TRUE)
                break()
            endif()
        endforeach()

        if(NOT NMAKE_FOUND)
            message(WARNING "nmake.exe not found for NMake generator. CMake may fail during compiler test.")
            message(STATUS "  Searched in: ${NMAKE_PATHS}")
        endif()
    else()
        message(STATUS "Generator '${CMAKE_GENERATOR}' detected - skipping nmake override (will use generator default)")
    endif()
endif()

# Project name and version
project(AIEqualizerPro VERSION 2.1.0 LANGUAGES C CXX)

set(PROJECT_DESCRIPTION "AI-Powered Parametric Equalizer with Intelligent Frequency Analysis")

# Set default C++ standard properties per target for modern best practice.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS: provide sensible defaults for universal builds while allowing overrides
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target" FORCE)
    endif()
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build universal binary (Intel + Apple Silicon)" FORCE)
    endif()
endif()


# Ensure a C++ compiler is available
if(NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR
        "No C++ compiler found!\n"
        "Please install one of the following:\n"
        "  1. Visual Studio 2019/2022/2024/2026 with 'Desktop development with C++' workload\n"
        "     Download: https://visualstudio.microsoft.com/\n"
        "     Note: VS 2026 (VS 18) is supported but may require generator fallback.\n"
        "  2. MinGW-w64 (https://www.mingw-w64.org/)\n"
        "  3. LLVM/Clang (https://llvm.org/)\n"
        "\n"
        "Alternatively, use BUILD_NOW.bat which automatically sets up the Visual Studio environment.\n"
        "Or specify the compiler manually:\n"
        "  cmake -DCMAKE_CXX_COMPILER=<path-to-compiler> .."
    )
endif()

# (Global C++ standard is kept for compatibility; per-target standard is set later.)

# Configure Resource Compiler (rc.exe) to find Windows SDK headers
# rc.exe needs INCLUDE paths to find windows.h
if(CMAKE_CXX_COMPILER MATCHES "cl\\.exe$" AND WIN32)
    # Get MSVC include directory
    get_filename_component(MSVC_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(MSVC_HOST_DIR "${MSVC_COMPILER_DIR}" DIRECTORY)
    get_filename_component(MSVC_BIN_PARENT "${MSVC_HOST_DIR}" DIRECTORY)
    get_filename_component(MSVC_VERSION_DIR "${MSVC_BIN_PARENT}" DIRECTORY)
    set(MSVC_INCLUDE_DIR "${MSVC_VERSION_DIR}/include")

    # Find Windows SDK include paths
    set(RC_INCLUDE_FLAGS "")
    if(EXISTS "${MSVC_INCLUDE_DIR}")
        set(RC_INCLUDE_FLAGS "/I\"${MSVC_INCLUDE_DIR}\"")
    endif()

    if(EXISTS "C:/Program Files (x86)/Windows Kits/10/Include")
        file(GLOB WINSDK_INCLUDE_VERSIONS "C:/Program Files (x86)/Windows Kits/10/Include/*")
        if(WINSDK_INCLUDE_VERSIONS)
            list(SORT WINSDK_INCLUDE_VERSIONS)
            list(REVERSE WINSDK_INCLUDE_VERSIONS)
            list(GET WINSDK_INCLUDE_VERSIONS 0 WINSDK_INCLUDE_VER)

            if(EXISTS "${WINSDK_INCLUDE_VER}/um")
                set(RC_INCLUDE_FLAGS "${RC_INCLUDE_FLAGS} /I\"${WINSDK_INCLUDE_VER}/um\"")
            endif()
            if(EXISTS "${WINSDK_INCLUDE_VER}/ucrt")
                set(RC_INCLUDE_FLAGS "${RC_INCLUDE_FLAGS} /I\"${WINSDK_INCLUDE_VER}/ucrt\"")
            endif()
            if(EXISTS "${WINSDK_INCLUDE_VER}/shared")
                set(RC_INCLUDE_FLAGS "${RC_INCLUDE_FLAGS} /I\"${WINSDK_INCLUDE_VER}/shared\"")
            endif()
        endif()
    endif()

    # Set Resource Compiler flags
    if(RC_INCLUDE_FLAGS)
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} ${RC_INCLUDE_FLAGS}" CACHE STRING "Flags for Windows Resource Compiler" FORCE)
        message(STATUS "Configured Resource Compiler flags: ${CMAKE_RC_FLAGS}")
    endif()
endif()

# Build type defaults to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Output directories (per-config where appropriate)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Also set per-config directories for multi-config generators
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" _cfg)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} ${CMAKE_BINARY_DIR}/${cfg}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_cfg} ${CMAKE_BINARY_DIR}/${cfg}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_cfg} ${CMAKE_BINARY_DIR}/${cfg}/lib)
endforeach()

# JUCE path - adjust this to your JUCE installation
if(NOT DEFINED JUCE_PATH)
    # Try common locations relative to the project
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
        set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to JUCE")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../JUCE/CMakeLists.txt")
        set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../JUCE" CACHE PATH "Path to JUCE")
    elseif(DEFINED ENV{JUCE_PATH} AND EXISTS "$ENV{JUCE_PATH}/CMakeLists.txt")
        set(JUCE_PATH "$ENV{JUCE_PATH}" CACHE PATH "Path to JUCE")
    else()
        set(JUCE_PATH "" CACHE PATH "Path to JUCE")
    endif()
endif()

set(HAVE_JUCE OFF)
# Try to add JUCE if found
if(NOT JUCE_PATH STREQUAL "" AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
    set(HAVE_JUCE ON)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/JUCE" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
    set(HAVE_JUCE ON)
else()
    message(WARNING "JUCE not found. Continuing configuration without JUCE. To build the real plugin, set JUCE_PATH to your JUCE directory containing CMakeLists.txt.")
endif()

# Plugin sources
set(PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    # DSP
    Source/DSP/SpectrumAnalyzer.cpp
    Source/DSP/SpectrumAnalyzer.h
    Source/DSP/ParametricEQProcessor.cpp
    Source/DSP/ParametricEQProcessor.h
    Source/DSP/DynamicEQProcessor.cpp
    Source/DSP/DynamicEQProcessor.h
    Source/DSP/LinearPhaseProcessor.cpp
    Source/DSP/LinearPhaseProcessor.h

    # AI
    Source/AI/AIEngine.cpp
    Source/AI/AIEngine.h
    Source/AI/AIEngine_Advanced.cpp
    Source/AI/ReferenceMatcher.cpp
    Source/AI/ReferenceMatcher.h
    Source/AI/UserLearning.cpp
    Source/AI/UserLearning.h
    Source/AI/MLEngine.cpp
    Source/AI/MLEngine.h
    Source/AI/SemanticEQEngine.cpp
    Source/AI/SemanticEQEngine.h
    Source/AI/MultiTrackUnmasking.cpp
    Source/AI/MultiTrackUnmasking.h
    Source/AI/NeuralNetworkWrapper.cpp
    Source/AI/NeuralNetworkWrapper.h
    Source/AI/AdaptiveAIEngine.cpp
    Source/AI/AdaptiveAIEngine.h
    Source/AI/OnlineLearningSystem.cpp
    Source/AI/OnlineLearningSystem.h

    # GUI
    Source/GUI/ModernLookAndFeel.h
    Source/GUI/AdvancedSpectrumDisplay.h
    Source/GUI/AIControlPanel.h
    Source/GUI/ReferencePanel.h
    Source/GUI/EQBandControl.h
    Source/GUI/AIProblemPanel.h
    Source/GUI/BandControlPanel.h
    Source/GUI/BandViewport.cpp
    Source/GUI/BandViewport.h
    Source/GUI/DynamicEQPanel.h
    Source/GUI/SemanticControlPanel.h

    # Utils
    Source/Utils/Logger.cpp
    Source/Utils/Logger.h
    Source/Utils/PresetManager.cpp
    Source/Utils/PresetManager.h
)

option(AIEQ_ENABLE_TORCH "Enable Torch C++ seq2seq parser" OFF)
set(AIEQ_TORCH_ROOT "" CACHE PATH "Path to libtorch (root directory)")

if(AIEQ_ENABLE_TORCH)
    if(NOT AIEQ_TORCH_ROOT AND DEFINED ENV{TORCH_HOME})
        set(AIEQ_TORCH_ROOT "$ENV{TORCH_HOME}")
    endif()
    if(NOT AIEQ_TORCH_ROOT AND DEFINED ENV{LIBTORCH_ROOT})
        set(AIEQ_TORCH_ROOT "$ENV{LIBTORCH_ROOT}")
    endif()

    if(AIEQ_TORCH_ROOT)
        list(APPEND CMAKE_PREFIX_PATH "${AIEQ_TORCH_ROOT}")
    endif()

    find_package(Torch REQUIRED)
endif()

if(HAVE_JUCE)
    # Configure plugin formats (add AU on macOS)
    set(AIEQ_PLUGIN_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND AIEQ_PLUGIN_FORMATS AU)
    endif()

    # Create the plugin using JUCE helper
    juce_add_plugin(AIEqualizerPro
        COMPANY_NAME "AIAudio"
        PLUGIN_MANUFACTURER_CODE AIAU
        PLUGIN_CODE AIEQ
        FORMATS ${AIEQ_PLUGIN_FORMATS}
        PRODUCT_NAME "AI Equalizer Pro"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES "Fx"
    )
    # Add AU only on macOS
    if(APPLE)
        set_target_properties(AIEqualizerPro PROPERTIES JUCE_AU_ENABLED ON)
        target_compile_definitions(AIEqualizerPro PRIVATE AU_MAIN_TYPE="kAudioUnitType_Effect")
    endif()
else()
    # Fallback: create a dummy static library target so configuration can complete in environments
    # where JUCE isn't available (useful for CI or static analysis).
    add_library(AIEqualizerPro STATIC)
endif()

# Only add sources if target exists
if(TARGET AIEqualizerPro)
    target_sources(AIEqualizerPro PRIVATE ${PLUGIN_SOURCES})

    # Require C++20 at target level
    target_compile_features(AIEqualizerPro PRIVATE cxx_std_20)

    if(HAVE_JUCE)
        # Compile definitions
        target_compile_definitions(AIEqualizerPro
            PUBLIC
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_VST3_CAN_REPLACE_VST2=0
                JUCE_DISPLAY_SPLASH_SCREEN=0
        )

        # Link JUCE modules
        target_link_libraries(AIEqualizerPro
            PRIVATE
                juce::juce_audio_basics
                juce::juce_audio_devices
                juce::juce_audio_formats
                juce::juce_audio_plugin_client
                juce::juce_audio_processors
                juce::juce_audio_utils
                juce::juce_core
                juce::juce_data_structures
                juce::juce_dsp
                juce::juce_events
                juce::juce_graphics
                juce::juce_gui_basics
                juce::juce_gui_extra
                juce::juce_osc
                juce::juce_opengl
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags
        )
    else()
        message(STATUS "Configuring without JUCE: some targets and settings are disabled.")
    endif()

    # Include directories
    target_include_directories(AIEqualizerPro PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        $<$<BOOL:${AIEQ_ENABLE_TORCH}>:${TORCH_INCLUDE_DIRS}>
        $<$<BOOL:${WIN32}>:${PROJECT_SOURCE_DIR}/ThirdParty/TFLite/include>
    )

    # Generate map file for Standalone in Debug to trace crashes without WinDbg
    if(MSVC AND TARGET AIEqualizerPro_Standalone)
        target_link_options(AIEqualizerPro_Standalone PRIVATE
            $<$<CONFIG:Debug>:/MAP>
        )
    endif()

    if(AIEQ_ENABLE_TORCH)
        target_compile_definitions(AIEqualizerPro PRIVATE AIEQ_ENABLE_TORCH)
        target_link_libraries(AIEqualizerPro PRIVATE ${TORCH_LIBRARIES})
        if(MSVC)
            target_compile_options(AIEqualizerPro PRIVATE /bigobj)
        endif()
    endif()

    # Optional TensorFlow Lite (C API) on Windows
    if(WIN32)
        target_compile_definitions(AIEqualizerPro PRIVATE AIEQ_ENABLE_TFLITE)
        target_link_directories(AIEqualizerPro PRIVATE "${PROJECT_SOURCE_DIR}/ThirdParty/TFLite/lib")
        target_link_libraries(AIEqualizerPro PRIVATE tensorflowlite_c.lib)

        # Ensure Standalone also links the TFLite import lib and search path
        if(TARGET AIEqualizerPro_Standalone)
            target_compile_definitions(AIEqualizerPro_Standalone PRIVATE AIEQ_ENABLE_TFLITE)
            target_link_directories(AIEqualizerPro_Standalone PRIVATE "${PROJECT_SOURCE_DIR}/ThirdParty/TFLite/lib")
            target_link_libraries(AIEqualizerPro_Standalone PRIVATE tensorflowlite_c.lib)
        endif()

        # Ensure VST3 target also links TFLite
        if(TARGET AIEqualizerPro_VST3)
            target_compile_definitions(AIEqualizerPro_VST3 PRIVATE AIEQ_ENABLE_TFLITE)
            target_link_directories(AIEqualizerPro_VST3 PRIVATE "${PROJECT_SOURCE_DIR}/ThirdParty/TFLite/lib")
            target_link_libraries(AIEqualizerPro_VST3 PRIVATE tensorflowlite_c.lib)
        endif()
    endif()

    # Enable debug logging macros across all targets
    target_compile_definitions(AIEqualizerPro PRIVATE AIEQ_ENABLE_DEBUG_LOGGING)
    if(TARGET AIEqualizerPro_Standalone)
        target_compile_definitions(AIEqualizerPro_Standalone PRIVATE AIEQ_ENABLE_DEBUG_LOGGING)
    endif()
    if(TARGET AIEqualizerPro_VST3)
        target_compile_definitions(AIEqualizerPro_VST3 PRIVATE AIEQ_ENABLE_DEBUG_LOGGING)
    endif()
    if(TARGET AIEqualizerPro_Tests)
        target_compile_definitions(AIEqualizerPro_Tests PRIVATE AIEQ_ENABLE_DEBUG_LOGGING)
    endif()

    # Compiler-specific optimizations
    if(MSVC)
        target_compile_options(AIEqualizerPro PRIVATE
            /MP                          # Parallel compilation
            /W4                          # Warning level 4
            /permissive-                 # Standards conformance
            /Zc:__cplusplus             # Correct __cplusplus macro
            $<$<CONFIG:Release>:/O2>    # Optimize for speed in Release
            $<$<CONFIG:Release>:/GL>    # Whole program optimization
        )
        target_link_options(AIEqualizerPro PRIVATE
            $<$<CONFIG:Release>:/LTCG>  # Link-time code generation
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(AIEqualizerPro PRIVATE
            -Wall -Wextra -Wpedantic
            $<$<CONFIG:Release>:-O3>
        )
        # -march=native is incompatible with macOS universal binaries (x86_64+arm64)
        # because the native CPU (e.g. apple-m1) is not a valid x86_64 target
        if(NOT APPLE OR NOT CMAKE_OSX_ARCHITECTURES MATCHES ";")
            target_compile_options(AIEqualizerPro PRIVATE
                $<$<CONFIG:Release>:-march=native>
            )
        endif()
    endif()

    # Platform-specific definitions
    if(WIN32)
        target_compile_definitions(AIEqualizerPro PRIVATE
            _USE_MATH_DEFINES
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
    endif()

    # macOS-specific settings
    if(APPLE)
        set_target_properties(AIEqualizerPro PROPERTIES
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES
            XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL $<$<CONFIG:Release>:3>
        )
    endif()

    # Windows: Copy VST3 plugin to standard VST3 directory for Ableton/DAW scanning
    if(HAVE_JUCE AND WIN32)
        # Try standard VST3 directory first, fallback to user directory if no admin rights
        # Most DAWs scan both locations automatically
        set(VST3_INSTALL_DIR_SYSTEM "$ENV{ProgramFiles}/Common Files/VST3")
        set(VST3_INSTALL_DIR_USER "$ENV{LOCALAPPDATA}/VST3")

        # Use user directory by default (no admin required)
        # User can manually copy to Program Files if needed
        set(VST3_INSTALL_DIR "${VST3_INSTALL_DIR_USER}")

        # Get the VST3 plugin path from JUCE
        # JUCE creates the plugin in: AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3
        set(VST3_SOURCE_DIR "${CMAKE_BINARY_DIR}/AIEqualizerPro_artefacts/Release/VST3")
        set(VST3_PLUGIN_NAME "AI Equalizer Pro.vst3")
        set(VST3_SOURCE_PATH "${VST3_SOURCE_DIR}/${VST3_PLUGIN_NAME}")
        set(VST3_DEST_PATH "${VST3_INSTALL_DIR}/${VST3_PLUGIN_NAME}")

        # Create a custom target to copy the plugin after build
        # Use PowerShell to handle directory copy with better error handling
        if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES)
            set(VST3_COPY_SCRIPT "${CMAKE_BINARY_DIR}/copy_vst3.ps1")
            # Get the DLL source path (JUCE creates it in Release/lib as a file)
            set(VST3_DLL_SOURCE "${CMAKE_BINARY_DIR}/Release/lib/${VST3_PLUGIN_NAME}")
            file(WRITE "${VST3_COPY_SCRIPT}"
                "if (Test-Path \"${VST3_SOURCE_PATH}\") {\n"
                "    # Fix: Ensure DLL is in the correct location within the bundle\n"
                "    $dllSource = \"${VST3_DLL_SOURCE}\"\n"
                "    $dllDest = \"${VST3_SOURCE_PATH}\\Contents\\x86_64-win64\\${VST3_PLUGIN_NAME}\"\n"
                "    if (Test-Path $dllSource -PathType Leaf) {\n"
                "        $dllDir = Split-Path -Parent $dllDest\n"
                "        if (-not (Test-Path $dllDir)) {\n"
                "            New-Item -ItemType Directory -Path $dllDir -Force | Out-Null\n"
                "        }\n"
                "        Copy-Item -Path $dllSource -Destination $dllDest -Force\n"
                "        Write-Host \"[FIX] DLL copiata nella struttura corretta del bundle\"\n"
                "    }\n"
                "    \n"
                "    # Try user directory first (no admin required)\n"
                "    $destDir = \"${VST3_INSTALL_DIR_USER}\"\n"
                "    $destPath = \"$destDir\\${VST3_PLUGIN_NAME}\"\n"
                "    \n"
                "    if (-not (Test-Path \"$destDir\")) {\n"
                "        New-Item -ItemType Directory -Path \"$destDir\" -Force | Out-Null\n"
                "    }\n"
                "    if (Test-Path \"$destPath\") {\n"
                "        Remove-Item -Path \"$destPath\" -Recurse -Force -ErrorAction SilentlyContinue\n"
                "    }\n"
                "    Copy-Item -Path \"${VST3_SOURCE_PATH}\" -Destination \"$destPath\" -Recurse -Force\n"
                "    Write-Host \"[OK] Plugin copiato in: $destPath\"\n"
                "    Write-Host \"[INFO] Ableton scansionerà automaticamente questa directory!\"\n"
                "    \n"
                "    # Try system directory too (requires admin, but continue if fails)\n"
                "    $systemDir = \"${VST3_INSTALL_DIR_SYSTEM}\"\n"
                "    $systemPath = \"$systemDir\\${VST3_PLUGIN_NAME}\"\n"
                "    try {\n"
                "        if (-not (Test-Path \"$systemDir\")) {\n"
                "            New-Item -ItemType Directory -Path \"$systemDir\" -Force -ErrorAction Stop | Out-Null\n"
                "        }\n"
                "        if (Test-Path \"$systemPath\") {\n"
                "            Remove-Item -Path \"$systemPath\" -Recurse -Force -ErrorAction Stop\n"
                "        }\n"
                "        Copy-Item -Path \"${VST3_SOURCE_PATH}\" -Destination \"$systemPath\" -Recurse -Force -ErrorAction Stop\n"
                "        Write-Host \"[OK] Plugin copiato anche in: $systemPath\"\n"
                "    } catch {\n"
                "        Write-Host \"[INFO] Impossibile copiare in Program Files (richiede permessi admin)\"\n"
                "        Write-Host \"[INFO] Il plugin è comunque disponibile in: $destPath\"\n"
                "    }\n"
                "} else {\n"
                "    Write-Host \"[ERRORE] Plugin non trovato in: ${VST3_SOURCE_PATH}\"\n"
                "    exit 1\n"
                "}\n"
            )

            add_custom_target(copy_vst3_to_common_files
                COMMAND powershell.exe -ExecutionPolicy Bypass -File "${VST3_COPY_SCRIPT}"
                COMMENT "Copying VST3 plugin to ${VST3_INSTALL_DIR} for DAW scanning"
                DEPENDS AIEqualizerPro_VST3
                VERBATIM
            )
        else()
            # Fallback for non-Release builds
            add_custom_target(copy_vst3_to_common_files
                COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_INSTALL_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${VST3_SOURCE_PATH}" "${VST3_DEST_PATH}"
                COMMENT "Copying VST3 plugin to ${VST3_INSTALL_DIR} for DAW scanning"
                DEPENDS AIEqualizerPro_VST3
                VERBATIM
            )
        endif()

        # Make the copy happen automatically after VST3 build
        add_dependencies(copy_vst3_to_common_files AIEqualizerPro_VST3)

        message(STATUS "VST3 plugin will be automatically copied to:")
        message(STATUS "  User directory: ${VST3_INSTALL_DIR_USER} (no admin required)")
        message(STATUS "  System directory: ${VST3_INSTALL_DIR_SYSTEM} (if admin available)")
        message(STATUS "  After build, Ableton will find it with Rescan!")
    endif()
endif()

# Unit tests - DSP-only, no AI/ML external dependencies (no TFLite, no Torch).
# Links only LinearPhaseProcessor, ParametricEQProcessor, DynamicEQProcessor
# directly, avoiding the full plugin build graph.
if(HAVE_JUCE)
    enable_testing()

    add_executable(AIEqualizerPro_Tests
        Source/DSP/LinearPhaseProcessor.cpp
        Source/DSP/ParametricEQProcessor.cpp
        Source/DSP/DynamicEQProcessor.cpp
        Source/Tests/ParametricEQTest.cpp
        Source/Tests/LinearPhaseIRSmokeTest.cpp
        Source/Tests/LinearPhaseGainRegressionTest.cpp
        Source/Tests/DynamicEQRegressionTest.cpp
        Source/Tests/FuzzBlockSizeTest.cpp
        Source/Tests/TestMain.cpp
    )
    target_compile_features(AIEqualizerPro_Tests PRIVATE cxx_std_20)
    target_compile_definitions(AIEqualizerPro_Tests PRIVATE
        JUCE_UNIT_TESTS=1
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_STANDALONE_APPLICATION=0
    )
    target_include_directories(AIEqualizerPro_Tests PRIVATE
        Source
        Source/DSP
        Source/Core
        Source/Utils
    )
    target_link_libraries(AIEqualizerPro_Tests PRIVATE
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
    )
    set_target_properties(AIEqualizerPro_Tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Tests)

    add_test(NAME aieq_dsp_regression
             COMMAND $<TARGET_FILE:AIEqualizerPro_Tests>)

    # ---------------------------------------------------------------------------
    # Integration tests — full plugin processor path via SharedCode.
    # Covers bypass, phase mode, oversampling/pendingReset, and state regressions.
    # ---------------------------------------------------------------------------
    add_executable(AIEqualizerPro_IntegrationTests
        Source/Tests/TransitionContinuityTests.cpp
        Source/Tests/IntegrationStateTest.cpp
        Source/Tests/BlockSizeRegressionTest.cpp
        Source/Tests/TestMain.cpp
    )
    target_compile_features(AIEqualizerPro_IntegrationTests PRIVATE cxx_std_20)
    target_compile_definitions(AIEqualizerPro_IntegrationTests PRIVATE
        JUCE_UNIT_TESTS=1
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_STANDALONE_APPLICATION=0
        JucePlugin_Build_VST3=0
        JucePlugin_Build_AU=0
        JucePlugin_Build_Standalone=0
    )
    target_include_directories(AIEqualizerPro_IntegrationTests PRIVATE
        Source
        Source/DSP
        Source/Core
        Source/Utils
        Source/AI
    )
    target_link_libraries(AIEqualizerPro_IntegrationTests PRIVATE
        AIEqualizerPro
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
    )
    set_target_properties(AIEqualizerPro_IntegrationTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Tests)

    add_test(NAME aieq_integration_regression
             COMMAND $<TARGET_FILE:AIEqualizerPro_IntegrationTests>)
endif()
