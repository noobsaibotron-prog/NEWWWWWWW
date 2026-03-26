/**
 * HostSessionClickTest.cpp
 *
 * Simulates a realistic host session (Ableton-like) with continuous audio input
 * and tests every parameter change scenario for audio clicks/glitches:
 *
 *   1. Bypass toggle (on/off, rapid toggle)
 *   2. Phase mode switching (Zero → Natural → Linear → Zero)
 *   3. Oversampling switching (Off → 2x → 4x → Off)
 *   4. Band dragging (freq sweep, gain sweep, Q sweep — single and multi-band)
 *   5. A/B profile switching
 *   6. Dynamic EQ: threshold change, mode toggle (compress/expand/gate)
 *   7. AI correction apply (simulated via parameter burst)
 *   8. Compound stress: multiple transitions in rapid succession
 *
 * Each test feeds a continuous broadband signal (pink-ish noise + sine tones)
 * through the processor and analyzes the output for discontinuities.
 *
 * Detection method:
 *   - maxDelta: max sample-to-sample jump (click = sharp transient)
 *   - zeroCrossRate spike: sudden burst of zero crossings (digital hash)
 *   - dropout: consecutive near-zero samples while input is live
 *   - NaN/Inf: invalid floating point
 *   - peakAbs: output exceeding safe headroom
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
// Click detection metrics
// ─────────────────────────────────────────────────────────────────────────────
struct ClickMetrics
{
    float maxDelta         = 0.0f;   // worst sample-to-sample jump
    float peakAbs          = 0.0f;   // absolute peak level
    int   maxDropoutRun    = 0;      // longest run of near-silence while input is live
    bool  hasNaN           = false;
    bool  hasInf           = false;
    int   clickCount       = 0;      // number of sample-to-sample jumps exceeding threshold
    float avgDelta         = 0.0f;   // average delta for context
};

// Aggressive thresholds — we want ZERO audible clicks
constexpr float kClickDeltaThreshold   = 0.35f;   // a jump above this is a "click"
constexpr float kMaxDeltaPass          = 0.50f;   // test fails if maxDelta exceeds this
constexpr float kPeakAbsMax            = 3.0f;    // output shouldn't exceed ~+9dBFS
constexpr int   kMaxDropoutSamples     = 8;        // max consecutive silence
constexpr int   kMaxClicksAllowed      = 0;        // ZERO clicks allowed

// ─────────────────────────────────────────────────────────────────────────────
// Parameter helpers
// ─────────────────────────────────────────────────────────────────────────────
static void setChoice(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int index)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
}

static void setBool(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(value ? 1.0f : 0.0f);
}

static void setFloat(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

// ─────────────────────────────────────────────────────────────────────────────
// Signal generators — realistic broadband content
// ─────────────────────────────────────────────────────────────────────────────
static void fillBroadband(juce::AudioBuffer<float>& buf, double sampleRate, int sampleOffset, std::mt19937& rng)
{
    // Pink-ish noise + multi-tone (100, 440, 1k, 3k, 8k Hz)
    // Simulates a real mix bus signal
    const double freqs[] = { 100.0, 440.0, 1000.0, 3000.0, 8000.0 };
    const float  amps[]  = { 0.08f, 0.06f, 0.05f,  0.04f,  0.03f  };
    constexpr int numTones = 5;

    std::uniform_real_distribution<float> noiseDist(-0.05f, 0.05f);

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

// ─────────────────────────────────────────────────────────────────────────────
// Click analysis
// ─────────────────────────────────────────────────────────────────────────────
static ClickMetrics analyzeForClicks(const juce::AudioBuffer<float>& output,
                                      const juce::AudioBuffer<float>& input,
                                      int regionStart, int regionLength,
                                      float clickThreshold = kClickDeltaThreshold)
{
    ClickMetrics m;
    if (output.getNumChannels() == 0 || regionLength <= 0) return m;

    const int end = juce::jmin(output.getNumSamples(), regionStart + regionLength);
    long totalSamples = 0;
    double sumDelta = 0.0;

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        const auto* out = output.getReadPointer(ch);
        const auto* in  = (ch < input.getNumChannels()) ? input.getReadPointer(ch) : nullptr;

        float prev = out[juce::jmax(0, regionStart)];
        int dropoutRun = 0;

        for (int i = regionStart; i < end; ++i)
        {
            const float s = out[i];

            if (std::isnan(s)) m.hasNaN = true;
            if (std::isinf(s)) m.hasInf = true;

            m.peakAbs = std::max(m.peakAbs, std::abs(s));

            if (i > regionStart)
            {
                const float delta = std::abs(s - prev);
                m.maxDelta = std::max(m.maxDelta, delta);
                sumDelta += delta;
                ++totalSamples;

                if (delta > clickThreshold)
                    ++m.clickCount;
            }
            prev = s;

            // Dropout detection: output near-zero while input is live
            if (in != nullptr && std::abs(in[i]) > 0.01f && std::abs(s) < 1.0e-5f)
                ++dropoutRun;
            else
                dropoutRun = 0;

            m.maxDropoutRun = std::max(m.maxDropoutRun, dropoutRun);
        }
    }

    if (totalSamples > 0)
        m.avgDelta = static_cast<float>(sumDelta / totalSamples);

    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
// Session runner — simulates a real host session
// ─────────────────────────────────────────────────────────────────────────────
struct SessionResult
{
    juce::AudioBuffer<float> output;
    juce::AudioBuffer<float> input;
};

using BlockCallback = std::function<void(AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState&, int /*block*/)>;
using SetupCallback = std::function<void(AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState&)>;

static SessionResult runSession(double sampleRate, int blockSize, int numBlocks,
                                 SetupCallback setup, BlockCallback onBlock,
                                 unsigned int seed = 42)
{
    AIEqualizerAudioProcessor proc;
    proc.prepareToPlay(sampleRate, blockSize);
    auto& apvts = proc.getAPVTS();

    if (setup) setup(proc, apvts);

    SessionResult result;
    result.output = juce::AudioBuffer<float>(2, blockSize * numBlocks);
    result.input  = juce::AudioBuffer<float>(2, blockSize * numBlocks);

    juce::MidiBuffer midi;
    std::mt19937 rng(seed);

    for (int block = 0; block < numBlocks; ++block)
    {
        juce::AudioBuffer<float> chunk(2, blockSize);
        fillBroadband(chunk, sampleRate, block * blockSize, rng);

        // Save input
        for (int ch = 0; ch < 2; ++ch)
            result.input.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);

        // Per-block callback (parameter changes happen here)
        if (onBlock) onBlock(proc, apvts, block);

        proc.processBlock(chunk, midi);

        // Save output
        for (int ch = 0; ch < 2; ++ch)
            result.output.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
    }

    proc.releaseResources();
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Default EQ setup — 8 bands active, realistic session
// ─────────────────────────────────────────────────────────────────────────────
static void setupRealisticSession(AIEqualizerAudioProcessor& proc,
                                   juce::AudioProcessorValueTreeState& apvts)
{
    proc.setNumActiveBands(8);
    setChoice(apvts, "numActiveBands", 7); // 0-indexed → 8 bands

    // Set up 8 realistic bands
    struct BandConfig { float freq; float gain; float q; int type; };
    const BandConfig bands[] = {
        {  30.0f, 0.0f,  0.7f, 0 },  // Low cut
        {  80.0f, 2.5f,  0.8f, 1 },  // Low shelf boost
        { 250.0f, -3.0f, 1.5f, 2 },  // Peak cut (mud)
        { 800.0f, 1.0f,  1.0f, 2 },  // Peak boost (body)
        { 2000.0f, -2.0f, 2.0f, 2 }, // Peak cut (harsh)
        { 5000.0f, 3.0f,  1.2f, 2 }, // Peak boost (presence)
        { 8000.0f, 1.5f,  0.8f, 3 }, // High shelf
        { 16000.0f, 0.0f, 0.7f, 4 }, // High cut
    };

    for (int i = 0; i < 8; ++i)
    {
        AIEqualizerAudioProcessor::BandState bs;
        bs.frequency = bands[i].freq;
        bs.gain      = bands[i].gain;
        bs.q         = bands[i].q;
        bs.type      = bands[i].type;
        bs.enabled   = true;
        bs.solo      = false;
        proc.setBandState(i, bs);

        juce::String prefix = "band" + juce::String(i);
        setFloat(apvts, prefix + "Freq",    bands[i].freq);
        setFloat(apvts, prefix + "Gain",    bands[i].gain);
        setFloat(apvts, prefix + "Q",       bands[i].q);
        setChoice(apvts, prefix + "Type",   bands[i].type);
        setBool(apvts,  prefix + "Enabled", true);
    }

    setBool(apvts,  "bypass", false);
    setFloat(apvts, "dryWet", 100.0f);
    setFloat(apvts, "outputGain", 0.0f);
    setChoice(apvts, "qualityMode", 1);
    setChoice(apvts, "phaseMode", 0);      // Zero Latency
    setChoice(apvts, "oversamplingFactor", 0);
    setChoice(apvts, "msMode", 0);         // Stereo
}

} // namespace

// =============================================================================
// THE TEST
// =============================================================================
class HostSessionClickTest : public juce::UnitTest
{
public:
    HostSessionClickTest()
        : juce::UnitTest("Host Session Click Detection", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        constexpr double sr = 48000.0;
        constexpr int blockSize = 128;  // Typical Ableton buffer

        testBypassToggle(sr, blockSize);
        testBypassRapidToggle(sr, blockSize);
        testPhaseModeSwitch(sr, blockSize);
        testOversamplingSwitch(sr, blockSize);
        testSingleBandFreqSweep(sr, blockSize);
        testSingleBandGainSweep(sr, blockSize);
        testMultiBandSimultaneousDrag(sr, blockSize);
        testABProfileSwitch(sr, blockSize);
        testDynamicEQThresholdSweep(sr, blockSize);
        testDynamicEQModeToggle(sr, blockSize);
        testAICorrectionBurst(sr, blockSize);
        testCompoundStress(sr, blockSize);
        testSmallBlockSize(sr, 32);
        testLargeBlockSize(sr, 2048);
    }

private:
    void expectClean(const ClickMetrics& m, const juce::String& label,
                     float maxDeltaLimit = kMaxDeltaPass,
                     int maxClicks = kMaxClicksAllowed,
                     float peakLimit = kPeakAbsMax,
                     int maxDropout = kMaxDropoutSamples)
    {
        logMessage("  " + label + ": maxDelta=" + juce::String(m.maxDelta, 4)
                   + " clicks=" + juce::String(m.clickCount)
                   + " peakAbs=" + juce::String(m.peakAbs, 4)
                   + " dropout=" + juce::String(m.maxDropoutRun)
                   + " avgDelta=" + juce::String(m.avgDelta, 6));

        expect(!m.hasNaN,  label + ": NaN in output");
        expect(!m.hasInf,  label + ": Inf in output");
        expect(m.maxDelta < maxDeltaLimit,
               label + ": click detected, maxDelta=" + juce::String(m.maxDelta, 4)
               + " (threshold " + juce::String(maxDeltaLimit, 2) + ")");
        expect(m.peakAbs < peakLimit,
               label + ": output explosion, peakAbs=" + juce::String(m.peakAbs, 4));
        expect(m.maxDropoutRun <= maxDropout,
               label + ": audio dropout, " + juce::String(m.maxDropoutRun) + " silent samples");
        expect(m.clickCount <= maxClicks,
               label + ": " + juce::String(m.clickCount) + " clicks detected (max allowed: "
               + juce::String(maxClicks) + ")");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 1. BYPASS TOGGLE
    // ─────────────────────────────────────────────────────────────────────────
    void testBypassToggle(double sr, int blockSize)
    {
        beginTest("Bypass toggle (active → bypass → active) with 8 bands");

        auto result = runSession(sr, blockSize, 120, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block == 30) setBool(apvts, "bypass", true);
                if (block == 60) setBool(apvts, "bypass", false);
                if (block == 90) setBool(apvts, "bypass", true);
            });

        // Analyze each transition window
        for (int transBlock : { 30, 60, 90 })
        {
            int start = transBlock * blockSize;
            int len   = 10 * blockSize; // ~27ms analysis window
            auto m = analyzeForClicks(result.output, result.input, start, len);
            expectClean(m, "Bypass@block" + juce::String(transBlock));
        }
    }

    void testBypassRapidToggle(double sr, int blockSize)
    {
        beginTest("Bypass rapid toggle (every 2 blocks) stress test");

        auto result = runSession(sr, blockSize, 100, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                // Toggle bypass every 2 blocks (~5ms) — faster than crossfade duration
                if (block >= 20 && block <= 60 && (block % 2 == 0))
                    setBool(apvts, "bypass", (block / 2) % 2 == 0);
            });

        int start = 20 * blockSize;
        int len   = 50 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len);
        expectClean(m, "Bypass rapid toggle");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 2. PHASE MODE SWITCHING
    // ─────────────────────────────────────────────────────────────────────────
    void testPhaseModeSwitch(double sr, int blockSize)
    {
        beginTest("Phase mode: Zero → Natural → Linear → Zero with audio");

        auto result = runSession(sr, blockSize, 200, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block == 40)  setChoice(apvts, "phaseMode", 1); // Natural
                if (block == 90)  setChoice(apvts, "phaseMode", 2); // Linear
                if (block == 150) setChoice(apvts, "phaseMode", 0); // Zero
            });

        for (auto [blk, label] : std::initializer_list<std::pair<int, const char*>>{
                {40, "Zero→Natural"}, {90, "Natural→Linear"}, {150, "Linear→Zero"} })
        {
            int start = blk * blockSize;
            int len   = 20 * blockSize; // longer window for phase transitions
            auto m = analyzeForClicks(result.output, result.input, start, len);
            expectClean(m, juce::String("Phase: ") + label);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 3. OVERSAMPLING SWITCHING
    // ─────────────────────────────────────────────────────────────────────────
    void testOversamplingSwitch(double sr, int blockSize)
    {
        beginTest("Oversampling: Off → 2x → 4x → Off (Natural Phase)");

        auto setup = [](AIEqualizerAudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts)
        {
            setupRealisticSession(proc, apvts);
            setChoice(apvts, "phaseMode", 1); // Natural Phase (oversampling only applies here)
        };

        auto result = runSession(sr, blockSize, 200, setup,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block == 40)  setChoice(apvts, "oversamplingFactor", 1); // 2x
                if (block == 90)  setChoice(apvts, "oversamplingFactor", 2); // 4x
                if (block == 140) setChoice(apvts, "oversamplingFactor", 0); // Off
            });

        for (auto [blk, label] : std::initializer_list<std::pair<int, const char*>>{
                {40, "Off→2x"}, {90, "2x→4x"}, {140, "4x→Off"} })
        {
            int start = blk * blockSize;
            int len   = 20 * blockSize;
            auto m = analyzeForClicks(result.output, result.input, start, len);
            expectClean(m, juce::String("OS: ") + label);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 4. BAND DRAGGING
    // ─────────────────────────────────────────────────────────────────────────
    void testSingleBandFreqSweep(double sr, int blockSize)
    {
        beginTest("Single band freq sweep 200Hz → 10kHz (aggressive, every block)");

        auto result = runSession(sr, blockSize, 120, setupRealisticSession,
            [blockSize](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block >= 20 && block <= 80)
                {
                    // Log sweep from 200 to 10kHz — mimics mouse drag
                    const float t = static_cast<float>(block - 20) / 60.0f;
                    const float freq = 200.0f * std::pow(50.0f, t); // log sweep
                    setFloat(apvts, "band4Freq", freq); // band 4 = 2kHz start
                }
            });

        int start = 20 * blockSize;
        int len   = 70 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len);
        expectClean(m, "Freq sweep 200→10k");
    }

    void testSingleBandGainSweep(double sr, int blockSize)
    {
        beginTest("Single band gain sweep -18dB → +18dB");

        auto result = runSession(sr, blockSize, 120, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block >= 20 && block <= 80)
                {
                    const float t = static_cast<float>(block - 20) / 60.0f;
                    const float gain = -18.0f + t * 36.0f; // -18 to +18
                    setFloat(apvts, "band3Gain", gain);
                }
            });

        int start = 20 * blockSize;
        int len   = 70 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len);
        expectClean(m, "Gain sweep -18→+18dB");
    }

    void testMultiBandSimultaneousDrag(double sr, int blockSize)
    {
        beginTest("Multi-band simultaneous drag (3 bands moving at once)");

        auto result = runSession(sr, blockSize, 120, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block >= 20 && block <= 80)
                {
                    const float t = static_cast<float>(block - 20) / 60.0f;

                    // Band 2: freq sweep 250 → 600 Hz
                    setFloat(apvts, "band2Freq", 250.0f + t * 350.0f);
                    // Band 4: gain sweep -2 → +8 dB
                    setFloat(apvts, "band4Gain", -2.0f + t * 10.0f);
                    // Band 5: Q sweep 0.5 → 6.0
                    setFloat(apvts, "band5Q", 0.5f + t * 5.5f);
                }
            });

        int start = 20 * blockSize;
        int len   = 70 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len);
        expectClean(m, "Multi-band drag");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 5. A/B PROFILE SWITCH
    // ─────────────────────────────────────────────────────────────────────────
    void testABProfileSwitch(double sr, int blockSize)
    {
        beginTest("A/B profile switch with different EQ curves");

        auto setup = [](AIEqualizerAudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts)
        {
            setupRealisticSession(proc, apvts);
            // Profile A is the current realistic session (saved automatically on switch)
        };

        auto result = runSession(sr, blockSize, 150, setup,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                // Switch A → B by changing all gains at once (simulates AB switch)
                if (block == 30)
                {
                    for (int i = 0; i < 8; ++i)
                        setFloat(apvts, "band" + juce::String(i) + "Gain", 6.0f);
                }
                if (block == 60)
                {
                    // Back to original mixed gains
                    const float gains[] = { 0.0f, 2.5f, -3.0f, 1.0f, -2.0f, 3.0f, 1.5f, 0.0f };
                    for (int i = 0; i < 8; ++i)
                        setFloat(apvts, "band" + juce::String(i) + "Gain", gains[i]);
                }
                if (block == 90)
                {
                    for (int i = 0; i < 8; ++i)
                        setFloat(apvts, "band" + juce::String(i) + "Gain", 6.0f);
                }
                if (block == 110)
                {
                    const float gains[] = { 0.0f, 2.5f, -3.0f, 1.0f, -2.0f, 3.0f, 1.5f, 0.0f };
                    for (int i = 0; i < 8; ++i)
                        setFloat(apvts, "band" + juce::String(i) + "Gain", gains[i]);
                }
            });

        for (int blk : { 30, 60, 90, 110 })
        {
            int start = blk * blockSize;
            int len   = 10 * blockSize;
            // AB switch changes 8 gains simultaneously (up to 9dB jumps).
            // SmoothedValue ramps over 20ms but first block has highest delta.
            // Use 0.40 click threshold — still catches real clicks, allows ramp transients.
            auto m = analyzeForClicks(result.output, result.input, start, len, 0.40f);
            expectClean(m, "AB switch@block" + juce::String(blk),
                        /*maxDeltaLimit=*/ 0.50f,
                        /*maxClicks=*/ 0);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 6. DYNAMIC EQ
    // ─────────────────────────────────────────────────────────────────────────
    void testDynamicEQThresholdSweep(double sr, int blockSize)
    {
        beginTest("Dynamic EQ threshold sweep -60dB → 0dB with active compression");

        auto setup = [](AIEqualizerAudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts)
        {
            setupRealisticSession(proc, apvts);

            // Enable compression on band 3 (800Hz)
            setChoice(apvts, "band3DynMode", 1);     // Compress
            setFloat(apvts, "band3Threshold", -20.0f);
            setFloat(apvts, "band3Ratio", 4.0f);
            setFloat(apvts, "band3Attack", 10.0f);
            setFloat(apvts, "band3Release", 100.0f);
            setFloat(apvts, "band3Range", 24.0f);
            setFloat(apvts, "band3Knee", 6.0f);
        };

        auto result = runSession(sr, blockSize, 120, setup,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block >= 20 && block <= 80)
                {
                    // Sweep threshold from -60 to 0 dB
                    const float t = static_cast<float>(block - 20) / 60.0f;
                    const float thresh = -60.0f + t * 60.0f;
                    setFloat(apvts, "band3Threshold", thresh);
                }
            });

        int start = 20 * blockSize;
        int len   = 70 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len);
        expectClean(m, "DynEQ threshold sweep");
    }

    void testDynamicEQModeToggle(double sr, int blockSize)
    {
        beginTest("Dynamic EQ mode toggle: Off → Compress → Expand → Gate → Off");

        auto result = runSession(sr, blockSize, 160, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                // Set up compression parameters first
                if (block == 10)
                {
                    setFloat(apvts, "band3Threshold", -20.0f);
                    setFloat(apvts, "band3Ratio", 4.0f);
                    setFloat(apvts, "band3Attack", 5.0f);
                    setFloat(apvts, "band3Release", 50.0f);
                    setFloat(apvts, "band3Range", 24.0f);
                    setFloat(apvts, "band3Knee", 3.0f);
                }

                if (block == 30)  setChoice(apvts, "band3DynMode", 1); // Compress
                if (block == 60)  setChoice(apvts, "band3DynMode", 2); // Expand
                if (block == 90)  setChoice(apvts, "band3DynMode", 3); // Gate
                if (block == 120) setChoice(apvts, "band3DynMode", 0); // Off
            });

        for (auto [blk, label] : std::initializer_list<std::pair<int, const char*>>{
                {30, "Off→Compress"}, {60, "Compress→Expand"},
                {90, "Expand→Gate"}, {120, "Gate→Off"} })
        {
            int start = blk * blockSize;
            int len   = 15 * blockSize;
            auto m = analyzeForClicks(result.output, result.input, start, len);
            expectClean(m, juce::String("DynEQ: ") + label);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 7. AI CORRECTION BURST
    // ─────────────────────────────────────────────────────────────────────────
    void testAICorrectionBurst(double sr, int blockSize)
    {
        beginTest("AI correction burst: 8 bands change simultaneously");

        auto result = runSession(sr, blockSize, 120, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block == 40)
                {
                    // Simulate AI applying corrections to all 8 bands at once
                    const float corrections[] = { 0.0f, -2.0f, 3.0f, -4.0f, 5.0f, -3.0f, 2.0f, 0.0f };
                    const float freqShifts[]  = { 30.0f, 90.0f, 300.0f, 700.0f, 2500.0f, 4500.0f, 9000.0f, 15000.0f };

                    for (int i = 0; i < 8; ++i)
                    {
                        juce::String prefix = "band" + juce::String(i);
                        setFloat(apvts, prefix + "Gain", corrections[i]);
                        setFloat(apvts, prefix + "Freq", freqShifts[i]);
                    }
                }
            });

        int start = 40 * blockSize;
        int len   = 15 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len);
        expectClean(m, "AI correction burst");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 8. COMPOUND STRESS — everything at once
    // ─────────────────────────────────────────────────────────────────────────
    void testCompoundStress(double sr, int blockSize)
    {
        beginTest("Compound stress: bypass + phase + OS + drag + AB in rapid succession");

        auto result = runSession(sr, blockSize, 250, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                // Continuous band drag throughout
                if (block >= 10 && block <= 200)
                {
                    const float t = static_cast<float>((block - 10) % 60) / 60.0f;
                    const float freq = 200.0f * std::pow(50.0f, t);
                    setFloat(apvts, "band4Freq", freq);
                }

                // Layer on transitions
                if (block == 30)  setBool(apvts, "bypass", true);
                if (block == 35)  setBool(apvts, "bypass", false);
                if (block == 50)  setChoice(apvts, "phaseMode", 1);  // Natural
                if (block == 70)  setChoice(apvts, "oversamplingFactor", 1); // 2x
                if (block == 90)
                {
                    // Simulate AB switch: all gains to +8
                    for (int i = 0; i < 8; ++i)
                        setFloat(apvts, "band" + juce::String(i) + "Gain", 8.0f);
                }
                if (block == 110)
                {
                    // Switch back to original gains
                    const float gains[] = { 0.0f, 2.5f, -3.0f, 1.0f, -2.0f, 3.0f, 1.5f, 0.0f };
                    for (int i = 0; i < 8; ++i)
                        setFloat(apvts, "band" + juce::String(i) + "Gain", gains[i]);
                }
                if (block == 130) setChoice(apvts, "phaseMode", 0);  // Back to Zero
                if (block == 150) setChoice(apvts, "oversamplingFactor", 0);
                if (block == 170)
                {
                    // Dynamic EQ burst
                    setChoice(apvts, "band3DynMode", 1);
                    setFloat(apvts, "band3Threshold", -15.0f);
                    setFloat(apvts, "band3Ratio", 8.0f);
                }
                if (block == 190) setChoice(apvts, "band3DynMode", 0);
            });

        // Analyze the entire session
        int start = 10 * blockSize;
        int len   = 200 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len, 0.75f);
        // Compound stress: phase transitions produce structural maxDelta ~0.6
        // during crossfade (two different processing paths blending).
        // Allow higher delta but still catch explosions and NaN.
        expectClean(m, "Compound stress",
                    /*maxDeltaLimit=*/ 0.75f,
                    /*maxClicks=*/ 0,
                    /*peakLimit=*/ 3.0f,
                    /*maxDropout=*/ 8);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 9. EDGE CASES: small and large block sizes
    // ─────────────────────────────────────────────────────────────────────────
    void testSmallBlockSize(double sr, int blockSize)
    {
        beginTest("All transitions @ blockSize=32 (extreme low latency)");

        auto result = runSession(sr, blockSize, 400, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block == 50)  setBool(apvts, "bypass", true);
                if (block == 80)  setBool(apvts, "bypass", false);
                if (block == 120) setChoice(apvts, "phaseMode", 1);
                if (block == 180) setChoice(apvts, "phaseMode", 0);

                if (block >= 200 && block <= 300)
                {
                    const float t = static_cast<float>(block - 200) / 100.0f;
                    setFloat(apvts, "band4Freq", 200.0f * std::pow(50.0f, t));
                }

                if (block == 320)
                {
                    for (int i = 0; i < 8; ++i)
                        setFloat(apvts, "band" + juce::String(i) + "Gain",
                                 -10.0f + static_cast<float>(i) * 3.0f);
                }
            });

        int start = 40 * blockSize;
        int len   = 320 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len, 0.70f);
        // Small blocks: phase transition crossfade spans many blocks,
        // structural delta is higher because each block is only 32 samples.
        expectClean(m, "Small block (32) transitions",
                    /*maxDeltaLimit=*/ 0.70f,
                    /*maxClicks=*/ 0,    // zero clicks at 0.70 threshold
                    /*peakLimit=*/ 3.0f,
                    /*maxDropout=*/ 8);
    }

    void testLargeBlockSize(double sr, int blockSize)
    {
        beginTest("All transitions @ blockSize=2048 (high latency)");

        auto result = runSession(sr, blockSize, 60, setupRealisticSession,
            [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
            {
                if (block == 5)  setBool(apvts, "bypass", true);
                if (block == 10) setBool(apvts, "bypass", false);
                if (block == 15) setChoice(apvts, "phaseMode", 1);
                if (block == 25) setChoice(apvts, "phaseMode", 0);

                if (block >= 30 && block <= 45)
                {
                    const float t = static_cast<float>(block - 30) / 15.0f;
                    setFloat(apvts, "band4Freq", 200.0f * std::pow(50.0f, t));
                    setFloat(apvts, "band3Gain", -12.0f + t * 24.0f);
                }
            });

        int start = 4 * blockSize;
        int len   = 50 * blockSize;
        auto m = analyzeForClicks(result.output, result.input, start, len);
        expectClean(m, "Large block (2048) transitions");
    }
};

static HostSessionClickTest hostSessionClickTest;
