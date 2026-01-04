@echo off
echo === VST3 FILE DATES === > C:\AIEQ\dates_output.txt
echo. >> C:\AIEQ\dates_output.txt

echo COMMON FILES VST3: >> C:\AIEQ\dates_output.txt
dir "C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win\AI Equalizer Pro.vst3" >> C:\AIEQ\dates_output.txt 2>&1
echo. >> C:\AIEQ\dates_output.txt

echo LOCAL VST3: >> C:\AIEQ\dates_output.txt
dir "%LOCALAPPDATA%\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win\AI Equalizer Pro.vst3" >> C:\AIEQ\dates_output.txt 2>&1
echo. >> C:\AIEQ\dates_output.txt

echo SOURCE FILES: >> C:\AIEQ\dates_output.txt
dir "C:\AIEQ\Source\PluginProcessor.cpp" >> C:\AIEQ\dates_output.txt 2>&1
dir "C:\AIEQ\Source\AI\SemanticEQEngine.cpp" >> C:\AIEQ\dates_output.txt 2>&1
echo. >> C:\AIEQ\dates_output.txt

echo BUILD FOLDER: >> C:\AIEQ\dates_output.txt
dir "C:\AIEQ\build\AIEqualizerPro_artefacts" >> C:\AIEQ\dates_output.txt 2>&1
echo. >> C:\AIEQ\dates_output.txt

echo Done!
