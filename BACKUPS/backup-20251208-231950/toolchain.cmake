set(CMAKE_C_COMPILER "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64/cl.exe")
set(CMAKE_CXX_COMPILER "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64/cl.exe")
set(CMAKE_RC_COMPILER "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe")

# Windows SDK paths
set(CMAKE_WINDOWS_KITS_10_DIR "C:/Program Files (x86)/Windows Kits/10")
set(CMAKE_SYSTEM_VERSION "10.0.22621.0")

# Library paths
link_directories(
    "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/lib/x64"
    "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/ucrt/x64"
    "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64"
)