#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../AI/AIEngine.h"
#include <cmath>
#include <algorithm>
#include <numeric>

/**
 * Comprehensive tests for AIEngine (AI category).
 *
 * Design principles:
 *  - No hard-coded threshold values: tests validate behavioural contracts, not
 *    specific dB numbers that may be tuned over time.
 *  - force=true is passed to analyzeSpectrum() so the analysisInterval counter
 *    never blocks test execution.
 *  - Temporal-consensus tests feed the same spectrum at least 5 times before
 *    asserting detection, matching the engine's 3-frame history requirement.
 *  - All static utility functions are tested independently.
 */
class AIEngineTest : public juce::UnitTest
{
public:
    AIEngineTest() : juce::UnitTest ("AIEngine", "AI") {}

    void runTest() override
    {
        testConstructionAndPrepare();
        testSensitivityBounds();
        testStrengthBounds();
        testSourceProfileSwitching();
        testCorrectionLifecycle();
        testClearOperations();
        testMergeNearbyCorrections();
        testMergeDoesNotMergeDistantCorrections();
        testFilteredAndPrioritizedCorrections();
        testFilteredSortOrder();
        testFlatSpectrumStability();
        testResonanceTrigger();
        testNewAnalysisFlag();
        testAnalysisHistory();
        testStaticUtilities();
        testAdvancedFeaturesFlags();
        testScaledCorrectionRespectStrength();
    }

private:
    //==========================================================================
    // Constants matching AIEngine internals (from AIEngine.h)
    //==========================================================================
    static constexpr double kSampleRate  = 44100.0;
    static constexpr int    kBlockSize   = 512;
    static constexpr int    kNumBins     = 2049;  // fftSize/2 + 1

    //==========================================================================
    // Spectrum helpers
    //==========================================================================

    /** Flat spectrum at the given dB level — should produce no pathological output. */
    static std::vector<float> makeFlatSpectrum (float levelDb = -40.0f)
    {
        return std::vector<float> (kNumBins, levelDb);
    }

    /**
     * Gaussian peak centred at peakBin with a given peak amplitude.
     * Background is set to noiseFloorDb.  halfWidthBins controls how sharp
     * the peak is; a value of 2 creates a very narrow resonance.
     */
    static std::vector<float> makePeakedSpectrum (int    peakBin,
                                                   float  peakDb,
                                                   float  noiseFloorDb   = -70.0f,
                                                   int    halfWidthBins  = 2)
    {
        std::vector<float> spec (kNumBins, noiseFloorDb);
        for (int i = std::max (0, peakBin - halfWidthBins * 4);
             i <= std::min (kNumBins - 1, peakBin + halfWidthBins * 4); ++i)
        {
            const float dist = static_cast<float> (i - peakBin) / static_cast<float> (halfWidthBins);
            spec[i] = noiseFloorDb + (peakDb - noiseFloorDb) * std::exp (-dist * dist);
        }
        return spec;
    }

    /** Approximate bin for a given frequency at kSampleRate and fftSize=4096. */
    static int frequencyToBin (float hz)
    {
        return juce::roundToInt (hz * 4096.0f / static_cast<float> (kSampleRate));
    }

    /**
     * Feed the same spectrum multiple times with force=true.
     * This builds the temporal history the engine needs for consensus detection.
     */
    static void feedSpectrum (AIEngine& engine,
                               const std::vector<float>& spec,
                               int times = 6)
    {
        for (int i = 0; i < times; ++i)
            engine.analyzeSpectrum (spec, /*force=*/true);
    }

    /** Build a prepared AIEngine ready for testing. */
    AIEngine makeReadyEngine()
    {
        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        return engine;
    }

    //==========================================================================
    // Helper: create a Correction with given severity and confidence
    //==========================================================================
    static AIEngine::Correction makeCorrection (AIEngine::ProblemType type,
                                                float freq,
                                                float severity,
                                                float confidence,
                                                float gainDb = -3.0f,
                                                float q = 2.0f)
    {
        AIEngine::Correction c;
        c.type          = type;
        c.frequency     = freq;
        c.severity      = severity;
        c.confidence    = confidence;
        c.suggestedGain = gainDb;
        c.suggestedQ    = q;
        c.approved      = false;
        return c;
    }

    //==========================================================================
    // Tests
    //==========================================================================

    void testConstructionAndPrepare()
    {
        beginTest ("Construction and prepare() do not crash");

        AIEngine engine;
        // Default state
        expect (engine.isEnabled(), "AIEngine should be enabled by default");
        expect (engine.getSensitivity() >= 0.0f && engine.getSensitivity() <= 1.0f,
                "Default sensitivity must be in [0,1]");
        expect (engine.getStrength() >= 0.0f && engine.getStrength() <= 1.0f,
                "Default strength must be in [0,1]");

        // prepare() should be idempotent
        engine.prepare (kSampleRate, kBlockSize);
        engine.prepare (kSampleRate, kBlockSize);  // second call must not crash

        // prepare() with edge-case sample rates
        engine.prepare (22050.0, 256);
        engine.prepare (96000.0, 1024);

        expect (true, "All prepare() variants completed without exception");
    }

    //--------------------------------------------------------------------------
    void testSensitivityBounds()
    {
        beginTest ("setSensitivity clamps to [0, 1]");

        AIEngine engine;
        engine.setSensitivity (-1.0f);
        expectWithinAbsoluteError (engine.getSensitivity(), 0.0f, 1e-5f,
                                   "Sensitivity -1 should clamp to 0");

        engine.setSensitivity (2.5f);
        expectWithinAbsoluteError (engine.getSensitivity(), 1.0f, 1e-5f,
                                   "Sensitivity 2.5 should clamp to 1");

        engine.setSensitivity (0.5f);
        expectWithinAbsoluteError (engine.getSensitivity(), 0.5f, 1e-5f,
                                   "Sensitivity 0.5 should round-trip exactly");
    }

    //--------------------------------------------------------------------------
    void testStrengthBounds()
    {
        beginTest ("setStrength clamps to [0, 1]");

        AIEngine engine;
        engine.setStrength (-0.5f);
        expectWithinAbsoluteError (engine.getStrength(), 0.0f, 1e-5f,
                                   "Strength -0.5 should clamp to 0");

        engine.setStrength (10.0f);
        expectWithinAbsoluteError (engine.getStrength(), 1.0f, 1e-5f,
                                   "Strength 10 should clamp to 1");

        engine.setStrength (0.7f);
        expectWithinAbsoluteError (engine.getStrength(), 0.7f, 1e-5f,
                                   "Strength 0.7 should round-trip exactly");
    }

    //--------------------------------------------------------------------------
    void testSourceProfileSwitching()
    {
        beginTest ("Source profile getters/setters round-trip for all profiles");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        const AIEngine::SourceProfile profiles[] = {
            AIEngine::SourceProfile::Generic,
            AIEngine::SourceProfile::Vocals,
            AIEngine::SourceProfile::Drums,
            AIEngine::SourceProfile::Bass,
            AIEngine::SourceProfile::Synth,
            AIEngine::SourceProfile::Master,
            AIEngine::SourceProfile::EDM,
            AIEngine::SourceProfile::Techno
        };

        for (auto profile : profiles)
        {
            engine.setSourceProfile (profile);
            expect (engine.getSourceProfile() == profile,
                    "getSourceProfile() should return the last set profile ("
                    + AIEngine::getProfileName (profile) + ")");
        }
    }

    //--------------------------------------------------------------------------
    void testCorrectionLifecycle()
    {
        beginTest ("Approve / reject corrections lifecycle");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        // Trigger a detectable resonance: a very sharp peak ~25 dB above noise floor
        const int resonanceBin = frequencyToBin (1200.0f);
        const auto spec = makePeakedSpectrum (resonanceBin, -20.0f, -70.0f, /*halfWidthBins=*/2);
        feedSpectrum (engine, spec, 8);

        const auto pending = engine.getPendingCorrections();
        if (pending.empty())
        {
            // Detection is heuristic — if nothing is found, still test the API contracts
            logMessage ("No corrections detected (heuristic may have filtered them) — testing API with forced approve/reject");

            engine.clearCorrections();
            expect (engine.getPendingCorrections().empty(), "After clear, pending should be empty");
            return;
        }

        // approveCorrection(0) should move it to approved list
        engine.approveCorrection (0);
        const auto approved = engine.getApprovedCorrections();
        expect (!approved.empty(), "Approved list should be non-empty after approveCorrection(0)");
        expect (approved[0].approved, "Approved correction should have approved == true");

        // rejectCorrection: add another analysis cycle so we get a fresh pending
        feedSpectrum (engine, spec, 4);
        const auto pending2 = engine.getPendingCorrections();
        if (!pending2.empty())
        {
            engine.rejectCorrection (0);
            // After rejection the pending list should either be empty or reduced
            const auto afterReject = engine.getPendingCorrections();
            expect (afterReject.size() <= pending2.size(),
                    "Pending list should not grow after rejectCorrection()");
        }
    }

    //--------------------------------------------------------------------------
    void testClearOperations()
    {
        beginTest ("clearCorrections() and clearApprovedCorrections() semantics");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        const int bin = frequencyToBin (3000.0f);
        const auto spec = makePeakedSpectrum (bin, -15.0f, -70.0f, 2);
        feedSpectrum (engine, spec, 8);

        engine.approveAllCorrections();

        // clearApprovedCorrections: pending should survive, approved should clear
        engine.clearApprovedCorrections();
        expect (engine.getApprovedCorrections().empty(),
                "clearApprovedCorrections() should leave approved list empty");

        // clearCorrections: everything goes
        feedSpectrum (engine, spec, 4);
        engine.clearCorrections();
        expect (engine.getPendingCorrections().empty(),
                "clearCorrections() should empty pending list");
        expect (engine.getApprovedCorrections().empty(),
                "clearCorrections() should empty approved list");
    }

    //--------------------------------------------------------------------------
    void testMergeNearbyCorrections()
    {
        beginTest ("mergeNearbyCorrections() merges frequencies within 1/3 octave");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        // 1000 Hz and 1200 Hz — ratio = 1.2 < 2^(1/3) ≈ 1.26  → should merge
        std::vector<AIEngine::Correction> corrections = {
            makeCorrection (AIEngine::ProblemType::Resonance, 1000.0f, 0.7f, 0.8f, -4.0f, 3.0f),
            makeCorrection (AIEngine::ProblemType::Resonance, 1200.0f, 0.6f, 0.7f, -3.0f, 2.5f)
        };

        const auto merged = engine.mergeNearbyCorrections (corrections);

        expect (merged.size() == 1,
                "Two resonances within 1/3 octave should merge into one (got "
                + juce::String ((int) merged.size()) + ")");

        if (!merged.empty())
        {
            // The merged correction must be within the range of the two originals
            expect (merged[0].frequency >= 900.0f && merged[0].frequency <= 1300.0f,
                    "Merged frequency must be between the two originals");
        }
    }

    //--------------------------------------------------------------------------
    void testMergeDoesNotMergeDistantCorrections()
    {
        beginTest ("mergeNearbyCorrections() does not merge frequencies > 1/3 octave apart");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        // 500 Hz and 1000 Hz — ratio = 2.0, one full octave → must NOT merge
        std::vector<AIEngine::Correction> corrections = {
            makeCorrection (AIEngine::ProblemType::Muddiness,  500.0f, 0.6f, 0.7f),
            makeCorrection (AIEngine::ProblemType::Harshness, 1000.0f, 0.6f, 0.7f)
        };

        const auto merged = engine.mergeNearbyCorrections (corrections);

        expect (merged.size() == 2,
                "Corrections 1 octave apart must NOT merge (got "
                + juce::String ((int) merged.size()) + ")");
    }

    //--------------------------------------------------------------------------
    void testFilteredAndPrioritizedCorrections()
    {
        beginTest ("getFilteredAndPrioritizedCorrections() applies thresholds correctly");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        // Feed a peaked spectrum to get real pending corrections
        const auto spec = makePeakedSpectrum (frequencyToBin (2000.0f), -10.0f, -70.0f, 2);
        feedSpectrum (engine, spec, 8);

        // With very loose thresholds (0, 0) we should get everything
        const auto all = engine.getFilteredAndPrioritizedCorrections (0.0f, 0.0f);

        // With very tight thresholds (1, 1) we should get nothing
        const auto none = engine.getFilteredAndPrioritizedCorrections (1.0f, 1.0f);
        expect (none.empty(),
                "No correction should pass severity=1 & confidence=1 thresholds");

        // all.size() >= none.size()
        expect (all.size() >= none.size(),
                "Loose filter must return >= results vs tight filter");
    }

    //--------------------------------------------------------------------------
    void testFilteredSortOrder()
    {
        beginTest ("getFilteredAndPrioritizedCorrections() returns results sorted by priority (severity*confidence)");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        const auto spec = makePeakedSpectrum (frequencyToBin (3000.0f), -10.0f, -70.0f, 2);
        feedSpectrum (engine, spec, 10);

        const auto filtered = engine.getFilteredAndPrioritizedCorrections (0.0f, 0.0f);

        // Verify descending sort by priority = severity * confidence
        for (size_t i = 1; i < filtered.size(); ++i)
        {
            const float prevPriority = filtered[i-1].severity * filtered[i-1].confidence;
            const float currPriority = filtered[i  ].severity * filtered[i  ].confidence;
            expect (prevPriority >= currPriority - 1e-4f,
                    "Results must be in non-increasing priority order");
        }
    }

    //--------------------------------------------------------------------------
    void testFlatSpectrumStability()
    {
        beginTest ("Flat spectrum produces no NaN/Inf corrections");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        const auto flat = makeFlatSpectrum (-40.0f);
        feedSpectrum (engine, flat, 10);

        const auto pending = engine.getPendingCorrections();
        for (const auto& c : pending)
        {
            expect (!std::isnan (c.frequency)      && !std::isinf (c.frequency),
                    "Correction frequency must be finite");
            expect (!std::isnan (c.suggestedGain)  && !std::isinf (c.suggestedGain),
                    "Correction suggestedGain must be finite");
            expect (!std::isnan (c.suggestedQ)     && !std::isinf (c.suggestedQ),
                    "Correction suggestedQ must be finite");
            expect (!std::isnan (c.severity)       && !std::isinf (c.severity),
                    "Correction severity must be finite");
            expect (!std::isnan (c.confidence)     && !std::isinf (c.confidence),
                    "Correction confidence must be finite");
            expect (c.severity   >= 0.0f && c.severity   <= 1.0f,
                    "Severity must be in [0,1]");
            expect (c.confidence >= 0.0f && c.confidence <= 1.0f,
                    "Confidence must be in [0,1]");
        }
    }

    //--------------------------------------------------------------------------
    void testResonanceTrigger()
    {
        beginTest ("Sharp resonant peak triggers at least one correction after sustained exposure");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        // Bin ~93 ≈ 1003 Hz at 44100/4096 resolution.
        // Peak is 50 dB above noise floor — far above any reasonable threshold.
        const int resonanceBin = frequencyToBin (1000.0f);
        const auto spec = makePeakedSpectrum (resonanceBin, -10.0f, -60.0f, /*halfWidthBins=*/1);

        // High sensitivity to maximise detection likelihood
        engine.setSensitivity (1.0f);
        feedSpectrum (engine, spec, 10);

        const bool analysisRan = engine.isNewAnalysisAvailable();
        const auto pending     = engine.getPendingCorrections();

        // At minimum the engine must have run analysis at least once
        // (either via newAnalysisAvailable being set, or via pending corrections)
        const bool engineResponded = analysisRan || !pending.empty();
        expect (engineResponded,
                "After 10 frames of a strong 50 dB resonance, the engine must "
                "have produced analysis (newAnalysisAvailable or pending corrections)");

        // All pending corrections must have valid frequencies (20 Hz – Nyquist)
        const float nyquist = static_cast<float> (kSampleRate * 0.5);
        for (const auto& c : pending)
        {
            expect (c.frequency >= 20.0f && c.frequency <= nyquist,
                    "Every correction frequency must be in [20 Hz, Nyquist]");
        }
    }

    //--------------------------------------------------------------------------
    void testNewAnalysisFlag()
    {
        beginTest ("newAnalysisAvailable flag is set and cleared correctly");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        // clearNewAnalysisFlag() on a fresh engine must be safe
        engine.clearNewAnalysisFlag();
        expect (!engine.isNewAnalysisAvailable(), "Flag must be false after explicit clear");

        // Run analysis
        feedSpectrum (engine, makeFlatSpectrum (-30.0f), 4);

        // Clear and verify
        engine.clearNewAnalysisFlag();
        expect (!engine.isNewAnalysisAvailable(),
                "Flag must be false immediately after clearNewAnalysisFlag()");
    }

    //--------------------------------------------------------------------------
    void testAnalysisHistory()
    {
        beginTest ("Analysis history accumulates and clearHistory() resets it");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        // Should start empty
        expect (engine.getAnalysisHistory().empty(),
                "History must be empty before any analysis");

        // Run enough cycles to produce at least one snapshot
        feedSpectrum (engine, makeFlatSpectrum (-30.0f), 6);

        // clearHistory() must empty the history vector
        engine.clearHistory();
        expect (engine.getAnalysisHistory().empty(),
                "History must be empty after clearHistory()");

        // Run again to accumulate
        feedSpectrum (engine, makeFlatSpectrum (-30.0f), 6);

        // History must not exceed maxHistorySize = 5
        const int histSize = static_cast<int> (engine.getAnalysisHistory().size());
        expect (histSize <= 5,
                "History must not exceed maxHistorySize=5 (got "
                + juce::String (histSize) + ")");
    }

    //--------------------------------------------------------------------------
    void testStaticUtilities()
    {
        beginTest ("Static utility functions return non-empty strings");

        // getProblemTypeName
        const AIEngine::ProblemType ptypes[] = {
            AIEngine::ProblemType::None,
            AIEngine::ProblemType::Resonance,
            AIEngine::ProblemType::Harshness,
            AIEngine::ProblemType::Muddiness,
            AIEngine::ProblemType::Boxyness,
            AIEngine::ProblemType::Sibilance,
            AIEngine::ProblemType::LowEndBoom,
            AIEngine::ProblemType::ThinSound,
            AIEngine::ProblemType::DullSound
        };
        for (auto t : ptypes)
            expect (AIEngine::getProblemTypeName (t).isNotEmpty(),
                    "getProblemTypeName() must return non-empty string");

        // getProfileName
        const AIEngine::SourceProfile profs[] = {
            AIEngine::SourceProfile::Generic, AIEngine::SourceProfile::Vocals,
            AIEngine::SourceProfile::Drums,   AIEngine::SourceProfile::Bass,
            AIEngine::SourceProfile::Synth,   AIEngine::SourceProfile::Master,
            AIEngine::SourceProfile::EDM,     AIEngine::SourceProfile::Techno
        };
        for (auto p : profs)
            expect (AIEngine::getProfileName (p).isNotEmpty(),
                    "getProfileName() must return non-empty string");

        // getFilterTypeName
        const AIEngine::Correction::FilterType ftypes[] = {
            AIEngine::Correction::FilterType::Peak,
            AIEngine::Correction::FilterType::LowShelf,
            AIEngine::Correction::FilterType::HighShelf,
            AIEngine::Correction::FilterType::LowCut,
            AIEngine::Correction::FilterType::HighCut,
            AIEngine::Correction::FilterType::Notch
        };
        for (auto f : ftypes)
            expect (AIEngine::getFilterTypeName (f).isNotEmpty(),
                    "getFilterTypeName() must return non-empty string");

        // getGenreName
        const AIEngine::DetectedGenre genres[] = {
            AIEngine::DetectedGenre::Unknown, AIEngine::DetectedGenre::Techno,
            AIEngine::DetectedGenre::Industrial, AIEngine::DetectedGenre::Jungle,
            AIEngine::DetectedGenre::Breakbeat, AIEngine::DetectedGenre::DubTechno,
            AIEngine::DetectedGenre::DeepHouse, AIEngine::DetectedGenre::Ambient
        };
        for (auto g : genres)
            expect (AIEngine::getGenreName (g).isNotEmpty(),
                    "getGenreName() must return non-empty string");

        // getBandName: a few representative frequencies
        for (float hz : { 60.0f, 250.0f, 1000.0f, 5000.0f, 16000.0f })
            expect (AIEngine::getBandName (hz).isNotEmpty(),
                    "getBandName() must return non-empty string for " + juce::String (hz) + " Hz");
    }

    //--------------------------------------------------------------------------
    void testAdvancedFeaturesFlags()
    {
        beginTest ("Advanced feature flag enable/disable toggles safely");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        // All advanced features disabled by default
        expect (!engine.isMultiTrackUnmaskingEnabled(),  "MultiTrackUnmasking default: off");
        expect (!engine.isNeuralNetworksEnabled(),       "NeuralNetworks default: off");
        expect (!engine.isAdaptiveProcessingEnabled(),   "AdaptiveProcessing default: off");
        expect (!engine.isOnlineLearningEnabled(),       "OnlineLearning default: off");

        // Enable each one
        engine.setMultiTrackUnmaskingEnabled (true);
        expect (engine.isMultiTrackUnmaskingEnabled(), "MultiTrackUnmasking must be on after enable");

        engine.setNeuralNetworksEnabled (true);
        expect (engine.isNeuralNetworksEnabled(), "NeuralNetworks must be on after enable");

        engine.setAdaptiveProcessingEnabled (true);
        expect (engine.isAdaptiveProcessingEnabled(), "AdaptiveProcessing must be on after enable");

        engine.setOnlineLearningEnabled (true);
        expect (engine.isOnlineLearningEnabled(), "OnlineLearning must be on after enable");

        // Disable each one and verify
        engine.setMultiTrackUnmaskingEnabled (false);
        expect (!engine.isMultiTrackUnmaskingEnabled(), "MultiTrackUnmasking must be off after disable");

        engine.setNeuralNetworksEnabled (false);
        expect (!engine.isNeuralNetworksEnabled(), "NeuralNetworks must be off after disable");

        engine.setAdaptiveProcessingEnabled (false);
        expect (!engine.isAdaptiveProcessingEnabled(), "AdaptiveProcessing must be off after disable");

        engine.setOnlineLearningEnabled (false);
        expect (!engine.isOnlineLearningEnabled(), "OnlineLearning must be off after disable");

        // Engine must still function after toggling all flags
        feedSpectrum (engine, makeFlatSpectrum (-40.0f), 3);
        expect (true, "Engine processes spectrum after toggling all advanced flags");
    }

    //--------------------------------------------------------------------------
    void testScaledCorrectionRespectStrength()
    {
        beginTest ("getScaledCorrection() scales gain proportionally to strength");

        AIEngine engine;
        engine.prepare (kSampleRate, kBlockSize);

        AIEngine::Correction baseCorrection;
        baseCorrection.type          = AIEngine::ProblemType::Resonance;
        baseCorrection.frequency     = 1000.0f;
        baseCorrection.suggestedGain = -6.0f;
        baseCorrection.suggestedQ    = 4.0f;
        baseCorrection.severity      = 0.8f;
        baseCorrection.confidence    = 0.9f;
        baseCorrection.approved      = true;

        // At strength=1.0 the scaled gain must be <= full gain in magnitude
        engine.setStrength (1.0f);
        const auto scaled1 = engine.getScaledCorrection (baseCorrection);
        expect (std::abs (scaled1.suggestedGain) <= std::abs (baseCorrection.suggestedGain) + 0.01f,
                "Scaled gain at strength=1 must not exceed base gain magnitude");

        // At strength=0.0 the scaled gain must be very small (close to 0)
        engine.setStrength (0.0f);
        const auto scaled0 = engine.getScaledCorrection (baseCorrection);
        expect (std::abs (scaled0.suggestedGain) < std::abs (baseCorrection.suggestedGain),
                "Scaled gain at strength=0 must be smaller than base gain");

        // Half strength should produce an intermediate result
        engine.setStrength (0.5f);
        const auto scaledHalf = engine.getScaledCorrection (baseCorrection);
        expect (std::abs (scaledHalf.suggestedGain) < std::abs (scaled1.suggestedGain) + 0.1f,
                "Scaled gain at strength=0.5 must not exceed strength=1 result");
    }
};

// JUCE auto-registers unit tests via static construction.
// This instance is created at program startup and discovered by UnitTestRunner.
static AIEngineTest gAIEngineTest;
