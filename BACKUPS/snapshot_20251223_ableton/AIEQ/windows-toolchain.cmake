# Windows Toolchain per Visual Studio
# Questo file specifica tutti i percorsi necessari per CMake
# Usato principalmente con NMake Makefiles generator
#
# NOTA: NON impostiamo CMAKE_SYSTEM_NAME qui perché questo farebbe
# trattare CMake come cross-compilation, impedendo la determinazione
# automatica delle variabili di compilazione come CMAKE_C_COMPILE_OBJECT.
# CMake determinerà automaticamente che siamo su Windows.

# Trova Visual Studio (2026 = versione 19, percorso "18")
set(VS_PATH "C:/Program Files/Microsoft Visual Studio/18/Community")
if(NOT EXISTS "${VS_PATH}")
    set(VS_PATH "C:/Program Files/Microsoft Visual Studio/2026/Community")
endif()
if(NOT EXISTS "${VS_PATH}")
    set(VS_PATH "C:/Program Files/Microsoft Visual Studio/2024/Community")
endif()
if(NOT EXISTS "${VS_PATH}")
    set(VS_PATH "C:/Program Files/Microsoft Visual Studio/2022/Community")
endif()

# Trova MSVC
file(GLOB MSVC_PATHS "${VS_PATH}/VC/Tools/MSVC/*")
if(MSVC_PATHS)
    # Ordina per versione (più recente prima)
    list(SORT MSVC_PATHS)
    list(REVERSE MSVC_PATHS)
    list(GET MSVC_PATHS 0 MSVC_PATH)
    set(MSVC_BIN "${MSVC_PATH}/bin/Hostx64/x64")
    
    # Verifica che cl.exe esista
    if(EXISTS "${MSVC_BIN}/cl.exe")
        # Imposta compilatori
        set(CMAKE_C_COMPILER "${MSVC_BIN}/cl.exe" CACHE FILEPATH "C compiler" FORCE)
        set(CMAKE_CXX_COMPILER "${MSVC_BIN}/cl.exe" CACHE FILEPATH "CXX compiler" FORCE)
        
        # Trova altri tool necessari
        set(CMAKE_LINKER "${MSVC_BIN}/link.exe" CACHE FILEPATH "Linker" FORCE)
        set(CMAKE_AR "${MSVC_BIN}/lib.exe" CACHE FILEPATH "Archiver" FORCE)
        set(CMAKE_RANLIB "${MSVC_BIN}/lib.exe" CACHE FILEPATH "Ranlib" FORCE)
        set(CMAKE_NM "${MSVC_BIN}/dumpbin.exe" CACHE FILEPATH "NM" FORCE)
        if(EXISTS "${VS_PATH}/Common7/IDE/mt.exe")
            set(CMAKE_MT "${VS_PATH}/Common7/IDE/mt.exe" CACHE FILEPATH "MT" FORCE)
        endif()
        
        message(STATUS "Found MSVC at: ${MSVC_PATH}")
        message(STATUS "Using C compiler: ${CMAKE_C_COMPILER}")
        message(STATUS "Using CXX compiler: ${CMAKE_CXX_COMPILER}")
    else()
        message(WARNING "cl.exe not found at ${MSVC_BIN}/cl.exe")
    endif()
endif()

# Trova Windows SDK per rc.exe (x64)
set(CMAKE_RC_COMPILER "")
if(EXISTS "C:/Program Files (x86)/Windows Kits/10/bin")
    file(GLOB WINSDK_VERSIONS "C:/Program Files (x86)/Windows Kits/10/bin/*")
    if(WINSDK_VERSIONS)
        # Ordina per versione (più recente prima)
        list(SORT WINSDK_VERSIONS)
        list(REVERSE WINSDK_VERSIONS)
        # Cerca rc.exe x64 nella versione più recente
        foreach(WINSDK_VER ${WINSDK_VERSIONS})
            if(EXISTS "${WINSDK_VER}/x64/rc.exe")
                set(CMAKE_RC_COMPILER "${WINSDK_VER}/x64/rc.exe")
                break()
            endif()
        endforeach()
    endif()
endif()

# Fallback: usa percorso specifico se trovato
if(NOT CMAKE_RC_COMPILER)
    if(EXISTS "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe")
        set(CMAKE_RC_COMPILER "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe")
    endif()
endif()

# Se rc.exe non trovato, cerca in altri percorsi
if(NOT CMAKE_RC_COMPILER)
    if(EXISTS "${VS_PATH}/VC/Tools/MSVC")
        file(GLOB MSVC_DIRS "${VS_PATH}/VC/Tools/MSVC/*")
        if(MSVC_DIRS)
            list(GET MSVC_DIRS 0 MSVC_DIR)
            if(EXISTS "${MSVC_DIR}/bin/Hostx64/x64/rc.exe")
                set(CMAKE_RC_COMPILER "${MSVC_DIR}/bin/Hostx64/x64/rc.exe")
            endif()
        endif()
    endif()
endif()

# Trova Windows SDK per librerie (libucrt.lib, ecc.)
set(WINSDK_LIB_PATH "")
if(EXISTS "C:/Program Files (x86)/Windows Kits/10/Lib")
    file(GLOB WINSDK_LIB_VERSIONS "C:/Program Files (x86)/Windows Kits/10/Lib/*")
    if(WINSDK_LIB_VERSIONS)
        list(SORT WINSDK_LIB_VERSIONS)
        list(REVERSE WINSDK_LIB_VERSIONS)
        foreach(WINSDK_LIB_VER ${WINSDK_LIB_VERSIONS})
            if(EXISTS "${WINSDK_LIB_VER}/um/x64")
                set(WINSDK_LIB_PATH "${WINSDK_LIB_VER}/um/x64")
                break()
            endif()
        endforeach()
    endif()
endif()

# Trova Universal CRT
set(UCRT_LIB_PATH "")
if(EXISTS "C:/Program Files (x86)/Windows Kits/10/Lib")
    file(GLOB WINSDK_LIB_VERSIONS "C:/Program Files (x86)/Windows Kits/10/Lib/*")
    if(WINSDK_LIB_VERSIONS)
        list(SORT WINSDK_LIB_VERSIONS)
        list(REVERSE WINSDK_LIB_VERSIONS)
        foreach(WINSDK_LIB_VER ${WINSDK_LIB_VERSIONS})
            if(EXISTS "${WINSDK_LIB_VER}/ucrt/x64")
                set(UCRT_LIB_PATH "${WINSDK_LIB_VER}/ucrt/x64")
                break()
            endif()
        endforeach()
    endif()
endif()

# Imposta flags con percorsi librerie
set(CMAKE_C_FLAGS_INIT "/DWIN32 /D_WINDOWS")
set(CMAKE_CXX_FLAGS_INIT "/DWIN32 /D_WINDOWS /EHsc")
set(CMAKE_C_FLAGS_RELEASE_INIT "/MD /O2 /Ob2 /DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "/MD /O2 /Ob2 /DNDEBUG")

# Per Visual Studio generator, imposta le variabili d'ambiente LIB
# MSBuild usa queste variabili invece dei linker flags
set(LIB_PATHS "")

# Aggiungi percorsi librerie ai linker flags (per NMake)
set(CMAKE_EXE_LINKER_FLAGS_INIT "")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "")

if(WINSDK_LIB_PATH)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} /LIBPATH:\"${WINSDK_LIB_PATH}\"")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} /LIBPATH:\"${WINSDK_LIB_PATH}\"")
    set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_MODULE_LINKER_FLAGS_INIT} /LIBPATH:\"${WINSDK_LIB_PATH}\"")
    list(APPEND LIB_PATHS "${WINSDK_LIB_PATH}")
    message(STATUS "Found Windows SDK libs at: ${WINSDK_LIB_PATH}")
endif()

if(UCRT_LIB_PATH)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} /LIBPATH:\"${UCRT_LIB_PATH}\"")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} /LIBPATH:\"${UCRT_LIB_PATH}\"")
    set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_MODULE_LINKER_FLAGS_INIT} /LIBPATH:\"${UCRT_LIB_PATH}\"")
    list(APPEND LIB_PATHS "${UCRT_LIB_PATH}")
    message(STATUS "Found UCRT libs at: ${UCRT_LIB_PATH}")
endif()

# Aggiungi anche percorsi MSVC
# MSVC_BIN è: .../MSVC/version/bin/Hostx64/x64
# Le librerie sono in: .../MSVC/version/lib/x64
if(MSVC_BIN)
    # Risali di 2 livelli da bin/Hostx64/x64 per arrivare a MSVC/version
    get_filename_component(MSVC_BIN_DIR "${MSVC_BIN}" ABSOLUTE)
    get_filename_component(MSVC_HOST_DIR "${MSVC_BIN_DIR}" DIRECTORY)
    get_filename_component(MSVC_BIN_PARENT "${MSVC_HOST_DIR}" DIRECTORY)
    get_filename_component(MSVC_VERSION_DIR "${MSVC_BIN_PARENT}" DIRECTORY)
    
    # Costruisci il percorso delle librerie
    set(MSVC_LIB_DIR "${MSVC_VERSION_DIR}/lib/x64")
    
    if(EXISTS "${MSVC_LIB_DIR}")
        set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} /LIBPATH:\"${MSVC_LIB_DIR}\"")
        set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} /LIBPATH:\"${MSVC_LIB_DIR}\"")
        set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_MODULE_LINKER_FLAGS_INIT} /LIBPATH:\"${MSVC_LIB_DIR}\"")
        list(APPEND LIB_PATHS "${MSVC_LIB_DIR}")
        message(STATUS "Found MSVC libs at: ${MSVC_LIB_DIR}")
    else()
        message(WARNING "MSVC lib directory not found: ${MSVC_LIB_DIR}")
        message(STATUS "  MSVC_BIN: ${MSVC_BIN}")
        message(STATUS "  Version dir: ${MSVC_VERSION_DIR}")
    endif()
endif()

# Per Visual Studio generator, imposta CMAKE_SYSTEM_LIBRARY_PATH
# Questo viene usato da CMake per trovare le librerie
if(LIB_PATHS)
    set(CMAKE_SYSTEM_LIBRARY_PATH ${LIB_PATHS} CACHE STRING "System library paths" FORCE)
    message(STATUS "CMAKE_SYSTEM_LIBRARY_PATH: ${CMAKE_SYSTEM_LIBRARY_PATH}")
endif()

# Trova e imposta percorsi INCLUDE per gli header files
set(INCLUDE_PATHS "")
if(MSVC_BIN)
    # MSVC include directory
    get_filename_component(MSVC_BIN_DIR "${MSVC_BIN}" ABSOLUTE)
    get_filename_component(MSVC_HOST_DIR "${MSVC_BIN_DIR}" DIRECTORY)
    get_filename_component(MSVC_BIN_PARENT "${MSVC_HOST_DIR}" DIRECTORY)
    get_filename_component(MSVC_VERSION_DIR "${MSVC_BIN_PARENT}" DIRECTORY)
    set(MSVC_INCLUDE_DIR "${MSVC_VERSION_DIR}/include")
    
    if(EXISTS "${MSVC_INCLUDE_DIR}")
        list(APPEND INCLUDE_PATHS "${MSVC_INCLUDE_DIR}")
        message(STATUS "Found MSVC include at: ${MSVC_INCLUDE_DIR}")
    endif()
endif()

# Trova Windows SDK include paths
if(EXISTS "C:/Program Files (x86)/Windows Kits/10/Include")
    file(GLOB WINSDK_INCLUDE_VERSIONS "C:/Program Files (x86)/Windows Kits/10/Include/*")
    if(WINSDK_INCLUDE_VERSIONS)
        list(SORT WINSDK_INCLUDE_VERSIONS)
        list(REVERSE WINSDK_INCLUDE_VERSIONS)
        list(GET WINSDK_INCLUDE_VERSIONS 0 WINSDK_INCLUDE_VER)
        
        # UCRT include
        if(EXISTS "${WINSDK_INCLUDE_VER}/ucrt")
            list(APPEND INCLUDE_PATHS "${WINSDK_INCLUDE_VER}/ucrt")
            message(STATUS "Found UCRT include at: ${WINSDK_INCLUDE_VER}/ucrt")
        endif()
        
        # UM include
        if(EXISTS "${WINSDK_INCLUDE_VER}/um")
            list(APPEND INCLUDE_PATHS "${WINSDK_INCLUDE_VER}/um")
            message(STATUS "Found Windows SDK UM include at: ${WINSDK_INCLUDE_VER}/um")
        endif()
        
        # Shared include
        if(EXISTS "${WINSDK_INCLUDE_VER}/shared")
            list(APPEND INCLUDE_PATHS "${WINSDK_INCLUDE_VER}/shared")
            message(STATUS "Found Windows SDK shared include at: ${WINSDK_INCLUDE_VER}/shared")
        endif()
    endif()
endif()

# Imposta INCLUDE environment variable per i subprocessi CMake
if(INCLUDE_PATHS)
    # Costruisci la stringa INCLUDE separata da punto e virgola
    string(JOIN ";" INCLUDE_STRING ${INCLUDE_PATHS})
    
    # Aggiungi INCLUDE all'ambiente se non già presente
    if(DEFINED ENV{INCLUDE})
        set(ENV{INCLUDE} "${INCLUDE_STRING};$ENV{INCLUDE}")
    else()
        set(ENV{INCLUDE} "${INCLUDE_STRING}")
    endif()
    
    message(STATUS "Set INCLUDE environment variable: $ENV{INCLUDE}")
    
    # Imposta anche CMAKE_CXX_FLAGS per includere i percorsi header
    # Questo aiuta quando CMake testa il compilatore
    foreach(INCLUDE_PATH ${INCLUDE_PATHS})
        set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} /I\"${INCLUDE_PATH}\"")
        set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} /I\"${INCLUDE_PATH}\"")
    endforeach()
endif()

message(STATUS "Using C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Using CXX compiler: ${CMAKE_CXX_COMPILER}")
if(CMAKE_RC_COMPILER)
    message(STATUS "Using RC compiler: ${CMAKE_RC_COMPILER}")
endif()

