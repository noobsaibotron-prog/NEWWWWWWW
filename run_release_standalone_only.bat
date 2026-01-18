@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d C:\AIEQ\build
cmake --build . --config Release --target AIEqualizerPro_Standalone

