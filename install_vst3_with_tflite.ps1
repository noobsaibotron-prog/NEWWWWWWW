param(
    [string]$DestVst3 = "C:\Program Files\Common Files\VST3\AI Equalizer Pro.vst3"
)

$ErrorActionPreference = 'Stop'

$binSrc = "C:\AIEQ\build\Release\lib\AI Equalizer Pro.vst3"
$modSrc = "C:\AIEQ\build\AIEqualizerPro_artefacts\Release\VST3\AI Equalizer Pro.vst3\Contents\Resources\moduleinfo.json"
$dllSrc = "C:\AIEQ\ThirdParty\TFLite\lib\tensorflowlite_c.dll"

Write-Host "Destinazione: $DestVst3" -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path "$DestVst3\Contents\x86_64-win" | Out-Null
New-Item -ItemType Directory -Force -Path "$DestVst3\Contents\Resources"    | Out-Null

Copy-Item $binSrc "$DestVst3\Contents\x86_64-win\AI Equalizer Pro.vst3" -Force
Copy-Item $dllSrc "$DestVst3\Contents\x86_64-win\" -Force
Copy-Item $modSrc "$DestVst3\Contents\Resources\moduleinfo.json" -Force

Write-Host "Copia completata." -ForegroundColor Green
