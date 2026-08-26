#include "PluginProcessor.h"
#include "DSP/NumericSafety.h"
#include "AI/SemanticSlotPreflight.h"
#include "PluginEditor.h"
#include "DSP/DefaultBandFrequencies.h"
#include "Utils/APVTSStateSchema.h"
#include "Utils/Logger.h"
#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2
#include "AI/MotoreV2Features.h"   // EXP hybrid: rawDb -> 64 log-mel (gated)
#endif
#include <limits>
#include <algorithm>
#include <complex>
#include <cmath>
#include <cstdio>

namespace
{
    static_assert(AIEqualizerAudioProcessor::maxBands
                  == static_cast<int>(AIEQDSP::defaultBandFrequencies.size()));
    static_assert(AIEqualizerAudioProcessor::maxBands == AIEQStateSchema::bandCount);

    // M/S encoding/decoding scale factor (1 / sqrt(2))
    // Used in both encodeStereoToMS() and decodeMSToStereo() to normalise
    // mid and side channels to the same RMS level as the input L/R pair.
    constexpr float kInvSqrt2 = 0.7071067811865476f;

    // Band-assignment scoring constants (used in applyAICorrections and
    // applySemanticCorrections to pick the best existing EQ band to reuse).
    // ~1/5 octave: if an existing band is within this ratio of the target
    // frequency it is a candidate for reuse instead of allocating a new band.
    constexpr float kBandReuseThresholdOctave = 0.148f;
    // Score multiplier applied to unused bands — strongly prefers free bands.
    constexpr float kUnusedBandScoreMult = 0.3f;
    // Score multiplier applied when a band is within reuse threshold — even
    // stronger preference for merging near-frequency corrections.
    constexpr float kReuseThresholdScoreMult = 0.2f;

    constexpr int kCurrentStateSchemaVersion = AIEQStateSchema::currentVersion;
    constexpr int kLegacyCurveMode = AIEQStateSchema::legacyCurveMode;
    constexpr int kSurgicalCurveMode = AIEQStateSchema::surgicalCurveMode;

    class AnalysisConcurrencyGuard
    {
    public:
        AnalysisConcurrencyGuard(std::atomic<int>& currentIn,
                                 std::atomic<int>& maximumIn) noexcept
            : current(currentIn), maximum(maximumIn)
        {
            const int now = current.fetch_add(1, std::memory_order_acq_rel) + 1;
            int observed = maximum.load(std::memory_order_relaxed);
            while (now > observed
                   && !maximum.compare_exchange_weak(observed,
                                                     now,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed))
            {
            }
        }

        ~AnalysisConcurrencyGuard()
        {
            current.fetch_sub(1, std::memory_order_acq_rel);
        }

        AnalysisConcurrencyGuard(const AnalysisConcurrencyGuard&) = delete;
        AnalysisConcurrencyGuard& operator=(const AnalysisConcurrencyGuard&) = delete;

    private:
        std::atomic<int>& current;
        std::atomic<int>& maximum;
    };
}

//==============================================================================
AIEqualizerAudioProcessor::AIEqualizerAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     .withInput("Sidechain", juce::AudioChannelSet::mono(), false)),
      apvts(*this, nullptr, "Parameters", createParameters())
{
    // Fresh instances are schema-v1 and default to Surgical. States without
    // this root property are treated as pre-v1 and migrated to Legacy on load.
    apvts.state.setProperty("stateSchemaVersion", kCurrentStateSchemaVersion, nullptr);

    apvts.addParameterListener("phaseMode", this);
    apvts.addParameterListener("msMode", this);
    apvts.addParameterListener("oversamplingFactor", this);
    apvts.addParameterListener("qualityMode", this);
    // AI knobs that are applied to the engine ONLY inside updateEQFromParameters()
    // (setSensitivity/setStrength at ~3349/3354). Without listeners, moving these
    // knobs never set parametersNeedUpdate, so updateEQFromParameters() was not called
    // during steady playback and the new value was never pushed to the AI engine -
    // the Sensitivity knob appeared "dead" (amber bars / AI panel did not update live).
    // (aiEnabled is read live every block at ~1380, so it does not need a listener.)
    apvts.addParameterListener("aiSensitivity", this);
    apvts.addParameterListener("aiStrength", this);
    // Listen to band parameters to trigger IR regeneration for linear-phase mode
    for (int i = 0; i < maxBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        eqParameterIDs.push_back(prefix + "Freq");
        eqParameterIDs.push_back(prefix + "Gain");
        eqParameterIDs.push_back(prefix + "Q");
        eqParameterIDs.push_back(prefix + "Type");
        eqParameterIDs.push_back(prefix + "Enabled");
        eqParameterIDs.push_back(prefix + "Slope");
        eqParameterIDs.push_back(prefix + "CurveMode");

        // FIX: Also listen to Dynamic EQ parameters so that tweaking
        // threshold/ratio/attack/release from the GUI triggers parameterChanged()
        // and marks parametersNeedUpdate = true. Without this, updateEQFromParameters()
        // was never called when the user moved a dynamic EQ knob, so setBandParams()
        // never updated the SmoothedValues - causing the GR meter to freeze.
        eqParameterIDs.push_back(prefix + "DynMode");
        eqParameterIDs.push_back(prefix + "DynTrigger");
        eqParameterIDs.push_back(prefix + "DetectionMode");
        eqParameterIDs.push_back(prefix + "DetectorSource");
        eqParameterIDs.push_back(prefix + "SidechainFreq");
        eqParameterIDs.push_back(prefix + "SidechainQ");
        eqParameterIDs.push_back(prefix + "Threshold");
        eqParameterIDs.push_back(prefix + "Ratio");
        eqParameterIDs.push_back(prefix + "Attack");
        eqParameterIDs.push_back(prefix + "Release");
        eqParameterIDs.push_back(prefix + "Range");
        eqParameterIDs.push_back(prefix + "Knee");
    }
    eqParameterIDs.push_back("numActiveBands");
    eqParameterIDs.push_back("dynEqEnabled");
    eqParameterIDs.push_back("dynEqMix");
    for (const auto& id : eqParameterIDs)
        apvts.addParameterListener(id, this);

    // Initial curve refresh + LP IR build if needed later
    triggerLinearPhaseIRUpdate();

    if (auto* phaseValue = apvts.getRawParameterValue("phaseMode"))
        parameterChanged("phaseMode", phaseValue->load());
    if (auto* msValue = apvts.getRawParameterValue("msMode"))
        parameterChanged("msMode", msValue->load());
    if (auto* osValue = apvts.getRawParameterValue("oversamplingFactor"))
        parameterChanged("oversamplingFactor", osValue->load());

    // Initialize A/B/C/D slots with spread-out default frequencies.
    // BandState defaults all freqs to 1000 Hz — without this, switching to
    // an unused slot stacks every band on top of each other at 1 kHz.
    {
        auto initSlot = [](EQSlot& slot, const char* name) {
            slot.bands.fill(BandState());
            for (int i = 0; i < maxBands; ++i)
            {
                slot.bands[static_cast<size_t>(i)].frequency =
                    AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)];
                slot.bands[static_cast<size_t>(i)].sidechainFrequency =
                    AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)];
            }
            slot.name = name;
        };
        initSlot(slotA, "A");
        initSlot(slotB, "B");
        initSlot(slotC, "C");
        initSlot(slotD, "D");
    }

    for (auto& perQuality : semanticBandAssignments)
        perQuality.fill(-1);

    // Initialize preset manager
    presetManager = std::make_unique<PresetManager>(apvts);

    // Initialize history manager with APVTS reference
    historyManager.initialize(apvts, *this);

    // OSC parameter server created here but started in prepareToPlay
    // (starting in constructor crashes during VST3 plugin scan)
    oscParamServer = std::make_unique<OSCParameterServer>(apvts, 11100);

    // Start IR builder thread (RAII with std::thread)
    stopIRBuilder.store(false);
    irBuilderThread = std::thread([this]() {
        irBuilderThreadFunc();
    });

    // Start AI analysis thread (off-audio-thread)
    stopAIAnalysis.store(false);
    aiAnalysisThread = std::thread([this]() {
        aiAnalysisThreadFunc();
    });

    // Initialize band targets with defaults (will be updated on first parameter sync)
    for (int i = 0; i < maxBands; ++i)
    {
        targetBandFreq[static_cast<size_t>(i)] = 1000.0f;
        targetBandGain[static_cast<size_t>(i)] = 0.0f;
        targetBandQ[static_cast<size_t>(i)] = 1.0f;
        targetBandType[static_cast<size_t>(i)] = ParametricEQProcessor::Peak;
        targetBandCurveMode[static_cast<size_t>(i)] = kSurgicalCurveMode;
        targetBandEnabled[static_cast<size_t>(i)] = (i < 8);
        targetBandSolo[static_cast<size_t>(i)] = false;
    }
}

//==============================================================================
// IR Builder Thread Function (runs in background)
//==============================================================================
void AIEqualizerAudioProcessor::irBuilderThreadFunc()
{
    try
    {
    juce::dsp::FFT fft(LinearPhaseProcessor::fftOrder);
    const size_t fftSize = LinearPhaseProcessor::fftSize;
    const size_t halfSize = fftSize / 2;
    std::vector<std::complex<float>> freqDomain(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> timeDomain(fftSize, { 0.0f, 0.0f });
    std::vector<float> irBuf(fftSize, 0.0f);
    std::vector<float> magDB(halfSize, -120.0f);

    while (!stopIRBuilder.load())
    {
        irBuildEvent.wait(-1);

        // Check exit flag after wake
        if (stopIRBuilder.load())
            break;

        if (!eqCurveNeedsUpdate.load(std::memory_order_acquire))
            continue;

        // Debounce: wait until no new request has arrived for irBuildDebounceMs.
        // Short debounce (20ms) coalesces rapid parameter changes while still
        // allowing several IR updates per second during band drag.  The
        // latest-wins crossfade system in PartitionedConvolver ensures each
        // transition gets a full 4096-sample fade — no mid-fade pops.
        {
            int64_t lastRequest = irBuildRequestedAt.load(std::memory_order_acquire);
            while (!stopIRBuilder.load())
            {
                juce::Thread::sleep(irBuildDebounceMs);
                int64_t nowRequest = irBuildRequestedAt.load(std::memory_order_acquire);
                if (nowRequest == lastRequest)
                    break;  // stable: no new request during the sleep window
                lastRequest = nowRequest;
            }
            if (stopIRBuilder.load()) break;
        }

        if (!eqCurveNeedsUpdate.exchange(false))
            continue;

        const double sr = currentSampleRate.load(std::memory_order_relaxed);

        // SAFETY: Skip IR building if sample rate not yet initialized
        if (sr <= 0.0)
            continue;

        // The mailbox slots are fixed-size members, so there is no allocation
        // to wait for here any more.

        // Dynamic EQ magnitude is intentionally EXCLUDED from the IR.
        // Reason: dynamicEQProcessor.process() runs AFTER the convolver in LP mode,
        // applying biquad EQ + dynamic gain modulation per-sample.  If we also bake the
        // dynamic EQ's static magnitude into the IR, the EQ shape is applied TWICE:
        //   1. Through the convolver (slow: 20ms debounce + 93ms crossfade)
        //   2. Through the biquad (fast: 128-sample crossfade)
        // The timing mismatch between the two produces audible crackle when the user
        // adjusts threshold/ratio — the biquad reacts instantly while the IR lags behind.
        //
        // Industry standard (FabFilter Pro-Q, etc.): dynamic bands are minimum-phase
        // even in Linear Phase mode.  Only the parametric EQ gets the LP treatment.

        uint64_t versionStart = 0;
        uint64_t versionEnd = 0;

        do
        {
            versionStart = irCoeffVersion.load(std::memory_order_acquire);
            if (versionStart & 1u)
            {
                std::this_thread::yield(); // FIX BUG #3: Release CPU to writer thread
                continue; // writer in progress, retry
            }

            std::atomic_thread_fence(std::memory_order_acquire);

            std::fill(magDB.begin(), magDB.end(), -120.0f);

            // Use shadow processor (eqProcessorForIR) to avoid data race with audio thread.
            // Dynamic EQ is NOT included — it runs as biquad post-convolver (see comment above).

            for (size_t bin = 0; bin < halfSize; ++bin)
            {
                const float freq = static_cast<float>(bin) * static_cast<float>(sr)
                                   / static_cast<float>(LinearPhaseProcessor::fftSize);

                float mag = eqProcessorForIR.getMagnitudeForFrequency(freq, sr);

                magDB[bin] = juce::Decibels::gainToDecibels(mag, -120.0f);

            }

            versionEnd = irCoeffVersion.load(std::memory_order_acquire);
        } while (versionStart != versionEnd || (versionEnd & 1u));

        // Build IR in frequency domain
        std::fill(freqDomain.begin(), freqDomain.end(), std::complex<float>(0.0f, 0.0f));
        std::fill(timeDomain.begin(), timeDomain.end(), std::complex<float>(0.0f, 0.0f));
        std::fill(irBuf.begin(), irBuf.end(), 0.0f);

        for (size_t k = 0; k < magDB.size(); ++k)
        {
            float magLin = juce::Decibels::decibelsToGain(magDB[k]);

            // SAFETY: avoid vanishing bins that generate near-zero IR
            const float magFloor = (k == 0) ? 1.0f : 1.0e-4f; // keep DC at unity, others at -80 dB floor
            if (magLin < magFloor)
                magLin = magFloor;

            freqDomain[k] = { magLin, 0.0f };

            if (k > 0 && k < halfSize)
            {
                const size_t mirror = LinearPhaseProcessor::fftSize - k;
                freqDomain[mirror] = { magLin, 0.0f };
            }
        }

        fft.perform(freqDomain.data(), timeDomain.data(), true);

        //======================================================================
        // FIX #4: IR SCALING DOCUMENTATION AND STABILIZATION
        //======================================================================
        // The IR scaling pipeline has multiple stages that must work together:
        //
        // STAGE 1: IFFT Normalization (ifftScale)
        //   - JUCE FFT doesn't auto-normalize inverse FFT
        //   - Standard normalization: 1/N where N = FFT size
        //   - Reference: Smith, "Mathematics of the DFT", Chapter 7
        //
        // STAGE 2: REMOVED — see the note at the removal site below. It applied a
        //   hidden, unconditional level makeup that Linear Phase had and the other
        //   phase modes did not.
        //
        // STAGE 3: Final Safety Scaling (irScale, applied later)
        //   - Rescues IRs that are still too small after stages 1-2
        //   - Threshold: 1e-04 peak = -80dBFS (below noise floor)
        //   - Target: 0.1 peak = -20dBFS (reasonable working level)
        //   - Max boost: 1e6 (60dB) to prevent runaway scaling
        //
        // TOTAL GAIN BOUNDS:
        //   - Minimum IR peak after all stages: 0.1 (-20dBFS)
        //   - Maximum IR sample value: ±10.0 (clamped for safety)
        //======================================================================

        // STAGE 1: IFFT normalization
        // NOTE: JUCE's fft.perform(inverse=true) already applies 1/N scaling internally.
        // Do NOT apply an additional 1/N here - that was causing the IR to be ~N times too quiet.

        // STAGE 2 removed. It scaled the IR by 1/(mean linear magnitude over the
        // FFT bins), meaning to rescue curves that cut everywhere. FFT bins are
        // linearly spaced, so half of them sit above a quarter of the sample
        // rate: a high cut silences most of the bins, the mean collapses, and the
        // "compensation" made the filter LOUDER the more it cut. Measured through
        // the real builder, the boost tracked the compensation exactly — +2.05 dB
        // at a 20 kHz cut, +4.43 at 15 kHz, +7.23 at 10 kHz — so switching to
        // Linear Phase acted as a volume control.
        //
        // The measure could not be repaired, only removed: a mean over bins
        // cannot distinguish "the whole curve is quieter" from "this part of the
        // spectrum is gone", and those need opposite treatment. Level makeup
        // already exists where the user can see and disable it, as the Auto Gain
        // parameter, which measures pre/post RMS and is bounded to +/-12 dB.
        // STAGE 3 below stays: it is a numerical guard for degenerate near-silent
        // IRs (peak under -40 dBFS), not a musical level decision.
        for (size_t n = 0; n < LinearPhaseProcessor::fftSize; ++n)
            irBuf[n] = timeDomain[n].real();

        // Center (circular shift to create zero-phase / linear-phase IR)
        //
        // BUG FIX: The previous rotation by halfSize (4096) placed the IR peak at index 4096,
        // which is then EXCLUDED when we load only the first irSize=4096 samples (indices 0..4095).
        // The result: only the tail of the IR was loaded, causing near-zero volume in linear phase mode.
        //
        // CORRECT rotation: shift so the peak lands at irSize/2 = 2048 (center of the loaded window).
        // The IFFT of a real-symmetric spectrum has its peak at index 0.
        // To move it to position irSize/2 within the first irSize samples, we rotate by:
        //   rotateOffset = fftSize - irSize/2 = 8192 - 2048 = 6144
        // After rotate, element that was at index 6144 becomes index 0,
        // so index 0 (the peak) ends up at position fftSize-6144 = 2048. ✓
        //
        // Reference: Oppenheim & Schafer, "Discrete-Time Signal Processing", Chapter 5
        {
            const size_t peakTargetIndex = LinearPhaseProcessor::irSize / 2;              // 2048
            const size_t rotateOffset    = LinearPhaseProcessor::fftSize - peakTargetIndex; // 6144
            std::rotate(irBuf.begin(), irBuf.begin() + static_cast<long>(rotateOffset), irBuf.end());
        }

        //======================================================================
        // HANN WINDOW APPLICATION
        //======================================================================
        // Purpose: Smooth time-domain truncation to reduce frequency ripple
        // The Hann (raised cosine) window provides -31dB sidelobe suppression
        //
        // Gain compensation: Hann window has coherent gain of 0.5.
        // We compensate with hannGainComp = 2.0f to preserve correct output level,
        // consistent with LinearPhaseProcessor::updateImpulseResponse().
        // Do NOT normalize to maxAbs=1.0 - this would destroy EQ gain information!
        //======================================================================
        // hannGainComp = 1.0: Hann window applied to IR for anti-ringing only, no gain compensation needed.
        // See LinearPhaseProcessor.cpp for full explanation.
        constexpr float hannGainComp = 1.0f;
        for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
        {
            const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(n)
                                                    / static_cast<float>(LinearPhaseProcessor::irSize - 1)));
            irBuf[n] *= (w * hannGainComp);
        }

        //======================================================================
        // SAFETY CLAMPING
        //======================================================================
        // Final sanity check to prevent audio blowup
        // Limits: ±10.0 linear (~+20dBFS) is extremely loud but prevents DAW crash
        // NaN/Inf samples are zeroed (indicates numerical instability upstream)
        //======================================================================
        for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
        {
            if (std::isnan(irBuf[n]) || std::isinf(irBuf[n]))
                irBuf[n] = 0.0f;
            else
                irBuf[n] = juce::jlimit(-10.0f, 10.0f, irBuf[n]);
        }

        // Final safety: abort loading if any sample is not finite
        bool finiteIR = true;
        for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
        {
            if (!std::isfinite(irBuf[n]))
            {
                finiteIR = false;
                break;
            }
        }
        if (!finiteIR)
            continue;

        // Measure IR magnitude for STAGE 3 decision
        {
            float maxAbs = 0.0f;
            for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
                maxAbs = std::max(maxAbs, std::abs(irBuf[n]));

            constexpr float minReasonableIR = 1e-02f;   // -40 dBFS threshold
            constexpr float targetIR = 0.1f;            // -20 dBFS target
            constexpr float maxScaleBoost = 10.0f;      // +20 dB max (was 1e4 = +80dB — caused explosions under stress)

            float irScale = 1.0f;
            if (maxAbs > 0.0f && maxAbs < minReasonableIR)
            {
                irScale = targetIR / maxAbs;
                irScale = std::min(irScale, maxScaleBoost);
            }

            // Apply STAGE 3 scaling
            std::vector<float> scaledIR(LinearPhaseProcessor::irSize);
            for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
                scaledIR[n] = irBuf[n] * irScale;

            // Pre-partition the time-domain IR here (builder thread, NOT audio thread).
            // This avoids the expensive IFFT + 32x FFT that was previously done on the audio thread.
            // The packed format is numParts * fftPartSize * 2 floats = 16384 (same size as old freqBuf).
            std::vector<float> freqBuf(PartitionedConvolver::numParts * PartitionedConvolver::fftPartSize * 2, 0.0f);
            PartitionedConvolver::buildPackedPartitions(scaledIR.data(), scaledIR.size(), freqBuf.data());

            // Publish into the ownership mailbox. The producer can only claim a
            // slot that is FREE, or reclaim one still READY (an IR the audio
            // thread never got to) — never one in READING. That is the whole
            // point: the old protocol picked a write index by testing
            // readingIndex, which the consumer had not set yet.
            PackedIRPayload packedIR {};
            jassert(freqBuf.size() == packedIR.size());
            std::copy(freqBuf.begin(), freqBuf.end(), packedIR.begin());

            const bool published = pendingFreqIR.publish(packedIR);
            jassert(published); // four slots vs one reader and one writer
            juce::ignoreUnused(published);
        }
    }
    }
    catch (const std::exception& e)
    {
        AIEQ_LOG_ERROR("IR builder thread exception: " + juce::String(e.what()));
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("IR builder thread unknown exception");
    }
}

//==============================================================================
// AI Analysis Thread Function (runs off the audio thread)
//==============================================================================
AIEqualizerAudioProcessor::FrontEndDiagnostics
AIEqualizerAudioProcessor::getAIFrontEndDiagnostics() const noexcept
{
    // Out-of-line on purpose — see the header note (test/plugin layout divergence).
    FrontEndDiagnostics d;
    d.frames = aiFrontEndFrames.load(std::memory_order_relaxed);
    d.meanMs = aiFrontEndMeanNs.load(std::memory_order_relaxed) / 1.0e6;
    d.maxMs  = static_cast<double>(aiFrontEndMaxNs.load(std::memory_order_relaxed)) / 1.0e6;
    d.droppedSamples = aiFrontEndDroppedSamples.load(std::memory_order_relaxed);
    d.discontinuities = aiFrontEndDiscontinuities.load(std::memory_order_relaxed);
    return d;
}

namespace
{
// File-static so no data member is added to AIEqualizerAudioProcessor: see the
// layout note above getAIFrontEndDiagnostics() in the header. The target
// pointer is compared, never dereferenced, so a stale value from a destroyed
// processor is harmless — it simply never matches the live one.
std::mutex& aiAnalysisObserverMutex()
{
    static std::mutex m;
    return m;
}
AIEqualizerAudioProcessor::AIAnalysisObserverForTests gAIAnalysisObserver;
const AIEqualizerAudioProcessor* gAIAnalysisObserverTarget = nullptr;
} // namespace

void AIEqualizerAudioProcessor::setAIAnalysisObserverForTests(
    AIAnalysisObserverForTests observer) noexcept
{
    std::lock_guard<std::mutex> lock(aiAnalysisObserverMutex());
    gAIAnalysisObserver = std::move(observer);
    gAIAnalysisObserverTarget = gAIAnalysisObserver ? this : nullptr;
}

juce::int64 AIEqualizerAudioProcessor::getAIHeadlessAnalysisCountForTests() const noexcept
{
    // Same out-of-line layout rule as getAIFrontEndDiagnostics().
    return aiHeadlessAnalyses.load(std::memory_order_relaxed);
}

void AIEqualizerAudioProcessor::analyzeSpectrumSerialized(const std::vector<float>& spectrum,
                                                          bool force)
{
    // All productive AIEngine::analyzeSpectrum() entry points must pass here:
    // live AI thread, transport-stop forced reanalysis, and capture analysis.
    // The mutex is deliberately outside processBlock; the audio thread only
    // enqueues spectra and never waits on this lock.
    aiAnalysisCallAttemptsForTests.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard<std::mutex> lock(aiAnalysisMutex);
    AnalysisConcurrencyGuard guard(aiConcurrentAnalyses, aiMaxConcurrentAnalyses);

    if (aiAnalysisBlockForTests.load(std::memory_order_acquire))
    {
        aiAnalysisEnteredForTests.fetch_add(1, std::memory_order_acq_rel);
        while (aiAnalysisBlockForTests.load(std::memory_order_acquire)
               && !stopAIAnalysis.load(std::memory_order_acquire))
        {
            juce::Thread::yield();
        }
    }

    aiEngine.analyzeSpectrum(spectrum, force);
}

std::optional<AIEQPerceptual::SpectralContext>
AIEqualizerAudioProcessor::getSpectralContextSnapshot() noexcept
{
    auto view = spectralContextMailbox.acquireLatest();
    if (!view)
        return std::nullopt;

    AIEQPerceptual::SpectralContext copy = *view.payload;
    spectralContextMailbox.release(view);
    return copy;
}

void AIEqualizerAudioProcessor::aiAnalysisThreadFunc()
{
    try
    {
        // Persistent last spectrum: used both for normal headless analysis and
        // transport-stop forced re-analysis. Sized once on the worker thread.
        std::vector<float> spectrum(aiSpectrumBins, PerceptualFrontEnd::kMinDb);
        bool haveLastSpectrum = false;

        // Cadence is scheduled on the actual end-sample position represented by
        // each frontend frame. The first 4096-point frame ends at sample 4096;
        // later frames advance by the 2048-sample hop. This preserves the legacy
        // ~10 Hz input cadence as closely as the frontend frame grid allows.
        juce::int64 frontEndFrameEndSample = 0;
        juce::int64 nextAnalysisSample = 0;
        bool timelinePrimed = false;
        bool frontEndSuspended = false;

        using FixedSpectrum = std::array<float, aiSpectrumBins>;
        std::array<FixedSpectrum, 4> selectedFrames {};
        // End-sample position of each selected frame, carried alongside the
        // payload so the test observer can prove input provenance and not just
        // input count.
        std::array<juce::int64, 4> selectedEndSamples {};

        auto resetHeadlessStream = [&]()
        {
            aiFrontEnd.reset();
            aiEngine.resetLiveDetectionState();
            aiProblemsChanged.store(true, std::memory_order_release);
            haveLastSpectrum = false;
            frontEndFrameEndSample = 0;
            nextAnalysisSample = 0;
            timelinePrimed = false;
            aiPendingReanalysis.store(false, std::memory_order_release);
            aiFrontEndDiscontinuities.fetch_add(1, std::memory_order_relaxed);
        };

        auto discardQueuedAudio = [&]()
        {
            bool discarded = false;
            while (aiFrontEndFifo.pullAudioBlock(aiFrontEndScratch.data(),
                                                  aiFrontEndScratch.size()) > 0)
            {
                discarded = true;
                if (stopAIAnalysis.load(std::memory_order_acquire))
                    break;
            }
            return discarded;
        };

        while (!stopAIAnalysis.load(std::memory_order_acquire))
        {
            bool didWork = false;

            // T5.2: suspended only when nothing needs analysis. Assist being
            // off is no longer sufficient - Semantic still needs the context.
            const bool suspendFrontEnd =
                aiOfflineRenderActive.load(std::memory_order_acquire)
                || !analysisNeeded();
            const bool discontinuity =
                aiFrontEndDiscontinuityPending.exchange(false, std::memory_order_acq_rel);

            // Offline/disabled periods intentionally stop the producer. Reset
            // once when entering suspension and keep draining residual audio.
            // Repeated skipped blocks may re-arm the sticky marker, but while
            // already suspended there is no new continuity to invalidate.
            if (suspendFrontEnd)
            {
                didWork |= discardQueuedAudio();
                if (!frontEndSuspended)
                    resetHeadlessStream();
                frontEndSuspended = true;
                if (!didWork)
                    aiSpectrumEvent.wait(5);
                continue;
            }

            // Active-stream FIFO overflow is fail-closed: throw away all queued
            // audio and reset overlap/history before accepting another detector
            // frame. If another overflow happens while draining, the producer
            // re-arms the sticky marker and the next loop repeats the reset.
            if (discontinuity)
            {
                didWork |= discardQueuedAudio();
                resetHeadlessStream();
                frontEndSuspended = false;
                if (!didWork)
                    aiSpectrumEvent.wait(1);
                continue;
            }

            frontEndSuspended = false;

            // EC-001/B4: the AI-owned PerceptualFrontEnd is the only continuous
            // detector source. The lifecycle busy region contains frontend work
            // and fixed-size copies only; AIEngine runs after busy is cleared.
            if (aiFrontEndReady.load(std::memory_order_seq_cst))
            {
                const size_t pulled = aiFrontEndFifo.pullAudioBlock(aiFrontEndScratch.data(),
                                                                    aiFrontEndScratch.size());
                if (pulled > 0)
                {
                    didWork = true;
                    size_t selectedCount = 0;

                    aiFrontEndDrainBusy.store(true, std::memory_order_seq_cst);
                    if (aiFrontEndReady.load(std::memory_order_seq_cst))
                    {
                        aiFrontEnd.pushMono(
                            aiFrontEndScratch.data(),
                            static_cast<int>(pulled),
                            [this, &frontEndFrameEndSample, &nextAnalysisSample,
                             &timelinePrimed, &selectedFrames, &selectedEndSamples,
                             &selectedCount]
                            (const PerceptualFrontEnd::Frame& f)
                            {
#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2
                                const double v2sr = getSampleRate();
                                const auto mel = aieq::melBandsFromDb(
                                    f.rawDb.data(), static_cast<int>(f.rawDb.size()), v2sr, 64);
                                aiEngine.pushMotoreV2Frame(mel.data());
#endif
                                // T5.1: every frame feeds the tonal context, not
                                // only the ones the detector cadence selects -
                                // the statistics need the whole stream. Welford
                                // over ~122 bands, no allocation.
                                aiSpectralContextAccumulator.pushFrame(f.bandDbFused, f.lfValid);

                                const int interval = aiAnalysisIntervalSamples;
                                if (interval <= 0)
                                    return;

                                if (!timelinePrimed)
                                {
                                    frontEndFrameEndSample = PerceptualFrontEnd::kFftSize;
                                    nextAnalysisSample = interval;
                                    timelinePrimed = true;
                                }
                                else
                                {
                                    frontEndFrameEndSample += PerceptualFrontEnd::kHopSize;
                                }

                                if (frontEndFrameEndSample < nextAnalysisSample)
                                    return;

                                // Advance the schedule to the first target after
                                // this frame. At supported rates this is normally
                                // one tick; the loop is robust to unusual rates.
                                do
                                    nextAnalysisSample += interval;
                                while (nextAnalysisSample <= frontEndFrameEndSample);

                                if (selectedCount >= selectedFrames.size())
                                    return;

                                selectedEndSamples[selectedCount] = frontEndFrameEndSample;
                                auto& dst = selectedFrames[selectedCount++];
                                std::copy(f.rawDb.begin(), f.rawDb.end(), dst.begin());
                            });
                    }
                    aiFrontEndDrainBusy.store(false, std::memory_order_seq_cst);

                    // T5.1: publish on the detection cadence, and deliberately
                    // after the busy flag is cleared - snapshot() allocates, and
                    // prepareToPlay waits on that flag.
                    if (selectedCount > 0)
                        spectralContextMailbox.publish(aiSpectralContextAccumulator.snapshot());

                    aiFrontEndFrames.store(aiFrontEnd.framesProcessed(), std::memory_order_relaxed);
                    aiFrontEndMeanNs.store(aiFrontEnd.meanFrameNs(), std::memory_order_relaxed);
                    aiFrontEndMaxNs.store(aiFrontEnd.maxFrameNs(), std::memory_order_relaxed);

                    for (size_t i = 0; i < selectedCount; ++i)
                    {
                        std::copy(selectedFrames[i].begin(), selectedFrames[i].end(), spectrum.begin());
                        haveLastSpectrum = true;
                        analyzeSpectrumSerialized(spectrum, /*force=*/false);
                        const auto seq =
                            aiHeadlessAnalyses.fetch_add(1, std::memory_order_relaxed) + 1;
                        {
                            std::lock_guard<std::mutex> obsLock(aiAnalysisObserverMutex());
                            if (gAIAnalysisObserver && gAIAnalysisObserverTarget == this)
                                gAIAnalysisObserver(seq, selectedEndSamples[i], spectrum);
                        }
                        aiProblemsChanged.store(true, std::memory_order_release);

                        if (stopAIAnalysis.load(std::memory_order_acquire))
                            break;
                    }
                }
            }

            // Transport-stop re-analysis: sensitivity/strength changes force one
            // evaluation of the last headless spectrum. Never bridge through the
            // GUI analyzer or a legacy spectrum queue.
            if (aiPendingReanalysis.exchange(false, std::memory_order_acq_rel))
            {
                if (haveLastSpectrum
                    && !aiOfflineRenderActive.load(std::memory_order_acquire)
                    && aiEngine.isEnabled())
                {
                    analyzeSpectrumSerialized(spectrum, /*force=*/true);
                    aiProblemsChanged.store(true, std::memory_order_release);
                    didWork = true;
                }
            }

            if (!didWork)
                aiSpectrumEvent.wait(5);
        }
    }
    catch (const std::exception& e)
    {
        AIEQ_LOG_ERROR("AI analysis thread exception: " + juce::String(e.what()));
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("AI analysis thread unknown exception");
    }
}

void AIEqualizerAudioProcessor::quiesceBackgroundWorkersForLifecycle()
{
    // Cold path only: prevent the AI worker from entering/re-entering the
    // perceptual front-end while lifecycle-owned storage is being rebuilt.
    aiFrontEndReady.store(false, std::memory_order_seq_cst);

    // Stop the continuously running workers and wake any wait. The AI test
    // barrier also observes stopAIAnalysis, so a test-held analysis cannot
    // deadlock prepare/release.
    stopAIAnalysis.store(true, std::memory_order_release);
    aiSpectrumEvent.signal();
    stopIRBuilder.store(true, std::memory_order_release);
    irBuildEvent.signal();

    if (aiAnalysisThread.joinable())
        aiAnalysisThread.join();
    if (irBuilderThread.joinable())
        irBuilderThread.join();

    // Capture analysis is a bounded one-shot rather than a persistent worker.
    // Join the owned thread, then also cover the synchronous/non-message-thread
    // path by waiting for its in-flight flag to clear.
    if (captureAnalysisThread.joinable())
        captureAnalysisThread.join();
    while (captureAnalysisInFlight.load(std::memory_order_acquire))
        juce::Thread::yield();

    // Prevent forced re-analysis requests from the previous prepare lifetime
    // leaking into the next one. aiFrontEndFifo is re-prepared later while the
    // sole consumer is joined.
    aiPendingReanalysis.store(false, std::memory_order_relaxed);
    aiOfflineRenderActive.store(false, std::memory_order_relaxed);
    aiFrontEndDiscontinuityPending.store(false, std::memory_order_relaxed);
    aiFrontEndDroppedSamples.store(0, std::memory_order_relaxed);
    aiFrontEndDiscontinuities.store(0, std::memory_order_relaxed);
    aiHeadlessAnalyses.store(0, std::memory_order_relaxed);
    aiFrontEndDrainBusy.store(false, std::memory_order_seq_cst);
}

void AIEqualizerAudioProcessor::restartBackgroundWorkersAfterLifecycle()
{
    // All DSP/front-end/AI state must already be fully prepared before this
    // function is called. Start workers while processorReady is still false;
    // processBlock is opened only after both workers exist.
    if (!irBuilderThread.joinable())
    {
        stopIRBuilder.store(false, std::memory_order_release);
        irBuilderThread = std::thread([this]() { irBuilderThreadFunc(); });
    }

    aiFrontEndReady.store(true, std::memory_order_seq_cst);
    if (!aiAnalysisThread.joinable())
    {
        stopAIAnalysis.store(false, std::memory_order_release);
        aiAnalysisThread = std::thread([this]() { aiAnalysisThreadFunc(); });
    }

    // A request may have been armed while the IR builder was intentionally
    // stopped. Wake the restarted worker after its thread exists.
    if (eqCurveNeedsUpdate.load(std::memory_order_acquire))
        irBuildEvent.signal();
}

AIEqualizerAudioProcessor::~AIEqualizerAudioProcessor()
{
    cancelPendingUpdate();
    // Request complete background quiescence before member teardown.
    quiesceBackgroundWorkersForLifecycle();

    // Remove parameter listeners
    apvts.removeParameterListener("phaseMode", this);
    apvts.removeParameterListener("msMode", this);
    apvts.removeParameterListener("oversamplingFactor", this);
    apvts.removeParameterListener("qualityMode", this);
    for (const auto& id : eqParameterIDs)
        apvts.removeParameterListener(id, this);

    // std::thread automatically requests stop and joins on destruction (RAII)
    // Signal the event to wake the thread so they can check stop flags
    irBuildEvent.signal();
    aiSpectrumEvent.signal();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AIEqualizerAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    // Global dry/wet mix (0 = dry, 100 = fully processed)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dryWet", 1}, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"autoGain", 1}, "Auto Gain", false));

    // Quality / latency mode: 0 = Zero Latency, 1 = High Quality (lookahead on)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"qualityMode", 1}, "Quality Mode",
        juce::StringArray{"Zero Latency", "High Quality"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"phaseMode", 1}, "Processing Mode",
        juce::StringArray{"Zero Latency", "Natural Phase", "Linear Phase"}, 0));

    // Mid/Side processing mode
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"msMode", 1}, "M/S Mode",
        juce::StringArray{"Stereo", "Mid Only", "Side Only", "M/S Linked"}, 0));

    // Oversampling factor
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"oversamplingFactor", 1}, "Oversampling",
        juce::StringArray{"Off", "2x", "4x", "Auto"}, 0));

    // AI controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"aiSensitivity", 1}, "AI Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"aiStrength", 1}, "AI Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"aiEnabled", 1}, "AI Enabled", true));

    // Source Profile (as choice parameter)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"sourceProfile", 1}, "Source Profile",
        juce::StringArray{"Generic", "Vocals", "Drums", "Bass", "Synth", "Master", "EDM"}, 0));

    // Analyzer display options
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"showPreSpectrum", 1}, "Show Pre-EQ Spectrum", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"showPostSpectrum", 1}, "Show Post-EQ Spectrum", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"showDeltaSpectrum", 1}, "Show Delta Spectrum", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"analyzerResolution", 1}, "Analyzer FFT",
        juce::StringArray{"Low (1024)", "Medium (2048)", "High (4096)", "Max (8192)"}, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"analyzerSpeed", 1}, "Analyzer Speed",
        juce::StringArray{"Fast", "Medium", "Slow"}, 1));

    // Analyzer slope (visual only): 0=Flat, 1=3dB, 2=4.5dB, 3=6dB
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"analyzerSlope", 1}, "Analyzer Slope",
        juce::StringArray{"Flat", "3 dB/oct", "4.5 dB/oct", "6 dB/oct"}, 2)); // default 4.5

    // Peak-hold visualization controls (visual only)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"showPeakHold", 1}, "Show Peak Hold", false)); // OFF by default
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"analyzerPeakHold", 1}, "Analyzer Peak Hold",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.1f), 2.0f)); // seconds
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"analyzerPeakDecay", 1}, "Analyzer Peak Decay",
        juce::NormalisableRange<float>(1.0f, 60.0f, 0.5f), 20.0f)); // dB/sec

    // Continuous spectrum tilt (visual only) — adjustable via drag widget
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"spectrumTilt", 1}, "Spectrum Tilt",
        juce::NormalisableRange<float>(0.0f, 8.0f, 0.1f), 4.5f)); // dB/oct, default 4.5

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"pianoRollOverlay", 1}, "Piano Roll Overlay", false));

    // Accessibility: High-contrast mode
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"highContrastMode", 1}, "High Contrast Mode", false));

    // User Learning privacy
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"learningEnabled", 1}, "User Learning Enabled", true));

    // Number of active bands (1-24)
    juce::StringArray bandChoices;
    for (int i = 1; i <= AIEqualizerAudioProcessor::maxBands; ++i)
        bandChoices.add(juce::String(i));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"numActiveBands", 1}, "Number of Bands",
        bandChoices, 7)); // default 8 (index 7)

    // EQ Bands (24 bands)
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Freq", 1}, "Band " + juce::String(i + 1) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f),
            AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Gain", 1}, "Band " + juce::String(i + 1) + " Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Q", 1}, "Band " + juce::String(i + 1) + " Q",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));

        // Filter type (per-band choice)
        int defaultType = 2; // Peak
        if (i == 0)
            defaultType = 1; // Low Shelf
        else if (i == maxBands - 1)
            defaultType = 3; // High Shelf

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "Type", 1}, "Band " + juce::String(i + 1) + " Type",
            juce::StringArray{"Low Cut", "Low Shelf", "Peak", "High Shelf", "High Cut", "Notch", "Band Pass", "Vintage Low Shelf", "Vintage High Shelf"},
            defaultType));

        const bool enabledDefault = (i < 8);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{prefix + "Enabled", 1}, "Band " + juce::String(i + 1) + " Enabled", enabledDefault));

        // Solo (per-band)
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{prefix + "Solo", 1}, "Band " + juce::String(i + 1) + " Solo", false));

        // Slope (per-band, LowCut/HighCut only)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "Slope", 1}, "Band " + juce::String(i + 1) + " Slope",
            juce::StringArray{"12 dB/oct", "24 dB/oct", "48 dB/oct"}, 0));

        //----------------------------------------------------------------------
        // DYNAMIC EQ Parameters (FabFilter Pro-Q / TDR Nova style)
        //----------------------------------------------------------------------
        // Dynamic mode: 0=Off, 1=Compress, 2=Expand, 3=Gate
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "DynMode", 1}, "Band " + juce::String(i + 1) + " Dynamic",
            juce::StringArray{"Off", "Compress", "Expand", "Gate"}, 0));

        // Threshold (-60 to 0 dB)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Threshold", 1}, "Band " + juce::String(i + 1) + " Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -20.0f));

        // Ratio (1:1 to 20:1)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Ratio", 1}, "Band " + juce::String(i + 1) + " Ratio",
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 2.0f));

        // Attack (0.1 to 500 ms)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Attack", 1}, "Band " + juce::String(i + 1) + " Attack",
            juce::NormalisableRange<float>(0.1f, 500.0f, 0.1f, 0.3f), 10.0f));

        // Release (1 to 2000 ms)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Release", 1}, "Band " + juce::String(i + 1) + " Release",
            juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.3f), 100.0f));

        // Range (max gain change, 0 to 48 dB)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Range", 1}, "Band " + juce::String(i + 1) + " Range",
            juce::NormalisableRange<float>(0.0f, 48.0f, 0.1f), 24.0f));

        // Knee (0 to 24 dB, 0 = hard knee)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Knee", 1}, "Band " + juce::String(i + 1) + " Knee",
            juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 6.0f));
    }

    //--------------------------------------------------------------------------
    // Global Dynamic EQ controls
    //--------------------------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"dynEqEnabled", 1}, "Dynamic EQ Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dynEqMix", 1}, "Dynamic EQ Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"dynAutoMakeup", 1}, "Dynamic Auto Makeup", false));

    // D1 exposure: append-only host surface addition, so existing parameter
    // indices stay stable for old sessions/automation. Default OFF = the engine
    // stays a bit-transparent no-op (contract-tested).
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"dynamicCorrections", 2}, "Dynamic AI Cuts", false));

    // Append-only host surface: never insert these inside the per-band block,
    // otherwise every parameter after band 0 would change host index.
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        const auto prefix = "band" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "CurveMode", 2},
            "Band " + juce::String(i + 1) + " Curve Mode",
            juce::StringArray{"Legacy", "Surgical"},
            kSurgicalCurveMode));
    }

    // Append-only dynamic trigger surface. DynMode deliberately retains its
    // historical four host values; value 3 is interpreted as Expand+Below and
    // overrides this new parameter so legacy automation remains deterministic.
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        const auto prefix = "band" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "DynTrigger", 2},
            "Band " + juce::String(i + 1) + " Dynamic Trigger",
            juce::StringArray{"Above", "Below"},
            DynamicEQProcessor::TriggerSide_Above));
    }

    // Append-only detector surface. Keep these four complete per-band blocks
    // at the host-surface tail; inserting them into the historical band group
    // would renumber existing automation parameters.
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        const auto prefix = "band" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "DetectionMode", 2},
            "Band " + juce::String(i + 1) + " Detection Mode",
            juce::StringArray{"Peak", "RMS"},
            DynamicEQProcessor::DetectionMode_RMS));
    }

    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        const auto prefix = "band" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "DetectorSource", 2},
            "Band " + juce::String(i + 1) + " Detector Source",
            juce::StringArray{"Internal Wideband", "Internal Filtered",
                              "External Wideband", "External Filtered"},
            DynamicEQProcessor::DetectorSource_InternalWideband));
    }

    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        const auto prefix = "band" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "SidechainFreq", 2},
            "Band " + juce::String(i + 1) + " Sidechain Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f),
            AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)]));
    }

    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        const auto prefix = "band" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "SidechainQ", 2},
            "Band " + juce::String(i + 1) + " Sidechain Q",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));
    }

    return {params.begin(), params.end()};
}

//==============================================================================
void AIEqualizerAudioProcessor::cacheParameterPointers()
{
    if (parametersCached.load())
        return;

    for (int i = 0; i < maxBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        cachedParams[i].freq = apvts.getRawParameterValue(prefix + "Freq");
        cachedParams[i].gain = apvts.getRawParameterValue(prefix + "Gain");
        cachedParams[i].q = apvts.getRawParameterValue(prefix + "Q");
        cachedParams[i].type = apvts.getRawParameterValue(prefix + "Type");
        cachedParams[i].enabled = apvts.getRawParameterValue(prefix + "Enabled");
        cachedParams[i].solo = apvts.getRawParameterValue(prefix + "Solo");
        cachedParams[i].dynMode = apvts.getRawParameterValue(prefix + "DynMode");
        cachedParams[i].dynTrigger = apvts.getRawParameterValue(prefix + "DynTrigger");
        cachedParams[i].detectionMode = apvts.getRawParameterValue(prefix + "DetectionMode");
        cachedParams[i].detectorSource = apvts.getRawParameterValue(prefix + "DetectorSource");
        cachedParams[i].sidechainFreq = apvts.getRawParameterValue(prefix + "SidechainFreq");
        cachedParams[i].sidechainQ = apvts.getRawParameterValue(prefix + "SidechainQ");
        cachedParams[i].dynThreshold = apvts.getRawParameterValue(prefix + "Threshold");
        cachedParams[i].dynRatio = apvts.getRawParameterValue(prefix + "Ratio");
        cachedParams[i].dynAttack = apvts.getRawParameterValue(prefix + "Attack");
        cachedParams[i].dynRelease = apvts.getRawParameterValue(prefix + "Release");
        cachedParams[i].dynKnee = apvts.getRawParameterValue(prefix + "Knee");
        cachedParams[i].dynRange = apvts.getRawParameterValue(prefix + "Range");
        cachedParams[i].slope = apvts.getRawParameterValue(prefix + "Slope");
        cachedParams[i].curveMode = apvts.getRawParameterValue(prefix + "CurveMode");
    }

    cachedOutputGain = apvts.getRawParameterValue("outputGain");
    cachedDryWet = apvts.getRawParameterValue("dryWet");
    cachedAutoGain = apvts.getRawParameterValue("autoGain");
    cachedDynamicCorrections = apvts.getRawParameterValue("dynamicCorrections");
    cachedDynEqEnabled = apvts.getRawParameterValue("dynEqEnabled");
    cachedNumActiveBands = apvts.getRawParameterValue("numActiveBands");
    cachedBypass = apvts.getRawParameterValue("bypass");
    cachedQualityMode = apvts.getRawParameterValue("qualityMode");
    cachedPhaseModeParam = apvts.getRawParameterValue("phaseMode");
    cachedMSModeParam = apvts.getRawParameterValue("msMode");
    cachedOversamplingParam = apvts.getRawParameterValue("oversamplingFactor");
    cachedAnalyzerResolution = apvts.getRawParameterValue("analyzerResolution");
    cachedAnalyzerSpeed = apvts.getRawParameterValue("analyzerSpeed");
    cachedAIEnabled = apvts.getRawParameterValue("aiEnabled");
    cachedSourceProfile = apvts.getRawParameterValue("sourceProfile");
    cachedShowPostSpectrum = apvts.getRawParameterValue("showPostSpectrum");
    cachedAISensitivity = apvts.getRawParameterValue("aiSensitivity");
    cachedAIStrength = apvts.getRawParameterValue("aiStrength");
    cachedDynEqMix = apvts.getRawParameterValue("dynEqMix");
    cachedDynAutoMakeup = apvts.getRawParameterValue("dynAutoMakeup");

    parametersCached.store(true);
}

//==============================================================================
void AIEqualizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    processorReady.store(false, std::memory_order_release);
    cancelPendingUpdate();

    // EC-005: lifecycle transition owns all mutable DSP/AI storage from here
    // until restartBackgroundWorkersAfterLifecycle(). No background worker may
    // observe partially re-prepared state.
    quiesceBackgroundWorkersForLifecycle();
    linearIRTestOverridePinned.store(false, std::memory_order_release);

    cacheParameterPointers();

    // FIX 3: Use atomic store
    currentSampleRate.store(sampleRate);

    // Known limitation: LinearPhaseProcessor uses a fixed irSize=4096 / fftSize=8192.
    // At 96 kHz the latency is ~42 ms and frequency resolution halves (2.34 Hz/bin vs 1.17 Hz/bin at 44.1 kHz).
    // At 192 kHz latency is ~21 ms but resolution degrades further.
    // This is acceptable for current release; adaptive irSize is planned for a future version.
    if (sampleRate > 48001.0)
        AIEQ_LOG_WARNING("Sample rate " + juce::String(sampleRate) + " Hz: linear phase IR resolution "
                         "is reduced (fixed irSize=4096). Latency = "
                         + juce::String(static_cast<int>((LinearPhaseProcessor::usePartitioned
                             ? LinearPhaseProcessor::partSize : LinearPhaseProcessor::hopSize) * 1000.0 / sampleRate)) + " ms.");
    currentBlockSize = samplesPerBlock;
    const int mainInputChannels = getChannelCountOfBus(true, 0);
    preparedNumInputChannels = mainInputChannels;
    preallocatedMaxSamples = juce::jmax(samplesPerBlock * 4, 32768);
    blockClampEvents.store(0, std::memory_order_relaxed);
    dryBuffer.setSize(mainInputChannels, preallocatedMaxSamples, false, true, false);
    dryBuffer.clear();
    // Dry/wet delay lines for phase-aligned bypass crossfade (Maximum Latency Padding)
    dryDelayBufferSize = preallocatedMaxSamples + 8192;
    dryDelayBuffer.setSize(mainInputChannels, dryDelayBufferSize, false, true, false);
    dryDelayBuffer.clear();
    dryDelayWritePos = 0;
    dryDelayLength = 0;
    wetPaddingBufferSize = preallocatedMaxSamples + 8192;
    wetPaddingDelayBuffer.setSize(mainInputChannels, wetPaddingBufferSize, false, true, false);
    wetPaddingDelayBuffer.clear();
    wetPaddingWritePos = 0;
    wetPaddingDelaySamples = 0;
    // Cat 2 Fix: wet padding smoothing state
    wetPadLastSamples = 0;
    wetPadRampStart   = 0;
    wetPadRampActive  = false;
    phaseTransitionBuffer.setSize(mainInputChannels, preallocatedMaxSamples, false, true, false);
    phaseTransitionBuffer.clear();
    oversamplingTransitionBuffer.setSize(mainInputChannels, preallocatedMaxSamples, false, true, false);
    oversamplingTransitionBuffer.clear();
    msModeTransitionBuffer.setSize(mainInputChannels, preallocatedMaxSamples, false, true, false);
    msModeTransitionBuffer.clear();
    // Pre-allocate LP first-load crossfade buffer — NEVER allocate on audio thread
    lpFirstLoadFallbackBuf.setSize(mainInputChannels, preallocatedMaxSamples, false, true, false);
    lpFirstLoadFallbackBuf.clear();
    phaseTransitionFromMode.store(-1, std::memory_order_relaxed);
    phaseTransitionSamplesRemaining.store(0, std::memory_order_relaxed);
    msModeTransitionSamplesRemaining.store(0, std::memory_order_relaxed);
    previousMSModeForCrossfade.store(static_cast<int>(currentMSMode.load(std::memory_order_relaxed)), std::memory_order_relaxed);
    oversamplingTransitionFromEffective.store(-1, std::memory_order_relaxed);
    oversamplingTransitionSamplesRemaining.store(0, std::memory_order_relaxed);
    bypassStateInitialized.store(false, std::memory_order_relaxed);
    bypassPhase.store(BypassPhase::Active, std::memory_order_relaxed);
    bypassCrossfadeRemaining.store(0, std::memory_order_relaxed);
    abCrossfadeSnapshotNeeded.store(false, std::memory_order_relaxed);
    for (int i = 0; i < maxBands; ++i)
        abCrossfadePendingBands[i].store(false, std::memory_order_relaxed);

    // === SOLO ACOUSTIC MONITOR SETUP ===
    soloMonitorFilterL.reset();
    soloMonitorFilterR.reset();
    preProcessingInputCopy.setSize(mainInputChannels, preallocatedMaxSamples, false, true, false);
    preProcessingInputCopy.clear();
    soloOutputBuffer.setSize(mainInputChannels, preallocatedMaxSamples, false, true, false);
    soloOutputBuffer.clear();
    soloWarmupBuffer.setSize(mainInputChannels, preallocatedMaxSamples, false, true, false);
    soloWarmupBuffer.clear();

    auto toResolution = [](int idx) {
        switch (idx)
        {
            case 0: return SpectrumAnalyzer::Resolution::Low;
            case 1: return SpectrumAnalyzer::Resolution::Medium;
            case 3: return SpectrumAnalyzer::Resolution::Max;
            case 2:
            default: return SpectrumAnalyzer::Resolution::High;
        }
    };

    auto toSpeed = [](int idx) {
        switch (idx)
        {
            case 0: return SpectrumAnalyzer::Speed::Fast;
            case 2: return SpectrumAnalyzer::Speed::Slow;
            case 1:
            default: return SpectrumAnalyzer::Speed::Medium;
        }
    };

    // FIX BUG #2: CORRECT ORDER - Prepare shadow processor BEFORE signaling IR builder
    // This ensures eqProcessorForIR is ready when IR builder thread reads it

    // 1. Allocate bands for all processors (including shadow)
    ensureBandCount(maxBands);

    // 2. CRITICAL: Prepare shadow processor BEFORE updateEQFromParameters()
    //    This sets currentSampleRate and isPrepared flag needed by getMagnitudeForFrequency()
    eqProcessorForIR.prepare(sampleRate, samplesPerBlock, mainInputChannels);
    dynamicEQProcessorForIR.prepare(sampleRate, samplesPerBlock, mainInputChannels);

    // 3. Synchronize coefficients from APVTS to shadow processor
    updateEQFromParameters();

    // 4. NOW signal IR builder (shadow processor is fully ready!)
    eqCurveNeedsUpdate.store(true, std::memory_order_release);
    irBuildEvent.signal();

    if (auto* res = apvts.getRawParameterValue("analyzerResolution"))
    {
        int resIdx = juce::jlimit(0, 3, static_cast<int>(std::round(res->load())));

        analyzerResolutionCached = resIdx;
        auto resolution = toResolution(analyzerResolutionCached);
        spectrumAnalyzer.setFFTResolution(resolution);
        postEQAnalyzer.setFFTResolution(resolution);
    }

    if (auto* spd = apvts.getRawParameterValue("analyzerSpeed"))
    {
        int spdIdx = juce::jlimit(0, 2, static_cast<int>(std::round(spd->load())));

        analyzerSpeedCached = spdIdx;
        auto speed = toSpeed(analyzerSpeedCached);
        spectrumAnalyzer.setSpeed(speed);
        postEQAnalyzer.setSpeed(speed);
    }

    // Prepare components
    spectrumAnalyzer.prepare(sampleRate, samplesPerBlock);
    postEQAnalyzer.prepare(sampleRate, samplesPerBlock);

    // Metrological pipeline FIFOs: 32768 samples (~680ms at 48kHz).
    // The AI worker is joined by the lifecycle barrier above, so reallocating
    // aiFrontEndFifo/front-end storage here cannot race a drain pass.
    preEqSpectrumFifo.prepare(32768);
    postEqSpectrumFifo.prepare(32768);
    aiFrontEndFifo.prepare(32768);   // dedicated SPSC: audio producer, AI consumer
    aiFrontEnd.prepare(sampleRate);
    {
        // T5.1: the accumulator is bound to the PFE's OWN band geometry rather
        // than a duplicated table, so the two can never drift apart.
        std::vector<float> contextCentres;
        contextCentres.reserve(static_cast<size_t>(aiFrontEnd.numBands()));
        for (int b = 0; b < aiFrontEnd.numBands(); ++b)
            contextCentres.push_back(aiFrontEnd.bandCenterHz(b));
        aiSpectralContextAccumulator.prepare(contextCentres);
        spectralContextMailbox.reset();
    }
    aiFrontEndScratch.assign(4096, 0.0f);
#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2
    {
        // EXP hybrid: load the v2 CNN next to the plugin binary (or its bundle
        // Resources). Absent => motoreV2 stays inert, shipped path unchanged.
        auto appFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        juce::File v2json = appFile.getSiblingFile("motore_v2.json");
        if (! v2json.existsAsFile())
            v2json = appFile.getParentDirectory()
                            .getSiblingFile("Resources")
                            .getChildFile("motore_v2.json");
        if (! v2json.existsAsFile())
            v2json = appFile.getChildFile("Contents")
                            .getChildFile("Resources")
                            .getChildFile("motore_v2.json");
        const bool ok = aiEngine.loadMotoreV2Model(v2json);
        juce::Logger::writeToLog(ok
            ? "[MotoreV2] model loaded: " + v2json.getFullPathName()
            : "[MotoreV2] model NOT found — heuristic-only fallback");
    }
#endif
    aiFrontEndFrames.store(0, std::memory_order_relaxed);
    aiFrontEndMeanNs.store(0.0, std::memory_order_relaxed);
    aiFrontEndMaxNs.store(0, std::memory_order_relaxed);
    eqProcessor.prepare(sampleRate, samplesPerBlock, mainInputChannels);
    dynamicEQProcessor.prepare(sampleRate, samplesPerBlock, mainInputChannels);

    // NOTE: Shadow processors already prepared earlier (before updateEQFromParameters)

    // Mid/Side processors
    eqProcessorMid.prepare(sampleRate, samplesPerBlock, 1);  // Mono for Mid
    eqProcessorSide.prepare(sampleRate, samplesPerBlock, 1); // Mono for Side
    dynamicEQProcessorMid.prepare(sampleRate, samplesPerBlock, 1);
    dynamicEQProcessorSide.prepare(sampleRate, samplesPerBlock, 1);

    // M/S buffer
    if (mainInputChannels >= 2)
    {
        msBuffer.setSize(2, preallocatedMaxSamples, false, false, true);
        msBuffer.clear();
    }

    // FIX 5: Pre-allocate M/S processing buffers with generous headroom
    midProcessBuffer.setSize(1, preallocatedMaxSamples, false, false, true);
    sideProcessBuffer.setSize(1, preallocatedMaxSamples, false, false, true);
    midProcessBuffer.clear();
    sideProcessBuffer.clear();

    // HQ (NaturalPhase) path with configurable oversampling
    int osFactor = static_cast<int>(apvts.getRawParameterValue("oversamplingFactor")->load());
    oversamplingFactor.store(osFactor, std::memory_order_relaxed);

    // Always create both oversamplers so Auto mode can select at runtime without re-allocating.
    oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(mainInputChannels),
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler2x->reset();
    oversampler2x->initProcessing(static_cast<size_t>(samplesPerBlock));

    oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(mainInputChannels),
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler4x->reset();
    oversampler4x->initProcessing(static_cast<size_t>(samplesPerBlock));

    // A separate mono oversampling chain keeps the external detector aligned
    // with the programme without ever feeding it through the audible EQ.
    sidechainOversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    sidechainOversampler2x->reset();
    sidechainOversampler2x->initProcessing(static_cast<size_t>(samplesPerBlock));
    sidechainOversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    sidechainOversampler4x->reset();
    sidechainOversampler4x->initProcessing(static_cast<size_t>(samplesPerBlock));

    // HQ (NaturalPhase) processors: allocate DynEQ for the worst-case 4x capacity
    // outside the audio callback, then retune to the active rate without resizing.
    // "Off" intentionally shares the same 2x HQ path as 2x to keep the Natural-phase chain coherent.
    const int activeHqMultiplier = (osFactor == 2 ? 4 : 2); // 0,1 -> 2x ; 2 -> 4x ; 3(Auto) starts 2x
    constexpr int maxHqMultiplier = 4;
    const int channels = mainInputChannels;
    const int maxHqBlockCapacity = preallocatedMaxSamples * maxHqMultiplier;
    const int activeHqBlockCapacity = preallocatedMaxSamples * activeHqMultiplier;
    const double activeHqSampleRate = sampleRate * static_cast<double>(activeHqMultiplier);

    // Natural/HQ always uses the double coefficient path, even when its
    // current internal rate (for example 48 kHz x2) is below 192 kHz.
    eqProcessorHQ.setHighPrecisionMode(true);
    dynamicEQProcessorHQ.setHighPrecisionMode(true);

    hqReconfigureFailures.store(0, std::memory_order_relaxed);
    dynamicEQProcessorHQ.prepare(sampleRate * static_cast<double>(maxHqMultiplier),
                                 maxHqBlockCapacity,
                                 channels);
    eqProcessorHQ.prepare(activeHqSampleRate, activeHqBlockCapacity, channels);
    const bool dynHqReady = dynamicEQProcessorHQ.reconfigureNoAllocation(activeHqSampleRate,
                                                                          activeHqBlockCapacity,
                                                                          channels);
    hqRuntimeReady.store(dynHqReady, std::memory_order_release);
    jassert(dynHqReady);

    naturalPhaseLatency = static_cast<int>(oversampler2x->getLatencyInSamples());
    const int maxHQSamples = maxHqBlockCapacity;
    naturalOversampledBuffer.setSize(mainInputChannels, maxHQSamples, false, false, true);
    naturalOversampledBuffer.clear();
    sidechainInputScratch.setSize(1, preallocatedMaxSamples, false, false, true);
    sidechainInputScratch.clear();

    // Linear-phase processors (double-buffer)
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(mainInputChannels);
    for (auto& lp : linearPhaseProcessors)
    {
        lp = std::make_unique<LinearPhaseProcessor>();
        lp->prepare(spec);
    }
    for (auto& loaded : linearIRLoaded)
        loaded.store(false, std::memory_order_relaxed);
    consecutiveIRReadyBlocks.store(0, std::memory_order_relaxed);
    activeIRIndex.store(0);
    readyIRIndex.store(-1);

    // FIX: Pre-allocate crossfade buffer for smooth IR transitions
    crossfadeBuffer.setSize(mainInputChannels, preallocatedMaxSamples);
    crossfadeBuffer.clear();
    crossfadeSamplesRemaining.store(0, std::memory_order_relaxed);

    // Mailbox slots are fixed-size members, so nothing is allocated here; this
    // only returns every slot to FREE and restarts the sequence. Safe because
    // the EC-005 lifecycle barrier keeps the IR builder joined for the whole
    // prepare window, so reset cannot race a publish, and the host contract
    // keeps processBlock out during prepareToPlay.
    pendingFreqIR.reset();

    // EC-001/B4: no GUI-spectrum scratch/queue is needed. The AI worker owns
    // its persistent 2049-bin spectrum and sources it from PerceptualFrontEnd.
    previousIRIndex.store(0, std::memory_order_relaxed);

    // Start OSC parameter server (deferred from constructor to avoid crash during plugin scan)
    // OSC remote control is OPT-IN (off by default): set the environment variable
    // AIEQ_ENABLE_OSC to open the UDP listener. Previously the socket+thread were
    // always running unused (and exposed a parser/teardown attack surface).
    if (oscParamServer && !oscParamServer->isRunning()
        && std::getenv("AIEQ_ENABLE_OSC") != nullptr)
        oscParamServer->start();

    // Prime band smoothers
    primeBandSmoothers(sampleRate);

    // Linear-phase delay buffer for fallback alignment (no realloc in audio thread)
    linearPhasePreDynamicLatencySamples = static_cast<int>(
        LinearPhaseProcessor::usePartitioned
            ? (LinearPhaseProcessor::partSize + LinearPhaseProcessor::irSize / 2)
            : (LinearPhaseProcessor::hopSize + LinearPhaseProcessor::irSize / 2));
    const int lpDelaySamples = worstCaseLatencySamples + preallocatedMaxSamples;
    linearPhaseDelayBuffer.setSize(mainInputChannels, lpDelaySamples, false, false, true);
    linearPhaseDelayBuffer.clear();
    linearPhaseDelayWritePos = 0;
    linearSidechainDelayBufferSize = linearPhasePreDynamicLatencySamples
                                   + preallocatedMaxSamples + 1;
    linearSidechainDelayBuffer.setSize(
        1, linearSidechainDelayBufferSize, false, false, true);
    linearSidechainDelayBuffer.clear();
    linearAlignedSidechainBuffer.setSize(
        1, preallocatedMaxSamples, false, false, true);
    linearAlignedSidechainBuffer.clear();
    linearSidechainDelayWritePos = 0;
    externalSidechainWasAvailable = false;

    // Apply quality/latency mode to dynamic EQ lookahead
    // RB-4 FIX: prepare() already pre-allocates for max 20ms and setLookahead()
    // updates lookaheadSamples + clears buffer.  Do NOT call updateLookaheadBuffer()
    // afterwards — it would shrink/deallocate the pre-allocated buffer when lookahead=0,
    // breaking the runtime switch path (Zero Latency → HQ).
    int qualityMode = static_cast<int>(apvts.getRawParameterValue("qualityMode")->load());
    float lookaheadMs = (qualityMode == 1) ? 5.0f : 0.0f; // HQ: 5ms lookahead, Zero-latency: 0ms
    dynamicEQProcessor.setLookahead(lookaheadMs);
    dynamicEQProcessorMid.setLookahead(lookaheadMs);
    dynamicEQProcessorSide.setLookahead(lookaheadMs);
    dynamicEQProcessorHQ.setLookahead(lookaheadMs);
    qualityModeCached = qualityMode;
    {
        // Covers even a synchronous/non-owned capture-analysis caller. The
        // persistent AI worker and owned capture thread are already joined, so
        // this lock is normally uncontended on the cold lifecycle path.
        std::lock_guard<std::mutex> lock(aiAnalysisMutex);
        aiEngine.prepare(sampleRate, samplesPerBlock);
    }
    referenceMatcher.prepare(sampleRate, samplesPerBlock);
    dynamicCorrectionEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    // Prepare lock-free capture service (replaces old mutex-based capture)
    captureService.prepare(sampleRate, mainInputChannels, samplesPerBlock);

    // Initialize bands if not already initialized
    if (eqProcessor.getNumBands() == 0)
    {
        int active = numActiveBands.load(std::memory_order_relaxed);
        for (int i = 0; i < active; ++i)
        {
            juce::String prefix = "band" + juce::String(i);
            int type = ParametricEQProcessor::Peak;
            if (auto* typeParam = apvts.getRawParameterValue(prefix + "Type"))
                type = static_cast<int>(typeParam->load());
            else if (i == 0)
                type = ParametricEQProcessor::LowShelf;
            else if (i == active - 1)
                type = ParametricEQProcessor::HighShelf;

            eqProcessor.addBand(AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)],
                                0.0f, 1.0f, type);
        }
        // If fewer than active bands were added (max limit), adjust numActiveBands
        numActiveBands.store(std::min(active, eqProcessor.getNumBands()), std::memory_order_relaxed);
    }

    // Reset RMS values (atomic)
    preEQRMS.store(0.0f, std::memory_order_relaxed);
    postEQRMS.store(0.0f, std::memory_order_relaxed);
    autoGainCompensation.store(0.0f, std::memory_order_relaxed);
    autoGainBlockCounter = 0;
    parametersNeedUpdate.store(true, std::memory_order_relaxed);
    lastProcessedParameterChangeCounter.store(parameterChangeCounter.load(std::memory_order_relaxed),
                                              std::memory_order_relaxed);

    // 50 ms ramps: auto-gain on the wet path, output trim on the mixed output.
    smoothedAutoGain.reset(sampleRate, 0.05);
    smoothedAutoGain.setCurrentAndTargetValue(1.0f);
    smoothedOutputGain.reset(sampleRate, 0.05);
    smoothedOutputGain.setCurrentAndTargetValue(1.0f);

    // Pre-compute AI analysis cadence (~10 Hz)
    aiAnalysisIntervalSamples = juce::jmax(static_cast<int>(std::round(sampleRate * 0.1)), samplesPerBlock);
    aiOfflineRenderActive.store(false, std::memory_order_relaxed);
    aiFrontEndDiscontinuityPending.store(false, std::memory_order_relaxed);
    aiFrontEndDroppedSamples.store(0, std::memory_order_relaxed);
    aiFrontEndDiscontinuities.store(0, std::memory_order_relaxed);
    aiHeadlessAnalyses.store(0, std::memory_order_relaxed);

    // === MAXIMUM LATENCY PADDING ===
    // Always report worst-case latency to DAW. Compensate internally with delay lines.
    // This prevents Ableton PDC recalculation glitches on mode switches.
    {
        // LP latency = block buffering (partSize) + zero-phase IR group delay (irSize/2).
        // The IR peak sits at tap irSize/2 = 2048, so the convolver output is delayed
        // by partSize + irSize/2 samples relative to the raw input.
        const int lpLatency = linearPhasePreDynamicLatencySamples;
        int oversamplingLatency = naturalPhaseLatency;
        if (oversampler4x)
            oversamplingLatency = std::max(oversamplingLatency,
                                           static_cast<int>(oversampler4x->getLatencyInSamples()));
        if (oversampler2x)
            oversamplingLatency = std::max(oversamplingLatency,
                                           static_cast<int>(oversampler2x->getLatencyInSamples()));
        worstCaseOversamplingLatency = oversamplingLatency;
        const int maximumDynamicLookahead =
            static_cast<int>(std::round(sampleRate * 0.005));
        worstCaseLatencySamples = std::max(lpLatency, oversamplingLatency)
                                  + maximumDynamicLookahead;
    }
    latencyPlan.maximumSamples = worstCaseLatencySamples;
    const int initialLatency = requiresPaddedLatencyPlan() ? worstCaseLatencySamples : 0;
    latencyPlan.activeSamples.store(initialLatency, std::memory_order_release);
    latencyPlan.reductionDeferred.store(false, std::memory_order_relaxed);
    setLatencySamples(initialLatency);
    lastReportedLatency = initialLatency;

    // Bypass crossfade must outlast worstCaseLatencySamples so that fresh wet data
    // has fully propagated through the wet padding delay before the dry signal
    // fades away.  With fade length = 2×latency + 512:
    //   - At t = latency/total ≈ 0.33 the first fresh wet sample arrives while
    //     dry still carries ~67% weight → no audible hole.
    //   - The extra 512 samples (~10ms @ 48kHz) provide headroom for hosts that
    //     deliver blocks slightly larger than expected.
    bypassCrossfadeSamples = worstCaseLatencySamples * 2 + 512;

    // Workers are reopened only after every object they may observe has been
    // fully prepared. processBlock remains closed until the final store below.
    restartBackgroundWorkersAfterLifecycle();

    // Signal that processor is ready for GUI/audio access
    processorReady.store(true, std::memory_order_release);
}

void AIEqualizerAudioProcessor::releaseResources()
{
    processorReady.store(false, std::memory_order_release);
    cancelPendingUpdate();

    // EC-005: release has the same quiescence contract as prepare. Do not reset
    // worker-visible state while a background operation can still be in flight.
    quiesceBackgroundWorkersForLifecycle();

    captureService.release();
    spectrumAnalyzer.reset();
    postEQAnalyzer.reset();
    eqProcessor.reset();
    dynamicEQProcessor.reset();
}

bool AIEqualizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Main input/output must be mono or stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // Sidechain input (optional) - mono only
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sidechainSet = layouts.getChannelSet(false, 1);
        if (!sidechainSet.isDisabled() && sidechainSet != juce::AudioChannelSet::mono())
            return false;
    }

    return true;
}

//==============================================================================
void AIEqualizerAudioProcessor::processBlock(juce::AudioBuffer<float>& processBuffer,
                                              juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // JUCE flattens all buses into processBuffer.  Work exclusively on the
    // main output view so a mono sidechain can never masquerade as the right
    // programme channel (especially for mono-main + mono-SC layouts).
    auto buffer = getBusBuffer(processBuffer, false, 0);
    auto sidechainBus = getBusBuffer(processBuffer, true, 1);

    // Guard: if prepareToPlay has not completed yet (e.g. host calls processBlock
    // during plugin scan before prepareToPlay), pass audio through silently and return.
    if (!processorReady.load(std::memory_order_acquire))
    {
        buffer.clear();
        return;
    }

    const int totalNumInputChannels = buffer.getNumChannels();
    const int totalNumOutputChannels = buffer.getNumChannels();
    const int inputBlockSamples = buffer.getNumSamples();
    const int blockSamples = juce::jmin(inputBlockSamples, preallocatedMaxSamples);

    // Snapshot all parameters once per block for atomic consistency (partial)
    AIEQCore::ProcessBlockParameters paramsSnapshot;
    loadParameterSnapshot(paramsSnapshot);

    if (pendingReset.exchange(false, std::memory_order_acq_rel))
    {
        if (oversampler2x) oversampler2x->reset();
        if (oversampler4x) oversampler4x->reset();
        if (sidechainOversampler2x) sidechainOversampler2x->reset();
        if (sidechainOversampler4x) sidechainOversampler4x->reset();
        linearSidechainDelayBuffer.clear();
        linearAlignedSidechainBuffer.clear();
        linearSidechainDelayWritePos = 0;

        for (auto& lp : linearPhaseProcessors)
        {
            if (lp)
                lp->reset();
        }

        readyIRIndex.store(-1, std::memory_order_relaxed);
        activeIRIndex.store(0, std::memory_order_relaxed);
        previousIRIndex.store(0, std::memory_order_relaxed);
        crossfadeSamplesRemaining.store(0, std::memory_order_relaxed);
        for (auto& loaded : linearIRLoaded)
            loaded.store(false, std::memory_order_relaxed);

        // If we reset while in Linear Phase, force an IR rebuild immediately
        if (currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::LinearPhase)
        {
            consecutiveIRReadyBlocks.store(0, std::memory_order_relaxed);
            triggerLinearPhaseIRUpdate();
        }

        // Retune eqProcessorHQ / dynamicEQProcessorHQ at the correct oversampled rate.
        // This must not call prepare(): DynamicEQProcessor::prepare() owns buffer
        // sizing and is not audio-thread safe.
        // Without this, switching oversampling factor at runtime leaves HQ EQ coefficients
        // calculated for the wrong sample rate (e.g. still sr*2 while oversampler is now
        // feeding sr*4), causing wrong frequency response until the next prepareToPlay.
        {
            const int newOsFactor   = oversamplingFactor.load(std::memory_order_relaxed);
            const int newOsMult     = (newOsFactor == 2 ? 4 : 2); // mirrors prepareToPlay logic
            const double newHqRate  = currentSampleRate.load(std::memory_order_relaxed)
                                      * static_cast<double>(newOsMult);
            const int newHqBlock    = preallocatedMaxSamples * newOsMult;
            const int numChs        = preparedNumInputChannels;
            if (!dynamicEQProcessorHQ.canReconfigureWithoutAllocation(newHqRate, newHqBlock, numChs))
            {
                hqReconfigureFailures.fetch_add(1, std::memory_order_relaxed);
                hqRuntimeReady.store(false, std::memory_order_release);
            }
            else
            {
                const bool dynOk = dynamicEQProcessorHQ.reconfigureNoAllocation(newHqRate, newHqBlock, numChs);
                const bool eqOk = dynOk && eqProcessorHQ.reconfigureNoAllocation(newHqRate, newHqBlock, numChs);
                hqRuntimeReady.store(dynOk && eqOk, std::memory_order_release);
                if (!dynOk || !eqOk)
                    hqReconfigureFailures.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Trigger a dry→wet crossfade to cover the reset transient.
        // Resetting oversamplers/LP processors from non-zero state causes a brief
        // dropout (IIR filters start from zero on a live signal). The bypass crossfade
        // mechanism already has the dry buffer capture and blend logic — reusing it
        // here covers the reset block with a smooth fade rather than an audible gap.
        currentBypassCrossfadeSamples = bypassCrossfadeSamples;
        bypassCrossfadeRemaining.store(currentBypassCrossfadeSamples, std::memory_order_relaxed);
    }

    // Defensive clamp: if host delivers a block bigger than we pre-allocated for
    // (e.g. Reaper with dynamic block sizes), process the safe portion and silence
    // the remainder to avoid buffer overruns.
    // NOTE: this should never happen in a correctly configured session - if it fires
    // frequently the host is not honouring the maximumBlockSize passed to prepareToPlay.
    if (inputBlockSamples > blockSamples)
    {
        blockClampEvents.fetch_add(1, std::memory_order_relaxed);
        {
            // RT-SAFE: use lock-free logger queue (no mutex/file I/O in audio thread)
            char msg[128];
            std::snprintf(msg, sizeof(msg), "BlockClamp host=%d max=%d",
                          inputBlockSamples, preallocatedMaxSamples);
            AIEQLogger::getInstance().logFromRTThread(AIEQLogger::Level::Warning, msg, "BlockClamp");
        }
       #if JUCE_DEBUG
        jassertfalse; // host is sending blocks larger than maximumBlockSize - investigate
       #endif
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.clear(ch, blockSamples, inputBlockSamples - blockSamples);
    }

    // Safety: if channel layout changed unexpectedly, bypass processing to avoid overflow
    if (buffer.getNumChannels() != preparedNumInputChannels)
    {
        blockClampEvents.fetch_add(1, std::memory_order_relaxed);
       #if JUCE_DEBUG
        jassertfalse; // layout mismatch: host likely reconfigured without new prepareToPlay
       #endif
        buffer.clear();
        return;
    }

    const bool externalDetectorAvailable = sidechainBus.getNumChannels() > 0
        && sidechainBus.getNumSamples() >= blockSamples;

    // A host may connect/disconnect an optional bus without rebuilding the
    // processor.  Never replay detector history from the previous connection.
    // All operations are bounded and allocation-free on the audio thread.
    if (externalDetectorAvailable != externalSidechainWasAvailable)
    {
        if (sidechainOversampler2x) sidechainOversampler2x->reset();
        if (sidechainOversampler4x) sidechainOversampler4x->reset();
        linearSidechainDelayBuffer.clear();
        linearAlignedSidechainBuffer.clear();
        linearSidechainDelayWritePos = 0;
        externalSidechainWasAvailable = externalDetectorAvailable;
    }
    const juce::AudioBuffer<float>* externalDetectorRaw =
        externalDetectorAvailable ? &sidechainBus : nullptr;

    // Keep a continuously primed detector delay, including while bypassed, so
    // entering Linear Phase cannot expose an empty/stale latency-sized region.
    // A present-but-silent bus remains available by contract; absence alone
    // selects nullptr and lets DynamicEQProcessor release toward neutral.
    juce::AudioBuffer<float> linearAlignedDetectorView(
        linearAlignedSidechainBuffer.getArrayOfWritePointers(), 1, blockSamples);
    const juce::AudioBuffer<float>* externalDetectorLinear = nullptr;
    if (externalDetectorAvailable
        && linearSidechainDelayBufferSize > linearPhasePreDynamicLatencySamples
        && linearAlignedSidechainBuffer.getNumSamples() >= blockSamples)
    {
        const float* source = sidechainBus.getReadPointer(0);
        float* delay = linearSidechainDelayBuffer.getWritePointer(0);
        float* aligned = linearAlignedSidechainBuffer.getWritePointer(0);
        for (int sample = 0; sample < blockSamples; ++sample)
        {
            delay[(linearSidechainDelayWritePos + sample)
                  % linearSidechainDelayBufferSize] = source[sample];
            const int readPosition = (linearSidechainDelayWritePos + sample
                                      - linearPhasePreDynamicLatencySamples
                                      + linearSidechainDelayBufferSize)
                                     % linearSidechainDelayBufferSize;
            aligned[sample] = delay[readPosition];
        }
        linearSidechainDelayWritePos =
            (linearSidechainDelayWritePos + blockSamples)
            % linearSidechainDelayBufferSize;
        externalDetectorLinear = &linearAlignedDetectorView;
    }

    auto loadParam = [](std::atomic<float>* ptr, float fallback) -> float
    {
        return ptr ? ptr->load(std::memory_order_relaxed) : fallback;
    };

    // Detect solo (audio thread, lock-free) and capture input for audition
    // FIX: Clamp to maxBands to prevent array out-of-bounds access
    const int activeBandsLocal = std::min(paramsSnapshot.numActiveBands, maxBands);
    bool hasSolo = false;
    for (int i = 0; i < activeBandsLocal; ++i)
    {
        const auto& p = cachedParams[static_cast<size_t>(i)];
        const bool enabled = loadParam(p.enabled, (i < 8 ? 1.0f : 0.0f)) > 0.5f && (i < activeBandsLocal);
        const bool solo = loadParam(p.solo, 0.0f) > 0.5f;
        if (enabled && solo)
        {
            hasSolo = true;
            break;
        }
    }

    // (solo state reset handled after solo monitor block below)

    if (hasSolo)
    {
        if (preProcessingInputCopy.getNumChannels() >= buffer.getNumChannels()
            && preProcessingInputCopy.getNumSamples() >= blockSamples)
        {
            const int chs = juce::jmin(buffer.getNumChannels(), preProcessingInputCopy.getNumChannels());
            for (int ch = 0; ch < chs; ++ch)
                preProcessingInputCopy.copyFrom(ch, 0, buffer, ch, 0, blockSamples);
        }
    }

    // Read bypass state early (needed to decide dryBuffer capture for crossfade)
    bool bypassed = false;
    if (cachedBypass)
        bypassed = cachedBypass->load(std::memory_order_relaxed) > 0.5f;

    // On the very first block after prepare, seed bypassPhase so we don't trigger
    // a spurious crossfade before the host has had a chance to set bypass state.
    if (!bypassStateInitialized.load(std::memory_order_relaxed))
    {
        bypassPhase.store(bypassed ? BypassPhase::Bypassed : BypassPhase::Active,
                          std::memory_order_relaxed);
        bypassStateInitialized.store(true, std::memory_order_relaxed);
    }

    // Global dry/wet mix / bypass dry reference
    const float dryWetPct = loadParam(cachedDryWet, 100.0f);
    const float wet = juce::jlimit(0.0f, 100.0f, dryWetPct) * 0.01f;
    const float dry = 1.0f - wet;
    // Need dry buffer whenever: explicit dry mix, bypass active, crossfade in progress,
    // OR bypass phase isn't Active (catches the first FadingToActive block where the
    // state machine hasn't run yet but the crossfade is about to start).
    const bool needsDry = dry > 0.0001f || bypassed
        || bypassCrossfadeRemaining.load(std::memory_order_relaxed) > 0
        || bypassPhase.load(std::memory_order_relaxed) != BypassPhase::Active;

    // ALWAYS feed the dry delay ring buffer so it has valid data when bypass is engaged.
    {
        dryDelayLength = latencyPlan.activeSamples.load(std::memory_order_acquire);
        const int chs = juce::jmin(buffer.getNumChannels(), dryDelayBuffer.getNumChannels());

        // Keep history even in the zero-latency plan. If a latent module is
        // enabled at runtime, the upgraded plan can immediately read valid dry
        // samples instead of emitting a latency-sized hole.
        if (dryDelayBufferSize > 0)
        {
            for (int ch = 0; ch < chs; ++ch)
            {
                const float* src = buffer.getReadPointer(ch);
                float* delayBuf = dryDelayBuffer.getWritePointer(ch);
                for (int s = 0; s < blockSamples; ++s)
                    delayBuf[(dryDelayWritePos + s) % dryDelayBufferSize] = src[s];
            }
            if (needsDry)
            {
                for (int ch = 0; ch < chs; ++ch)
                {
                    const float* delayBuf = dryDelayBuffer.getReadPointer(ch);
                    float* dryOut = dryBuffer.getWritePointer(ch);
                    for (int s = 0; s < blockSamples; ++s)
                    {
                        const int readPos = (dryDelayWritePos + s - dryDelayLength
                                             + dryDelayBufferSize) % dryDelayBufferSize;
                        dryOut[s] = delayBuf[readPos];
                    }
                }
            }
            dryDelayWritePos = (dryDelayWritePos + blockSamples) % dryDelayBufferSize;
        }
        else if (needsDry)
        {
            const int chsDry = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());
            for (int ch = 0; ch < chsDry; ++ch)
                dryBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);
        }
    }
    const int qualityModeParam = static_cast<int>(std::round(loadParam(cachedQualityMode, static_cast<float>(qualityModeCached))));
    const auto phaseModeSnapshot = static_cast<PhaseMode>(paramsSnapshot.phaseMode);
    const auto msModeSnapshot = static_cast<MSMode>(paramsSnapshot.msMode);
    // Cat 2 Fix: snapshot phase transition remaining BEFORE the crossfade blocks
    // decrement it, so the wet padding ramp (far below) sees the same pre-block
    // value as the EQ crossfade and stays in sync with it.
    const int phaseTransitionRemainingAtBlockStart =
        phaseTransitionSamplesRemaining.load(std::memory_order_acquire);
    const int analyzerResParam = static_cast<int>(std::round(loadParam(cachedAnalyzerResolution, static_cast<float>(analyzerResolutionCached))));
    const int analyzerSpdParam = static_cast<int>(std::round(loadParam(cachedAnalyzerSpeed, static_cast<float>(analyzerSpeedCached))));
    const bool autoGainEnabledLocal = loadParam(cachedAutoGain, 0.0f) > 0.5f;
    autoGainEnabled.store(autoGainEnabledLocal, std::memory_order_relaxed);
    const bool dynEqEnabledLocal = paramsSnapshot.dynamicEQEnabled;

    // NOTE: do NOT clear the meter cache here - the GUI reads at 30Hz while processBlock
    // runs at ~86Hz. Clearing every block means the GUI almost always reads 0.
    // Instead, let updateDynamicMeterCacheFrom() overwrite on each block where DynEQ runs,
    // and let the GUI meter decay visually on its own timer when no new data arrives.
    const bool aiEnabledLocal = loadParam(cachedAIEnabled, 1.0f) > 0.5f;
    const int sourceProfileIndex = static_cast<int>(std::round(loadParam(cachedSourceProfile, 0.0f)));
    const bool showPost = loadParam(cachedShowPostSpectrum, 0.0f) > 0.5f;
    float outputGainDB = paramsSnapshot.outputGainDB;

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // FA-001 containment: with capture disabled for shipping there is no reader,
    // so feeding the retroactive ring would copy every block for nothing. The
    // check is runtime rather than `if constexpr` on purpose: PluginProcessor.cpp
    // is compiled once into SharedCode, which does NOT define JUCE_UNIT_TESTS,
    // so a compile-time constant would strip the ring feed from the test builds
    // too and silently break the CaptureService coverage this containment was
    // designed to preserve. What actually costs anything here is the per-block
    // copy, and that is gone either way; what remains in the shipping build is
    // one relaxed load of a static that is always false.
    if (isCaptureAllowed())
    {
        // Lock-free CaptureService push (even if bypassed) — fully RT-safe.
        const int captureDropped = captureService.pushSamples(buffer);
        juce::ignoreUnused(captureDropped);
    }

    // Process any pending AI commands from the lock-free queue
    processAICommands();

    // ── Bypass state machine ─────────────────────────────────────────────
    // Four phases:
    //   Active         – normal DSP
    //   FadingToBypass – crossfading wet→dry, DSP still running
    //   Bypassed       – steady-state, no DSP, wet padding fed with dry
    //   FadingToActive – crossfading dry→wet, DSP running (filters reset)
    {
        const auto phase = bypassPhase.load(std::memory_order_relaxed);

        if (bypassed && phase == BypassPhase::Active)
        {
            // User engaged bypass → start fading out wet
            bypassPhase.store(BypassPhase::FadingToBypass, std::memory_order_relaxed);
            currentBypassCrossfadeSamples = bypassCrossfadeSamples;
            bypassCrossfadeRemaining.store(currentBypassCrossfadeSamples, std::memory_order_relaxed);
        }
        else if (!bypassed && phase == BypassPhase::Bypassed)
        {
            // User disengaged bypass after steady-state → THE KEY FIX:
            // Reset all DSP filter state (IIR, convolver FDL, oversamplers)
            // before re-exposing the wet path.  Stale internal energy from
            // the last block processed before bypass would otherwise mix with
            // new input and produce a transient: y[0] = b0·x[0] + v1_stale.
            // After reset, filters start from zero — producing a smooth onset
            // that the 2400-sample crossfade fully masks.
            resetDSPStateForBypassExit();
            bypassPhase.store(BypassPhase::FadingToActive, std::memory_order_relaxed);
            currentBypassCrossfadeSamples = bypassCrossfadeSamples;
            bypassCrossfadeRemaining.store(currentBypassCrossfadeSamples, std::memory_order_relaxed);
        }
        else if (!bypassed && phase == BypassPhase::FadingToBypass)
        {
            // Rapid toggle OFF while still fading to bypass — reverse direction.
            // Filters are still warm (DSP ran during fade), no reset needed.
            //
            // The crossfade formulas use t = (total - remaining) / total:
            //   FadingToBypass: out = wet*(1-t) + dry*t     → wet_fraction = 1-t
            //   FadingToActive: out = dry*(1-t) + wet*t     → wet_fraction = t
            //
            // Continuity requires the wet_fraction to be identical at the
            // transition point.  Swapping remaining ↔ progress while keeping
            // total unchanged achieves this:
            //   old wet_frac = 1 - (total-R)/total = R/total
            //   new wet_frac = (total - R_new)/total = (total - (total-R))/total = R/total  ✓
            const int remaining = bypassCrossfadeRemaining.load(std::memory_order_relaxed);
            const int swapped   = juce::jmax(1, currentBypassCrossfadeSamples - remaining);
            bypassPhase.store(BypassPhase::FadingToActive, std::memory_order_relaxed);
            // currentBypassCrossfadeSamples unchanged — keeps fade rate constant
            bypassCrossfadeRemaining.store(swapped, std::memory_order_relaxed);
        }
        else if (bypassed && phase == BypassPhase::FadingToActive)
        {
            // Rapid toggle ON while still fading to active — reverse (symmetric).
            const int remaining = bypassCrossfadeRemaining.load(std::memory_order_relaxed);
            const int swapped   = juce::jmax(1, currentBypassCrossfadeSamples - remaining);
            bypassPhase.store(BypassPhase::FadingToBypass, std::memory_order_relaxed);
            // currentBypassCrossfadeSamples unchanged
            bypassCrossfadeRemaining.store(swapped, std::memory_order_relaxed);
        }
    }

    // AI corrections keep the same hard slew ceilings as manual automation,
    // but use an 80 ms minimum transition where the requested distance permits.
    // Rate configuration is deferred until after updateEQFromParameters() has
    // published the new targets in this same process quantum.
    if (aiCorrectionCrossfadePending.exchange(false, std::memory_order_acquire))
    {
        correctionSmoothingActive.store(true, std::memory_order_relaxed);
        correctionSmoothingNeedsRateConfig = true;
    }

    if (bypassPhase.load(std::memory_order_relaxed) == BypassPhase::Bypassed)
    {
        // Steady-state bypass: output delayed dry (must match reported latency)
        if (needsDry)
        {
            const int chs = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());
            for (int ch = 0; ch < chs; ++ch)
                buffer.copyFrom(ch, 0, dryBuffer, ch, 0, blockSamples);
        }

        // Keep wet padding ring buffer fed with dry signal during bypass.
        // Without this, un-bypass reads STALE data from the buffer → pop.
        if (wetPaddingBufferSize > 0)
        {
            const int chs = juce::jmin(buffer.getNumChannels(), wetPaddingDelayBuffer.getNumChannels());
            for (int ch = 0; ch < chs; ++ch)
            {
                const float* dry = dryBuffer.getReadPointer(ch);
                float* padBuf = wetPaddingDelayBuffer.getWritePointer(ch);
                for (int s = 0; s < blockSamples; ++s)
                    padBuf[(wetPaddingWritePos + s) % wetPaddingBufferSize] = dry[s];
            }
            wetPaddingWritePos = (wetPaddingWritePos + blockSamples) % wetPaddingBufferSize;
        }

        clearDynamicMeterCache();
        return;
    }
    // If fading to bypass: fall through to DSP, clear meters (bypass is pending)
    if (bypassPhase.load(std::memory_order_relaxed) == BypassPhase::FadingToBypass)
        clearDynamicMeterCache();

    // Handle quality/latency mode (adjust lookahead dynamically)
    // Until a host-visible latency upgrade has been installed, a requested HQ
    // detector remains in its genuinely zero-latency mode.
    const bool paddedLatencyActive =
        latencyPlan.activeSamples.load(std::memory_order_acquire) > 0;
    int qualityMode = paddedLatencyActive ? juce::jlimit(0, 1, qualityModeParam) : 0;
    if (qualityMode != qualityModeCached)
    {
        qualityModeCached = qualityMode;
        float lookaheadMs = (qualityMode == 1) ? 5.0f : 0.0f; // HQ: 5ms, Zero-Latency: 0ms
        dynamicEQProcessor.setLookahead(lookaheadMs);
        dynamicEQProcessorMid.setLookahead(lookaheadMs);
        dynamicEQProcessorSide.setLookahead(lookaheadMs);
        dynamicEQProcessorHQ.setLookahead(lookaheadMs);
    }

    auto toResolution = [](int idx) {
        switch (idx)
        {
            case 0: return SpectrumAnalyzer::Resolution::Low;
            case 1: return SpectrumAnalyzer::Resolution::Medium;
            case 3: return SpectrumAnalyzer::Resolution::Max;
            case 2:
            default: return SpectrumAnalyzer::Resolution::High;
        }
    };

    auto toSpeed = [](int idx) {
        switch (idx)
        {
            case 0: return SpectrumAnalyzer::Speed::Fast;
            case 2: return SpectrumAnalyzer::Speed::Slow;
            case 1:
            default: return SpectrumAnalyzer::Speed::Medium;
        }
    };

    int resIdx = juce::jlimit(0, 3, analyzerResParam);
    if (resIdx != analyzerResolutionCached)
    {
        analyzerResolutionCached = resIdx;
        auto resolution = toResolution(resIdx);
        spectrumAnalyzer.setFFTResolution(resolution);
        postEQAnalyzer.setFFTResolution(resolution);
    }

    int spdIdx = juce::jlimit(0, 2, analyzerSpdParam);
    if (spdIdx != analyzerSpeedCached)
    {
        analyzerSpeedCached = spdIdx;
        auto speed = toSpeed(spdIdx);
        spectrumAnalyzer.setSpeed(speed);
        postEQAnalyzer.setSpeed(speed);
    }

    // Dispatch pending A/B whole-chain crossfade BEFORE updating coefficients,
    // so beginWholeChainCrossfade() snapshots the OLD coefficients/filter state.
    // The flag is armed by setABState() on the message thread BEFORE it calls
    // loadStateFromSlot(), guaranteeing the snapshot precedes coefficient changes.
    if (abCrossfadeSnapshotNeeded.exchange(false, std::memory_order_acq_rel))
    {
        eqProcessor.beginWholeChainCrossfade(abSwitchCrossfadeSamples);
        eqProcessorHQ.beginWholeChainCrossfade(abSwitchCrossfadeSamples);
        eqProcessorMid.beginWholeChainCrossfade(abSwitchCrossfadeSamples);
        eqProcessorSide.beginWholeChainCrossfade(abSwitchCrossfadeSamples);
    }

    // Update EQ parameters only when something actually changed
    const auto currentParamCounter = parameterChangeCounter.load(std::memory_order_acquire);
    const bool needsParamUpdate = parametersNeedUpdate.exchange(false, std::memory_order_acq_rel)
                                  || currentParamCounter != lastProcessedParameterChangeCounter.load(std::memory_order_relaxed);
    if (needsParamUpdate)
    {
        updateEQFromParameters();
        lastProcessedParameterChangeCounter.store(currentParamCounter, std::memory_order_relaxed);
    }

    // Apply smoothed band params (anti-zippering) before processing
    const bool smoothedBandParamsApplied =
        applySmoothedBandParams(blockSamples, needsParamUpdate);

    // FIX: Bump EQ curve version from audio thread after band params are written.
    // Without this, AI corrections cause a race: the message-thread version bump
    // fires before eqProcessor.bandParams are updated, so rebuildEQCurvePath()
    // reads stale data (old filter type/gain) and consumes the version counter.
    // The curve then stays stale until the next user interaction.
    //
    // Additionally, every quantum that actually publishes a bounded-slew or
    // topology value bumps the curve, including the final quantum that reaches
    // the exact target. This prevents the GUI from freezing one step short.
    if (needsParamUpdate || smoothedBandParamsApplied)
        eqCurveChangeCounter.fetch_add(1, std::memory_order_relaxed);

    if (autoGainEnabledLocal)
    {
        float currentPreRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (totalNumInputChannels > 1)
            currentPreRMS = (currentPreRMS + buffer.getRMSLevel(1, 0, buffer.getNumSamples())) * 0.5f;

        // FIX: Use atomic load/store for thread-safe access
        float currentRMS = preEQRMS.load(std::memory_order_relaxed);
        preEQRMS.store(currentRMS * rmsSmoothing + currentPreRMS * (1.0f - rmsSmoothing),
                       std::memory_order_relaxed);
    }

    // ── Click detector helper (debug/test builds only) ──────────────────────
    // Disabled in release: kClickThreshold (0.25) fires on normal musical
    // transients, generating hundreds of logFromRTThread calls/sec.  Each call
    // does mach_absolute_time() + SPSC push — unnecessary RT overhead.
    // The AntiPopRegressionTest uses its own click detection on controlled signals.
#if JUCE_DEBUG || defined(AIEQ_TESTING)
    static constexpr float kClickThreshold = 0.25f; // ~-12 dBFS jump
    auto checkClicks = [&](uint8_t checkpoint) noexcept
    {
        if (buffer.getNumChannels() == 0 || blockSamples == 0)
            return;
        const float* ch0 = buffer.getReadPointer(0);
        float prev = (checkpoint == 0) ? clickPrevSample : ch0[0];
        bool fired = false;
        for (int s = (checkpoint == 0 ? 0 : 1); s < blockSamples; ++s)
        {
            if (std::abs(ch0[s] - prev) > kClickThreshold)
            {
                fired = true;
                break;
            }
            prev = ch0[s];
        }
        if (checkpoint == 0 && std::abs(ch0[0] - clickPrevSample) > kClickThreshold)
            fired = true;
        if (fired)
        {
            clickEventCount.fetch_add(1, std::memory_order_relaxed);
            clickLastCheckpoint.store(checkpoint, std::memory_order_relaxed);
            char msg[64];
            std::snprintf(msg, sizeof(msg), "CLICK cp=%u delta>%.2f",
                          static_cast<unsigned>(checkpoint), kClickThreshold);
            AIEQLogger::getInstance().logFromRTThread(AIEQLogger::Level::Warning, msg, "ClickDetector");
        }
        if (checkpoint == 4)
            clickPrevSample = ch0[blockSamples - 1];
    };
    checkClicks(0);
#else
    auto checkClicks = [](uint8_t) noexcept {};  // no-op in release
#endif

    // RT heartbeat (debug/test only — no RT logging in release)
#if JUCE_DEBUG || defined(AIEQ_TESTING)
    {
        rtHeartbeatCounter += blockSamples;
        const int sr = static_cast<int>(currentSampleRate.load(std::memory_order_relaxed));
        if (sr > 0 && rtHeartbeatCounter >= sr * 5)
        {
            rtHeartbeatCounter = 0;
            AIEQLogger::getInstance().logFromRTThread(
                AIEQLogger::Level::Info, "RT heartbeat", "AudioThread");
        }
    }
#endif

    // GUI analyzer is display-only. Continuous AI detection has its own
    // dedicated headless producer/consumer path.
    spectrumAnalyzer.pushSamples(buffer);
    preEqSpectrumFifo.pushStereoMix(buffer);  // metrological pipeline FIFO (GUI consumer)

    const bool isOffline = isNonRealtime();
    aiOfflineRenderActive.store(isOffline, std::memory_order_release);

    // These setters are atomic/RT-safe. Keep parameter propagation separate
    // from analysis scheduling so the worker is the sole detector owner.
    aiEngine.setEnabled(aiEnabledLocal);
    aiEngine.setSourceProfile(
        static_cast<AIEngine::SourceProfile>(juce::jlimit(0, 6, sourceProfileIndex)));

    // Never feed the production AI frontend across offline/disabled gaps. The
    // discontinuity marker is sticky until the worker has discarded queued
    // history and reset overlap, so even a very short gap cannot be missed.
    setAnalysisConsumer(AnalysisConsumer::Assist, aiEnabledLocal);
    if (analysisNeeded() && !isOffline)
    {
        const int accepted = aiFrontEndFifo.pushStereoMixAllOrNothing(buffer);
        if (accepted != blockSamples)
        {
            aiFrontEndDroppedSamples.fetch_add(blockSamples - accepted,
                                               std::memory_order_relaxed);
            aiFrontEndDiscontinuityPending.store(true, std::memory_order_release);
        }
    }
    else
    {
        aiFrontEndDiscontinuityPending.store(true, std::memory_order_release);
    }

    spectrumDataReady.store(true, std::memory_order_release);

    // Checkpoint 1 — pre-EQ (after param update / spectrum capture, before EQ processing)
    checkClicks(1);

    // A latent phase path is held back until the message thread has installed
    // the matching host-visible latency plan.
    const auto mode = (!paddedLatencyActive && phaseModeSnapshot != PhaseMode::ZeroLatency)
        ? PhaseMode::ZeroLatency
        : phaseModeSnapshot;

    auto resolveNaturalOsEffective = [&]()
    {
        int osUser = oversamplingFactor.load(std::memory_order_relaxed);
        int osEffective = 1; // keep Natural-phase on a coherent 2x/4x HQ path; "Off" maps to 2x.

        if (osUser == 2)
        {
            osEffective = 2;
        }
        else if (osUser == oversamplingAutoIndex)
        {
            // Simple auto heuristic: use 4x if any band has Q > 8 or qualityMode == HQ
            osEffective = (qualityModeCached == 1) ? 2 : 1;
            const int activeBandsForAuto = std::min(numActiveBands.load(std::memory_order_relaxed), maxBands);
            for (int i = 0; i < activeBandsForAuto; ++i)
            {
                float qVal = eqProcessor.getBandQ(i);
                if (qVal > 8.0f)
                {
                    osEffective = 2;
                    break;
                }
            }
        }

        return osEffective;
    };

    auto processNaturalStereo = [&](juce::AudioBuffer<float>& targetBuffer,
                                    int osEffective,
                                    bool updateMeters)
    {
        if (!hqRuntimeReady.load(std::memory_order_acquire))
        {
            oversamplingEffectiveFactor.store(0, std::memory_order_relaxed);

            eqProcessor.process(targetBuffer);

            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(targetBuffer, externalDetectorRaw);
                if (updateMeters)
                    updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }

            return;
        }

        oversamplingEffectiveFactor.store(osEffective, std::memory_order_relaxed);

        juce::dsp::Oversampling<float>* activeOversampler = (osEffective == 2 && oversampler4x)
            ? oversampler4x.get()
            : oversampler2x.get();

        if (activeOversampler)
        {
            juce::dsp::AudioBlock<float> block(targetBuffer.getArrayOfWritePointers(),
                                               static_cast<size_t>(targetBuffer.getNumChannels()),
                                               static_cast<size_t>(targetBuffer.getNumSamples()));
            auto upBlock = activeOversampler->processSamplesUp(block);

            const auto upChannels = static_cast<int>(upBlock.getNumChannels());
            const auto upSamples = static_cast<int>(upBlock.getNumSamples());
            jassert(naturalOversampledBuffer.getNumChannels() >= upChannels);
            jassert(naturalOversampledBuffer.getNumSamples() >= upSamples);
            for (int ch = 0; ch < upChannels; ++ch)
            {
                juce::FloatVectorOperations::copy(naturalOversampledBuffer.getWritePointer(ch),
                                                  upBlock.getChannelPointer(static_cast<size_t>(ch)),
                                                  upSamples);
            }

            juce::AudioBuffer<float> hqProcessBuffer(
                naturalOversampledBuffer.getArrayOfWritePointers(),
                upChannels,
                upSamples);

            eqProcessorHQ.process(hqProcessBuffer);

            if (dynEqEnabledLocal)
            {
                bool detectorProcessed = false;
                if (externalDetectorRaw != nullptr
                    && sidechainInputScratch.getNumSamples() >= targetBuffer.getNumSamples())
                {
                    sidechainInputScratch.copyFrom(
                        0, 0, *externalDetectorRaw, 0, 0,
                        targetBuffer.getNumSamples());
                    juce::dsp::AudioBlock<float> sidechainBlock(
                        sidechainInputScratch.getArrayOfWritePointers(), 1,
                        static_cast<size_t>(targetBuffer.getNumSamples()));
                    auto* sidechainOversampler = osEffective == 2
                        ? sidechainOversampler4x.get()
                        : sidechainOversampler2x.get();
                    if (sidechainOversampler != nullptr)
                    {
                        auto upsampledDetector =
                            sidechainOversampler->processSamplesUp(sidechainBlock);
                        float* detectorChannels[] = {
                            upsampledDetector.getChannelPointer(0)
                        };
                        juce::AudioBuffer<float> detectorView(
                            detectorChannels, 1,
                            static_cast<int>(upsampledDetector.getNumSamples()));
                        dynamicEQProcessorHQ.process(hqProcessBuffer, &detectorView);
                        detectorProcessed = true;
                    }
                }
                if (!detectorProcessed)
                    dynamicEQProcessorHQ.process(hqProcessBuffer, nullptr);
                if (updateMeters)
                    updateDynamicMeterCacheFrom(dynamicEQProcessorHQ);
            }

            for (int ch = 0; ch < upChannels; ++ch)
            {
                juce::FloatVectorOperations::copy(
                    upBlock.getChannelPointer(static_cast<size_t>(ch)),
                    naturalOversampledBuffer.getReadPointer(ch),
                    upSamples);
            }

            activeOversampler->processSamplesDown(block);
        }
        else
        {
            juce::AudioBuffer<float> processView(
                targetBuffer.getArrayOfWritePointers(),
                targetBuffer.getNumChannels(),
                targetBuffer.getNumSamples());
            eqProcessor.process(processView);
            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(processView, externalDetectorRaw);
                if (updateMeters)
                    updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
    };

    auto processStereoForPhaseMode = [&](juce::AudioBuffer<float>& targetBuffer,
                                         PhaseMode targetMode,
                                         bool updateMeters,
                                         int forcedNaturalOsEffective = 0)
    {
        if (targetMode == PhaseMode::NaturalPhase)
        {
            const int osEffective = forcedNaturalOsEffective > 0
                ? forcedNaturalOsEffective
                : resolveNaturalOsEffective();
            processNaturalStereo(targetBuffer, osEffective, updateMeters);
        }
        else if (targetMode == PhaseMode::LinearPhase)
        {
            // Linear Phase path for crossfade transitions
            updateLinearPhaseIRIfNeeded();
            const bool irLoaded = linearIRLoaded[0].load(std::memory_order_acquire);
            if (irLoaded)
            {
                auto* lp = linearPhaseProcessors[0].get();
                if (lp != nullptr)
                {
                    juce::dsp::AudioBlock<float> block(targetBuffer.getArrayOfWritePointers(),
                                                       static_cast<size_t>(targetBuffer.getNumChannels()),
                                                       static_cast<size_t>(targetBuffer.getNumSamples()));
                    juce::dsp::ProcessContextReplacing<float> ctx(block);
                    lp->process(ctx);
                }
                else
                {
                    eqProcessor.process(targetBuffer);
                }
            }
            else
            {
                // No IR yet: fall back to zero-latency EQ
                eqProcessor.process(targetBuffer);
            }

            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(
                    targetBuffer, irLoaded ? externalDetectorLinear
                                           : externalDetectorRaw);
                if (updateMeters)
                    updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
        else
        {
            // ZeroLatency mode
            eqProcessor.process(targetBuffer);

            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(targetBuffer, externalDetectorRaw);
                if (updateMeters)
                    updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
    };

    // ── M/S mode crossfade: save pre-M/S input if transition is active ──
    const int msTransitionRemaining = msModeTransitionSamplesRemaining.load(std::memory_order_acquire);
    const bool msModeTransitionActive = msTransitionRemaining > 0 && totalNumInputChannels >= 2;
    if (msModeTransitionActive)
    {
        const int chs = juce::jmin(buffer.getNumChannels(), msModeTransitionBuffer.getNumChannels());
        for (int ch = 0; ch < chs; ++ch)
            msModeTransitionBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);
    }

    // Encode to M/S if needed (available for all phase modes)
    // Bug N/O fix: Mid-Only and Side-Only modes must NOT use M/S path - they should
    // process the corresponding component through the standard stereo EQ and then
    // reconstruct L/R correctly. The M/S encode+decode path is only correct for
    // MSLinked (process both components independently) because zeroing one component
    // before decode produces -3dB attenuation and/or phase inversion artefacts.
    // Fix: limit useMS to MSLinked only; Mid/Side solo handled after decode.
    const bool useMS = (msModeSnapshot == MSMode::MSLinked) && (totalNumInputChannels >= 2);
    const bool processMid = useMS; // MSLinked always processes both
    const bool processSide = useMS;
    if (useMS)
        encodeMidSide(buffer, blockSamples);

    // ── Phase transition crossfade involving Linear Phase ──────────────
    // When crossfading to/from LP, we handle it here BEFORE the mode-specific
    // blocks to avoid double-processing. processStereoForPhaseMode handles all modes.
    bool lpTransitionHandled = false;
    {
        const int transitionRemaining = phaseTransitionSamplesRemaining.load(std::memory_order_acquire);
        const auto transitionFromMode = static_cast<PhaseMode>(phaseTransitionFromMode.load(std::memory_order_acquire));
        const bool lpTransitionActive = transitionRemaining > 0
            && (mode == PhaseMode::LinearPhase || transitionFromMode == PhaseMode::LinearPhase);

        if (lpTransitionActive
            && phaseTransitionBuffer.getNumChannels() >= buffer.getNumChannels()
            && phaseTransitionBuffer.getNumSamples() >= blockSamples)
        {
            // Check if LP is in fallback mode (IR not yet loaded).
            // When both old and new modes would use the same eqProcessor.process(),
            // running it twice corrupts filter state and causes clicks.
            // In that case, skip the crossfade and process once through the current mode.
            // Detect when both sides of the crossfade would use the same eqProcessor.
            // Case 1: ZL→LP but IR not ready — new mode falls back to ZL EQ
            // Case 2: LP→ZL but IR not ready — old mode falls back to ZL EQ
            // In both cases, running eqProcessor.process() twice corrupts filter state.
            const bool lpIRReady = linearIRLoaded[0].load(std::memory_order_acquire);
            const bool newModeIsFallbackZL = (mode == PhaseMode::LinearPhase) && !lpIRReady;
            const bool oldModeIsFallbackZL = (transitionFromMode == PhaseMode::LinearPhase) && !lpIRReady;
            const bool newIsZL = (mode == PhaseMode::ZeroLatency);
            const bool oldIsZL = (transitionFromMode == PhaseMode::ZeroLatency);
            const bool skipCrossfade = (newModeIsFallbackZL && oldIsZL)
                                    || (oldModeIsFallbackZL && newIsZL);

            lpTransitionHandled = true;

            if (skipCrossfade)
            {
                // Both paths would use eqProcessor — just process once to preserve filter state.
                processStereoForPhaseMode(buffer, PhaseMode::ZeroLatency, true);
                // Drain the crossfade counter so it ends normally
                const int remaining = juce::jmax(0, transitionRemaining - blockSamples);
                phaseTransitionSamplesRemaining.store(remaining, std::memory_order_release);
                if (remaining == 0)
                    phaseTransitionFromMode.store(-1, std::memory_order_release);
            }
            else
            {
                const int chs = juce::jmin(buffer.getNumChannels(), phaseTransitionBuffer.getNumChannels());
                for (int ch = 0; ch < chs; ++ch)
                    phaseTransitionBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);

                // Process main buffer through new (current) mode
                processStereoForPhaseMode(buffer, mode, true);

                // Process transition buffer through old mode
                juce::AudioBuffer<float> oldModeView(phaseTransitionBuffer.getArrayOfWritePointers(),
                                                      buffer.getNumChannels(),
                                                      blockSamples);
                processStereoForPhaseMode(oldModeView, transitionFromMode, false);

                // Crossfade old → new
                const int fadeLen = juce::jmin(transitionRemaining, blockSamples);
                const int fadeProgressStart = phaseTransitionCrossfadeSamples - transitionRemaining;
                for (int ch = 0; ch < chs; ++ch)
                {
                    float* newPtr = buffer.getWritePointer(ch);
                    const float* oldPtr = phaseTransitionBuffer.getReadPointer(ch);
                    for (int s = 0; s < fadeLen; ++s)
                    {
                        const float tBase = static_cast<float>(fadeProgressStart + s)
                                          / static_cast<float>(phaseTransitionCrossfadeSamples);
                        const float t = juce::jlimit(0.0f, 1.0f, tBase);
                        newPtr[s] = oldPtr[s] * (1.0f - t) + newPtr[s] * t;
                    }
                }

                const int remaining = juce::jmax(0, transitionRemaining - blockSamples);
                phaseTransitionSamplesRemaining.store(remaining, std::memory_order_release);
                if (remaining == 0)
                    phaseTransitionFromMode.store(-1, std::memory_order_release);
            }
        }
    }

    // Skip zero-latency/Natural processing when in Linear Phase to avoid double-processing
    if (mode != PhaseMode::LinearPhase && !lpTransitionHandled)
    {
        if (useMS)
        {
            const int samples = blockSamples;

            if (processMid)
                midProcessBuffer.copyFrom(0, 0, buffer, 0, 0, samples);
            else
                midProcessBuffer.clear(0, 0, samples);

            if (processSide)
                sideProcessBuffer.copyFrom(0, 0, buffer, 1, 0, samples);
            else
                sideProcessBuffer.clear(0, 0, samples);

            if (processMid)
            {
                juce::AudioBuffer<float> midView(midProcessBuffer.getArrayOfWritePointers(), 1, samples);
                eqProcessorMid.process(midView);
                if (dynEqEnabledLocal)
                    dynamicEQProcessorMid.process(midView, externalDetectorRaw);
            }

            if (processSide)
            {
                juce::AudioBuffer<float> sideView(sideProcessBuffer.getArrayOfWritePointers(), 1, samples);
                eqProcessorSide.process(sideView);
                if (dynEqEnabledLocal)
                    dynamicEQProcessorSide.process(sideView, externalDetectorRaw);
            }

            if (processMid)
                buffer.copyFrom(0, 0, midProcessBuffer, 0, 0, samples);
            else
                buffer.clear(0, 0, samples);

            if (processSide)
                buffer.copyFrom(1, 0, sideProcessBuffer, 0, 0, samples);
            else
                buffer.clear(1, 0, samples);

            if (dynEqEnabledLocal)
                updateDynamicMeterCacheFromMS(dynamicEQProcessorMid, dynamicEQProcessorSide, processMid, processSide);
        }
        else
        {
            const int transitionRemaining = phaseTransitionSamplesRemaining.load(std::memory_order_acquire);
            const auto transitionFromMode = static_cast<PhaseMode>(phaseTransitionFromMode.load(std::memory_order_acquire));
            const bool zeroNaturalTransition = transitionRemaining > 0
                && ((transitionFromMode == PhaseMode::ZeroLatency && mode == PhaseMode::NaturalPhase)
                 || (transitionFromMode == PhaseMode::NaturalPhase && mode == PhaseMode::ZeroLatency));

            const int currentNaturalOsEffective = resolveNaturalOsEffective();
            const int osTransitionRemaining = oversamplingTransitionSamplesRemaining.load(std::memory_order_acquire);
            const int osTransitionFromEffective = oversamplingTransitionFromEffective.load(std::memory_order_acquire);
            const bool naturalOversamplingTransition = mode == PhaseMode::NaturalPhase
                && osTransitionRemaining > 0
                && osTransitionFromEffective > 0
                && osTransitionFromEffective != currentNaturalOsEffective;

            if (zeroNaturalTransition
                && phaseTransitionBuffer.getNumChannels() >= buffer.getNumChannels()
                && phaseTransitionBuffer.getNumSamples() >= blockSamples)
            {
                const int chs = juce::jmin(buffer.getNumChannels(), phaseTransitionBuffer.getNumChannels());
                for (int ch = 0; ch < chs; ++ch)
                    phaseTransitionBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);

                processStereoForPhaseMode(buffer, mode, true);

                juce::AudioBuffer<float> oldModeView(phaseTransitionBuffer.getArrayOfWritePointers(),
                                                     buffer.getNumChannels(),
                                                     blockSamples);
                processStereoForPhaseMode(oldModeView, transitionFromMode, false);

                const int fadeLen = juce::jmin(transitionRemaining, blockSamples);
                const int fadeProgressStart = phaseTransitionCrossfadeSamples - transitionRemaining;
                for (int ch = 0; ch < chs; ++ch)
                {
                    float* newPtr = buffer.getWritePointer(ch);
                    const float* oldPtr = phaseTransitionBuffer.getReadPointer(ch);
                    for (int s = 0; s < fadeLen; ++s)
                    {
                        const float tBase = static_cast<float>(fadeProgressStart + s)
                                          / static_cast<float>(phaseTransitionCrossfadeSamples);
                        const float t = juce::jlimit(0.0f, 1.0f, tBase);
                        newPtr[s] = oldPtr[s] * (1.0f - t) + newPtr[s] * t;
                    }
                }

                const int remaining = juce::jmax(0, transitionRemaining - blockSamples);
                phaseTransitionSamplesRemaining.store(remaining, std::memory_order_release);
                if (remaining == 0)
                    phaseTransitionFromMode.store(-1, std::memory_order_release);
            }
            else if (naturalOversamplingTransition
                     && oversamplingTransitionBuffer.getNumChannels() >= buffer.getNumChannels()
                     && oversamplingTransitionBuffer.getNumSamples() >= blockSamples)
            {
                const int chs = juce::jmin(buffer.getNumChannels(), oversamplingTransitionBuffer.getNumChannels());
                for (int ch = 0; ch < chs; ++ch)
                    oversamplingTransitionBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);

                processStereoForPhaseMode(buffer, mode, true, currentNaturalOsEffective);

                juce::AudioBuffer<float> oldOsView(oversamplingTransitionBuffer.getArrayOfWritePointers(),
                                                   buffer.getNumChannels(),
                                                   blockSamples);
                processStereoForPhaseMode(oldOsView, mode, false, osTransitionFromEffective);

                const int fadeLen = juce::jmin(osTransitionRemaining, blockSamples);
                const int fadeProgressStart = oversamplingTransitionCrossfadeSamples - osTransitionRemaining;
                for (int ch = 0; ch < chs; ++ch)
                {
                    float* newPtr = buffer.getWritePointer(ch);
                    const float* oldPtr = oversamplingTransitionBuffer.getReadPointer(ch);
                    for (int s = 0; s < fadeLen; ++s)
                    {
                        const float tBase = static_cast<float>(fadeProgressStart + s)
                                          / static_cast<float>(oversamplingTransitionCrossfadeSamples);
                        const float t = juce::jlimit(0.0f, 1.0f, tBase);
                        newPtr[s] = oldPtr[s] * (1.0f - t) + newPtr[s] * t;
                    }
                }

                const int remaining = juce::jmax(0, osTransitionRemaining - blockSamples);
                oversamplingTransitionSamplesRemaining.store(remaining, std::memory_order_release);
                if (remaining == 0)
                    oversamplingTransitionFromEffective.store(-1, std::memory_order_release);
            }
            else
            {
                processStereoForPhaseMode(buffer, mode, true);
            }
        }
    } // end if mode != LinearPhase

    // Linear Phase processing (applied only here, zero-latency path skipped above)
    // Note on M/S: when useMS=true (MSLinked), the buffer is already in M/S domain here
    // (ch0=Mid, ch1=Side). LP processes both channels identically with the same EQ curve,
    // which is correct for MSLinked (same EQ on both components). Mid/Side solo modes
    // are handled after decodeMidSide below, so they do not reach this path in M/S domain.
    if (mode == PhaseMode::LinearPhase && !lpTransitionHandled)
    {
        updateLinearPhaseIRIfNeeded();

        const bool irLoaded = linearIRLoaded[0].load(std::memory_order_acquire);

        if (!irLoaded)
        {
            // No IR yet: fall back to zero-latency EQ
            lpWasFallback = true;
            eqProcessor.process(buffer);
            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(buffer, externalDetectorRaw);
                updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
        else
        {
            // IR just became ready — arm crossfade from fallback ZL to LP convolution
            if (lpWasFallback)
            {
                lpWasFallback = false;
                lpFirstLoadCrossfadeRemaining = lpFirstLoadCrossfadeSamples;
            }

            // If crossfade is active, save a copy of the raw input BEFORE
            // LP convolution so we can render a ZL version for blending.
            // Without this, we'd re-process the previous block's ZL *output*
            // through the EQ again, causing exponential gain accumulation.
            if (lpFirstLoadCrossfadeRemaining > 0)
            {
                // Buffer pre-allocated in prepareToPlay() — no heap allocation here
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    lpFirstLoadFallbackBuf.copyFrom(ch, 0, buffer, ch, 0, blockSamples);
            }

            // Process through LP convolution (primary path)
            auto* lp = linearPhaseProcessors[0].get();
            if (lp != nullptr)
            {
                juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),
                                                   static_cast<size_t>(buffer.getNumChannels()),
                                                   static_cast<size_t>(blockSamples));
                juce::dsp::ProcessContextReplacing<float> ctx(block);
                lp->process(ctx);
            }
            else
            {
                eqProcessor.process(buffer);
            }

            // Crossfade from fallback ZL output to LP convolution output
            if (lpFirstLoadCrossfadeRemaining > 0)
            {
                // Process the fresh input copy through ZL EQ for blending
                eqProcessor.process(lpFirstLoadFallbackBuf);

                const int samplesToFade = juce::jmin(lpFirstLoadCrossfadeRemaining, blockSamples);
                const int fadeStart = lpFirstLoadCrossfadeSamples - lpFirstLoadCrossfadeRemaining;

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto* lpOut = buffer.getWritePointer(ch);
                    const auto* zlOut = lpFirstLoadFallbackBuf.getReadPointer(ch);

                    for (int i = 0; i < samplesToFade; ++i)
                    {
                        const float t = static_cast<float>(fadeStart + i)
                                      / static_cast<float>(lpFirstLoadCrossfadeSamples);
                        lpOut[i] = zlOut[i] * (1.0f - t) + lpOut[i] * t;
                    }
                }
                lpFirstLoadCrossfadeRemaining -= samplesToFade;
            }

            // Bug fix: Dynamic EQ must run after LP convolution in Linear Phase mode.
            // Previously it was skipped entirely when irLoaded=true, causing:
            // 1. Dynamic compression not applied in LP mode
            // 2. GR meter always showing 0 in LP mode
            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(buffer, externalDetectorLinear);
                updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
    }

    // Decode back to L/R if we processed in M/S domain (MSLinked only)
    if (useMS)
        decodeMidSide(buffer, blockSamples);

    // ── M/S crossfade: save post-EQ output before solo section (for non-MSLinked transitions) ──
    const auto previousMSMode = static_cast<MSMode>(previousMSModeForCrossfade.load(std::memory_order_acquire));
    const bool msNonLinkedTransition = msModeTransitionActive
        && previousMSMode != MSMode::MSLinked
        && msModeSnapshot != MSMode::MSLinked;
    if (msNonLinkedTransition)
    {
        // Both old and new modes use the same stereo EQ — difference is only solo encoding.
        // Save post-EQ output so we can apply old mode's solo pattern on a copy.
        const int chs = juce::jmin(buffer.getNumChannels(), msModeTransitionBuffer.getNumChannels());
        for (int ch = 0; ch < chs; ++ch)
            msModeTransitionBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);
    }

    // Bug N/O fix: Mid Only and Side Only solo modes.
    // Encode to M/S, keep only the desired component, decode back.
    // This avoids the -3dB / phase-inversion artifacts from the old approach.
    if (totalNumInputChannels >= 2)
    {
        if (msModeSnapshot == MSMode::Mid)
        {
            // Encode → zero side → decode: result is the Mid component as mono-compatible stereo
            encodeMidSide(buffer, blockSamples);
            buffer.clear(1, 0, blockSamples); // zero Side channel
            decodeMidSide(buffer, blockSamples);
        }
        else if (msModeSnapshot == MSMode::Side)
        {
            // Encode → zero mid → decode: result is the Side (difference) signal
            encodeMidSide(buffer, blockSamples);
            buffer.clear(0, 0, blockSamples); // zero Mid channel
            decodeMidSide(buffer, blockSamples);
        }
    }

    // ── M/S mode crossfade: blend old mode output with new mode output ──
    if (msModeTransitionActive)
    {
        const auto oldMode = previousMSMode;  // already loaded atomically above
        const bool oldIsLinked = (oldMode == MSMode::MSLinked);
        const bool newIsLinked = (msModeSnapshot == MSMode::MSLinked);

        if (oldIsLinked != newIsLinked)
        {
            // Case A: MSLinked ↔ non-MSLinked — different EQ paths, need dual processing.
            // msModeTransitionBuffer holds pre-M/S input (saved earlier).
            // In LinearPhase mode, M/S processing is not used (LP convolver processes stereo),
            // so the transition buffer stays as raw input — the crossfade acts as a simple
            // fade between raw and LP-processed output, which is acceptable.
            if (mode != PhaseMode::LinearPhase)
            {
                if (oldIsLinked)
                {
                    // Old was MSLinked: encode → process mid+side → decode
                    encodeMidSide(msModeTransitionBuffer, blockSamples);
                    {
                        const int samples = blockSamples;
                        midProcessBuffer.copyFrom(0, 0, msModeTransitionBuffer, 0, 0, samples);
                        sideProcessBuffer.copyFrom(0, 0, msModeTransitionBuffer, 1, 0, samples);
                        juce::AudioBuffer<float> midView(midProcessBuffer.getArrayOfWritePointers(), 1, samples);
                        eqProcessorMid.process(midView);
                        if (dynEqEnabledLocal)
                            dynamicEQProcessorMid.process(midView, externalDetectorRaw);
                        juce::AudioBuffer<float> sideView(sideProcessBuffer.getArrayOfWritePointers(), 1, samples);
                        eqProcessorSide.process(sideView);
                        if (dynEqEnabledLocal)
                            dynamicEQProcessorSide.process(sideView, externalDetectorRaw);
                        msModeTransitionBuffer.copyFrom(0, 0, midProcessBuffer, 0, 0, samples);
                        msModeTransitionBuffer.copyFrom(1, 0, sideProcessBuffer, 0, 0, samples);
                    }
                    decodeMidSide(msModeTransitionBuffer, blockSamples);
                }
                else
                {
                    // Old was Stereo/Mid/Side: process through stereo EQ.
                    // blockSamples-limited VIEW, like oldModeView/oldOsView in the sibling
                    // crossfades: msModeTransitionBuffer is preallocated at
                    // preallocatedMaxSamples (32768), and passing it raw made
                    // processNaturalStereo size its AudioBlock at getNumSamples()==32768 —
                    // driving the oversampler (initProcessing'd for samplesPerBlock) into a
                    // heap-buffer-overflow (juce_Oversampling.cpp:355). See
                    // MSTransitionOversamplingOverflowTest.
                    juce::AudioBuffer<float> oldMsView(msModeTransitionBuffer.getArrayOfWritePointers(),
                                                       buffer.getNumChannels(),
                                                       blockSamples);
                    processStereoForPhaseMode(oldMsView, mode, false);
                    // Apply old mode's solo encoding
                    if (totalNumInputChannels >= 2)
                    {
                        if (oldMode == MSMode::Mid)
                        {
                            encodeMidSide(msModeTransitionBuffer, blockSamples);
                            msModeTransitionBuffer.clear(1, 0, blockSamples);
                            decodeMidSide(msModeTransitionBuffer, blockSamples);
                        }
                        else if (oldMode == MSMode::Side)
                        {
                            encodeMidSide(msModeTransitionBuffer, blockSamples);
                            msModeTransitionBuffer.clear(0, 0, blockSamples);
                            decodeMidSide(msModeTransitionBuffer, blockSamples);
                        }
                    }
                }
            }
        }
        else
        {
            // Case B: Both modes use the same EQ path (Stereo/Mid/Side ↔ Stereo/Mid/Side,
            // or MSLinked ↔ MSLinked). msModeTransitionBuffer holds post-EQ output
            // (saved before the solo section). Apply old mode's solo encoding.
            if (totalNumInputChannels >= 2)
            {
                if (oldMode == MSMode::Mid)
                {
                    encodeMidSide(msModeTransitionBuffer, blockSamples);
                    msModeTransitionBuffer.clear(1, 0, blockSamples);
                    decodeMidSide(msModeTransitionBuffer, blockSamples);
                }
                else if (oldMode == MSMode::Side)
                {
                    encodeMidSide(msModeTransitionBuffer, blockSamples);
                    msModeTransitionBuffer.clear(0, 0, blockSamples);
                    decodeMidSide(msModeTransitionBuffer, blockSamples);
                }
                // else oldMode == Stereo: no encode/decode needed, buffer is already stereo EQ output
            }
        }

        // Sample-by-sample linear crossfade: old → new
        const int fadeLen = juce::jmin(msTransitionRemaining, blockSamples);
        const int fadeProgressStart = msModeTransitionCrossfadeSamples - msTransitionRemaining;
        const int chs = juce::jmin(buffer.getNumChannels(), msModeTransitionBuffer.getNumChannels());
        for (int ch = 0; ch < chs; ++ch)
        {
            float* newOut = buffer.getWritePointer(ch);
            const float* oldOut = msModeTransitionBuffer.getReadPointer(ch);
            for (int s = 0; s < fadeLen; ++s)
            {
                const float t = juce::jlimit(0.0f, 1.0f,
                    static_cast<float>(fadeProgressStart + s)
                  / static_cast<float>(msModeTransitionCrossfadeSamples));
                newOut[s] = oldOut[s] * (1.0f - t) + newOut[s] * t;
            }
        }
        msModeTransitionSamplesRemaining.store(
            juce::jmax(0, msTransitionRemaining - blockSamples), std::memory_order_release);
    }

    // Global dry/wet mix is applied AFTER wet padding (below). Mixing here
    // delayed dry by the pad a second time: at 100% wet the dry path is silent
    // so PDC sounded aligned; lowering the mix introduced a pad-length flam.
    // Solo / post-EQ meters / output gain stay on the wet path only.

    // === SOLO ACOUSTIC MONITOR ===
    constexpr bool enableSoloMonitor = true; // Enabled: allow band audition in Solo mode
    if (hasSolo && enableSoloMonitor)
    {
        float soloFreq = 1000.0f;
        float soloQ = 1.0f;
        bool foundSolo = false;

        for (int i = 0; i < activeBandsLocal && !foundSolo; ++i)
        {
            const auto& p = cachedParams[static_cast<size_t>(i)];
            const bool enabled = loadParam(p.enabled, 0.0f) > 0.5f;
            const bool solo = loadParam(p.solo, 0.0f) > 0.5f;

            if (enabled && solo)
            {
                soloFreq = loadParam(p.freq, 1000.0f);
                soloQ = loadParam(p.q, 1.0f);
                foundSolo = true;
            }
        }

        if (foundSolo)
        {
            const double sr = currentSampleRate.load(std::memory_order_relaxed);
            const int numSamples = buffer.getNumSamples();

            // Update coefficients only when freq/Q actually change.
            // Also reset filter state on first use (wasSoloed == false)
            // to avoid stale IIR state producing a click on first block.
            const bool freqChanged = std::abs(soloFreq - lastSoloFreq) > 0.5f
                                  || std::abs(soloQ    - lastSoloQ)    > 0.01f;
            if (freqChanged || !wasSoloed)
            {
                auto coeffs = juce::IIRCoefficients::makeBandPass(sr, soloFreq, soloQ);

                if (!wasSoloed)
                {
                    // First enable: reset state + warmup + crossfade
                    soloMonitorFilterL.reset();
                    soloMonitorFilterR.reset();
                    soloMonitorFilterL.setCoefficients(coeffs);
                    soloMonitorFilterR.setCoefficients(coeffs);

                    // Warm up filter with pre-EQ input to avoid transient on enable
                    if (preProcessingInputCopy.getNumSamples() >= numSamples)
                    {
                        constexpr int warmupBlocks = 2;
                        const int warmupChs = juce::jmin(soloWarmupBuffer.getNumChannels(), preProcessingInputCopy.getNumChannels());
                        for (int wb = 0; wb < warmupBlocks; ++wb)
                        {
                            for (int ch = 0; ch < warmupChs; ++ch)
                                soloWarmupBuffer.copyFrom(ch, 0, preProcessingInputCopy, ch, 0, numSamples);
                            if (warmupChs > 0)
                                soloMonitorFilterL.processSamples(soloWarmupBuffer.getWritePointer(0), numSamples);
                            if (warmupChs > 1)
                                soloMonitorFilterR.processSamples(soloWarmupBuffer.getWritePointer(1), numSamples);
                        }
                    }

                    soloCrossfadeRemaining = soloCrossfadeSamples;
                }
                else
                {
                    // Already soloed, freq/Q changed during drag:
                    // Update coefficients WITHOUT resetting filter state.
                    // The IIR state adapts naturally to the new coefficients,
                    // avoiding the impulse/crackle caused by a hard reset.
                    soloMonitorFilterL.setCoefficients(coeffs);
                    soloMonitorFilterR.setCoefficients(coeffs);
                }

                lastSoloFreq = soloFreq;
                lastSoloQ    = soloQ;
            }

            // Build solo output using pre-allocated buffer (NO heap allocation)
            const int soloChs = juce::jmin(soloOutputBuffer.getNumChannels(), buffer.getNumChannels());
            if (preProcessingInputCopy.getNumChannels() >= buffer.getNumChannels()
                && preProcessingInputCopy.getNumSamples() >= numSamples)
            {
                for (int ch = 0; ch < soloChs; ++ch)
                    soloOutputBuffer.copyFrom(ch, 0, preProcessingInputCopy, ch, 0, numSamples);
            }
            else
            {
                for (int ch = 0; ch < soloChs; ++ch)
                    soloOutputBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            }

            if (soloChs > 0)
                soloMonitorFilterL.processSamples(soloOutputBuffer.getWritePointer(0), numSamples);
            if (soloChs > 1)
                soloMonitorFilterR.processSamples(soloOutputBuffer.getWritePointer(1), numSamples);

            // Apply makeup gain in-place on pre-allocated buffer
            for (int ch = 0; ch < soloChs; ++ch)
                juce::FloatVectorOperations::multiply(soloOutputBuffer.getWritePointer(ch),
                    juce::Decibels::decibelsToGain(soloMakeupGainDB), numSamples);

            // Crossfade from wet EQ output → solo bandpass output to avoid click on enable
            if (soloCrossfadeRemaining > 0)
            {
                const int fadeLen = juce::jmin(soloCrossfadeRemaining, numSamples);
                for (int ch = 0; ch < soloChs; ++ch)
                {
                    float* wet  = buffer.getWritePointer(ch);
                    const float* dryS = soloOutputBuffer.getReadPointer(ch);
                    for (int s = 0; s < fadeLen; ++s)
                    {
                        const float t = static_cast<float>(soloCrossfadeSamples - soloCrossfadeRemaining + s)
                                      / static_cast<float>(soloCrossfadeSamples);
                        wet[s] = wet[s] * (1.0f - t) + dryS[s] * t;
                    }
                    // Rest of block: pure solo
                    for (int s = fadeLen; s < numSamples; ++s)
                        wet[s] = dryS[s];
                }
                soloCrossfadeRemaining = juce::jmax(0, soloCrossfadeRemaining - numSamples);
            }
            else
            {
                // Full solo — copy directly from pre-allocated buffer
                for (int ch = 0; ch < soloChs; ++ch)
                    buffer.copyFrom(ch, 0, soloOutputBuffer, ch, 0, numSamples);
            }

            wasSoloed = true;
        }
    }

    // Solo was active last block but not now — crossfade back to normal
    if (!hasSolo && wasSoloed)
    {
        wasSoloed = false;
        soloCrossfadeRemaining = 0;
        lastSoloFreq = -1.0f;
        lastSoloQ    = -1.0f;
        soloMonitorFilterL.reset();
        soloMonitorFilterR.reset();
    }

    // Checkpoint 2 — post-EQ
    checkClicks(2);

    // Feed post-EQ spectrum analyzer
    if (showPost)
    {
        postEQAnalyzer.pushSamples(buffer);
        postEqSpectrumFifo.pushStereoMix(buffer);  // metrological pipeline FIFO
        spectrumDataReady.store(true, std::memory_order_release);
    }

    // Calculate auto-gain compensation
    if (autoGainEnabledLocal)
    {
        float currentPostRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (totalNumInputChannels > 1)
            currentPostRMS = (currentPostRMS + buffer.getRMSLevel(1, 0, buffer.getNumSamples())) * 0.5f;

        // FIX: Use atomic load/store for thread-safe access
        float currentRMS = postEQRMS.load(std::memory_order_relaxed);
        postEQRMS.store(currentRMS * rmsSmoothing + currentPostRMS * (1.0f - rmsSmoothing),
                        std::memory_order_relaxed);

        autoGainBlockCounter = (autoGainBlockCounter + 1) % autoGainUpdateStride;
        if (autoGainBlockCounter == 0)
            calculateAutoGain();
    }
    else
    {
        autoGainBlockCounter = 0;
    }

    // D1 (AI-evolution): per-band DYNAMIC corrections — soothe-class engine on
    // the AI's approved corrections. Gated OFF by default (engine no-ops until
    // enabled AND a snapshot is published); runs on the WET path pre output
    // gain, uniformly for every phase mode. Promotion to a shipped default is
    // a separate corpus-gated decision.
    // D1 exposure: per-block sync from the APVTS param (same cached-pointer
    // pattern as autoGain). setEnabled is a relaxed atomic store, documented
    // any-thread — no listener/marshaling needed.
    dynamicCorrectionEngine.setEnabled(loadParam(cachedDynamicCorrections, 0.0f) > 0.5f);
    dynamicCorrectionEngine.process(buffer);

    // Wet-only auto-gain makeup, before padding/mix. Manual Output Gain is applied
    // after the mix so trim still works at 0% wet and does not change the blend ratio.
    const float autoGainDB = autoGainEnabledLocal
        ? autoGainCompensation.load(std::memory_order_relaxed)
        : 0.0f;
    smoothedAutoGain.setTargetValue(juce::Decibels::decibelsToGain(autoGainDB));
    const int numSamples = buffer.getNumSamples();
    smoothedAutoGain.applyGain(buffer, numSamples);

    // ── Wet padding delay: align wet output with reported worst-case latency ──
    // LP convolver provides full latency naturally (partSize + irSize/2).
    // ZL and NaturalPhase modes need explicit padding so delayed dry (captured
    // at block start with dryDelayLength = activeSamples) lines up with wet
    // for dry/wet mix, bypass crossfade, and DAW PDC.
    //
    // Cat 2 Fix (phase 0->1 click): when phaseMode switches, padSamples changes
    // abruptly (e.g. 2176 -> 2161 for ZL->NaturalPhase with 2x oversampling).
    // The old integer read jumped the ring position by ~15 samples in one block,
    // causing a ~0.6-amplitude click on a 1 kHz sine. During a phase transition
    // we now linearly ramp the pad target from wetPadRampStart to padSamples
    // over phaseTransitionCrossfadeSamples and use fractional-delay reads so
    // the read position creeps smoothly — inaudible micro-pitch-shift.
    {
        int actualWetLatency = 0;
        if (mode == PhaseMode::LinearPhase
            && linearIRLoaded[0].load(std::memory_order_acquire))
        {
            actualWetLatency = static_cast<int>(
                LinearPhaseProcessor::partSize + LinearPhaseProcessor::irSize / 2);
        }
        else if (mode == PhaseMode::NaturalPhase)
        {
            actualWetLatency = worstCaseOversamplingLatency;
        }
        // else ZL: phase latency = 0

        if (dynEqEnabledLocal && qualityMode == 1)
            actualWetLatency += static_cast<int>(std::round(
                currentSampleRate.load(std::memory_order_relaxed) * 0.005));

        const int activeLatency = latencyPlan.activeSamples.load(std::memory_order_acquire);
        const int padSamples = juce::jmax(0,
            juce::jmin(activeLatency - actualWetLatency, wetPaddingBufferSize - 1));

        // Phase transition ramp detection (uses pre-crossfade snapshot so we
        // stay in sync with the EQ crossfade counter that was decremented earlier).
        const bool transitionActive = phaseTransitionRemainingAtBlockStart > 0;
        if (transitionActive && !wetPadRampActive)
        {
            // Edge: transition just started -> snapshot previous block's pad
            // as the ramp start. Clamp to a valid ring offset.
            wetPadRampStart = juce::jlimit(0, wetPaddingBufferSize - 1, wetPadLastSamples);
            wetPadRampActive = true;
        }
        else if (!transitionActive && wetPadRampActive)
        {
            // Edge: transition just ended -> disarm ramp
            wetPadRampActive = false;
        }

        const int chs = juce::jmin(buffer.getNumChannels(),
                                    wetPaddingDelayBuffer.getNumChannels());

        if (wetPadRampActive && wetPaddingBufferSize > 0 && chs > 0
            && wetPadRampStart != padSamples)
        {
            // Fractional-delay ramp from wetPadRampStart -> padSamples over the
            // phase transition window. Per-sample t advances by 1/total.
            const float padStartF = static_cast<float>(wetPadRampStart);
            const float padEndF   = static_cast<float>(padSamples);
            const int   total     = juce::jmax(1, phaseTransitionCrossfadeSamples);
            const int   fadeProgressStart = total - phaseTransitionRemainingAtBlockStart;
            const float ringSizeF = static_cast<float>(wetPaddingBufferSize);

            for (int ch = 0; ch < chs; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                float* padBuf = wetPaddingDelayBuffer.getWritePointer(ch);
                for (int s = 0; s < blockSamples; ++s)
                {
                    const int wp = (wetPaddingWritePos + s) % wetPaddingBufferSize;
                    const float incoming = data[s];

                    const float t = juce::jlimit(0.0f, 1.0f,
                                    static_cast<float>(fadeProgressStart + s)
                                  / static_cast<float>(total));
                    const float localPad = padStartF * (1.0f - t) + padEndF * t;

                    // Fractional delay: linear interpolation between two
                    // adjacent ring positions (wp - floor(pad)) and (wp - floor(pad) - 1)
                    float rpf = static_cast<float>(wp) - localPad;
                    while (rpf < 0.0f)        rpf += ringSizeF;
                    while (rpf >= ringSizeF)  rpf -= ringSizeF;

                    const int   rp0  = static_cast<int>(rpf);
                    const float frac = rpf - static_cast<float>(rp0);
                    const int   rp1  = (rp0 + 1 < wetPaddingBufferSize)
                                         ? rp0 + 1 : 0;

                    data[s] = padBuf[rp0] * (1.0f - frac) + padBuf[rp1] * frac;
                    padBuf[wp] = incoming;
                }
            }
        }
        else if (padSamples > 0 && wetPaddingBufferSize > 0)
        {
            // Steady-state integer-delay path (common case, zero overhead vs. original)
            for (int ch = 0; ch < chs; ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                float* padBuf = wetPaddingDelayBuffer.getWritePointer(ch);
                for (int s = 0; s < blockSamples; ++s)
                {
                    const int wp = (wetPaddingWritePos + s) % wetPaddingBufferSize;
                    const float incoming = data[s];
                    int rp = wp - padSamples;
                    if (rp < 0) rp += wetPaddingBufferSize;
                    data[s] = padBuf[rp];
                    padBuf[wp] = incoming;
                }
            }
        }
        else if (wetPaddingBufferSize > 0)
        {
            // No padding needed (LP mode) — still feed ring buffer so it has
            // valid data if mode switches to ZL/NP later.
            for (int ch = 0; ch < chs; ++ch)
            {
                const float* data = buffer.getReadPointer(ch);
                float* padBuf = wetPaddingDelayBuffer.getWritePointer(ch);
                for (int s = 0; s < blockSamples; ++s)
                {
                    const int wp = (wetPaddingWritePos + s) % wetPaddingBufferSize;
                    padBuf[wp] = data[s];
                }
            }
        }
        wetPaddingWritePos = (wetPaddingWritePos + blockSamples) % wetPaddingBufferSize;

        // Record this block's applied pad so the next transition's ramp can
        // pick up where this block left off.
        wetPadLastSamples = padSamples;
    }

    // Apply global dry/wet mix after padding so delayed dry and padded wet share
    // the host-visible latency. Solo already replaced the wet buffer and must
    // not be blended back with dry (same as the previous mix-then-overwrite
    // order).
    if (needsDry && !hasSolo)
    {
        const int chs = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());
        for (int ch = 0; ch < chs; ++ch)
        {
            auto* wetPtr = buffer.getWritePointer(ch);
            const auto* dryPtr = dryBuffer.getReadPointer(ch);
            for (int i = 0; i < blockSamples; ++i)
                wetPtr[i] = dry * dryPtr[i] + wet * wetPtr[i];
        }
    }

    // Manual output trim on the full mix (and on solo). Bypass still fades to
    // delayed dry that never received this gain.
    smoothedOutputGain.setTargetValue(juce::Decibels::decibelsToGain(outputGainDB));
    smoothedOutputGain.applyGain(buffer, numSamples);

    // Checkpoint 4 — OUTPUT (after auto-gain, pad, mix, output trim; before bypass)
    checkClicks(4);

    // Bypass crossfade: blend processed+trimmed output ↔ dry (ungained) to match
    // steady-state bypass, which copies the delayed dry reference.
    {
        int remaining = bypassCrossfadeRemaining.load(std::memory_order_relaxed);
        const auto bpPhase = bypassPhase.load(std::memory_order_relaxed);
        const bool fadingToDry = (bpPhase == BypassPhase::FadingToBypass);
        const bool fadingToWet = (bpPhase == BypassPhase::FadingToActive);

        if (remaining > 0 && (fadingToDry || fadingToWet))
        {
            const int fadeLen = juce::jmin(remaining, numSamples);
            const int fadeTotal = juce::jmax(1, currentBypassCrossfadeSamples);
            const int fadeProgressStart = fadeTotal - remaining;
            const int chs = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());

            for (int ch = 0; ch < chs; ++ch)
            {
                float* out = buffer.getWritePointer(ch);
                const float* dry = dryBuffer.getReadPointer(ch);

                for (int s = 0; s < fadeLen; ++s)
                {
                    const float t = juce::jlimit(0.0f, 1.0f,
                                    static_cast<float>(fadeProgressStart + s)
                                  / static_cast<float>(fadeTotal));
                    // t: 0→1.  FadingToBypass: wet→dry.  FadingToActive: dry→wet.
                    if (fadingToDry)
                        out[s] = out[s] * (1.0f - t) + dry[s] * t;
                    else
                        out[s] = dry[s] * (1.0f - t) + out[s] * t;
                }

                // After crossfade ends mid-block: rest is target signal
                if (fadingToDry)
                {
                    for (int s = fadeLen; s < numSamples; ++s)
                        out[s] = dry[s];
                }
            }

            const int newRemaining = juce::jmax(0, remaining - numSamples);
            bypassCrossfadeRemaining.store(newRemaining, std::memory_order_relaxed);

            // Crossfade complete → transition to next phase
            if (newRemaining <= 0)
            {
                if (fadingToDry)
                    bypassPhase.store(BypassPhase::Bypassed, std::memory_order_relaxed);
                else
                    bypassPhase.store(BypassPhase::Active, std::memory_order_relaxed);
            }
        }
    }

    // Checkpoint 5 — post bypass-crossfade (final output)
    checkClicks(5);

    // ── Safety soft limiter ────────────────────────────────────────────
    // Exactly transparent through +/-8, then C1-soft-limited towards +/-40.
    // Non-finite samples are flushed to zero and counted diagnostically.
    {
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int s = 0; s < numSamples; ++s)
            {
                bool fault = false;
                data[s] = AIEQDSP::NumericSafety::safetyLimit(data[s], fault);
                if (fault)
                    safetyLimiterFaultCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    // Output peak metering (lock-free, for GUI level meter)
    {
        float peakL = buffer.getMagnitude(0, 0, numSamples);
        float peakR = totalNumInputChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : peakL;
        outputPeakLeft.store(peakL, std::memory_order_relaxed);
        outputPeakRight.store(peakR, std::memory_order_relaxed);
    }
}

void AIEqualizerAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Increment the general parameter change counter for host automation, etc.
    parameterChangeCounter.fetch_add(1, std::memory_order_relaxed);

    // --- OPTIMIZATION: Selective EQ Curve Counter Increment ---
    // Only increment the EQ curve counter if a parameter that visually affects
    // the curve has changed. This prevents the expensive EQ image rebuild
    // from being triggered by host polling of non-visual parameters.
    
    bool affectsEQCurve = false;
    if (parameterID.startsWith("band"))
    {
        // A simple string check is faster than parsing the band index.
        // We only care if *any* band's visual parameter has changed.
        if (parameterID.contains("Freq") || parameterID.contains("Gain") || parameterID.contains("Q") ||
            parameterID.contains("Type") || parameterID.contains("Enabled") || parameterID.contains("Slope") ||
            parameterID.contains("CurveMode"))
        {
            affectsEQCurve = true;
        }
    }
    else if (parameterID == "numActiveBands")
    {
        affectsEQCurve = true;
    }

    if (affectsEQCurve)
    {
        eqCurveChangeCounter.fetch_add(1, std::memory_order_relaxed);
    }

    // --- End of Optimization ---

    // Handle specific parameter changes that require immediate action
    if (parameterID == "phaseMode")
    {
        const int modeIdx = juce::jlimit(0, 2, static_cast<int>(std::round(newValue)));
        const auto newMode = static_cast<PhaseMode>(modeIdx);
        const auto oldMode = currentPhaseMode.exchange(newMode);

        if (newMode != oldMode)
        {
            // Arm crossfade for ALL phase mode transitions (including Linear Phase).
            // Previously only ZeroLatency <-> NaturalPhase used crossfade; LP transitions
            // used pendingReset which caused audible clicks.
            phaseTransitionFromMode.store(static_cast<int>(oldMode), std::memory_order_release);
            phaseTransitionSamplesRemaining.store(phaseTransitionCrossfadeSamples, std::memory_order_release);
        }

        if (newMode == PhaseMode::LinearPhase)
        {
            consecutiveIRReadyBlocks.store(0, std::memory_order_relaxed);
            readyIRIndex.store(-1, std::memory_order_relaxed);
            crossfadeSamplesRemaining.store(0, std::memory_order_relaxed);
            triggerLinearPhaseIRUpdate();
        }

        updateReportedLatency();
    }
    else if (parameterID == "msMode")
    {
        const int modeIdx = juce::jlimit(0, 3, static_cast<int>(std::round(newValue)));
        const auto newMode = static_cast<MSMode>(modeIdx);
        const auto oldMode = currentMSMode.exchange(newMode);
        if (oldMode != newMode)
        {
            previousMSModeForCrossfade.store(static_cast<int>(oldMode), std::memory_order_relaxed);
            msModeTransitionSamplesRemaining.store(msModeTransitionCrossfadeSamples, std::memory_order_release);
        }
    }
    else if (parameterID == "oversamplingFactor")
    {
        const int factor = juce::jlimit(0, 3, static_cast<int>(std::round(newValue)));
        const int oldFactor = oversamplingFactor.exchange(factor);

        auto normalizeNaturalPath = [](int value)
        {
            if (value == 2)
                return 2; // 4x
            if (value == oversamplingAutoIndex)
                return oversamplingAutoIndex;
            return 1; // Off and 2x both share the 2x Natural path
        };

        const int oldEffective = normalizeNaturalPath(oldFactor);
        const int newEffective = normalizeNaturalPath(factor);

        if (oldEffective != newEffective)
        {
            const bool naturalPhaseActive = currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::NaturalPhase;
            const bool canCrossfadeNaturalOversampling = naturalPhaseActive
                && oldEffective > 0
                && newEffective > 0
                && oldEffective != oversamplingAutoIndex
                && newEffective != oversamplingAutoIndex;

            if (canCrossfadeNaturalOversampling)
            {
                oversamplingTransitionFromEffective.store(oldEffective, std::memory_order_release);
                oversamplingTransitionSamplesRemaining.store(oversamplingTransitionCrossfadeSamples,
                                                            std::memory_order_release);
            }
            else
            {
                oversamplingTransitionFromEffective.store(-1, std::memory_order_release);
                oversamplingTransitionSamplesRemaining.store(0, std::memory_order_release);
                pendingReset.store(true, std::memory_order_release);
            }
        }
    }
    else if (parameterID == "qualityMode" || parameterID == "dynEqEnabled")
    {
        updateReportedLatency();
    }
    else if (parameterID == "aiSensitivity" || parameterID == "aiStrength")
    {
        // (B) Push the new value to the AI engine NOW, from the message thread.
        // setSensitivity/setStrength are plain atomic stores (safe off the audio
        // thread). updateEQFromParameters() also pushes it, but ONLY inside
        // processBlock; when transport is stopped processBlock never runs, so
        // without this direct push the AI thread would re-analyze with the stale
        // value. During playback this just stores the same value twice (idempotent).
        if (parameterID == "aiSensitivity")
            aiEngine.setSensitivity(newValue);
        else
            aiEngine.setStrength(newValue);

        // (A-arm) Ask the AI thread to force ONE re-analysis of the last spectrum,
        // so the amber bars / AI panel update even though no new audio frame will
        // arrive (transport stopped). release pairs with the thread's acquire
        // exchange and also publishes the setSensitivity store above.
        aiPendingReanalysis.store(true, std::memory_order_release);
        aiSpectrumEvent.signal();
    }

    // Band changes always dirty the visible EQ/analyzer curve, but only Linear Phase
    // actually needs a background IR rebuild. Decoupling these avoids arming the LP
    // machinery on every live edit in Zero/Natural modes.
    if (parameterID.startsWith("band") || parameterID == "numActiveBands")
    {
        triggerEQCurveUpdate();

        // Only rebuild IR for parametric EQ changes (freq/gain/Q/type/enabled/slope).
        // Dynamic EQ params (threshold/ratio/attack/release/range/knee/mode/detection)
        // do NOT affect the IR — dynamic EQ runs as biquad post-convolver.
        // Rebuilding IR on dynamic-only changes caused unnecessary 93ms crossfades
        // in the convolver, contributing to crackle during threshold adjustment.
        if (affectsEQCurve && currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::LinearPhase)
            triggerLinearPhaseIRUpdate();
    }

    // Mark parameters as needing update for the audio thread's next processing block.
    parametersNeedUpdate.store(true, std::memory_order_release);
    markParametersChanged();
}

void AIEqualizerAudioProcessor::triggerEQCurveUpdate()
{
    eqCurveNeedsUpdate.store(true, std::memory_order_release);
}

void AIEqualizerAudioProcessor::triggerLinearPhaseIRUpdate()
{
    eqCurveNeedsUpdate.store(true, std::memory_order_release);
    requestIRBuild();
}

void AIEqualizerAudioProcessor::updateLinearPhaseIRIfNeeded()
{
    if (linearIRTestOverridePinned.load(std::memory_order_acquire))
    {
        // Keep the explicitly injected fixture authoritative. Drain any result
        // produced by an earlier queued request so the mailbox remains reusable.
        auto ignored = pendingFreqIR.acquireLatest();
        if (ignored)
            pendingFreqIR.release(ignored);
        return;
    }

    // Claim and pin are one compare-exchange: while this view is held the slot
    // is READING and the builder cannot reclaim or overwrite it. Bounded
    // retries inside acquireLatest() keep this audio-thread call free of any
    // unbounded loop; an empty view simply means "no newer IR yet", and the
    // next block will pick it up.
    auto view = pendingFreqIR.acquireLatest();
    if (!view)
        return;

    // Feed pre-partitioned IR data to LinearPhaseProcessor.
    // The builder thread already did the heavy IFFT + partition FFTs;
    // this only copies the partitions and arms the crossfade (no FFT work).
    auto* lp = linearPhaseProcessors[0].get();
    if (lp != nullptr)
        lp->storePrePartitionedIRDirect(view.payload->data());

    // Hand the slot back so the builder may reuse it.
    pendingFreqIR.release(view);

    linearIRLoaded[0].store(true, std::memory_order_release);
    activeIRIndex.store(0, std::memory_order_relaxed);
}

void AIEqualizerAudioProcessor::forceLinearIRReady()
{
    // Build a Dirac-delta IR (flat magnitude, zero phase = identity convolution).
    // This injects the IR directly into the convolver, bypassing the builder thread,
    // so tests can deterministically control when linearIRLoaded transitions to true.
    linearIRTestOverridePinned.store(true, std::memory_order_release);

    std::vector<float> diracIR(PartitionedConvolver::irSize, 0.0f);
    diracIR[0] = 1.0f;  // unit impulse → flat frequency response

    // Pre-partition into frequency domain (same path as the builder thread)
    std::vector<float> packed(PartitionedConvolver::numParts *
                              PartitionedConvolver::fftPartSize * 2, 0.0f);
    PartitionedConvolver::buildPackedPartitions(diracIR.data(), diracIR.size(), packed.data());

    // Inject into the LP processor
    auto* lp = linearPhaseProcessors[0].get();
    if (lp != nullptr)
        lp->storePrePartitionedIRDirect(packed.data());

    // Mark IR as loaded — this is the flag processBlock() checks
    linearIRLoaded[0].store(true, std::memory_order_release);
    activeIRIndex.store(0, std::memory_order_relaxed);
}

void AIEqualizerAudioProcessor::requestIRBuild()
{
    // Record timestamp; the IR builder thread checks if debounce has elapsed
    irBuildRequestedAt.store(juce::Time::currentTimeMillis(),
                             std::memory_order_release);
    irBuildEvent.signal();
}


//==============================================================================
void AIEqualizerAudioProcessor::calculateAutoGain()
{
    // FIX: Use atomic load for thread-safe access (UI may read these values)
    const float currentPreRMS = preEQRMS.load(std::memory_order_relaxed);
    const float currentPostRMS = postEQRMS.load(std::memory_order_relaxed);

    // Avoid division by zero and handle silence
    constexpr float minRMS = 0.0001f;
    if (currentPostRMS < minRMS || currentPreRMS < minRMS)
    {
        autoGainCompensation.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Calculate the dB difference
    float preDB = juce::Decibels::gainToDecibels(currentPreRMS, -100.0f);
    float postDB = juce::Decibels::gainToDecibels(currentPostRMS, -100.0f);

    // Compensation = how much we need to boost to match pre-EQ level
    float targetCompensation = preDB - postDB;

    // Limit compensation range (-12 to +12 dB) for safety
    constexpr float maxCompensation = 12.0f;
    targetCompensation = juce::jlimit(-maxCompensation, maxCompensation, targetCompensation);

    // Smooth the compensation to avoid sudden jumps (1% per sample at 60Hz = ~0.6s time constant)
    constexpr float smoothingFactor = 0.99f;
    const float currentComp = autoGainCompensation.load(std::memory_order_relaxed);
    autoGainCompensation.store(currentComp * smoothingFactor + targetCompensation * (1.0f - smoothingFactor),
                               std::memory_order_relaxed);
}

//==============================================================================
DynamicEQProcessor::BandMeter AIEqualizerAudioProcessor::getDynamicBandMeter(int bandIndex) const noexcept
{
    DynamicEQProcessor::BandMeter meter;

    if (bandIndex >= 0 && bandIndex < maxBands)
    {
        const auto& entry = dynamicMeterCache[static_cast<size_t>(bandIndex)];
        meter.inputLevel = entry.input.load(std::memory_order_relaxed);
        meter.gainReduction = entry.gainReduction.load(std::memory_order_relaxed);
        meter.outputLevel = entry.output.load(std::memory_order_relaxed);
    }

    return meter;
}

float AIEqualizerAudioProcessor::getDynamicTotalGainReduction() const noexcept
{
    return dynamicTotalGR.load(std::memory_order_relaxed);
}

DynamicEQProcessor::DetectorAvailability
AIEqualizerAudioProcessor::getDynamicDetectorAvailability(int bandIndex) const noexcept
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return DynamicEQProcessor::DetectorAvailability::ExternalUnavailable;

    const auto phase = currentPhaseMode.load(std::memory_order_relaxed);
    const auto msMode = currentMSMode.load(std::memory_order_relaxed);
    if (phase != PhaseMode::LinearPhase && msMode == MSMode::MSLinked)
    {
        const auto mid = dynamicEQProcessorMid.getDetectorAvailability(bandIndex);
        const auto side = dynamicEQProcessorSide.getDetectorAvailability(bandIndex);
        if (mid == DynamicEQProcessor::DetectorAvailability::ExternalUnavailable
            || side == DynamicEQProcessor::DetectorAvailability::ExternalUnavailable)
            return DynamicEQProcessor::DetectorAvailability::ExternalUnavailable;
        if (mid == DynamicEQProcessor::DetectorAvailability::ExternalAvailable
            || side == DynamicEQProcessor::DetectorAvailability::ExternalAvailable)
            return DynamicEQProcessor::DetectorAvailability::ExternalAvailable;
        return DynamicEQProcessor::DetectorAvailability::Internal;
    }

    if (phase == PhaseMode::NaturalPhase
        && hqRuntimeReady.load(std::memory_order_acquire))
        return dynamicEQProcessorHQ.getDetectorAvailability(bandIndex);
    return dynamicEQProcessor.getDetectorAvailability(bandIndex);
}

void AIEqualizerAudioProcessor::resetDSPStateForBypassExit()
{
    // Reset all IIR filter states to zero.  After extended bypass the
    // biquad delay lines contain energy from old audio; feeding new input
    // with stale v1/v2 produces a transient proportional to Δcoeff × stored_energy.
    // A reset-to-zero filter has a smooth exponential onset that the
    // bypass crossfade (2×latency + 512 samples) fully masks.
    eqProcessor.reset();
    eqProcessorHQ.reset();
    eqProcessorMid.reset();
    eqProcessorSide.reset();
    dynamicEQProcessor.reset();
    dynamicEQProcessorHQ.reset();
    dynamicEQProcessorMid.reset();
    dynamicEQProcessorSide.reset();

    // Reset oversampler anti-aliasing filter state
    if (oversampler2x) oversampler2x->reset();
    if (oversampler4x) oversampler4x->reset();
    if (sidechainOversampler2x) sidechainOversampler2x->reset();
    if (sidechainOversampler4x) sidechainOversampler4x->reset();
    linearSidechainDelayBuffer.clear();
    linearAlignedSidechainBuffer.clear();
    linearSidechainDelayWritePos = 0;

    // Reset LP convolver FDL — stale frequency-domain data from before
    // bypass would produce ghost audio on the first blocks after resume.
    for (auto& lp : linearPhaseProcessors)
    {
        if (lp) lp->reset();
    }

    // Clear wet padding ring buffer.  During bypass, this buffer is fed with
    // dry signal to prevent stale data.  However, the ring read position is
    // padSamples behind the write position, so on bypass exit the first
    // padSamples of wet output would contain OLD bypass-fed dry data.  When
    // fresh DSP output (near-zero from filter reset) arrives at the read
    // position, the jump from ~0.5 amplitude to ~0 produces a click inside
    // the crossfade window.  Zeroing ensures a smooth 0→0 transition.
    wetPaddingDelayBuffer.clear();
}

void AIEqualizerAudioProcessor::clearDynamicMeterCache() noexcept
{
    for (auto& entry : dynamicMeterCache)
    {
        entry.input.store(-100.0f, std::memory_order_relaxed);
        entry.gainReduction.store(0.0f, std::memory_order_relaxed);
        entry.output.store(-100.0f, std::memory_order_relaxed);
    }
    dynamicTotalGR.store(0.0f, std::memory_order_relaxed);
}

const DynamicEQProcessor&
AIEqualizerAudioProcessor::getActiveDynamicEQProcessorForDisplay() const noexcept
{
    // Mirrors the routing in processBlock. Kept next to nothing else on purpose:
    // if a fifth instance or a new mode appears, this is the one place that has
    // to learn about it, and the display follows automatically.
    if (cachedMSModeParam != nullptr
        && cachedMSModeParam->load(std::memory_order_relaxed) > 0.5f)
        return dynamicEQProcessorMid;

    if (currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::NaturalPhase
        && hqRuntimeReady.load(std::memory_order_acquire))
        return dynamicEQProcessorHQ;

    return dynamicEQProcessor;
}

void AIEqualizerAudioProcessor::updateDynamicMeterCacheFrom(const DynamicEQProcessor& src) noexcept
{
    // FIX: Use minimum of both maxBands to prevent out-of-bounds access
    const int safeMax = std::min(DynamicEQProcessor::maxBands, maxBands);
    for (int i = 0; i < safeMax; ++i)
    {
        const auto meter = src.getBandMeter(i);
        auto& entry = dynamicMeterCache[static_cast<size_t>(i)];
        entry.input.store(meter.inputLevel, std::memory_order_relaxed);
        entry.gainReduction.store(meter.gainReduction, std::memory_order_relaxed);
        entry.output.store(meter.outputLevel, std::memory_order_relaxed);
    }
    dynamicTotalGR.store(src.getTotalGainReduction(), std::memory_order_relaxed);


}

void AIEqualizerAudioProcessor::updateDynamicMeterCacheFromMS(const DynamicEQProcessor& mid,
                                                              const DynamicEQProcessor& side,
                                                              bool includeMid,
                                                              bool includeSide) noexcept
{
    // FIX: Use minimum of both maxBands to prevent out-of-bounds access
    const int safeMax = std::min(DynamicEQProcessor::maxBands, maxBands);
    for (int i = 0; i < safeMax; ++i)
    {
        DynamicEQProcessor::BandMeter selected {};

        if (includeMid)
            selected = mid.getBandMeter(i);

        if (includeSide)
        {
            const auto sideMeter = side.getBandMeter(i);
            if (!includeMid || std::abs(sideMeter.gainReduction) > std::abs(selected.gainReduction))
                selected = sideMeter;
        }

        auto& entry = dynamicMeterCache[static_cast<size_t>(i)];
        entry.input.store(selected.inputLevel, std::memory_order_relaxed);
        entry.gainReduction.store(selected.gainReduction, std::memory_order_relaxed);
        entry.output.store(selected.outputLevel, std::memory_order_relaxed);
    }

    const float midTotal = includeMid ? mid.getTotalGainReduction() : 0.0f;
    const float sideTotal = includeSide ? side.getTotalGainReduction() : 0.0f;
    dynamicTotalGR.store((std::abs(sideTotal) > std::abs(midTotal)) ? sideTotal : midTotal,
                         std::memory_order_relaxed);
}

//==============================================================================
bool AIEqualizerAudioProcessor::requiresPaddedLatencyPlan() const noexcept
{
    const auto read = [this](const char* id, float fallback) noexcept
    {
        if (auto* value = apvts.getRawParameterValue(id))
            return value->load(std::memory_order_relaxed);
        return fallback;
    };

    const int phase = juce::jlimit(0, 2,
        static_cast<int>(std::round(read("phaseMode", 0.0f))));
    const bool dynamicLookahead = read("dynEqEnabled", 1.0f) > 0.5f
        && static_cast<int>(std::round(read("qualityMode", 0.0f))) == 1;
    return phase != static_cast<int>(PhaseMode::ZeroLatency) || dynamicLookahead;
}

// Latency plans are monotonic within a prepareToPlay lifetime. Increases are
// installed on the message thread before a latent path is allowed to run;
// decreases are applied by the next prepareToPlay. This avoids live PDC
// contraction, which cannot be sample-continuous across all wrappers and DAWs.
void AIEqualizerAudioProcessor::updateReportedLatency()
{
    if (!processorReady.load(std::memory_order_acquire))
        return;

    auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    if (messageManager == nullptr || !messageManager->isThisTheMessageThread())
    {
        triggerAsyncUpdate();
        return;
    }

    const int requested = requiresPaddedLatencyPlan() ? latencyPlan.maximumSamples : 0;
    const int active = latencyPlan.activeSamples.load(std::memory_order_acquire);
    if (requested < active)
    {
        latencyPlan.reductionDeferred.store(true, std::memory_order_release);
        return;
    }
    if (requested == active)
        return;

    latencyPlan.activeSamples.store(requested, std::memory_order_release);
    latencyPlan.reductionDeferred.store(false, std::memory_order_relaxed);
    setLatencySamples(requested);
    lastReportedLatency = requested;
}

void AIEqualizerAudioProcessor::handleAsyncUpdate()
{
    updateReportedLatency();
}

void AIEqualizerAudioProcessor::primeBandSmoothers(double sampleRate)
{
    constexpr double maxLog2SlewPerSecond = 100.0;
    constexpr double maxGainDbSlewPerSecond = 2400.0;
    for (int i = 0; i < maxBands; ++i)
    {
        auto idx = static_cast<size_t>(i);
        smoothedBandFreq[idx].prepare(sampleRate, maxLog2SlewPerSecond);
        smoothedBandGain[idx].prepare(sampleRate, maxGainDbSlewPerSecond);
        smoothedBandQ[idx].prepare(sampleRate, maxLog2SlewPerSecond);
        smoothedBandFreq[idx].setCurrentAndTargetValue(targetBandFreq[idx]);
        smoothedBandGain[idx].setCurrentAndTargetValue(targetBandGain[idx]);
        smoothedBandQ[idx].setCurrentAndTargetValue(targetBandQ[idx]);
        prevAppliedBandType[idx] = targetBandType[idx];
        prevAppliedBandCurveMode[idx] = targetBandCurveMode[idx];
        bandTopologyFadeSamplesRemaining[idx] = 0;
    }
    previousSmoothedBandBlockSamples = 0;
    correctionSmoothingNeedsRateConfig = false;
    correctionSmoothingActive.store(false, std::memory_order_relaxed);
    bandSmoothingPrimed = true;
}

bool AIEqualizerAudioProcessor::applySmoothedBandParams(int blockSamples, bool paramsChanged)
{
    constexpr double maxLog2SlewPerSecond = 100.0;
    constexpr double maxGainDbSlewPerSecond = 2400.0;
    constexpr double correctionRampSeconds = 0.08;
    constexpr int topologyFadeSamples = 1024;

    const int activeBandsLocal = numActiveBands.load(std::memory_order_relaxed);
    const int availableBands = std::min({ activeBandsLocal, maxBands,
                                          eqProcessor.getNumBands(),
                                          eqProcessorHQ.getNumBands(),
                                          eqProcessorMid.getNumBands(),
                                          eqProcessorSide.getNumBands() });
    bool anyBandApplied = false;

    // The AI request flag is observed before parameter synchronization. Configure
    // its gentler rates only now, after the new targets are available. A smaller
    // correction still takes 80 ms; a larger one is capped by the normative
    // 100 oct/s and 2400 dB/s ceilings.
    if (correctionSmoothingActive.load(std::memory_order_relaxed)
        && correctionSmoothingNeedsRateConfig)
    {
        for (int i = 0; i < maxBands; ++i)
        {
            const auto idx = static_cast<size_t>(i);
            const double freqDistance = std::abs(smoothedBandFreq[idx].getTargetLog2()
                                                - smoothedBandFreq[idx].getCurrentLog2());
            const double qDistance = std::abs(smoothedBandQ[idx].getTargetLog2()
                                             - smoothedBandQ[idx].getCurrentLog2());
            const double gainDistance = std::abs(smoothedBandGain[idx].getTargetValueDouble()
                                                - smoothedBandGain[idx].getCurrentValueDouble());

            smoothedBandFreq[idx].setMaximumRate(std::min(
                maxLog2SlewPerSecond,
                freqDistance > 0.0 ? freqDistance / correctionRampSeconds
                                   : maxLog2SlewPerSecond));
            smoothedBandQ[idx].setMaximumRate(std::min(
                maxLog2SlewPerSecond,
                qDistance > 0.0 ? qDistance / correctionRampSeconds
                                : maxLog2SlewPerSecond));
            smoothedBandGain[idx].setMaximumRate(std::min(
                maxGainDbSlewPerSecond,
                gainDistance > 0.0 ? gainDistance / correctionRampSeconds
                                   : maxGainDbSlewPerSecond));
        }
        correctionSmoothingNeedsRateConfig = false;
    }

    for (int i = 0; i < availableBands; ++i)
    {
        auto idx = static_cast<size_t>(i);

        // This countdown represents samples processed by the preceding call.
        // While it is non-zero, new Type/CurveMode requests remain only in the
        // target arrays. The last target observed is applied after completion.
        if (bandTopologyFadeSamplesRemaining[idx] > 0)
        {
            bandTopologyFadeSamplesRemaining[idx] = std::max(
                0, bandTopologyFadeSamplesRemaining[idx]
                 - previousSmoothedBandBlockSamples);
        }

        const bool topologyReady = bandTopologyFadeSamplesRemaining[idx] == 0
            && (targetBandType[idx] != prevAppliedBandType[idx]
                || targetBandCurveMode[idx] != prevAppliedBandCurveMode[idx]);
        const bool isMoving = smoothedBandFreq[idx].isSmoothing()
                           || smoothedBandGain[idx].isSmoothing()
                           || smoothedBandQ[idx].isSmoothing();

        // Advance smoothing by one block
        smoothedBandFreq[idx].skip(blockSamples);
        smoothedBandGain[idx].skip(blockSamples);
        smoothedBandQ[idx].skip(blockSamples);

        if (!isMoving && !paramsChanged && !topologyReady)
            continue;

        anyBandApplied = true;

        const float freq = smoothedBandFreq[idx].getCurrentValue();
        const float gain = smoothedBandGain[idx].getCurrentValue();
        const float q = smoothedBandQ[idx].getCurrentValue();
        if (topologyReady)
        {
            if (i < eqProcessor.getNumBands())
                eqProcessor.beginBandCrossfade(i, topologyFadeSamples);
            if (i < eqProcessorHQ.getNumBands())
                eqProcessorHQ.beginBandCrossfade(i, topologyFadeSamples);
            if (i < eqProcessorMid.getNumBands())
                eqProcessorMid.beginBandCrossfade(i, topologyFadeSamples);
            if (i < eqProcessorSide.getNumBands())
                eqProcessorSide.beginBandCrossfade(i, topologyFadeSamples);

            prevAppliedBandType[idx] = targetBandType[idx];
            prevAppliedBandCurveMode[idx] = targetBandCurveMode[idx];
            bandTopologyFadeSamplesRemaining[idx] = topologyFadeSamples;
        }

        const int type = prevAppliedBandType[idx];
        const int curveMode = prevAppliedBandCurveMode[idx];
        const int slope = targetBandSlope[idx];
        const bool enabled = targetBandEnabled[idx];
        const bool solo = targetBandSolo[idx];

        const auto dspCurveMode = curveMode == kSurgicalCurveMode
            ? ParametricEQProcessor::CurveMode::Surgical
            : ParametricEQProcessor::CurveMode::Legacy;

        // targetDynamicBandParams[idx].enabled is the routing ownership flag:
        // true means this band is currently owned by / enabled inside the
        // dynamic stage, so the static EQ bypasses it in audio while the white
        // curve keeps showing the configured band shape.
        const bool bandOwnedByDynamicStage = (i < maxBands)
            && targetDynamicBandParams[idx].enabled;

        if (i < eqProcessor.getNumBands())
        {
            eqProcessor.setBandCurveMode(i, dspCurveMode);
            eqProcessor.setBandParameters(i, freq, gain, q, type);
            eqProcessor.setBandSlope(i, slope);
            eqProcessor.setBandEnabled(i, enabled);
            eqProcessor.setBandAudioBypass(i, bandOwnedByDynamicStage);
            eqProcessor.setBandSolo(i, solo);
        }
        if (i < eqProcessorHQ.getNumBands())
        {
            eqProcessorHQ.setBandCurveMode(i, dspCurveMode);
            eqProcessorHQ.setBandParameters(i, freq, gain, q, type);
            eqProcessorHQ.setBandSlope(i, slope);
            eqProcessorHQ.setBandEnabled(i, enabled);
            eqProcessorHQ.setBandAudioBypass(i, bandOwnedByDynamicStage);
            eqProcessorHQ.setBandSolo(i, solo);
        }
        if (i < eqProcessorMid.getNumBands())
        {
            eqProcessorMid.setBandCurveMode(i, dspCurveMode);
            eqProcessorMid.setBandParameters(i, freq, gain, q, type);
            eqProcessorMid.setBandSlope(i, slope);
            eqProcessorMid.setBandEnabled(i, enabled);
            eqProcessorMid.setBandAudioBypass(i, bandOwnedByDynamicStage);
            eqProcessorMid.setBandSolo(i, solo);
        }
        if (i < eqProcessorSide.getNumBands())
        {
            eqProcessorSide.setBandCurveMode(i, dspCurveMode);
            eqProcessorSide.setBandParameters(i, freq, gain, q, type);
            eqProcessorSide.setBandSlope(i, slope);
            eqProcessorSide.setBandEnabled(i, enabled);
            eqProcessorSide.setBandAudioBypass(i, bandOwnedByDynamicStage);
            eqProcessorSide.setBandSolo(i, solo);
        }

        if (i < maxBands)
        {
            auto dynParams = targetDynamicBandParams[idx];
            dynParams.frequency = freq;
            dynParams.gain = gain;
            dynParams.q = q;
            dynParams.filterType = type;
            dynParams.enabled = bandOwnedByDynamicStage;
            dynamicEQProcessor.setBandParams(i, dynParams);
            dynamicEQProcessorHQ.setBandParams(i, dynParams);
            dynamicEQProcessorMid.setBandParams(i, dynParams);
            dynamicEQProcessorSide.setBandParams(i, dynParams);
        }
    }

    previousSmoothedBandBlockSamples = blockSamples;

    // When an AI correction finishes, restore the authority maxima used for
    // direct user/host automation. Current values are never reset.
    if (correctionSmoothingActive.load(std::memory_order_relaxed))
    {
        bool anyStillSmoothing = false;
        for (int i = 0; i < availableBands && !anyStillSmoothing; ++i)
        {
            auto idx = static_cast<size_t>(i);
            anyStillSmoothing = smoothedBandFreq[idx].isSmoothing()
                             || smoothedBandGain[idx].isSmoothing()
                             || smoothedBandQ[idx].isSmoothing();
        }
        if (!anyStillSmoothing)
        {
            for (int i = 0; i < maxBands; ++i)
            {
                auto idx = static_cast<size_t>(i);
                smoothedBandFreq[idx].setMaximumRate(maxLog2SlewPerSecond);
                smoothedBandQ[idx].setMaximumRate(maxLog2SlewPerSecond);
                smoothedBandGain[idx].setMaximumRate(maxGainDbSlewPerSecond);
            }
            correctionSmoothingActive.store(false, std::memory_order_relaxed);
        }
    }

    return anyBandApplied;
}

//==============================================================================
void AIEqualizerAudioProcessor::updateEQFromParameters()
{
    if (!parametersCached.load())
        cacheParameterPointers();

    auto loadParam = [](std::atomic<float>* ptr, float fallback) -> float
    {
        return ptr ? ptr->load() : fallback;
    };

    auto isGainBearingDynFilterType = [](int filterType) noexcept
    {
        switch (filterType)
        {
            case ParametricEQProcessor::LowShelf:
            case ParametricEQProcessor::Peak:
            case ParametricEQProcessor::HighShelf:
            case ParametricEQProcessor::VintageLowShelf:
            case ParametricEQProcessor::VintageHighShelf:
                return true;
            default:
                return false;
        }
    };

    // Update AI parameters (use cached pointers - NO hash map lookups in audio thread!)
    if (cachedAISensitivity)
    {
        float sensitivity = cachedAISensitivity->load(std::memory_order_relaxed);
        aiEngine.setSensitivity(sensitivity);
    }
    if (cachedAIStrength)
    {
        float strength = cachedAIStrength->load(std::memory_order_relaxed);
        aiEngine.setStrength(strength);
    }

    //--------------------------------------------------------------------------
    // Update Global Dynamic EQ settings (use cached pointers)
    //--------------------------------------------------------------------------
    const bool dynEqEnabledTarget = cachedDynEqEnabled
        ? (cachedDynEqEnabled->load(std::memory_order_relaxed) > 0.5f)
        : true;
    float dynMix = 1.0f;
    bool dynAutoMakeup = false;
    if (cachedDynEqMix)
        dynMix = cachedDynEqMix->load(std::memory_order_relaxed) / 100.0f;
    if (cachedDynAutoMakeup)
        dynAutoMakeup = cachedDynAutoMakeup->load(std::memory_order_relaxed) > 0.5f;

    dynamicEQProcessor.setGlobalMix(dynMix);
    dynamicEQProcessor.setAutoMakeup(dynAutoMakeup);
    dynamicEQProcessorHQ.setGlobalMix(dynMix);
    dynamicEQProcessorHQ.setAutoMakeup(dynAutoMakeup);
    dynamicEQProcessorMid.setGlobalMix(dynMix);
    dynamicEQProcessorMid.setAutoMakeup(dynAutoMakeup);
    dynamicEQProcessorSide.setGlobalMix(dynMix);
    dynamicEQProcessorSide.setAutoMakeup(dynAutoMakeup);
    dynamicEQProcessorForIR.setGlobalMix(dynMix);
    dynamicEQProcessorForIR.setAutoMakeup(dynAutoMakeup);

    // Update active bands count (robust against NaN / invalid)
    {
        const float raw = loadParam(cachedNumActiveBands, 7.0f);
        const int idx = std::isfinite(raw) ? static_cast<int>(raw) : 7;
        numActiveBands.store(juce::jlimit(1, maxBands, idx + 1), std::memory_order_relaxed); // choice index starts at 0
    }

    // Clamp active bands to what is actually available in processors
    const int availableBands = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands(),
                                         eqProcessorMid.getNumBands(), eqProcessorSide.getNumBands() });
    const int currentBands = numActiveBands.load(std::memory_order_relaxed);
    numActiveBands.store(juce::jlimit(1, availableBands, currentBands), std::memory_order_relaxed);

    const int bandsAvailable = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands() });
    // FIX: Clamp to maxBands to prevent array out-of-bounds access
    const int activeBandsLocal = std::min(numActiveBands.load(std::memory_order_relaxed), maxBands);

    // Detect if any band is soloed (among active and enabled bands)
    bool hasSolo = false;
    for (int i = 0; i < activeBandsLocal; ++i)
    {
        const auto& p = cachedParams[static_cast<size_t>(i)];
        const bool enabled = loadParam(p.enabled, (i < 8 ? 1.0f : 0.0f)) > 0.5f && (i < activeBandsLocal);
        const bool solo = loadParam(p.solo, 0.0f) > 0.5f;
        if (enabled && solo)
        {
            hasSolo = true;
            break;
        }
    }

    // Begin seqlock for IR shadow processors
    irCoeffVersion.fetch_add(1, std::memory_order_acq_rel); // mark write in progress (odd)

    // Update EQ bands
    for (int i = 0; i < bandsAvailable; ++i)
    {
        const auto& p = cachedParams[static_cast<size_t>(i)];

        float freq = loadParam(p.freq, 1000.0f);
        float gain = loadParam(p.gain, 0.0f);
        float q = loadParam(p.q, 1.0f);
        bool enabled = loadParam(p.enabled, (i < 8 ? 1.0f : 0.0f)) > 0.5f && (i < activeBandsLocal);
        bool solo = loadParam(p.solo, 0.0f) > 0.5f;
        const bool enabledFiltered = enabled && (!hasSolo || solo);

        int type = static_cast<int>(loadParam(p.type, 2.0f));
        int slopeVal = static_cast<int>(loadParam(p.slope, 0.0f));
        const int curveMode = juce::jlimit(kLegacyCurveMode, kSurgicalCurveMode,
            static_cast<int>(std::round(loadParam(p.curveMode,
                                                  static_cast<float>(kSurgicalCurveMode)))));
        // Fallback for legacy states
        if (p.type == nullptr)
        {
            if (i == 0) type = ParametricEQProcessor::LowShelf;
            else if (i == activeBandsLocal - 1) type = ParametricEQProcessor::HighShelf;
        }

        // Do not slam the active audible EQ processors here.
        // updateEQFromParameters() should only refresh targets + shadow/IR state;
        // applySmoothedBandParams() is the single live-apply point for the audible EQ path.

        //----------------------------------------------------------------------
        // Update Dynamic EQ band parameters
        //----------------------------------------------------------------------
        const int dynMode = juce::jlimit(0, 3,
            static_cast<int>(std::round(loadParam(p.dynMode, 0.0f))));
        const int storedTrigger = juce::jlimit(
            DynamicEQProcessor::TriggerSide_Above,
            DynamicEQProcessor::TriggerSide_Below,
            static_cast<int>(std::round(loadParam(
                p.dynTrigger,
                static_cast<float>(DynamicEQProcessor::TriggerSide_Above)))));
        const int effectiveTrigger = dynMode == DynamicEQProcessor::DynamicMode_Gate
            ? DynamicEQProcessor::TriggerSide_Below
            : storedTrigger;
        const int detectionMode = juce::jlimit(
            DynamicEQProcessor::DetectionMode_Peak,
            DynamicEQProcessor::DetectionMode_RMS,
            static_cast<int>(std::round(loadParam(
                p.detectionMode,
                static_cast<float>(DynamicEQProcessor::DetectionMode_RMS)))));
        const int detectorSource = juce::jlimit(
            DynamicEQProcessor::DetectorSource_InternalWideband,
            DynamicEQProcessor::DetectorSource_ExternalFiltered,
            static_cast<int>(std::round(loadParam(
                p.detectorSource,
                static_cast<float>(DynamicEQProcessor::DetectorSource_InternalWideband)))));
        const float sidechainFreq = juce::jlimit(
            20.0f, 20000.0f,
            loadParam(p.sidechainFreq,
                      AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)]));
        const float sidechainQ = juce::jlimit(0.1f, 10.0f,
                                              loadParam(p.sidechainQ, 1.0f));
        float threshold = loadParam(p.dynThreshold, -20.0f);
        float ratio = loadParam(p.dynRatio, 2.0f);
        float attack = loadParam(p.dynAttack, 10.0f);
        float release = loadParam(p.dynRelease, 100.0f);
        float range = loadParam(p.dynRange, 24.0f);
        float knee = loadParam(p.dynKnee, 6.0f);

        const bool bandOwnedByDynamicStage = dynEqEnabledTarget
            && enabledFiltered
            && dynMode != DynamicEQProcessor::DynamicMode_Off
            && (dynMode == DynamicEQProcessor::DynamicMode_Gate
                || isGainBearingDynFilterType(type));

        DynamicEQProcessor::DynamicBandParams dynParams;
        dynParams.frequency = freq;
        dynParams.gain = gain;
        dynParams.q = q;
        dynParams.filterType = type;
        // In plugin integration, .enabled means "owned by / enabled inside the
        // dynamic stage", not the raw UI band-enabled state.
        dynParams.enabled = bandOwnedByDynamicStage;
        dynParams.dynamicMode = dynMode;
        dynParams.triggerSide = effectiveTrigger;
        dynParams.detection = detectionMode;
        dynParams.detectorSource = detectorSource;
        dynParams.sidechainEnabled =
            detectorSource == DynamicEQProcessor::DetectorSource_InternalFiltered
            || detectorSource == DynamicEQProcessor::DetectorSource_ExternalFiltered;
        dynParams.sidechainFreq = sidechainFreq;
        dynParams.sidechainQ = sidechainQ;
        dynParams.threshold = threshold;
        dynParams.ratio = ratio;
        dynParams.attackMs = attack;
        dynParams.releaseMs = release;
        dynParams.range = range;
        dynParams.knee = knee;

        // Cache band targets for smoothing
        const auto idx = static_cast<size_t>(i);
        targetBandFreq[idx] = freq;
        targetBandGain[idx] = gain;
        targetBandQ[idx] = q;
        targetBandType[idx] = type;
        targetBandCurveMode[idx] = curveMode;
        targetBandSlope[idx] = slopeVal;
        targetBandEnabled[idx] = enabledFiltered;
        targetBandSolo[idx] = solo;

        if (bandSmoothingPrimed)
        {
            smoothedBandFreq[idx].setTargetValue(freq);
            smoothedBandGain[idx].setTargetValue(gain);
            smoothedBandQ[idx].setTargetValue(q);
        }
        else
        {
            smoothedBandFreq[idx].setCurrentAndTargetValue(freq);
            smoothedBandGain[idx].setCurrentAndTargetValue(gain);
            smoothedBandQ[idx].setCurrentAndTargetValue(q);
        }

        targetDynamicBandParams[idx] = dynParams;

        // FIX: Update shadow processors for thread-safe IR building
        // These are read by the IR builder thread without locking
        if (i < eqProcessorForIR.getNumBands())
        {
            eqProcessorForIR.setBandCurveMode(i,
                curveMode == kSurgicalCurveMode
                    ? ParametricEQProcessor::CurveMode::Surgical
                    : ParametricEQProcessor::CurveMode::Legacy);
            eqProcessorForIR.setBandParameters(i, freq, gain, q, type);
            eqProcessorForIR.setBandEnabled(i, enabledFiltered && !bandOwnedByDynamicStage);
            eqProcessorForIR.setBandSolo(i, solo);
            eqProcessorForIR.setBandSlope(i, static_cast<int>(loadParam(p.slope, 0.0f)));
        }
        dynamicEQProcessorForIR.setBandParams(i, dynParams);
    }

    // Complete seqlock for IR shadow processors
    std::atomic_thread_fence(std::memory_order_release);
    irCoeffVersion.fetch_add(1, std::memory_order_release); // mark write complete (even)

    // Signal that new coefficients are ready for IR builder
    irCoefficientsUpdated.store(true, std::memory_order_release);

    bandSmoothingPrimed = true;
}

//==============================================================================
// A/B Comparison Implementation

void AIEqualizerAudioProcessor::setABState(ABState state)
{
    // CRITICAL: Must be called from Message Thread (GUI thread)
    // JUCE's APVTS parameter changes are NOT thread-safe
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        // SAFETY: If called from wrong thread, defer to Message Thread
        juce::WeakReference<AIEqualizerAudioProcessor> weakThis(this);
        juce::MessageManager::callAsync([weakThis, state]()
        {
            if (auto* self = weakThis.get())
                self->setABState(state);
        });
        return;
    }

    const ABState current = currentABState.load(std::memory_order_relaxed);
    if (state == current)
        return;

    // RB-2 FIX: lock the full save→switch→load sequence so no other thread
    // can observe a partial state (e.g., host calling getStateInformation mid-switch).
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);

    // Save current state to current slot BEFORE switching
    saveCurrentStateToSlot(current);

    // Switch to new state atomically
    currentABState.store(state, std::memory_order_release);

    // Arm crossfade snapshot BEFORE loading new parameters so the audio thread
    // will snapshot the OLD filter state before applying the new coefficients.
    abCrossfadeSnapshotNeeded.store(true, std::memory_order_release);

    // Load new state (must be on Message Thread for APVTS access)
    loadStateFromSlot(state);
}

void AIEqualizerAudioProcessor::saveCurrentStateToSlot(ABState slot)
{
    // RB-2 FIX: mutex replaces message-thread-only guard.
    // This function only reads APVTS atomics and writes to slot structs —
    // safe from any thread under slotMutex_.
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);

    EQSlot* targetSlot = nullptr;
    switch (slot)
    {
        case ABState::A: targetSlot = &slotA; break;
        case ABState::B: targetSlot = &slotB; break;
        case ABState::C: targetSlot = &slotC; break;
        case ABState::D: targetSlot = &slotD; break;
    }
    if (!targetSlot) return;

    // Save all band states with bounds checking
    for (int i = 0; i < maxBands; ++i)
    {
        targetSlot->bands[static_cast<size_t>(i)] = getBandState(i);
    }

    // Save output gain safely
    if (auto* outputGainParam = apvts.getRawParameterValue("outputGain"))
    {
        targetSlot->outputGain = outputGainParam->load();
    }
    else
    {
        targetSlot->outputGain = 0.0f; // Default if parameter not found
    }

    if (auto* dynEqEnabledParam = apvts.getRawParameterValue("dynEqEnabled"))
        targetSlot->dynEqEnabled = dynEqEnabledParam->load() > 0.5f;
    else
        targetSlot->dynEqEnabled = true;

    if (auto* dynEqMixParam = apvts.getRawParameterValue("dynEqMix"))
        targetSlot->dynEqMix = dynEqMixParam->load();
    else
        targetSlot->dynEqMix = 100.0f;

    if (auto* dynAutoMakeupParam = apvts.getRawParameterValue("dynAutoMakeup"))
        targetSlot->dynAutoMakeup = dynAutoMakeupParam->load() > 0.5f;
    else
        targetSlot->dynAutoMakeup = false;
}

void AIEqualizerAudioProcessor::loadStateFromSlot(ABState slot)
{
    // CRITICAL: Must be called from Message Thread (writes APVTS via gesture API)
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        jassertfalse;
        return;
    }

    // RB-2 FIX: mutex protects slot reads against concurrent host-thread writes
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);

    const EQSlot* sourceSlot = nullptr;
    switch (slot)
    {
        case ABState::A: sourceSlot = &slotA; break;
        case ABState::B: sourceSlot = &slotB; break;
        case ABState::C: sourceSlot = &slotC; break;
        case ABState::D: sourceSlot = &slotD; break;
    }
    if (!sourceSlot) return;

    bool anyMaterialChange = false;

    // Load band states — clamp frequency instead of silently skipping the band,
    // so an out-of-range value never causes a band to disappear unexpectedly.
    for (int i = 0; i < maxBands; ++i)
    {
        auto bandState = sourceSlot->bands[static_cast<size_t>(i)];
        bandState.frequency = juce::jlimit(20.0f, 20000.0f, bandState.frequency);

        const auto currentState = getBandState(i);
        const bool materiallyChanged =
            std::abs(currentState.frequency - bandState.frequency) > 1.0f ||
            std::abs(currentState.gain - bandState.gain) > 0.05f ||
            std::abs(currentState.q - bandState.q) > 0.02f ||
            currentState.type != bandState.type ||
            currentState.enabled != bandState.enabled ||
            currentState.solo != bandState.solo ||
            currentState.slope != bandState.slope ||
            currentState.curveMode != bandState.curveMode ||
            currentState.dynMode != bandState.dynMode ||
            currentState.dynTrigger != bandState.dynTrigger ||
            currentState.detectionMode != bandState.detectionMode ||
            currentState.detectorSource != bandState.detectorSource ||
            std::abs(currentState.sidechainFrequency - bandState.sidechainFrequency) > 1.0f ||
            std::abs(currentState.sidechainQ - bandState.sidechainQ) > 0.02f ||
            std::abs(currentState.dynThreshold - bandState.dynThreshold) > 0.05f ||
            std::abs(currentState.dynRatio - bandState.dynRatio) > 0.02f ||
            std::abs(currentState.dynAttack - bandState.dynAttack) > 0.1f ||
            std::abs(currentState.dynRelease - bandState.dynRelease) > 0.5f ||
            std::abs(currentState.dynRange - bandState.dynRange) > 0.05f ||
            std::abs(currentState.dynKnee - bandState.dynKnee) > 0.05f;

        // Arm per-band crossfade flag BEFORE applying delta (message thread).
        // The audio thread will dispatch beginBandCrossfade() before updating coefficients.
        abCrossfadePendingBands[i].store(materiallyChanged, std::memory_order_relaxed);

        if (materiallyChanged)
        {
            anyMaterialChange = applyBandStateDelta(i, bandState, false) || anyMaterialChange;
        }
    }

    // Load output gain with validation
    if (auto* param = apvts.getParameter("outputGain"))
    {
        const float gain = juce::jlimit(-24.0f, 24.0f, sourceSlot->outputGain);
        if (auto* raw = apvts.getRawParameterValue("outputGain"); raw != nullptr)
        {
            if (std::abs(raw->load() - gain) > 0.01f)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(gain));
                param->endChangeGesture();
                anyMaterialChange = true;
            }
        }
    }

    if (auto* param = apvts.getParameter("dynEqEnabled"))
    {
        const bool desired = sourceSlot->dynEqEnabled;
        if (auto* raw = apvts.getRawParameterValue("dynEqEnabled"); raw != nullptr)
        {
            const bool current = raw->load() > 0.5f;
            if (current != desired)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(desired ? 1.0f : 0.0f));
                param->endChangeGesture();
                anyMaterialChange = true;
            }
        }
    }

    if (auto* param = apvts.getParameter("dynEqMix"))
    {
        const float mix = juce::jlimit(0.0f, 100.0f, sourceSlot->dynEqMix);
        if (auto* raw = apvts.getRawParameterValue("dynEqMix"); raw != nullptr)
        {
            if (std::abs(raw->load() - mix) > 0.01f)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(mix));
                param->endChangeGesture();
                anyMaterialChange = true;
            }
        }
    }

    if (auto* param = apvts.getParameter("dynAutoMakeup"))
    {
        const bool desired = sourceSlot->dynAutoMakeup;
        if (auto* raw = apvts.getRawParameterValue("dynAutoMakeup"); raw != nullptr)
        {
            const bool current = raw->load() > 0.5f;
            if (current != desired)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(desired ? 1.0f : 0.0f));
                param->endChangeGesture();
                anyMaterialChange = true;
            }
        }
    }

    // NOTE: crossfade snapshot is armed in setABState() BEFORE loadStateFromSlot()
    // is called, so the audio thread snapshots old state before seeing new coefficients.
}

void AIEqualizerAudioProcessor::copyAtoB()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    saveCurrentStateToSlot(ABState::A);
    slotB = slotA;
}

void AIEqualizerAudioProcessor::copyBtoA()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    saveCurrentStateToSlot(ABState::B);
    slotA = slotB;
}

void AIEqualizerAudioProcessor::copyAtoC()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    saveCurrentStateToSlot(ABState::A);
    slotC = slotA;
}

void AIEqualizerAudioProcessor::copyAtoD()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    saveCurrentStateToSlot(ABState::A);
    slotD = slotA;
}

void AIEqualizerAudioProcessor::copyBtoC()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    saveCurrentStateToSlot(ABState::B);
    slotC = slotB;
}

void AIEqualizerAudioProcessor::copyBtoD()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    saveCurrentStateToSlot(ABState::B);
    slotD = slotB;
}

void AIEqualizerAudioProcessor::copyCtoD()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    saveCurrentStateToSlot(ABState::C);
    slotD = slotC;
}

void AIEqualizerAudioProcessor::swapAB()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    const ABState current = currentABState.load(std::memory_order_relaxed);
    saveCurrentStateToSlot(current);
    std::swap(slotA, slotB);
    loadStateFromSlot(current);
}

void AIEqualizerAudioProcessor::swapCD()
{
    std::lock_guard<std::recursive_mutex> lock(slotMutex_);
    const ABState current = currentABState.load(std::memory_order_relaxed);
    saveCurrentStateToSlot(current);
    std::swap(slotC, slotD);
    loadStateFromSlot(current);
}

//==============================================================================
// Source Profile

void AIEqualizerAudioProcessor::setSourceProfile(AIEngine::SourceProfile profile)
{
    aiEngine.setSourceProfile(profile);

    // Also update the parameter
    if (auto* param = apvts.getParameter("sourceProfile"))
    {
        int index = static_cast<int>(profile);
        param->setValueNotifyingHost(static_cast<float>(index) / 6.0f);
    }
}

void AIEqualizerAudioProcessor::setNumActiveBands(int n) noexcept
{
    numActiveBands.store(juce::jlimit(1, maxBands, n), std::memory_order_relaxed);
    ensureBandCount(maxBands);
    const int availableBands = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands() });
    const int currentBands = numActiveBands.load(std::memory_order_relaxed);
    const int clampedBands = juce::jlimit(1, availableBands, currentBands);
    numActiveBands.store(clampedBands, std::memory_order_relaxed);
    for (int i = clampedBands; i < eqProcessor.getNumBands(); ++i)
        eqProcessor.setBandEnabled(i, false);
    for (int i = clampedBands; i < eqProcessorHQ.getNumBands(); ++i)
        eqProcessorHQ.setBandEnabled(i, false);
}

void AIEqualizerAudioProcessor::ensureBandCount(int count)
{
    const int target = juce::jlimit(1, maxBands, count);
    const int activeBands = numActiveBands.load(std::memory_order_relaxed);  // Cache for lambda

    auto addMissing = [this, target, activeBands](ParametricEQProcessor& proc)
    {
        int current = proc.getNumBands();
        for (int i = current; i < target; ++i)
        {
            int type = ParametricEQProcessor::Peak;
            if (i == 0)
                type = ParametricEQProcessor::LowShelf;
            else if (i == maxBands - 1)
                type = ParametricEQProcessor::HighShelf;

            float freq = AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)];
            proc.addBand(freq, 0.0f, 1.0f, type);
            proc.setBandEnabled(i, i < activeBands);
        }
    };

    addMissing(eqProcessor);
    addMissing(eqProcessorHQ);
    addMissing(eqProcessorMid);
    addMissing(eqProcessorSide);

    // FIX: Ensure shadow processor has same band count for thread-safe IR building
    addMissing(eqProcessorForIR);
}

//==============================================================================
// D1 (AI-evolution): approved AI corrections -> dynamic correction snapshot
//==============================================================================
void AIEqualizerAudioProcessor::publishDynamicCorrectionsFromApplied(
    const std::vector<AIEngine::Correction>& appliedCorrections)
{
    auto* const messageManager = juce::MessageManager::getInstanceWithoutCreating();
    jassert(messageManager != nullptr && messageManager->isThisTheMessageThread());
    if (messageManager == nullptr || !messageManager->isThisTheMessageThread())
        return; // Fail closed: the mailbox contract permits exactly one producer.

    // Message-thread publication (the engine's ownership mailbox keeps the
    // payload immutable while the audio thread reads it). The caller passes the exact merged/limited
    // correction list that was applied to static bands, so the dynamic snapshot
    // cannot diverge from the user-visible AI application.

    DynamicCorrectionEngine::Snapshot snap;
    snap.version = ++dynamicCorrectionsVersion;
    int slot = 0;
    for (const auto& corr : appliedCorrections)
    {
        if (slot >= DynamicCorrectionEngine::kMaxCorrections)
            break;
        const auto scaled = aiEngine.getScaledCorrection(corr);
        if (scaled.suggestedGain >= 0.0f)
            continue;   // dynamic engine is cut-only; boosts stay in the static EQ

        auto& c = snap.corrections[static_cast<size_t>(slot)];
        c.frequencyHz = scaled.frequency;
        c.q = juce::jlimit(0.5f, 24.0f, scaled.suggestedQ);
        c.maxCutDb = juce::jlimit(0.5f, 18.0f, -scaled.suggestedGain);
        // v1 threshold model: cut engages when the band rises above a fixed
        // program-relative floor; per-correction adaptive thresholds are the
        // documented next step (needs the band-energy statistics from P3).
        c.thresholdDb = -35.0f;
        c.ratio = 3.0f;
        c.attackMs = 3.0f;
        c.releaseMs = 60.0f;
        c.dynamic = true;
        c.enabled = true;
        ++slot;
    }
    snap.numActive = slot;
    dynamicCorrectionEngine.publishCorrections(snap);
}

//==============================================================================
// Audio Capture Forwarding to CaptureService
//==============================================================================

// Note: pushToCaptureRing removed - now using captureService.pushSamples() directly in processBlock

void AIEqualizerAudioProcessor::captureAudioSnapshotMs(int lengthMs)
{
    // FA-001: the retroactive path arms the service just as the manual button
    // does, so it has to be closed here too or the gate would only be partial.
    if (! isCaptureAllowed())
        return;

    // Delegate to lock-free CaptureService
    captureBufferReady.store(false, std::memory_order_release);
    captureService.captureSnapshotMs(lengthMs);
    captureBufferReady.store(true, std::memory_order_release);
}

bool AIEqualizerAudioProcessor::runCapturedAudioAnalysis()
{
    // Get captured audio from CaptureService (no locks needed - already thread-safe)
    const auto& monoCopy = captureService.getCapturedAudioMono();
    const double sr = captureService.getCapturedSampleRate();

    // Validate data
    if (monoCopy.empty() || sr <= 0.0 || sr > 192000.0 || sr < 8000.0)
        return false;

    try
    {
        // Offline spectrum analysis on captured audio
        SpectrumAnalyzer analyzer;
        analyzer.prepare(sr, 512);

        // Create buffer safely
        const int bufferSize = std::min(static_cast<int>(monoCopy.size()),
                                        static_cast<int>(sr * 60.0)); // Max 60 seconds
        if (bufferSize <= 0)
            return false;

        juce::AudioBuffer<float> tempBuffer(1, bufferSize);

        // Copy data safely
        float* writePtr = tempBuffer.getWritePointer(0);
        if (writePtr != nullptr)
        {
            std::memcpy(writePtr, monoCopy.data(), static_cast<size_t>(bufferSize) * sizeof(float));
        }
        else
        {
            return false;
        }

        // Process in chunks (512 samples)
        int offset = 0;
        const int total = tempBuffer.getNumSamples();
        const int block = 512;

        while (offset < total)
        {
            const int chunk = std::min(block, total - offset);
            if (chunk <= 0)
                break;

            juce::AudioBuffer<float> slice(tempBuffer.getArrayOfWritePointers(), 1, offset, chunk);
            if (slice.getNumSamples() > 0)
                analyzer.pushSamples(slice);

            offset += chunk;
        }

        // Run FFT and analyze
        analyzer.processFFT();
        const auto& spectrum = analyzer.getSmoothedSpectrum();
        if (!spectrum.empty())
        {
            analyzeSpectrumSerialized(spectrum, true);
            return true;
        }
    }
    catch (...)
    {
        return false;
    }

    return false;
}

bool AIEqualizerAudioProcessor::analyzeCapturedAudioSnapshot()
{
    // FA-001: refuse before any capture-analysis thread is created. Gating the
    // two arming points alone would still leave this reachable by a caller that
    // already holds a buffer.
    if (! isCaptureAllowed())
        return false;

    captureAnalysisCompleted.store(false, std::memory_order_release);

    // A lifecycle transition owns CaptureService/AIEngine state while
    // processorReady is false. A caller that passed this check immediately
    // before prepare closes the gate is still covered by captureAnalysisInFlight
    // and aiAnalysisMutex in quiesce/prepare.
    if (!processorReady.load(std::memory_order_acquire))
        return false;

    if (!captureBufferReady.load(std::memory_order_acquire))
        return false;

    bool expected = false;
    if (!captureAnalysisInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return false;

    auto finish = [this](bool ok)
    {
        captureAnalysisResult.store(ok, std::memory_order_release);
        captureAnalysisCompleted.store(true, std::memory_order_release);
        captureAnalysisInFlight.store(false, std::memory_order_release);
    };

    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    if (mm != nullptr && mm->isThisTheMessageThread())
    {
        juce::WeakReference<AIEqualizerAudioProcessor> weakThis(this);

        // A previous capture-analysis thread may have finished (captureAnalysisInFlight
        // already cleared in finish()) yet still be joinable; assigning over a joinable
        // std::thread calls std::terminate. Join it first — it has already returned (or
        // is just finishing its async post), so this returns promptly.
        if (captureAnalysisThread.joinable())
            captureAnalysisThread.join();

        captureAnalysisThread = std::thread([this, weakThis, finish]() mutable
        {
                        const bool ok = runCapturedAudioAnalysis();
            finish(ok);

            juce::MessageManager::callAsync([weakThis, ok]()
            {
                if (auto* self = weakThis.get())
                    self->aiProblemsChanged.store(true, std::memory_order_release);
                juce::ignoreUnused(ok);
            });
        });

        return true;
    }

    const bool ok = runCapturedAudioAnalysis();
    finish(ok);
    return ok;
}

namespace
{
// File-static rather than a member: adding data to AIEqualizerAudioProcessor
// shifts the layout between test TUs and SharedCode. Defaults to the product
// answer, so nothing has to opt out.
std::atomic<bool> gCaptureAllowedOverride { false };
} // namespace

bool AIEqualizerAudioProcessor::isCaptureAllowed() noexcept
{
    return kCaptureEnabledForShipping
        || gCaptureAllowedOverride.load(std::memory_order_acquire);
}

void AIEqualizerAudioProcessor::setCaptureAllowedForTests(bool allowed) noexcept
{
    gCaptureAllowedOverride.store(allowed, std::memory_order_release);
}

bool AIEqualizerAudioProcessor::startManualCapture()
{
    // FA-001: never arm the service while capture is disabled for shipping.
    if (! isCaptureAllowed())
        return false;

    // Delegate to lock-free CaptureService
    captureBufferReady.store(false, std::memory_order_release);
    return captureService.startManualCapture();
}

void AIEqualizerAudioProcessor::stopManualCapture()
{
    // Delegate to lock-free CaptureService
    captureService.stopManualCapture();
    captureBufferReady.store(true, std::memory_order_release);
}

void AIEqualizerAudioProcessor::getManualCapturePreview(std::vector<float>& outMono, size_t maxSamples) const
{
    // Delegate to lock-free CaptureService
    captureService.getManualCapturePreview(outMono, maxSamples);
}

//==============================================================================
// FIX: applyAICorrections() with undo support and user learning recording

// Type-safe mapping from AI suggestion to processor filter enum (avoids fragile integer indices)
static ParametricEQProcessor::FilterType aiFilterTypeToProcessorType(AIEngine::Correction::FilterType filterType)
{
    switch (filterType)
    {
        case AIEngine::Correction::FilterType::LowCut:    return ParametricEQProcessor::FilterType::LowCut;
        case AIEngine::Correction::FilterType::LowShelf:  return ParametricEQProcessor::FilterType::LowShelf;
        case AIEngine::Correction::FilterType::HighShelf: return ParametricEQProcessor::FilterType::HighShelf;
        case AIEngine::Correction::FilterType::HighCut:   return ParametricEQProcessor::FilterType::HighCut;
        case AIEngine::Correction::FilterType::Notch:     return ParametricEQProcessor::FilterType::Notch;
        case AIEngine::Correction::FilterType::Peak:
        default:
            return ParametricEQProcessor::FilterType::Peak;
    }
}

void AIEqualizerAudioProcessor::applyAICorrections()
{
    auto* const messageManager = juce::MessageManager::getInstanceWithoutCreating();
    jassert(messageManager != nullptr && messageManager->isThisTheMessageThread());
    if (messageManager == nullptr || !messageManager->isThisTheMessageThread())
        return; // Do not mutate state or publish from a second producer thread.

    // FIX: Get ONLY approved corrections (user-approved, not all pending)
    // This ensures that when user clicks to fix a single problem, only that one is applied
    auto approved = aiEngine.getApprovedCorrections();

    if (approved.empty())
        return;

    // Cache numActiveBands for this function (atomic load once)
    const int activeBands = numActiveBands.load(std::memory_order_relaxed);

    // Merge nearby approved corrections to avoid duplicate bands
    auto merged = aiEngine.mergeNearbyCorrections(approved);

    // Limit to available bands (but don't hard-limit to 8)
    const int maxAssignable = std::min(static_cast<int>(merged.size()), activeBands);
    merged.resize(maxAssignable);

    if (merged.empty())
        return;

    // Save current state for undo BEFORE applying corrections (using HistoryManager)
    historyManager.pushUndoState("AI Correction Applied (" + juce::String(merged.size()) + " bands)");

    // Track which bands are already used
    std::vector<bool> bandUsed(maxBands, false);
    std::vector<float> bandFreqs(maxBands, 0.0f);

    // Pre-scan existing bands
    for (int i = 0; i < activeBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        if (auto* freqParam = apvts.getRawParameterValue(prefix + "Freq"))
        {
            bandFreqs[i] = freqParam->load();
            if (auto* gainParam = apvts.getRawParameterValue(prefix + "Gain"))
            {
                // Consider band "used" if gain is significant
                bandUsed[i] = std::abs(gainParam->load()) > 0.5f;
            }
        }
    }

    bool anyMaterialChange = false;

    // Assign corrections to bands intelligently
    for (const auto& corr : merged)
    {
        // Get scaled correction based on strength
        auto scaled = aiEngine.getScaledCorrection(corr);

        // Intelligent band assignment:
        // 1. Try to reuse existing band if within 1/5 octave and compatible type
        // 2. Otherwise find unused band closest to target frequency
        // 3. If no unused band, use closest available band

        int bestBand = -1;
        float bestScore = std::numeric_limits<float>::max();

        const float targetFreq = scaled.frequency;
        const float reuseThreshold = targetFreq * kBandReuseThresholdOctave;

        for (int i = 0; i < activeBands; ++i)
        {
            float dist = std::abs(bandFreqs[i] - targetFreq);
            float score = dist;

            // Prefer unused bands (much lower score)
            if (!bandUsed[i])
            {
                score *= kUnusedBandScoreMult;
            }

            // Prefer bands within reuse threshold (can merge)
            if (dist < reuseThreshold)
            {
                score *= kReuseThresholdScoreMult;
            }

            if (score < bestScore)
            {
                bestScore = score;
                bestBand = i;
            }
        }

        if (bestBand >= 0)
        {
            auto state = getBandState(bestBand);
            state.frequency = scaled.frequency;
            state.gain = scaled.suggestedGain;
            state.q = scaled.suggestedQ;
            state.type = static_cast<int>(aiFilterTypeToProcessorType(scaled.suggestedFilter));
            // AI cuts default to a steeper, more effective slope than 12 dB/oct.
            // Slope is inert for non-cut types (the DSP ignores it unless LowCut/
            // HighCut), so only override it for cuts. The only AI cut today is a
            // sub-50Hz LowCut from LowEndBoom — 24 dB/oct removes sub rumble cleanly.
            if (state.type == static_cast<int>(ParametricEQProcessor::LowCut)
                || state.type == static_cast<int>(ParametricEQProcessor::HighCut))
                state.slope = 1; // index 1 = 24 dB/oct
            state.enabled = true;
            state.solo = false;

            anyMaterialChange = applyBandStateDelta(bestBand, state, false) || anyMaterialChange;

            // FIX: Record this to user learning system for better future suggestions
            // Only record if learning is enabled (privacy control)
            bool learningEnabled = apvts.getRawParameterValue("learningEnabled")->load() > 0.5f;
            if (learningEnabled)
            {
                userLearning.recordAISuggestionAccepted(
                    AIEngine::getProblemTypeName(corr.type),
                    scaled.frequency,
                    scaled.suggestedGain,
                    scaled.suggestedQ
                );
            }

            // Mark band as used
            bandUsed[bestBand] = true;
            bandFreqs[bestBand] = scaled.frequency;
        }
    }

    // Trigger dry→wet crossfade only if AI application actually changed band state.
    if (anyMaterialChange)
        aiCorrectionCrossfadePending.store(true, std::memory_order_release);

    // D1 exposure: convert the SAME merged/limited corrections actually
    // assigned to static EQ bands into the dynamic-correction snapshot BEFORE
    // approved corrections are cleared below. Published unconditionally
    // (message thread): with the "dynamicCorrections" param OFF the engine is a
    // bit-transparent no-op, and the snapshot is simply ready if the user
    // toggles it on. Known lifecycle gap (documented, D1-UX follow-up): a
    // manual EQ reset does not clear the snapshot — the param toggle is the
    // user-facing kill switch. applySingleCorrection() path: same follow-up.
    publishDynamicCorrectionsFromApplied(merged);

    // Clear ONLY approved corrections after applying (keep pending for future approval)
    // This allows user to approve more corrections later without losing pending ones
    aiEngine.clearApprovedCorrections();
}

void AIEqualizerAudioProcessor::applySingleCorrection(const AIEngine::Correction& correction)
{
    // Apply ONLY this specific correction (not all approved ones)
    // This is used when user clicks on a single problem to fix it

    // Cache numActiveBands for this function (atomic load once)
    const int activeBands = numActiveBands.load(std::memory_order_relaxed);

    // Get scaled correction based on strength
    auto scaled = aiEngine.getScaledCorrection(correction);

    // Save current state for undo BEFORE applying correction (using HistoryManager)
    historyManager.pushUndoState("AI Correction Applied: " + AIEngine::getProblemTypeName(correction.type) + " @ " + juce::String(correction.frequency, 1) + " Hz");

    // Track which bands are already used
    std::vector<bool> bandUsed(maxBands, false);
    std::vector<float> bandFreqs(maxBands, 0.0f);

    // Pre-scan existing bands
    for (int i = 0; i < activeBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        if (auto* freqParam = apvts.getRawParameterValue(prefix + "Freq"))
        {
            bandFreqs[i] = freqParam->load();
            if (auto* gainParam = apvts.getRawParameterValue(prefix + "Gain"))
            {
                // Consider band "used" if gain is significant
                bandUsed[i] = std::abs(gainParam->load()) > 0.5f;
            }
        }
    }

    // Find best band for this single correction
    int bestBand = -1;
    float bestScore = std::numeric_limits<float>::max();

    const float targetFreq = scaled.frequency;
    const float reuseThreshold = targetFreq * kBandReuseThresholdOctave;

    for (int i = 0; i < activeBands; ++i)
    {
        float dist = std::abs(bandFreqs[i] - targetFreq);
        float score = dist;

        // Prefer unused bands (much lower score)
        if (!bandUsed[i])
        {
            score *= kUnusedBandScoreMult;
        }

        // Prefer bands within reuse threshold (can merge)
        if (dist < reuseThreshold)
        {
            score *= kReuseThresholdScoreMult;
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestBand = i;
        }
    }

    bool anyMaterialChange = false;

    if (bestBand >= 0)
    {
        auto state = getBandState(bestBand);
        state.frequency = scaled.frequency;
        state.gain = scaled.suggestedGain;
        state.q = scaled.suggestedQ;
        state.type = static_cast<int>(aiFilterTypeToProcessorType(scaled.suggestedFilter));
        // AI cuts get a steeper 24 dB/oct slope (inert for non-cut types). See applyAICorrections.
        if (state.type == static_cast<int>(ParametricEQProcessor::LowCut)
            || state.type == static_cast<int>(ParametricEQProcessor::HighCut))
            state.slope = 1; // index 1 = 24 dB/oct
        state.enabled = true;
        state.solo = false;

        anyMaterialChange = applyBandStateDelta(bestBand, state, false) || anyMaterialChange;

        // Record this to user learning system for better future suggestions
        // Only record if learning is enabled (privacy control)
        bool learningEnabled = apvts.getRawParameterValue("learningEnabled")->load() > 0.5f;
        if (learningEnabled)
        {
            userLearning.recordAISuggestionAccepted(
                AIEngine::getProblemTypeName(correction.type),
                scaled.frequency,
                scaled.suggestedGain,
                scaled.suggestedQ
            );
        }
    }

    // Trigger dry→wet crossfade only when the AI correction actually changed state.
    if (anyMaterialChange)
        aiCorrectionCrossfadePending.store(true, std::memory_order_release);

    // Remove this correction from pending (user has applied it)
    // Find and remove the matching correction from pendingCorrections
    auto pending = aiEngine.getPendingCorrections();
    for (int i = 0; i < static_cast<int>(pending.size()); ++i)
    {
        const auto& p = pending[i];
        const float freqRatio = std::abs(std::log2(correction.frequency / juce::jmax(20.0f, p.frequency)));
        const bool freqMatch = freqRatio < 0.01f;  // Within 1%
        const bool typeMatch = (p.type == correction.type);
        const bool gainMatch = std::abs(p.suggestedGain - correction.suggestedGain) < 0.5f;  // Within 0.5dB

        if (freqMatch && typeMatch && gainMatch)
        {
            aiEngine.rejectCorrection(i);  // Remove from pending
            break;
        }
    }
}

AIEqualizerAudioProcessor::SemanticApplyResult
AIEqualizerAudioProcessor::applySemanticAdjustments(
    const std::vector<SemanticEQEngine::SemanticEQAdjustment>& adjustments,
    SemanticApplyPolicy policy)
{
    SemanticApplyResult result;
    result.requestedBands = static_cast<int>(adjustments.size());

    // APVTS writes are message-thread owned. The typed PLAN/APPLY UI calls this
    // on the message thread and therefore receives authoritative feedback. Any
    // defensive off-thread caller is deferred and explicitly told that the
    // synchronous result is not authoritative.
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        result.deferredToMessageThread = true;
        result.rejectedBands = result.requestedBands;

        juce::WeakReference<AIEqualizerAudioProcessor> weakThis(this);
        juce::MessageManager::callAsync([weakThis, adjustments, policy]()
        {
            if (auto* self = weakThis.get())
                (void) self->applySemanticAdjustments(adjustments, policy);
        });
        return result;
    }

    auto bandStatesEquivalent = [](const BandState& a, const BandState& b) noexcept
    {
        return std::abs(a.frequency - b.frequency) <= 1.0f
            && std::abs(a.gain - b.gain) <= 0.05f
            && std::abs(a.q - b.q) <= 0.02f
            && a.type == b.type
            && a.enabled == b.enabled
            && a.solo == b.solo
            && a.slope == b.slope
            && a.curveMode == b.curveMode
            && a.dynMode == b.dynMode
            && a.dynTrigger == b.dynTrigger
            && a.detectionMode == b.detectionMode
            && a.detectorSource == b.detectorSource
            && std::abs(a.sidechainFrequency - b.sidechainFrequency) <= 1.0f
            && std::abs(a.sidechainQ - b.sidechainQ) <= 0.02f
            && std::abs(a.dynThreshold - b.dynThreshold) <= 0.05f
            && std::abs(a.dynRatio - b.dynRatio) <= 0.02f
            && std::abs(a.dynAttack - b.dynAttack) <= 0.05f
            && std::abs(a.dynRelease - b.dynRelease) <= 0.05f
            && std::abs(a.dynRange - b.dynRange) <= 0.05f
            && std::abs(a.dynKnee - b.dynKnee) <= 0.05f;
    };

    auto hasAnySemanticOwnership = [&]() noexcept
    {
        for (bool owned : semanticBandOwned)
            if (owned)
                return true;
        return false;
    };

    // Translate the requested vector into stable (quality, ordinal) keys first.
    // This also removes the historical artificial 4-band cap: the assignment
    // table now spans maxBands per quality, while the global plugin band count
    // remains the real physical budget.
    std::array<std::array<bool, kMaxSemanticBandSlots>,
               SemanticEQEngine::numQualities> desiredAssignments {};
    for (auto& perQuality : desiredAssignments)
        perQuality.fill(false);

    std::vector<int> requestedOrdinals(adjustments.size(), -1);
    std::vector<int> requestedQualityIndices(adjustments.size(), -1);
    std::array<int, SemanticEQEngine::numQualities> ordinalCounter {};
    ordinalCounter.fill(0);

    for (std::size_t i = 0; i < adjustments.size(); ++i)
    {
        const int qIdx = static_cast<int>(adjustments[i].sourceQuality);
        if (qIdx < 0 || qIdx >= SemanticEQEngine::numQualities)
            continue;

        const int ordinal = ordinalCounter[static_cast<std::size_t>(qIdx)]++;
        if (ordinal < 0 || ordinal >= kMaxSemanticBandSlots)
            continue;

        requestedQualityIndices[i] = qIdx;
        requestedOrdinals[i] = ordinal;
        desiredAssignments[static_cast<std::size_t>(qIdx)]
                          [static_cast<std::size_t>(ordinal)] = true;
    }

    // Read-only preflight. Snapshot JUCE/APVTS-owned state into a pure model,
    // then let the independently-tested allocator prove whether the complete
    // typed plan can fit before any product state is mutated.
    auto assignmentStillSemantic = [&](int owner) noexcept
    {
        if (owner < 0 || owner >= maxBands)
            return false;
        const auto slot = static_cast<std::size_t>(owner);
        if (!semanticBandOwned[slot])
            return false;
        return bandStatesEquivalent(getBandState(owner), semanticBandLastAppliedStates[slot]);
    };

    std::vector<EmberSemantic::SlotAvailability> preflightSlots;
    preflightSlots.reserve(maxBands);
    for (int slot = 0; slot < maxBands; ++slot)
    {
        const auto state = getBandState(slot);
        preflightSlots.push_back({ state.enabled, state.solo });
    }

    std::vector<EmberSemantic::RequestedSemanticSlot> preflightRequests;
    preflightRequests.reserve(adjustments.size());
    for (std::size_t i = 0; i < adjustments.size(); ++i)
        preflightRequests.push_back({ requestedQualityIndices[i], requestedOrdinals[i] });

    std::vector<EmberSemantic::ExistingSemanticSlot> preflightExisting;
    for (std::size_t q = 0; q < semanticBandAssignments.size(); ++q)
    {
        for (std::size_t ordinal = 0; ordinal < semanticBandAssignments[q].size(); ++ordinal)
        {
            const int owner = semanticBandAssignments[q][ordinal];
            if (owner < 0 || owner >= maxBands)
                continue;

            const auto slot = static_cast<std::size_t>(owner);
            EmberSemantic::ExistingSemanticSlot existing;
            existing.group = static_cast<int>(q);
            existing.ordinal = static_cast<int>(ordinal);
            existing.slot = owner;
            existing.stillSemantic = assignmentStillSemantic(owner);
            existing.hasSnapshot = semanticBandHasSnapshot[slot];
            if (existing.hasSnapshot)
            {
                const auto& original = semanticBandOriginalStates[slot];
                existing.originalState = { original.enabled, original.solo };
            }
            preflightExisting.push_back(existing);
        }
    }

    const auto preflight = EmberSemantic::preflightSemanticSlots(
        preflightSlots, preflightRequests, preflightExisting);
    const auto& resolvedSlots = preflight.resolvedSlots;
    const int resolvableCount = preflight.resolvableCount;

    const int invalidOrUnresolved = result.requestedBands - resolvableCount;
    if (policy == SemanticApplyPolicy::RequireCompletePlan && invalidOrUnresolved > 0)
    {
        result.atomicRejected = true;
        result.rejectedBands = result.requestedBands;
        return result;
    }

    bool semanticHistoryCaptured = false;
    auto ensureSemanticHistorySnapshot = [&]()
    {
        if (!semanticHistoryCaptured)
        {
            historyManager.pushUndoState("Semantic EQ Plan");
            semanticHistoryCaptured = true;
        }
    };

    const bool hadOwnershipAtEntry = hasAnySemanticOwnership();
    if (!hadOwnershipAtEntry && !adjustments.empty())
    {
        semanticOriginalActiveBandCount = getNumActiveBands();
        semanticLastRequestedActiveBandCount = semanticOriginalActiveBandCount;
    }

    // First reconcile stale/manual-taken-over assignments and restore obsolete
    // semantic slots BEFORE claiming the new plan. This makes the preflight and
    // actual allocator equivalent and lets a new plan reuse slots released by
    // the previous plan within the same undo transaction.
    for (std::size_t q = 0; q < semanticBandAssignments.size(); ++q)
    {
        for (std::size_t ordinal = 0; ordinal < semanticBandAssignments[q].size(); ++ordinal)
        {
            auto& owner = semanticBandAssignments[q][ordinal];
            if (owner < 0 || owner >= maxBands)
            {
                owner = -1;
                continue;
            }

            const auto slot = static_cast<std::size_t>(owner);
            if (!semanticBandOwned[slot])
            {
                owner = -1;
                continue;
            }

            const auto current = getBandState(owner);
            const bool stillSemantic = bandStatesEquivalent(
                current, semanticBandLastAppliedStates[slot]);

            if (!stillSemantic)
            {
                // Explicit user takeover: preserve the user's current state.
                semanticBandOwned[slot] = false;
                semanticBandHasSnapshot[slot] = false;
                owner = -1;
                continue;
            }

            if (desiredAssignments[q][ordinal])
                continue;

            if (semanticBandHasSnapshot[slot])
            {
                const auto& original = semanticBandOriginalStates[slot];
                if (!bandStatesEquivalent(current, original))
                {
                    ensureSemanticHistorySnapshot();
                    setBandState(owner, original);
                }
            }

            semanticBandOwned[slot] = false;
            semanticBandHasSnapshot[slot] = false;
            owner = -1;
        }
    }

    int desiredActiveBands = getNumActiveBands();

    for (std::size_t i = 0; i < adjustments.size(); ++i)
    {
        const int qIdx = requestedQualityIndices[i];
        const int ordinal = requestedOrdinals[i];
        const int slotIndex = resolvedSlots[i];
        if (qIdx < 0 || ordinal < 0 || slotIndex < 0 || slotIndex >= maxBands)
            continue;

        auto& owner = semanticBandAssignments[static_cast<std::size_t>(qIdx)]
                                             [static_cast<std::size_t>(ordinal)];
        const auto slot = static_cast<std::size_t>(slotIndex);

        const bool reusingSameOwnedSlot = owner == slotIndex && semanticBandOwned[slot]
            && bandStatesEquivalent(getBandState(slotIndex), semanticBandLastAppliedStates[slot]);

        if (!reusingSameOwnedSlot)
        {
            // The preflight guarantees this slot is disabled/unclaimed after
            // reconciliation. Snapshot it so Reset/next plan can restore the
            // exact pre-semantic state.
            owner = slotIndex;
            semanticBandOriginalStates[slot] = getBandState(slotIndex);
            semanticBandLastAppliedStates[slot] = semanticBandOriginalStates[slot];
            semanticBandHasSnapshot[slot] = true;
            semanticBandOwned[slot] = true;
        }

        desiredActiveBands = std::max(desiredActiveBands, slotIndex + 1);

        const auto& adj = adjustments[i];
        auto state = getBandState(slotIndex);
        const BandState previousState = state;
        state.frequency = adj.frequency;
        state.gain = adj.gain;
        state.q = adj.q;
        state.type = adj.filterType;
        state.enabled = adj.enabled;
        state.solo = false;
        state.dynMode = 0; // typed/legacy semantic static plan cannot inherit stale dynamics
        state.dynTrigger = DynamicEQProcessor::TriggerSide_Above;

        if (!bandStatesEquivalent(previousState, state))
        {
            ensureSemanticHistorySnapshot();
            setBandState(slotIndex, state);
        }

        semanticBandLastAppliedStates[slot] = state;
        ++result.appliedBands;
    }

    result.rejectedBands = result.requestedBands - result.appliedBands;

    const bool needsActiveBandCountUpdate = desiredActiveBands > getNumActiveBands();
    if (needsActiveBandCountUpdate)
    {
        if (auto* param = apvts.getParameter("numActiveBands"))
        {
            const int clamped = juce::jlimit(1, maxBands, desiredActiveBands);
            ensureSemanticHistorySnapshot();
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(clamped - 1)));
            param->endChangeGesture();
            semanticLastRequestedActiveBandCount = clamped;
        }
    }

    // Empty/reset plans release semantic ownership and restore the pre-semantic
    // active-band count only if the user has not changed that control since.
    if (!hasAnySemanticOwnership())
    {
        if (semanticOriginalActiveBandCount > 0
            && semanticLastRequestedActiveBandCount > 0
            && getNumActiveBands() == semanticLastRequestedActiveBandCount
            && semanticOriginalActiveBandCount != semanticLastRequestedActiveBandCount)
        {
            if (auto* param = apvts.getParameter("numActiveBands"))
            {
                const int restored = juce::jlimit(1, maxBands, semanticOriginalActiveBandCount);
                ensureSemanticHistorySnapshot();
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(restored - 1)));
                param->endChangeGesture();
            }
        }

        semanticOriginalActiveBandCount = -1;
        semanticLastRequestedActiveBandCount = -1;
    }

    return result;
}

//==============================================================================
AIEqualizerAudioProcessor::BandState AIEqualizerAudioProcessor::getBandState(int bandIndex) const
{
    BandState state;

    // SAFETY: Bounds check
    if (bandIndex < 0 || bandIndex >= maxBands)
        return state;

    juce::String prefix = "band" + juce::String(bandIndex);

    // SAFETY: Check for null pointers before dereferencing
    if (auto* freqParam = apvts.getRawParameterValue(prefix + "Freq"))
        state.frequency = freqParam->load();
    else
        state.frequency = 1000.0f; // Default

    if (auto* gainParam = apvts.getRawParameterValue(prefix + "Gain"))
        state.gain = gainParam->load();
    else
        state.gain = 0.0f; // Default

    if (auto* qParam = apvts.getRawParameterValue(prefix + "Q"))
        state.q = qParam->load();
    else
        state.q = 1.0f; // Default

    if (auto* enabledParam = apvts.getRawParameterValue(prefix + "Enabled"))
        state.enabled = enabledParam->load() > 0.5f;
    else
        state.enabled = (bandIndex < 8); // Default: first 8 bands enabled

    if (auto* typeParam = apvts.getRawParameterValue(prefix + "Type"))
    {
        state.type = static_cast<int>(typeParam->load());
    }
    else
    {
        // Legacy fallback
        if (bandIndex == 0)
            state.type = 1; // Low Shelf
        else if (bandIndex == maxBands - 1)
            state.type = 3; // High Shelf
        else
            state.type = 2; // Peak
    }

    if (auto* soloParam = apvts.getRawParameterValue(prefix + "Solo"))
        state.solo = soloParam->load() > 0.5f;
    else
        state.solo = false;

    if (auto* slopeParam = apvts.getRawParameterValue(prefix + "Slope"))
        state.slope = static_cast<int>(std::round(slopeParam->load()));
    else
        state.slope = 0;

    if (auto* curveModeParam = apvts.getRawParameterValue(prefix + "CurveMode"))
        state.curveMode = juce::jlimit(kLegacyCurveMode, kSurgicalCurveMode,
                                      static_cast<int>(std::round(curveModeParam->load())));
    else
        state.curveMode = kLegacyCurveMode;

    if (auto* dynModeParam = apvts.getRawParameterValue(prefix + "DynMode"))
        state.dynMode = static_cast<int>(std::round(dynModeParam->load()));
    else
        state.dynMode = 0;

    if (auto* dynTriggerParam = apvts.getRawParameterValue(prefix + "DynTrigger"))
        state.dynTrigger = juce::jlimit(
            DynamicEQProcessor::TriggerSide_Above,
            DynamicEQProcessor::TriggerSide_Below,
            static_cast<int>(std::round(dynTriggerParam->load())));
    else
        state.dynTrigger = DynamicEQProcessor::TriggerSide_Above;

    if (auto* detectionParam = apvts.getRawParameterValue(prefix + "DetectionMode"))
        state.detectionMode = juce::jlimit(
            DynamicEQProcessor::DetectionMode_Peak,
            DynamicEQProcessor::DetectionMode_RMS,
            static_cast<int>(std::round(detectionParam->load())));
    else
        state.detectionMode = DynamicEQProcessor::DetectionMode_RMS;

    if (auto* sourceParam = apvts.getRawParameterValue(prefix + "DetectorSource"))
        state.detectorSource = juce::jlimit(
            DynamicEQProcessor::DetectorSource_InternalWideband,
            DynamicEQProcessor::DetectorSource_ExternalFiltered,
            static_cast<int>(std::round(sourceParam->load())));
    else
        state.detectorSource = DynamicEQProcessor::DetectorSource_InternalWideband;

    if (auto* sidechainFreqParam = apvts.getRawParameterValue(prefix + "SidechainFreq"))
        state.sidechainFrequency = sidechainFreqParam->load();
    else
        state.sidechainFrequency =
            AIEQDSP::defaultBandFrequencies[static_cast<size_t>(bandIndex)];

    if (auto* sidechainQParam = apvts.getRawParameterValue(prefix + "SidechainQ"))
        state.sidechainQ = sidechainQParam->load();
    else
        state.sidechainQ = 1.0f;

    if (auto* dynThresholdParam = apvts.getRawParameterValue(prefix + "Threshold"))
        state.dynThreshold = dynThresholdParam->load();
    else
        state.dynThreshold = -24.0f;

    if (auto* dynRatioParam = apvts.getRawParameterValue(prefix + "Ratio"))
        state.dynRatio = dynRatioParam->load();
    else
        state.dynRatio = 2.0f;

    if (auto* dynAttackParam = apvts.getRawParameterValue(prefix + "Attack"))
        state.dynAttack = dynAttackParam->load();
    else
        state.dynAttack = 10.0f;

    if (auto* dynReleaseParam = apvts.getRawParameterValue(prefix + "Release"))
        state.dynRelease = dynReleaseParam->load();
    else
        state.dynRelease = 100.0f;

    if (auto* dynRangeParam = apvts.getRawParameterValue(prefix + "Range"))
        state.dynRange = dynRangeParam->load();
    else
        state.dynRange = 24.0f;

    if (auto* dynKneeParam = apvts.getRawParameterValue(prefix + "Knee"))
        state.dynKnee = dynKneeParam->load();
    else
        state.dynKnee = 6.0f;

    return state;
}

bool AIEqualizerAudioProcessor::applyBandStateDelta(int bandIndex, const BandState& targetState, bool useGestures)
{
    // CRITICAL: Must be called from Message Thread for APVTS access
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        jassertfalse;
        return false;
    }

    if (bandIndex < 0 || bandIndex >= maxBands)
        return false;

    const BandState currentState = getBandState(bandIndex);
    BandState clampedState = targetState;
    clampedState.frequency = juce::jlimit(20.0f, 20000.0f, clampedState.frequency);
    clampedState.gain = juce::jlimit(-24.0f, 24.0f, clampedState.gain);
    clampedState.q = juce::jlimit(0.1f, 10.0f, clampedState.q);
    clampedState.type = juce::jlimit(0, 8, clampedState.type);
    clampedState.slope = juce::jlimit(0, 2, clampedState.slope);
    clampedState.curveMode = juce::jlimit(kLegacyCurveMode, kSurgicalCurveMode,
                                         clampedState.curveMode);
    clampedState.dynMode = juce::jlimit(0, 3, clampedState.dynMode);
    clampedState.dynTrigger = juce::jlimit(
        DynamicEQProcessor::TriggerSide_Above,
        DynamicEQProcessor::TriggerSide_Below,
        clampedState.dynTrigger);
    clampedState.detectionMode = juce::jlimit(
        DynamicEQProcessor::DetectionMode_Peak,
        DynamicEQProcessor::DetectionMode_RMS,
        clampedState.detectionMode);
    clampedState.detectorSource = juce::jlimit(
        DynamicEQProcessor::DetectorSource_InternalWideband,
        DynamicEQProcessor::DetectorSource_ExternalFiltered,
        clampedState.detectorSource);
    clampedState.sidechainFrequency = juce::jlimit(
        20.0f, 20000.0f, clampedState.sidechainFrequency);
    clampedState.sidechainQ = juce::jlimit(0.1f, 10.0f, clampedState.sidechainQ);
    clampedState.dynThreshold = juce::jlimit(-60.0f, 0.0f, clampedState.dynThreshold);
    clampedState.dynRatio = juce::jlimit(1.0f, 20.0f, clampedState.dynRatio);
    clampedState.dynAttack = juce::jlimit(0.1f, 500.0f, clampedState.dynAttack);
    clampedState.dynRelease = juce::jlimit(1.0f, 2000.0f, clampedState.dynRelease);
    clampedState.dynRange = juce::jlimit(0.0f, 48.0f, clampedState.dynRange);
    clampedState.dynKnee = juce::jlimit(0.0f, 24.0f, clampedState.dynKnee);

    const bool freqChanged = std::abs(currentState.frequency - clampedState.frequency) > 1.0f;
    const bool gainChanged = std::abs(currentState.gain - clampedState.gain) > 0.05f;
    const bool qChanged = std::abs(currentState.q - clampedState.q) > 0.02f;
    const bool typeChanged = currentState.type != clampedState.type;
    const bool enabledChanged = currentState.enabled != clampedState.enabled;
    const bool soloChanged = currentState.solo != clampedState.solo;
    const bool slopeChanged = currentState.slope != clampedState.slope;
    const bool curveModeChanged = currentState.curveMode != clampedState.curveMode;
    const bool dynModeChanged = currentState.dynMode != clampedState.dynMode;
    const bool dynTriggerChanged = currentState.dynTrigger != clampedState.dynTrigger;
    const bool detectionModeChanged = currentState.detectionMode != clampedState.detectionMode;
    const bool detectorSourceChanged = currentState.detectorSource != clampedState.detectorSource;
    const bool sidechainFrequencyChanged =
        std::abs(currentState.sidechainFrequency - clampedState.sidechainFrequency) > 1.0f;
    const bool sidechainQChanged = std::abs(currentState.sidechainQ - clampedState.sidechainQ) > 0.02f;
    const bool dynThresholdChanged = std::abs(currentState.dynThreshold - clampedState.dynThreshold) > 0.05f;
    const bool dynRatioChanged = std::abs(currentState.dynRatio - clampedState.dynRatio) > 0.02f;
    const bool dynAttackChanged = std::abs(currentState.dynAttack - clampedState.dynAttack) > 0.1f;
    const bool dynReleaseChanged = std::abs(currentState.dynRelease - clampedState.dynRelease) > 0.5f;
    const bool dynRangeChanged = std::abs(currentState.dynRange - clampedState.dynRange) > 0.05f;
    const bool dynKneeChanged = std::abs(currentState.dynKnee - clampedState.dynKnee) > 0.05f;

    const bool anyChanged = freqChanged || gainChanged || qChanged || typeChanged || enabledChanged || soloChanged ||
                            slopeChanged || curveModeChanged || dynModeChanged || dynTriggerChanged ||
                            detectionModeChanged || detectorSourceChanged || sidechainFrequencyChanged || sidechainQChanged ||
                            dynThresholdChanged || dynRatioChanged ||
                            dynAttackChanged || dynReleaseChanged || dynRangeChanged || dynKneeChanged;
    if (!anyChanged)
        return false;

    juce::String prefix = "band" + juce::String(bandIndex);

    auto applyParam = [useGestures](juce::RangedAudioParameter* param, float normalized)
    {
        if (param == nullptr)
            return;
        if (useGestures) param->beginChangeGesture();
        param->setValueNotifyingHost(normalized);
        if (useGestures) param->endChangeGesture();
    };

    if (freqChanged)
        applyParam(apvts.getParameter(prefix + "Freq"), apvts.getParameter(prefix + "Freq")->convertTo0to1(clampedState.frequency));
    if (gainChanged)
        applyParam(apvts.getParameter(prefix + "Gain"), apvts.getParameter(prefix + "Gain")->convertTo0to1(clampedState.gain));
    if (qChanged)
        applyParam(apvts.getParameter(prefix + "Q"), apvts.getParameter(prefix + "Q")->convertTo0to1(clampedState.q));
    if (typeChanged)
        applyParam(apvts.getParameter(prefix + "Type"), apvts.getParameter(prefix + "Type")->convertTo0to1(static_cast<float>(clampedState.type)));
    if (enabledChanged)
        applyParam(apvts.getParameter(prefix + "Enabled"), clampedState.enabled ? 1.0f : 0.0f);
    if (soloChanged)
        applyParam(apvts.getParameter(prefix + "Solo"), clampedState.solo ? 1.0f : 0.0f);
    if (slopeChanged)
        applyParam(apvts.getParameter(prefix + "Slope"), apvts.getParameter(prefix + "Slope")->convertTo0to1(static_cast<float>(clampedState.slope)));
    if (curveModeChanged)
        applyParam(apvts.getParameter(prefix + "CurveMode"),
                   apvts.getParameter(prefix + "CurveMode")->convertTo0to1(
                       static_cast<float>(clampedState.curveMode)));
    if (dynModeChanged)
        applyParam(apvts.getParameter(prefix + "DynMode"), apvts.getParameter(prefix + "DynMode")->convertTo0to1(static_cast<float>(clampedState.dynMode)));
    if (dynTriggerChanged)
        applyParam(apvts.getParameter(prefix + "DynTrigger"),
                   apvts.getParameter(prefix + "DynTrigger")->convertTo0to1(
                       static_cast<float>(clampedState.dynTrigger)));
    if (detectionModeChanged)
        applyParam(apvts.getParameter(prefix + "DetectionMode"),
                   apvts.getParameter(prefix + "DetectionMode")->convertTo0to1(
                       static_cast<float>(clampedState.detectionMode)));
    if (detectorSourceChanged)
        applyParam(apvts.getParameter(prefix + "DetectorSource"),
                   apvts.getParameter(prefix + "DetectorSource")->convertTo0to1(
                       static_cast<float>(clampedState.detectorSource)));
    if (sidechainFrequencyChanged)
        applyParam(apvts.getParameter(prefix + "SidechainFreq"),
                   apvts.getParameter(prefix + "SidechainFreq")->convertTo0to1(
                       clampedState.sidechainFrequency));
    if (sidechainQChanged)
        applyParam(apvts.getParameter(prefix + "SidechainQ"),
                   apvts.getParameter(prefix + "SidechainQ")->convertTo0to1(
                       clampedState.sidechainQ));
    if (dynThresholdChanged)
        applyParam(apvts.getParameter(prefix + "Threshold"), apvts.getParameter(prefix + "Threshold")->convertTo0to1(clampedState.dynThreshold));
    if (dynRatioChanged)
        applyParam(apvts.getParameter(prefix + "Ratio"), apvts.getParameter(prefix + "Ratio")->convertTo0to1(clampedState.dynRatio));
    if (dynAttackChanged)
        applyParam(apvts.getParameter(prefix + "Attack"), apvts.getParameter(prefix + "Attack")->convertTo0to1(clampedState.dynAttack));
    if (dynReleaseChanged)
        applyParam(apvts.getParameter(prefix + "Release"), apvts.getParameter(prefix + "Release")->convertTo0to1(clampedState.dynRelease));
    if (dynRangeChanged)
        applyParam(apvts.getParameter(prefix + "Range"), apvts.getParameter(prefix + "Range")->convertTo0to1(clampedState.dynRange));
    if (dynKneeChanged)
        applyParam(apvts.getParameter(prefix + "Knee"), apvts.getParameter(prefix + "Knee")->convertTo0to1(clampedState.dynKnee));

    markParametersChanged();
    return true;
}

void AIEqualizerAudioProcessor::setBandState(int bandIndex, const BandState& state)
{
    juce::ignoreUnused(applyBandStateDelta(bandIndex, state, true));
}

void AIEqualizerAudioProcessor::setBandGeometry(int bandIndex, float frequency,
                                                float gain, float q, int type,
                                                bool enabled)
{
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || ! mm->isThisTheMessageThread()
        || bandIndex < 0 || bandIndex >= maxBands)
    {
        jassertfalse;
        return;
    }

    const auto prefix = "band" + juce::String(bandIndex);
    auto write = [this](const juce::String& id, float plainValue)
    {
        if (auto* parameter = apvts.getParameter(id))
        {
            const float normalized = parameter->convertTo0to1(plainValue);
            if (std::abs(parameter->getValue() - normalized) <= 1.0e-7f)
                return;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(normalized);
            parameter->endChangeGesture();
        }
    };

    // Deliberately no BandState read/merge here: host automation may update an
    // advanced parameter concurrently with a graph drag.  This operation owns
    // exactly these five parameters and cannot overwrite anything else.
    write(prefix + "Freq", juce::jlimit(20.0f, 20000.0f, frequency));
    write(prefix + "Gain", juce::jlimit(-24.0f, 24.0f, gain));
    write(prefix + "Q", juce::jlimit(0.1f, 10.0f, q));
    write(prefix + "Type", static_cast<float>(juce::jlimit(0, 8, type)));
    write(prefix + "Enabled", enabled ? 1.0f : 0.0f);
    markParametersChanged();
}

//==============================================================================
const juce::String AIEqualizerAudioProcessor::getName() const { return JucePlugin_Name; }
bool AIEqualizerAudioProcessor::acceptsMidi() const { return false; }
bool AIEqualizerAudioProcessor::producesMidi() const { return false; }
bool AIEqualizerAudioProcessor::isMidiEffect() const { return false; }
double AIEqualizerAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int AIEqualizerAudioProcessor::getNumPrograms() { return 1; }
int AIEqualizerAudioProcessor::getCurrentProgram() { return 0; }
void AIEqualizerAudioProcessor::setCurrentProgram(int) {}
const juce::String AIEqualizerAudioProcessor::getProgramName(int) { return {}; }
void AIEqualizerAudioProcessor::changeProgramName(int, const juce::String&) {}
bool AIEqualizerAudioProcessor::hasEditor() const { return true; }

//==============================================================================
void AIEqualizerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // RB-2 FIX: slotMutex_ guarantees a transactional snapshot of all 4 slots.
    // This function can be called from ANY thread (Pro Tools, some VST3 hosts).
    // The mutex ensures no concurrent slot mutation during serialization.

    // Take a local copy of all 4 slots under the lock, then serialize outside.
    EQSlot localA, localB, localC, localD;
    ABState activeSlot;
    {
        std::lock_guard<std::recursive_mutex> lock(slotMutex_);

        activeSlot = currentABState.load(std::memory_order_relaxed);

        // Sync active slot from APVTS atomics (thread-safe reads)
        EQSlot* active = nullptr;
        switch (activeSlot)
        {
            case ABState::A: active = &slotA; break;
            case ABState::B: active = &slotB; break;
            case ABState::C: active = &slotC; break;
            case ABState::D: active = &slotD; break;
        }
        if (active != nullptr)
        {
            for (int i = 0; i < maxBands; ++i)
                active->bands[static_cast<size_t>(i)] = getBandState(i);
            if (auto* p = apvts.getRawParameterValue("outputGain"))
                active->outputGain = p->load();
            // RB-2 FIX: sync all slot-level APVTS fields (same as saveCurrentStateToSlot)
            if (auto* p = apvts.getRawParameterValue("dynEqEnabled"))
                active->dynEqEnabled = p->load() > 0.5f;
            if (auto* p = apvts.getRawParameterValue("dynEqMix"))
                active->dynEqMix = p->load();
            if (auto* p = apvts.getRawParameterValue("dynAutoMakeup"))
                active->dynAutoMakeup = p->load() > 0.5f;
        }

        // Copy all 4 slots under the lock → transactional snapshot
        localA = slotA;
        localB = slotB;
        localC = slotC;
        localD = slotD;
    }
    // Lock released — serialize from local copies (no contention during XML build)

    auto state = apvts.copyState();

    state.setProperty("stateSchemaVersion", kCurrentStateSchemaVersion, nullptr);
    state.setProperty("abState", static_cast<int>(activeSlot), nullptr);
    state.setProperty("slotAName", localA.name, nullptr);
    state.setProperty("slotBName", localB.name, nullptr);
    state.setProperty("slotCName", localC.name, nullptr);
    state.setProperty("slotDName", localD.name, nullptr);

    auto addSlotTree = [&state](const EQSlot& slot, const juce::Identifier& id)
    {
        state.removeChild(state.getChildWithName(id), nullptr);

        juce::ValueTree slotTree(id);
        slotTree.setProperty("name", slot.name, nullptr);
        slotTree.setProperty("outputGain", slot.outputGain, nullptr);
        slotTree.setProperty("dynEqEnabled", slot.dynEqEnabled, nullptr);
        slotTree.setProperty("dynEqMix", slot.dynEqMix, nullptr);
        slotTree.setProperty("dynAutoMakeup", slot.dynAutoMakeup, nullptr);

        for (int i = 0; i < maxBands; ++i)
        {
            const auto& band = slot.bands[static_cast<size_t>(i)];
            juce::ValueTree bandTree("band" + juce::String(i));
            bandTree.setProperty("freq", band.frequency, nullptr);
            bandTree.setProperty("gain", band.gain, nullptr);
            bandTree.setProperty("q", band.q, nullptr);
            bandTree.setProperty("type", band.type, nullptr);
            bandTree.setProperty("enabled", band.enabled, nullptr);
            bandTree.setProperty("solo", band.solo, nullptr);
            bandTree.setProperty("slope", band.slope, nullptr);
            bandTree.setProperty("curveMode", band.curveMode, nullptr);
            bandTree.setProperty("dynMode", band.dynMode, nullptr);
            bandTree.setProperty("dynTrigger", band.dynTrigger, nullptr);
            bandTree.setProperty("detectionMode", band.detectionMode, nullptr);
            bandTree.setProperty("detectorSource", band.detectorSource, nullptr);
            bandTree.setProperty("sidechainFrequency", band.sidechainFrequency, nullptr);
            bandTree.setProperty("sidechainQ", band.sidechainQ, nullptr);
            bandTree.setProperty("dynThreshold", band.dynThreshold, nullptr);
            bandTree.setProperty("dynRatio", band.dynRatio, nullptr);
            bandTree.setProperty("dynAttack", band.dynAttack, nullptr);
            bandTree.setProperty("dynRelease", band.dynRelease, nullptr);
            bandTree.setProperty("dynRange", band.dynRange, nullptr);
            bandTree.setProperty("dynKnee", band.dynKnee, nullptr);
            slotTree.addChild(bandTree, -1, nullptr);
        }

        state.addChild(std::move(slotTree), -1, nullptr);
    };

    addSlotTree(localA, "SlotA");
    addSlotTree(localB, "SlotB");
    addSlotTree(localC, "SlotC");
    addSlotTree(localD, "SlotD");

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AIEqualizerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            auto candidateState = juce::ValueTree::fromXml(*xmlState);
            const auto loadKind = AIEQStateSchema::prepareForLoad(candidateState);
            if (loadKind == AIEQStateSchema::LoadKind::Reject)
                return;

            const bool migratingLegacyState =
                loadKind == AIEQStateSchema::LoadKind::LegacyWithoutSchema;

            // Phase 1: Restore APVTS — synchronous, audio thread sees new values immediately.
            // Schema classification happens before this point, so unsupported or
            // malformed future states leave both APVTS and A/B slots untouched.
            apvts.replaceState(candidateState);

            // FIX: Force parameter listeners to fire so the UI EQ curve updates.
            // replaceState() only swaps the tree without calling updateParameterConnectionsToChildTrees,
            // so parameterChanged() callbacks never trigger and the spectrum display stays stale.
            for (auto* param : apvts.processor.getParameters())
            {
                if (auto* apvtsParam = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                {
                    const auto& id = apvtsParam->paramID;
                    const bool isCurveAffecting = id.startsWith("band")
                        || id == "numActiveBands"
                        || id == "phaseMode"
                        || id == "msMode"
                        || id == "oversamplingFactor";
                    if (isCurveAffecting)
                    {
                        const float currentValue = apvtsParam->getValue();
                        apvtsParam->sendValueChangedMessageToListeners(currentValue);
                    }
                }
            }

            // Phase 2: Restore all 4 slot structs under the lock — synchronous, no async gap.
            // RB-2 FIX: slotMutex_ ensures no concurrent reader (getStateInformation, UI)
            // can observe a half-restored state. The entire restore is transactional.
            {
                std::lock_guard<std::recursive_mutex> lock(slotMutex_);

                auto loadedState = apvts.state;

                // A pre-v1 host state predates CurveMode. Make the migration
                // total even when an old state has no persisted inactive-slot
                // trees: every extant slot starts in Legacy before any
                // explicitly serialized slot data is restored below.
                if (migratingLegacyState)
                {
                    for (auto* slot : { &slotA, &slotB, &slotC, &slotD })
                        for (auto& band : slot->bands)
                            band.curveMode = kLegacyCurveMode;
                }

                // Restore active slot index
                if (loadedState.hasProperty("abState"))
                {
                    int abIdx = static_cast<int>(loadedState.getProperty("abState"));
                    currentABState.store(static_cast<ABState>(juce::jlimit(0, 3, abIdx)), std::memory_order_relaxed);
                }

                // Restore slot names
                if (loadedState.hasProperty("slotAName"))
                    slotA.name = loadedState.getProperty("slotAName").toString();
                if (loadedState.hasProperty("slotBName"))
                    slotB.name = loadedState.getProperty("slotBName").toString();
                if (loadedState.hasProperty("slotCName"))
                    slotC.name = loadedState.getProperty("slotCName").toString();
                if (loadedState.hasProperty("slotDName"))
                    slotD.name = loadedState.getProperty("slotDName").toString();

                // Restore full slot contents (bands + output gain)
                auto restoreSlot = [&loadedState, migratingLegacyState](const juce::Identifier& id,
                                                                        EQSlot& slot)
                {
                    auto slotTree = loadedState.getChildWithName(id);
                    if (!slotTree.isValid())
                        return;

                    if (slotTree.hasProperty("name"))
                        slot.name = slotTree.getProperty("name").toString();
                    if (slotTree.hasProperty("outputGain"))
                        slot.outputGain = static_cast<float>(slotTree.getProperty("outputGain"));
                    if (slotTree.hasProperty("dynEqEnabled"))
                        slot.dynEqEnabled = static_cast<bool>(slotTree.getProperty("dynEqEnabled"));
                    if (slotTree.hasProperty("dynEqMix"))
                        slot.dynEqMix = static_cast<float>(slotTree.getProperty("dynEqMix"));
                    if (slotTree.hasProperty("dynAutoMakeup"))
                        slot.dynAutoMakeup = static_cast<bool>(slotTree.getProperty("dynAutoMakeup"));

                    for (int i = 0; i < maxBands; ++i)
                    {
                        auto bandTree = slotTree.getChildWithName("band" + juce::String(i));
                        if (!bandTree.isValid())
                            continue;

                        auto& band = slot.bands[static_cast<size_t>(i)];
                        if (bandTree.hasProperty("freq"))
                            band.frequency = static_cast<float>(bandTree.getProperty("freq"));
                        if (bandTree.hasProperty("gain"))
                            band.gain = static_cast<float>(bandTree.getProperty("gain"));
                        if (bandTree.hasProperty("q"))
                            band.q = static_cast<float>(bandTree.getProperty("q"));
                        if (bandTree.hasProperty("type"))
                            band.type = static_cast<int>(bandTree.getProperty("type"));
                        if (bandTree.hasProperty("enabled"))
                            band.enabled = static_cast<bool>(bandTree.getProperty("enabled"));
                        if (bandTree.hasProperty("solo"))
                            band.solo = static_cast<bool>(bandTree.getProperty("solo"));
                        if (bandTree.hasProperty("slope"))
                            band.slope = static_cast<int>(bandTree.getProperty("slope"));
                        if (bandTree.hasProperty("curveMode"))
                            band.curveMode = juce::jlimit(
                                kLegacyCurveMode, kSurgicalCurveMode,
                                static_cast<int>(bandTree.getProperty("curveMode")));
                        else if (migratingLegacyState)
                            band.curveMode = kLegacyCurveMode;
                        if (bandTree.hasProperty("dynMode"))
                            band.dynMode = static_cast<int>(bandTree.getProperty("dynMode"));
                        if (bandTree.hasProperty("dynTrigger"))
                            band.dynTrigger = juce::jlimit(
                                DynamicEQProcessor::TriggerSide_Above,
                                DynamicEQProcessor::TriggerSide_Below,
                                static_cast<int>(bandTree.getProperty("dynTrigger")));
                        else
                            band.dynTrigger = DynamicEQProcessor::TriggerSide_Above;
                        if (bandTree.hasProperty("detectionMode"))
                            band.detectionMode = juce::jlimit(
                                DynamicEQProcessor::DetectionMode_Peak,
                                DynamicEQProcessor::DetectionMode_RMS,
                                static_cast<int>(bandTree.getProperty("detectionMode")));
                        else
                            band.detectionMode = DynamicEQProcessor::DetectionMode_RMS;
                        if (bandTree.hasProperty("detectorSource"))
                            band.detectorSource = juce::jlimit(
                                DynamicEQProcessor::DetectorSource_InternalWideband,
                                DynamicEQProcessor::DetectorSource_ExternalFiltered,
                                static_cast<int>(bandTree.getProperty("detectorSource")));
                        else
                            band.detectorSource = DynamicEQProcessor::DetectorSource_InternalWideband;
                        if (bandTree.hasProperty("sidechainFrequency"))
                            band.sidechainFrequency = juce::jlimit(
                                20.0f, 20000.0f,
                                static_cast<float>(bandTree.getProperty("sidechainFrequency")));
                        else
                            band.sidechainFrequency =
                                AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)];
                        if (bandTree.hasProperty("sidechainQ"))
                            band.sidechainQ = juce::jlimit(
                                0.1f, 10.0f,
                                static_cast<float>(bandTree.getProperty("sidechainQ")));
                        else
                            band.sidechainQ = 1.0f;
                        if (bandTree.hasProperty("dynThreshold"))
                            band.dynThreshold = static_cast<float>(bandTree.getProperty("dynThreshold"));
                        if (bandTree.hasProperty("dynRatio"))
                            band.dynRatio = static_cast<float>(bandTree.getProperty("dynRatio"));
                        if (bandTree.hasProperty("dynAttack"))
                            band.dynAttack = static_cast<float>(bandTree.getProperty("dynAttack"));
                        if (bandTree.hasProperty("dynRelease"))
                            band.dynRelease = static_cast<float>(bandTree.getProperty("dynRelease"));
                        if (bandTree.hasProperty("dynRange"))
                            band.dynRange = static_cast<float>(bandTree.getProperty("dynRange"));
                        if (bandTree.hasProperty("dynKnee"))
                            band.dynKnee = static_cast<float>(bandTree.getProperty("dynKnee"));
                    }
                };

                restoreSlot("SlotA", slotA);
                restoreSlot("SlotB", slotB);
                restoreSlot("SlotC", slotC);
                restoreSlot("SlotD", slotD);

                // RB-2 FIX: sync active slot from APVTS synchronously (was callAsync — async gap
                // allowed stale slot data if user switched slots before callback fired).
                // saveCurrentStateToSlot now works from any thread under slotMutex_.
                const ABState activeSlot = currentABState.load(std::memory_order_relaxed);
                saveCurrentStateToSlot(activeSlot);
            }
            // Lock released — slots are fully consistent with APVTS.

            // IR rebuild must still happen on message thread (triggers background work).
            if (currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::LinearPhase)
            {
                juce::WeakReference<AIEqualizerAudioProcessor> weakThis(this);
                auto doIRRebuild = [weakThis]()
                {
                    if (auto* self = weakThis.get())
                    {
                        for (auto& loaded : self->linearIRLoaded)
                            loaded.store(false, std::memory_order_relaxed);
                        self->triggerLinearPhaseIRUpdate();
                    }
                };

                if (auto* mm = juce::MessageManager::getInstance())
                {
                    if (mm->isThisTheMessageThread())
                        doIRRebuild();
                    else
                        juce::MessageManager::callAsync(doIRRebuild);
                }
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* AIEqualizerAudioProcessor::createEditor()
{
    return new AIEqualizerAudioProcessorEditor(*this);
}

//==============================================================================
// Undo/Redo Implementation (now delegated to HistoryManager)
//==============================================================================

// Note: undo(), redo(), canUndo(), canRedo(), getUndoDescription(), getRedoDescription()
// are now inline in the header, delegating to historyManager.
// pushUndoState() is also inline in the header.

//==============================================================================
// Mid/Side Encoding/Decoding Implementation
//==============================================================================

void AIEqualizerAudioProcessor::encodeMidSide(juce::AudioBuffer<float>& buffer, int numSamples)
{
    // M/S Encoding in-place: write Mid/Side into the buffer (ch0=Mid, ch1=Side)
    if (buffer.getNumChannels() < 2)
        return;

    const int samples = juce::jmin(numSamples, msBuffer.getNumSamples(), buffer.getNumSamples());
    if (samples <= 0)
        return;

    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(1);
    float* midTmp = msBuffer.getWritePointer(0);
    float* sideTmp = msBuffer.getWritePointer(1);

    juce::FloatVectorOperations::add(midTmp, left, right, samples);
    juce::FloatVectorOperations::multiply(midTmp, kInvSqrt2, samples);

    juce::FloatVectorOperations::copy(sideTmp, left, samples);
    juce::FloatVectorOperations::subtract(sideTmp, right, samples);
    juce::FloatVectorOperations::multiply(sideTmp, kInvSqrt2, samples);

    buffer.copyFrom(0, 0, midTmp, samples);
    buffer.copyFrom(1, 0, sideTmp, samples);
}

void AIEqualizerAudioProcessor::decodeMidSide(juce::AudioBuffer<float>& buffer, int numSamples)
{
    // M/S Decoding in-place: buffer has Mid/Side (ch0/ch1), convert to L/R
    if (buffer.getNumChannels() < 2)
        return;

    const int samples = juce::jmin(numSamples, msBuffer.getNumSamples(), buffer.getNumSamples());
    if (samples <= 0)
        return;

    const float* mid = buffer.getReadPointer(0);
    const float* side = buffer.getReadPointer(1);
    float* left = msBuffer.getWritePointer(0);
    float* right = msBuffer.getWritePointer(1);

    juce::FloatVectorOperations::add(left, mid, side, samples);
    juce::FloatVectorOperations::multiply(left, kInvSqrt2, samples);

    juce::FloatVectorOperations::copy(right, mid, samples);
    juce::FloatVectorOperations::subtract(right, side, samples);
    juce::FloatVectorOperations::multiply(right, kInvSqrt2, samples);

    buffer.copyFrom(0, 0, left, samples);
    buffer.copyFrom(1, 0, right, samples);
}

//==============================================================================
// AI Command Queue (Lock-Free Communication)
//==============================================================================

void AIEqualizerAudioProcessor::queueAICommand(const AIEQCore::AICommand& command) noexcept
{
    // Queue command for audio thread (lock-free SPSC queue)
    // If queue is full, command is dropped (prevents blocking)
    const bool queued = aiCommandQueue.tryPush(command);
    juce::ignoreUnused(queued);
}

void AIEqualizerAudioProcessor::processAICommands() noexcept
{
    // Process all pending AI commands (RT-SAFE, lock-free)
    // Called at the start of each processBlock

    AIEQCore::AICommand cmd;
    while (aiCommandQueue.tryPop(cmd))
    {
        switch (cmd.type)
        {
            case AIEQCore::AICommandType::ApplyCorrection:
            {
                // Apply correction to the specified band
                const int bandIdx = cmd.bandIndex;
                if (bandIdx >= 0 && bandIdx < maxBands)
                {
                    // Update EQ processor directly (RT-safe, no APVTS access)
                    if (bandIdx < eqProcessor.getNumBands())
                    {
                        eqProcessor.setBandParameters(bandIdx, cmd.frequency, cmd.gain, cmd.q, cmd.filterType);
                        eqProcessor.setBandEnabled(bandIdx, true);
                    }
                    if (bandIdx < eqProcessorHQ.getNumBands())
                    {
                        eqProcessorHQ.setBandParameters(bandIdx, cmd.frequency, cmd.gain, cmd.q, cmd.filterType);
                        eqProcessorHQ.setBandEnabled(bandIdx, true);
                    }

                    // Mark parameters changed for GUI update
                    markParametersChanged();
                }
                break;
            }

            case AIEQCore::AICommandType::ClearCorrections:
            {
                // Reset all bands to neutral (0 dB gain)
                for (int i = 0; i < eqProcessor.getNumBands(); ++i)
                {
                    eqProcessor.setBandParameters(i, eqProcessor.getBandFrequency(i), 0.0f,
                                                  eqProcessor.getBandQ(i), eqProcessor.getBandType(i));
                }
                markParametersChanged();
                break;
            }

            case AIEQCore::AICommandType::SetBandParameter:
            {
                // Single parameter update
                const int bandIdx = cmd.bandIndex;
                if (bandIdx >= 0 && bandIdx < eqProcessor.getNumBands())
                {
                    eqProcessor.setBandParameters(bandIdx, cmd.frequency, cmd.gain, cmd.q, cmd.filterType);
                }
                break;
            }

            case AIEQCore::AICommandType::TriggerAnalysis:
            {
                // Signal AI to run analysis on next frame
                // This is handled by the GUI timer, just mark dirty flag
                aiProblemsChanged.store(true, std::memory_order_release);
                break;
            }

            case AIEQCore::AICommandType::None:
            default:
                break;
        }
    }
}

//==============================================================================
// Parameter Snapshot Loading (for per-block caching)
//==============================================================================

void AIEqualizerAudioProcessor::loadParameterSnapshot(AIEQCore::ProcessBlockParameters& params) noexcept
{
    // Load all parameters once at block start to avoid repeated atomic loads in sample loops
    // This is a key optimization for professional audio plugin performance

    auto loadParam = [](std::atomic<float>* ptr, float fallback) -> float
    {
        return ptr ? ptr->load(std::memory_order_relaxed) : fallback;
    };

    // FIX: Clamp to maxBands to prevent array out-of-bounds access in loops
    params.numActiveBands = std::min(numActiveBands.load(std::memory_order_relaxed), maxBands);
    params.outputGainDB = loadParam(cachedOutputGain, 0.0f);
    params.autoGainEnabled = autoGainEnabled.load(std::memory_order_relaxed);
    params.autoGainCompensationDB = autoGainCompensation.load(std::memory_order_relaxed);
    params.dynamicEQEnabled = loadParam(cachedDynEqEnabled, 1.0f) > 0.5f;
    params.phaseMode = static_cast<int>(currentPhaseMode.load(std::memory_order_relaxed));
    params.msMode = static_cast<int>(currentMSMode.load(std::memory_order_relaxed));
    params.oversamplingFactor = oversamplingFactor.load(std::memory_order_relaxed);

    // Load band parameters
    for (int i = 0; i < params.numActiveBands && i < AIEQCore::kMaxBands; ++i)
    {
        const auto& cached = cachedParams[static_cast<size_t>(i)];
        auto& band = params.bands[static_cast<size_t>(i)];

        band.frequency = juce::jlimit(10.0f, 24000.0f, loadParam(cached.freq, 1000.0f));
        band.gain = juce::jlimit(-48.0f, 48.0f, loadParam(cached.gain, 0.0f));
        band.q = juce::jlimit(0.05f, 36.0f, loadParam(cached.q, 1.0f));
        const int type = static_cast<int>(loadParam(cached.type, 2.0f));
        band.filterType = juce::jlimit(0, 8, type);
        band.enabled = loadParam(cached.enabled, 1.0f) > 0.5f;

        // Dynamic EQ (clamped for safety)
        const int hostDynamicMode = juce::jlimit(
            0, 3, static_cast<int>(std::round(loadParam(cached.dynMode, 0.0f))));
        band.dynamicMode = hostDynamicMode == DynamicEQProcessor::DynamicMode_Gate
            ? DynamicEQProcessor::DynamicMode_Expand
            : hostDynamicMode;
        band.triggerSide = hostDynamicMode == DynamicEQProcessor::DynamicMode_Gate
            ? DynamicEQProcessor::TriggerSide_Below
            : juce::jlimit(
                DynamicEQProcessor::TriggerSide_Above,
                DynamicEQProcessor::TriggerSide_Below,
                static_cast<int>(std::round(loadParam(
                    cached.dynTrigger,
                    static_cast<float>(DynamicEQProcessor::TriggerSide_Above)))));
        band.threshold = juce::jlimit(-120.0f, 24.0f, loadParam(cached.dynThreshold, -20.0f));
        band.ratio = juce::jlimit(0.1f, 20.0f, loadParam(cached.dynRatio, 2.0f));
        band.attackMs = juce::jlimit(0.05f, 500.0f, loadParam(cached.dynAttack, 10.0f));
        band.releaseMs = juce::jlimit(1.0f, 4000.0f, loadParam(cached.dynRelease, 100.0f));
        band.range = juce::jlimit(0.0f, 48.0f, loadParam(cached.dynRange, 24.0f));
        band.knee = juce::jlimit(0.0f, 24.0f, loadParam(cached.dynKnee, 6.0f));
    }

    // Increment version for change detection
    params.version++;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    try
    {
        auto proc = std::make_unique<AIEqualizerAudioProcessor>();
        return proc.release();
    }
    catch (const std::exception& e)
    {
        AIEQ_LOG_ERROR("Failed to create plugin: " + juce::String(e.what()));
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("Failed to create plugin: unknown error");
    }

    return nullptr;
}
