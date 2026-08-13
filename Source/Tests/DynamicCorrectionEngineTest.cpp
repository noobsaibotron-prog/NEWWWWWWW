#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AI/DynamicCorrectionEngine.h"

#include <cmath>
#include <thread>

/**
 * DynamicCorrectionEngine behavioural contracts (AI-evolution D1, category "AI").
 *
 *  1. disabled/empty => bit-transparent
 *  2. dynamic cut engages on a hot resonance and RELEASES on silence-in-band
 *  3. static correction attenuates the band unconditionally
 *  4. out-of-band material is (near-)untouched while cutting in-band
 *  5. publisher/reader stress: publish storms + process concurrently => finite
 *     output, no crash (TSan-friendly design witness)
 */
class DynamicCorrectionEngineTest : public juce::UnitTest
{
public:
    DynamicCorrectionEngineTest()
        : juce::UnitTest("Dynamic Correction Engine (D1)", "AI-RT") {}

    static float bandRmsDb(const juce::AudioBuffer<float>& buffer, double sr,
                           float freq, float q)
    {
        // simple measurement bandpass, one channel
        DynamicCorrectionEngine::CorrectionParams unused;
        juce::ignoreUnused(unused);
        const float omega = 2.0f * juce::MathConstants<float>::pi * freq / static_cast<float>(sr);
        const float sinW = std::sin(omega), cosW = std::cos(omega);
        const float alpha = sinW / (2.0f * q);
        const float a0 = 1.0f + alpha;
        const float b0 = (sinW * 0.5f) / a0, b2 = -(sinW * 0.5f) / a0;
        const float a1 = (-2.0f * cosW) / a0, a2 = (1.0f - alpha) / a0;

        double sum = 0.0;
        float z1 = 0.0f, z2 = 0.0f;
        const float* d = buffer.getReadPointer(0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float y = b0 * d[i] + z1;
            z1 = -a1 * y + z2;               // b1 == 0
            z2 = b2 * d[i] - a2 * y;
            sum += static_cast<double>(y) * y;
        }
        const float rms = static_cast<float>(std::sqrt(sum / juce::jmax(1, buffer.getNumSamples())));
        return juce::Decibels::gainToDecibels(rms, -120.0f);
    }

    static void fillSine(juce::AudioBuffer<float>& buffer, double sr, float freq,
                         float amp, double& phase)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float s = amp * static_cast<float>(std::sin(phase));
            phase += 2.0 * juce::MathConstants<double>::pi * freq / sr;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample(ch, i, s);
        }
    }

    void runTest() override
    {
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;

        beginTest("disabled or empty => bit-transparent");
        {
            DynamicCorrectionEngine engine;
            engine.prepare(kSr, kBlock, 2);

            juce::AudioBuffer<float> buf(2, kBlock);
            double phase = 0.0;
            fillSine(buf, kSr, 1000.0f, 0.5f, phase);
            juce::AudioBuffer<float> ref(2, kBlock);
            for (int ch = 0; ch < 2; ++ch)
                ref.copyFrom(ch, 0, buf, ch, 0, kBlock);

            engine.process(buf);   // disabled
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    if (buf.getSample(ch, i) != ref.getSample(ch, i))
                    {
                        expect(false, "disabled engine modified audio");
                        return;
                    }

            engine.setEnabled(true);  // enabled but EMPTY snapshot
            engine.process(buf);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    if (buf.getSample(ch, i) != ref.getSample(ch, i))
                    {
                        expect(false, "empty snapshot modified audio");
                        return;
                    }
            expect(true);
        }

        beginTest("dynamic cut engages on hot resonance, releases on in-band silence");
        {
            DynamicCorrectionEngine engine;
            engine.prepare(kSr, kBlock, 2);
            engine.setEnabled(true);

            DynamicCorrectionEngine::Snapshot snap;
            snap.numActive = 1;
            snap.version = 1;
            auto& c = snap.corrections[0];
            c.frequencyHz = 3200.0f;
            c.q = 8.0f;
            c.maxCutDb = 9.0f;
            c.thresholdDb = -30.0f;
            c.ratio = 4.0f;
            c.attackMs = 2.0f;
            c.releaseMs = 40.0f;
            c.dynamic = true;
            c.enabled = true;
            engine.publishCorrections(snap);

            // hot 3200 Hz tone (well above threshold)
            double phase = 0.0;
            float cutWhileHot = 0.0f;
            for (int b = 0; b < 60; ++b)
            {
                juce::AudioBuffer<float> buf(2, kBlock);
                fillSine(buf, kSr, 3200.0f, 0.5f, phase);
                engine.process(buf);
                cutWhileHot = engine.getCurrentCutDb(0);
            }
            logMessage("  cut while hot: " + juce::String(cutWhileHot, 2) + " dB");
            expect(cutWhileHot < -3.0f, "dynamic cut did not engage on a hot resonance (got "
                                        + juce::String(cutWhileHot, 2) + " dB)");
            expect(cutWhileHot >= -9.5f, "cut exceeded maxCutDb");

            // out-of-band content only -> must RELEASE toward 0
            double phase2 = 0.0;
            float cutAfterRelease = -100.0f;
            for (int b = 0; b < 120; ++b)
            {
                juce::AudioBuffer<float> buf(2, kBlock);
                fillSine(buf, kSr, 200.0f, 0.3f, phase2);
                engine.process(buf);
                cutAfterRelease = engine.getCurrentCutDb(0);
            }
            logMessage("  cut after out-of-band release: " + juce::String(cutAfterRelease, 2) + " dB");
            expect(cutAfterRelease > -1.0f, "dynamic cut did not release when the band went quiet");
        }

        beginTest("static correction attenuates the band; out-of-band near-untouched");
        {
            DynamicCorrectionEngine engine;
            engine.prepare(kSr, kBlock, 2);
            engine.setEnabled(true);

            DynamicCorrectionEngine::Snapshot snap;
            snap.numActive = 1;
            snap.version = 7;
            auto& c = snap.corrections[0];
            c.frequencyHz = 1000.0f;
            c.q = 4.0f;
            c.maxCutDb = 6.0f;
            c.dynamic = false;   // static cut
            c.enabled = true;
            engine.publishCorrections(snap);

            // two-tone: 1 kHz (target) + 6 kHz (bystander)
            juce::AudioBuffer<float> processedTotal(2, kBlock * 40);
            double p1 = 0.0, p2 = 0.0;
            for (int b = 0; b < 40; ++b)
            {
                juce::AudioBuffer<float> buf(2, kBlock);
                for (int i = 0; i < kBlock; ++i)
                {
                    const float s = 0.25f * static_cast<float>(std::sin(p1))
                                  + 0.25f * static_cast<float>(std::sin(p2));
                    p1 += 2.0 * juce::MathConstants<double>::pi * 1000.0 / kSr;
                    p2 += 2.0 * juce::MathConstants<double>::pi * 6000.0 / kSr;
                    buf.setSample(0, i, s);
                    buf.setSample(1, i, s);
                }
                engine.process(buf);
                for (int ch = 0; ch < 2; ++ch)
                    processedTotal.copyFrom(ch, b * kBlock, buf, ch, 0, kBlock);
            }

            // reference: same signal, no processing
            juce::AudioBuffer<float> reference(2, kBlock * 40);
            p1 = 0.0; p2 = 0.0;
            for (int i = 0; i < kBlock * 40; ++i)
            {
                const float s = 0.25f * static_cast<float>(std::sin(p1))
                              + 0.25f * static_cast<float>(std::sin(p2));
                p1 += 2.0 * juce::MathConstants<double>::pi * 1000.0 / kSr;
                p2 += 2.0 * juce::MathConstants<double>::pi * 6000.0 / kSr;
                reference.setSample(0, i, s);
                reference.setSample(1, i, s);
            }

            const float target = bandRmsDb(processedTotal, kSr, 1000.0f, 4.0f)
                               - bandRmsDb(reference, kSr, 1000.0f, 4.0f);
            const float bystander = bandRmsDb(processedTotal, kSr, 6000.0f, 4.0f)
                                  - bandRmsDb(reference, kSr, 6000.0f, 4.0f);
            logMessage("  target band delta: " + juce::String(target, 2)
                       + " dB, bystander delta: " + juce::String(bystander, 2) + " dB");
            expect(target < -4.0f, "static cut did not attenuate the target band (got "
                                   + juce::String(target, 2) + " dB)");
            expect(std::abs(bystander) < 1.0f, "bystander band moved more than 1 dB ("
                                               + juce::String(bystander, 2) + " dB)");
        }

        beginTest("publish storm concurrent with process: finite output, no tearing crash");
        {
            DynamicCorrectionEngine engine;
            engine.prepare(kSr, kBlock, 2);
            engine.setEnabled(true);

            std::atomic<bool> stop { false };
            std::atomic<bool> publisherStarted { false };
            std::thread publisher([&engine, &stop, &publisherStarted]
            {
                juce::Random rng(777);
                uint32_t version = 1;
                while (!stop.load())
                {
                    DynamicCorrectionEngine::Snapshot snap;
                    snap.numActive = 1 + rng.nextInt(DynamicCorrectionEngine::kMaxCorrections);
                    snap.version = version++;
                    for (int i = 0; i < snap.numActive; ++i)
                    {
                        auto& cc = snap.corrections[static_cast<size_t>(i)];
                        cc.frequencyHz = 100.0f + rng.nextFloat() * 10000.0f;
                        cc.q = 1.0f + rng.nextFloat() * 10.0f;
                        cc.maxCutDb = 1.0f + rng.nextFloat() * 12.0f;
                        cc.thresholdDb = -60.0f + rng.nextFloat() * 40.0f;
                        cc.dynamic = rng.nextBool();
                        cc.enabled = true;
                    }
                    engine.publishCorrections(snap);
                    publisherStarted.store(true, std::memory_order_release);
                }
            });

            // Make the test scheduler-independent: on a saturated CI runner the
            // consumer can finish 400 blocks before the publisher receives its
            // first timeslice. That tests scheduler luck, not mailbox safety.
            while (!publisherStarted.load(std::memory_order_acquire))
                std::this_thread::yield();

            double phase = 0.0;
            bool allFinite = true;
            bool versionsMonotonic = true;
            uint32_t previousVersion = 0;
            for (int b = 0; b < 400; ++b)
            {
                juce::AudioBuffer<float> buf(2, kBlock);
                fillSine(buf, kSr, 500.0f + (b % 50) * 100.0f, 0.4f, phase);
                engine.process(buf);
                for (int ch = 0; ch < 2 && allFinite; ++ch)
                {
                    const float* d = buf.getReadPointer(ch);
                    for (int i = 0; i < kBlock; ++i)
                        if (!std::isfinite(d[i])) { allFinite = false; break; }
                }
                if (!allFinite)
                    break;

                const auto consumedVersion = engine.getActiveSnapshotVersion();
                if (consumedVersion < previousVersion)
                    versionsMonotonic = false;
                previousVersion = consumedVersion;
            }
            stop.store(true);
            publisher.join();
            expect(allFinite, "non-finite output under publish storm");
            expect(versionsMonotonic, "audio thread consumed snapshot versions out of order");
            expect(engine.getActiveSnapshotVersion() > 0, "audio thread never consumed a snapshot");
        }

        beginTest("mailbox liveness and newest-wins are deterministic protocol invariants");
        {
            // The concurrent storm above exercises the CAS paths. This test
            // proves the two protocol properties without wall-clock or
            // scheduler thresholds:
            //   1. after a burst, one process() consumes the newest publication;
            //   2. repeated acquisitions release the previously owned slot, so
            //      the mailbox remains writable indefinitely.
            //
            // Three writes per round deliberately leave obsolete READY slots.
            // Choosing the oldest READY slot fails on round one. Removing the
            // previous-slot release exhausts all four slots and fails within a
            // handful of rounds. Both mutations are therefore killed by exact
            // version equality rather than a timeout.
            constexpr double kSr = 48000.0;
            constexpr int kBlock = 64;
            constexpr int kRounds = 32;
            constexpr int kBurstSize = 3;

            DynamicCorrectionEngine engine;
            engine.prepare(kSr, kBlock, 2);
            engine.setEnabled(true);

            uint32_t publishedVersion = 0;
            uint32_t previousConsumed = 0;
            bool exactNewestEveryRound = true;
            bool monotonic = true;
            double phase = 0.0;
            juce::AudioBuffer<float> buf(2, kBlock);

            for (int round = 0; round < kRounds; ++round)
            {
                for (int publication = 0; publication < kBurstSize; ++publication)
                {
                    ++publishedVersion;
                    DynamicCorrectionEngine::Snapshot snap;
                    snap.version = publishedVersion;
                    snap.numActive = 1 + static_cast<int>(
                        publishedVersion % DynamicCorrectionEngine::kMaxCorrections);
                    for (int i = 0; i < snap.numActive; ++i)
                    {
                        auto& cc = snap.corrections[static_cast<size_t>(i)];
                        cc.frequencyHz = 200.0f + static_cast<float>(
                            (publishedVersion + static_cast<uint32_t>(i)) % 8000u);
                        cc.q = 2.0f;
                        cc.maxCutDb = 6.0f;
                        cc.thresholdDb = -80.0f;
                        cc.ratio = 3.0f;
                        cc.attackMs = 3.0f;
                        cc.releaseMs = 60.0f;
                        cc.dynamic = false;
                        cc.enabled = true;
                    }
                    engine.publishCorrections(snap);
                }
                fillSine(buf, kSr, 1000.0f, 0.4f, phase);
                engine.process(buf);

                const auto consumed = engine.getActiveSnapshotVersion();
                if (consumed < previousConsumed)
                    monotonic = false;
                if (consumed != publishedVersion)
                    exactNewestEveryRound = false;
                previousConsumed = consumed;
            }

            expect(monotonic, "consumed versions regressed");
            expect(exactNewestEveryRound,
                   "reader did not consume the newest publication in every burst");
            expectEquals(static_cast<int>(engine.getActiveSnapshotVersion()),
                         static_cast<int>(publishedVersion),
                         "reader did not reach the final publication");
        }
    }
};

static DynamicCorrectionEngineTest gDynamicCorrectionEngineTest;
