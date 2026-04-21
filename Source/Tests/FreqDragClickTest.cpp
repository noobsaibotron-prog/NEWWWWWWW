/**
 * FreqDragClickTest.cpp
 *
 * Tests for micro-clicks/pops during frequency dragging with:
 *   - Narrow Q (8.0-12.0)
 *   - High gain (+12 to +18 dB)
 *   - Sweep from low (40 Hz) to high (16 kHz) frequencies
 *   - Zero Latency and Natural Phase modes
 *
 * This reproduces the exact scenario reported: dragging a band with
 * narrow Q and high gain from lows to highs causes micro-pops,
 * especially in the low frequency range.
 *
 * The test sweeps frequency across multiple blocks, analyzing each
 * segment for sample-to-sample discontinuities.
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <random>
#include <functional>

#include "../PluginProcessor.h"

namespace
{

// ─────────────────────────────────────────────────────────────────────────────
// Click detection (same infrastructure as HostSessionClickTest)
// ─────────────────────────────────────────────────────────────────────────────
struct ClickMetrics
{
    float maxDelta         = 0.0f;
    float peakAbs          = 0.0f;
    int   maxDropoutRun    = 0;
    bool  hasNaN           = false;
    bool  hasInf           = false;
    int   clickCount       = 0;
    float avgDelta         = 0.0f;
    int   worstClickSample = -1;   // sample index of the worst delta
    float worstClickFreq   = 0.0f; // freq at the time of the worst delta
};

// Thresholds — tuned for parameter drag scenarios
constexpr float kClickThreshold    = 0.25f;   // tighter than session test (0.35)
constexpr float kMaxDeltaPass      = 0.40f;   // fail threshold
constexpr float kPeakAbsMax        = 6.0f;    // high gain = high peaks expected
constexpr int   kMaxDropoutSamples = 4;
constexpr int   kMaxClicksAllowed  = 0;

static void fillBroadband(juce::AudioBuffer<float>& buf, double sampleRate,
                           int sampleOffset, std::mt19937& rng)
{
    const double freqs[] = { 60.0, 150.0, 440.0, 1000.0, 3000.0, 8000.0 };
    const float  amps[]  = { 0.10f, 0.08f, 0.06f, 0.05f,  0.04f,  0.03f  };
    constexpr int numTones = 6;

    std::uniform_real_distribution<float> noiseDist(-0.04f, 0.04f);

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* data = buf.getWritePointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            float sample = noiseDist(rng);
            for (int t = 0; t < numTones; ++t)
            {
                const double w = juce::MathConstants<double>::twoPi * freqs[t] / sampleRate;
                sample += amps[t] * static_cast<float>(std::sin(w * static_cast<double>(sampleOffset + i)));
            }
            data[i] = sample;
        }
    }
}

static ClickMetrics analyzeForClicks(const juce::AudioBuffer<float>& output,
                                      int regionStart, int regionLength,
                                      float clickThreshold = kClickThreshold)
{
    ClickMetrics m;
    if (output.getNumChannels() == 0 || regionLength <= 0) return m;

    const int end = juce::jmin(output.getNumSamples(), regionStart + regionLength);
    long totalSamples = 0;
    double sumDelta = 0.0;

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        const auto* out = output.getReadPointer(ch);
        float prev = out[juce::jmax(0, regionStart)];

        for (int i = regionStart + 1; i < end; ++i)
        {
            const float s = out[i];

            if (std::isnan(s)) m.hasNaN = true;
            if (std::isinf(s)) m.hasInf = true;

            m.peakAbs = std::max(m.peakAbs, std::abs(s));

            const float delta = std::abs(s - prev);
            if (delta > m.maxDelta)
            {
                m.maxDelta = delta;
                m.worstClickSample = i;
            }
            sumDelta += delta;
            ++totalSamples;

            if (delta > clickThreshold)
                ++m.clickCount;

            prev = s;
        }
    }

    if (totalSamples > 0)
        m.avgDelta = static_cast<float>(sumDelta / totalSamples);

    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parameter helpers
// ─────────────────────────────────────────────────────────────────────────────
static void setParam(juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

static void setChoice(juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& id, int index)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
}

// ─────────────────────────────────────────────────────────────────────────────
// Frequency sweep runner
//
// Sweeps band frequency from startHz to endHz over numBlocks.
// Returns per-segment click metrics so we can identify which frequency
// range is problematic.
// ─────────────────────────────────────────────────────────────────────────────
struct SweepResult
{
    juce::AudioBuffer<float> output;
    std::vector<ClickMetrics> segmentMetrics;  // one per block
    std::vector<float>        segmentFreqs;    // freq at each block
    ClickMetrics              overall;
    int totalClicks = 0;
    float worstFreq = 0.0f;
    float worstDelta = 0.0f;
};

static SweepResult runFreqSweep(double sampleRate, int blockSize, int numBlocks,
                                 int bandIndex, float gain, float q,
                                 float startHz, float endHz,
                                 int phaseMode,     // 0=ZL, 1=Natural, 2=Linear
                                 unsigned int seed = 42)
{
    AIEqualizerAudioProcessor proc;
    proc.prepareToPlay(sampleRate, blockSize);
    auto& apvts = proc.getAPVTS();

    // Setup: single active band, peak filter
    proc.setNumActiveBands(1);
    setChoice(apvts, "numActiveBands", 0);  // 0-indexed → 1 band

    juce::String prefix = "band1";
    setParam(apvts, prefix + "Freq", startHz);
    setParam(apvts, prefix + "Gain", gain);
    setParam(apvts, prefix + "Q", q);
    setChoice(apvts, prefix + "Type", 2);   // Peak filter
    setChoice(apvts, "qualityMode", 0);      // Zero Latency
    setChoice(apvts, "phaseMode", phaseMode);

    // Pre-roll: 10 blocks to let the processor settle
    constexpr int preRollBlocks = 10;
    juce::MidiBuffer midi;
    std::mt19937 rng(seed);

    for (int b = 0; b < preRollBlocks; ++b)
    {
        juce::AudioBuffer<float> chunk(2, blockSize);
        fillBroadband(chunk, sampleRate, b * blockSize, rng);
        proc.processBlock(chunk, midi);
    }

    // Sweep
    SweepResult result;
    result.output = juce::AudioBuffer<float>(2, blockSize * numBlocks);

    // Logarithmic frequency sweep (perceptually uniform)
    const float logStart = std::log2(startHz);
    const float logEnd   = std::log2(endHz);

    for (int block = 0; block < numBlocks; ++block)
    {
        // Interpolate frequency (log scale)
        float t = static_cast<float>(block) / static_cast<float>(numBlocks - 1);
        float freq = std::pow(2.0f, logStart + t * (logEnd - logStart));
        result.segmentFreqs.push_back(freq);

        // Set frequency (simulates mouse drag)
        setParam(apvts, prefix + "Freq", freq);

        // Generate and process audio
        juce::AudioBuffer<float> chunk(2, blockSize);
        int sampleOffset = (preRollBlocks + block) * blockSize;
        fillBroadband(chunk, sampleRate, sampleOffset, rng);
        proc.processBlock(chunk, midi);

        // Save output
        for (int ch = 0; ch < 2; ++ch)
            result.output.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);

        // Analyze this block for clicks
        auto seg = analyzeForClicks(result.output, block * blockSize, blockSize);
        seg.worstClickFreq = freq;
        result.segmentMetrics.push_back(seg);

        if (seg.maxDelta > result.worstDelta)
        {
            result.worstDelta = seg.maxDelta;
            result.worstFreq = freq;
        }
        result.totalClicks += seg.clickCount;
    }

    // Overall analysis
    result.overall = analyzeForClicks(result.output, 0, result.output.getNumSamples());

    proc.releaseResources();
    return result;
}

} // namespace

// =============================================================================
// Test class
// =============================================================================
class FreqDragClickTest : public juce::UnitTest
{
public:
    FreqDragClickTest() : juce::UnitTest("Freq Drag Click Detection", "ClickTests") {}

    void runTest() override
    {
        // =====================================================================
        // Test 1: Narrow Q + High Gain, ZL mode, low-to-high sweep
        //         This is the exact scenario reported.
        // =====================================================================
        beginTest("Narrow Q (+15dB, Q=10) freq drag 40Hz→16kHz — Zero Latency");
        {
            auto r = runFreqSweep(
                48000.0,      // sampleRate
                256,          // blockSize (user's setting)
                200,          // numBlocks (~1 second sweep)
                1,            // band index
                15.0f,        // gain dB (high)
                10.0f,        // Q (narrow)
                40.0f,        // start Hz
                16000.0f,     // end Hz
                0             // phaseMode = Zero Latency
            );

            logMessage("  Overall: maxDelta=" + juce::String(r.overall.maxDelta, 4)
                        + " clicks=" + juce::String(r.totalClicks)
                        + " peakAbs=" + juce::String(r.overall.peakAbs, 4)
                        + " worstFreq=" + juce::String(r.worstFreq, 1) + "Hz");

            // Report worst segments (top 5)
            std::vector<std::pair<float, int>> ranked;
            for (int i = 0; i < (int)r.segmentMetrics.size(); ++i)
                ranked.push_back({ r.segmentMetrics[i].maxDelta, i });
            std::sort(ranked.begin(), ranked.end(), [](auto& a, auto& b) { return a.first > b.first; });

            for (int i = 0; i < juce::jmin(5, (int)ranked.size()); ++i)
            {
                int idx = ranked[i].second;
                auto& seg = r.segmentMetrics[idx];
                logMessage("    #" + juce::String(i + 1)
                            + " block=" + juce::String(idx)
                            + " freq=" + juce::String(r.segmentFreqs[idx], 1) + "Hz"
                            + " maxDelta=" + juce::String(seg.maxDelta, 4)
                            + " clicks=" + juce::String(seg.clickCount));
            }

            expect(!r.overall.hasNaN, "NaN detected in output");
            expect(!r.overall.hasInf, "Inf detected in output");
            expect(r.overall.maxDelta < kMaxDeltaPass,
                   "Click detected during freq drag, maxDelta=" + juce::String(r.overall.maxDelta, 4)
                   + " at " + juce::String(r.worstFreq, 1) + "Hz");
            expect(r.totalClicks <= kMaxClicksAllowed,
                   juce::String(r.totalClicks) + " clicks during sweep");
        }

        // =====================================================================
        // Test 2: Same but Natural Phase
        // =====================================================================
        beginTest("Narrow Q (+15dB, Q=10) freq drag 40Hz→16kHz — Natural Phase");
        {
            auto r = runFreqSweep(48000.0, 256, 200, 1, 15.0f, 10.0f,
                                   40.0f, 16000.0f, 1);

            logMessage("  Overall: maxDelta=" + juce::String(r.overall.maxDelta, 4)
                        + " clicks=" + juce::String(r.totalClicks)
                        + " peakAbs=" + juce::String(r.overall.peakAbs, 4)
                        + " worstFreq=" + juce::String(r.worstFreq, 1) + "Hz");

            std::vector<std::pair<float, int>> ranked;
            for (int i = 0; i < (int)r.segmentMetrics.size(); ++i)
                ranked.push_back({ r.segmentMetrics[i].maxDelta, i });
            std::sort(ranked.begin(), ranked.end(), [](auto& a, auto& b) { return a.first > b.first; });
            for (int i = 0; i < juce::jmin(5, (int)ranked.size()); ++i)
            {
                int idx = ranked[i].second;
                logMessage("    #" + juce::String(i + 1)
                            + " block=" + juce::String(idx)
                            + " freq=" + juce::String(r.segmentFreqs[idx], 1) + "Hz"
                            + " maxDelta=" + juce::String(r.segmentMetrics[idx].maxDelta, 4)
                            + " clicks=" + juce::String(r.segmentMetrics[idx].clickCount));
            }

            expect(!r.overall.hasNaN, "NaN in output");
            expect(!r.overall.hasInf, "Inf in output");
            expect(r.overall.maxDelta < kMaxDeltaPass,
                   "Click in Natural Phase drag, maxDelta=" + juce::String(r.overall.maxDelta, 4)
                   + " at " + juce::String(r.worstFreq, 1) + "Hz");
            expect(r.totalClicks <= kMaxClicksAllowed,
                   juce::String(r.totalClicks) + " clicks during Natural Phase sweep");
        }

        // =====================================================================
        // Test 3: Extreme case — Q=12, +18dB, slow sweep through sub-bass
        //         (40-200 Hz only — where the user reported the worst pops)
        // =====================================================================
        beginTest("Extreme sub-bass drag (+18dB, Q=12) 40Hz→200Hz — Zero Latency");
        {
            auto r = runFreqSweep(48000.0, 256, 150, 1, 18.0f, 12.0f,
                                   40.0f, 200.0f, 0);

            logMessage("  Sub-bass: maxDelta=" + juce::String(r.overall.maxDelta, 4)
                        + " clicks=" + juce::String(r.totalClicks)
                        + " peakAbs=" + juce::String(r.overall.peakAbs, 4)
                        + " worstFreq=" + juce::String(r.worstFreq, 1) + "Hz");

            std::vector<std::pair<float, int>> ranked;
            for (int i = 0; i < (int)r.segmentMetrics.size(); ++i)
                ranked.push_back({ r.segmentMetrics[i].maxDelta, i });
            std::sort(ranked.begin(), ranked.end(), [](auto& a, auto& b) { return a.first > b.first; });
            for (int i = 0; i < juce::jmin(5, (int)ranked.size()); ++i)
            {
                int idx = ranked[i].second;
                logMessage("    #" + juce::String(i + 1)
                            + " freq=" + juce::String(r.segmentFreqs[idx], 1) + "Hz"
                            + " maxDelta=" + juce::String(r.segmentMetrics[idx].maxDelta, 4)
                            + " clicks=" + juce::String(r.segmentMetrics[idx].clickCount));
            }

            expect(!r.overall.hasNaN, "NaN in sub-bass sweep");
            expect(!r.overall.hasInf, "Inf in sub-bass sweep");
            expect(r.overall.maxDelta < kMaxDeltaPass,
                   "Click in sub-bass sweep, maxDelta=" + juce::String(r.overall.maxDelta, 4)
                   + " at " + juce::String(r.worstFreq, 1) + "Hz");
            expect(r.totalClicks <= kMaxClicksAllowed,
                   juce::String(r.totalClicks) + " clicks in sub-bass sweep");
        }

        // =====================================================================
        // Test 4: User's exact config — 48kHz, 512 buffer, both modes
        // =====================================================================
        beginTest("User config (48kHz/512) narrow Q drag — Zero Latency");
        {
            auto r = runFreqSweep(48000.0, 512, 100, 1, 15.0f, 10.0f,
                                   40.0f, 16000.0f, 0);

            logMessage("  512buf ZL: maxDelta=" + juce::String(r.overall.maxDelta, 4)
                        + " clicks=" + juce::String(r.totalClicks)
                        + " worstFreq=" + juce::String(r.worstFreq, 1) + "Hz");

            expect(r.overall.maxDelta < kMaxDeltaPass,
                   "Click at 512 buf ZL, maxDelta=" + juce::String(r.overall.maxDelta, 4));
            expect(r.totalClicks <= kMaxClicksAllowed,
                   juce::String(r.totalClicks) + " clicks at 512 buf ZL");
        }

        beginTest("User config (48kHz/512) narrow Q drag — Natural Phase");
        {
            auto r = runFreqSweep(48000.0, 512, 100, 1, 15.0f, 10.0f,
                                   40.0f, 16000.0f, 1);

            logMessage("  512buf NP: maxDelta=" + juce::String(r.overall.maxDelta, 4)
                        + " clicks=" + juce::String(r.totalClicks)
                        + " worstFreq=" + juce::String(r.worstFreq, 1) + "Hz");

            expect(r.overall.maxDelta < kMaxDeltaPass,
                   "Click at 512 buf NP, maxDelta=" + juce::String(r.overall.maxDelta, 4));
            expect(r.totalClicks <= kMaxClicksAllowed,
                   juce::String(r.totalClicks) + " clicks at 512 buf NP");
        }
    }
};

static FreqDragClickTest freqDragClickTestInstance;
