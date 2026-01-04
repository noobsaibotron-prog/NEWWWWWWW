if (Test-Path "C:/AIEQ/build/AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3") {
    # Fix: Ensure DLL is in the correct location within the bundle
    $dllSource = "C:/AIEQ/build/Release/lib/AI Equalizer Pro.vst3"
    $dllDest = "C:/AIEQ/build/AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3\Contents\x86_64-win64\AI Equalizer Pro.vst3"
    if (Test-Path $dllSource -PathType Leaf) {
        $dllDir = Split-Path -Parent $dllDest
        if (-not (Test-Path $dllDir)) {
            New-Item -ItemType Directory -Path $dllDir -Force | Out-Null
        }
        Copy-Item -Path $dllSource -Destination $dllDest -Force
        Write-Host "[FIX] DLL copiata nella struttura corretta del bundle"
    }
    
    # Try user directory first (no admin required)
    $destDir = "C:\Users\noobs\AppData\Local/VST3"
    $destPath = "$destDir\AI Equalizer Pro.vst3"
    
    if (-not (Test-Path "$destDir")) {
        New-Item -ItemType Directory -Path "$destDir" -Force | Out-Null
    }
    if (Test-Path "$destPath") {
        Remove-Item -Path "$destPath" -Recurse -Force -ErrorAction SilentlyContinue
    }
    Copy-Item -Path "C:/AIEQ/build/AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3" -Destination "$destPath" -Recurse -Force
    Write-Host "[OK] Plugin copiato in: $destPath"
    Write-Host "[INFO] Ableton scansionerà automaticamente questa directory!"
    
    # Try system directory too (requires admin, but continue if fails)
    $systemDir = "C:\Program Files/Common Files/VST3"
    $systemPath = "$systemDir\AI Equalizer Pro.vst3"
    try {
        if (-not (Test-Path "$systemDir")) {
            New-Item -ItemType Directory -Path "$systemDir" -Force -ErrorAction Stop | Out-Null
        }
        if (Test-Path "$systemPath") {
            Remove-Item -Path "$systemPath" -Recurse -Force -ErrorAction Stop
        }
        Copy-Item -Path "C:/AIEQ/build/AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3" -Destination "$systemPath" -Recurse -Force -ErrorAction Stop
        Write-Host "[OK] Plugin copiato anche in: $systemPath"
    } catch {
        Write-Host "[INFO] Impossibile copiare in Program Files (richiede permessi admin)"
        Write-Host "[INFO] Il plugin è comunque disponibile in: $destPath"
    }
} else {
    Write-Host "[ERRORE] Plugin non trovato in: C:/AIEQ/build/AIEqualizerPro_artefacts/Release/VST3/AI Equalizer Pro.vst3"
    exit 1
}
