# Rimuovi vecchia installazione
if (Test-Path 'C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3') {
    Remove-Item 'C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3' -Recurse -Force
}

# Crea struttura
New-Item 'C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win' -ItemType Directory -Force | Out-Null
New-Item 'C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\Resources' -ItemType Directory -Force | Out-Null

# Copia file
Copy-Item 'C:\AIEQ\build\Release\lib\AI Equalizer Pro.vst3' 'C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\x86_64-win\AI Equalizer Pro.vst3' -Force
Copy-Item 'C:\AIEQ\build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3\Contents\Resources\moduleinfo.json' 'C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3\Contents\Resources\moduleinfo.json' -Force

Write-Host 'DONE'
