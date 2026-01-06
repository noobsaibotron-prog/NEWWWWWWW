# Code Review – 2026-01-06

Scope: sintesi dei bug rilevati e fix proposti per i principali moduli (PluginProcessor, AIEngine, DSP, Logger, GUI).

## Critici (fix immediato)
- `Source/PluginProcessor.cpp` – path di debug hardcoded `C:\AIEQ\linear_phase_debug.txt`; directory potrebbe non esistere e non è cross‑platform. **Fix:** usare `juce::File::getSpecialLocation(...userApplicationDataDirectory)` con sotto-cartella `AIEqualizerPro/Logs`, `createDirectory()`, file aperto su thread non-RT o tramite logger centrale.
- `Source/AI/AIEngine.h/.cpp` – mutex (`std::mutex`, `std::shared_mutex`) e copie `std::vector` nel percorso dichiarato lock‑free; `spectrumMutex`/`correctionsMutex` possono bloccare l’audio/GUI. **Fix:** sostituire con snapshot/queue lock-free (già presenti in `Source/Core/LockFreeStructures.h`), usare triple-buffer per spettri/correzioni, evitare allocazioni nel path RT.
- `Source/Utils/Logger.h` – `juce::SpinLock` in RT path e I/O sincrono (`std::ofstream`). **Fix:** usare SPSC queue già disponibile, writer su thread di background, nessun lock nel thread audio; aggiungere flush condizionato e gestione errori async.

## Maggiori
- `Source/PluginProcessor.cpp` – builder IR con scaling multipli e magic numbers; potenziale race sul pattern di version-check; busy loop con `yield`. **Fix:** documentare/giustificare i coefficienti, aggiungere test unitari IR, valutare RCU/atomic snapshot per selezione coeff, sostituire busy loop con wait/condition appropriata in non-RT.
- `Source/AI/AIEngine.cpp` – rate limiting disattivato → CPU waste; path di analyze fa copie e lock. **Fix:** reintrodurre rate limit basato su change-detection o intervallo minimo; evitare allocazioni nel loop.
- `Source/Utils/Logger.h` – macro hygiene (`__FUNCTION__`, manca `do{}while(0)`), logging non offloaded. **Fix:** usare `__func__`, incapsulare macro, rinviare tutto al thread writer.

## Moderati/Minori
- `Source/DSP/SpectrumAnalyzer.h` – `fifoCapacity` fisso 32768 e smoothing non frequency-dependent. **Fix:** dimensionare su max FFT + block size, introdurre smoothing per banda.
- `Source/DSP/DynamicEQProcessor.h` – gestione latenza lookahead non esplicitata; auto-makeup da verificare. **Fix:** assicurare reporting latenza al host e testare makeup.
- `Source/PluginProcessor.cpp` – file di debug non chiusi, commenti misti IT/EN. **Fix:** usare RAII/unique_ptr stream e normalizzare commenti.

## Raccomandazioni prioritarie (ordine)
1) Rimuovere path hardcoded e centralizzare logging async.
2) Rendere `AIEngine` realmente lock-free (snapshot/queue + zero allocazioni in RT).
3) Rifattorizzare logger per eliminare spinlock e I/O sincrono nel thread audio.
4) Documentare e testare il builder IR (scaling, magic numbers, race).
5) Reintrodurre rate limiting nel percorso AI; ottimizzare analyze path.
6) Rifinire SpectrumAnalyzer e DynamicEQ (capacity, smoothing, latenza).

## Note positive
- `Source/DSP/ParametricEQProcessor.h` e `Source/Core/LockFreeStructures.h`: architettura lock-free eccellente, nessun bug rilevato.
- `Source/DSP/SpectrumAnalyzer.h`: FFT preallocata e FIFO lock-free ben strutturati.
- `Source/PluginEditor.h`: UI organizzata e responsive.

