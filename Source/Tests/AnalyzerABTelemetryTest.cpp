#include <juce_core/juce_core.h>

#include "../GUI/AnalyzerABTelemetry.h"

using namespace aieq::gui::analyzer_ab;

class AnalyzerABTelemetryTest final : public juce::UnitTest
{
public:
    AnalyzerABTelemetryTest()
        : juce::UnitTest("Analyzer A/B telemetry", "Integration") {}

    void runTest() override
    {
        testConfigurationAndRebuildAccounting();
        testDeterministicNearestRankSummary();
        testDeadlineBoundary();
        testRingOverwriteIsChronological();
        testReset();
        testWriteDump();
    }

private:
    static Configuration config(uint32_t sampleRate, uint32_t fftSize,
                                int8_t resolution, bool visible, bool frozen)
    {
        Configuration result;
        result.sampleRateHz = sampleRate;
        result.fftSize = fftSize;
        result.resolutionChoice = resolution;
        result.timerHz = visible ? 60 : 5;
        result.visible = visible;
        result.frozen = frozen;
        result.injectedPipeline = true;
        return result;
    }

    void testConfigurationAndRebuildAccounting()
    {
        beginTest("Tick records variant/configuration and aggregates rebuilds without logging");

        Collector collector;
        const auto expected = config(96000, 8192, 3, true, false);
        collector.beginTimerTick(expected);
        collector.recordRebuild(2'000);
        collector.recordRebuild(3'000);
        collector.finishTimerTick(9'000);

        const auto snapshot = collector.snapshot();
        expectEquals(static_cast<int>(snapshot.samples.size()), 1);
        expectEquals(static_cast<juce::int64>(snapshot.totalTicksSeen), static_cast<juce::int64>(1));
        expectEquals(static_cast<juce::int64>(snapshot.totalRebuildsSeen), static_cast<juce::int64>(2));
        expectEquals(static_cast<juce::int64>(snapshot.totalRebuildDurationNs), static_cast<juce::int64>(5'000));

        const auto& sample = snapshot.samples.front();
        expectEquals(static_cast<int>(sample.configuration.variant), static_cast<int>(buildVariant()));
        expectEquals(static_cast<int>(sample.configuration.sampleRateHz), 96000);
        expectEquals(static_cast<int>(sample.configuration.fftSize), 8192);
        expectEquals(static_cast<int>(sample.configuration.resolutionChoice), 3);
        expectEquals(static_cast<int>(sample.configuration.timerHz), 60);
        expect(sample.configuration.visible);
        expect(!sample.configuration.frozen);
        expect(sample.configuration.injectedPipeline);
        expectEquals(static_cast<int>(sample.rebuildCount), 2);
        expectEquals(static_cast<juce::int64>(sample.rebuildDurationNs), static_cast<juce::int64>(5'000));
        expectEquals(static_cast<juce::int64>(sample.timerDurationNs), static_cast<juce::int64>(9'000));
    }

    void testDeterministicNearestRankSummary()
    {
        beginTest("p50/p95/p99/max use deterministic nearest-rank outside hot path");

        Collector collector;
        const auto configuration = config(48000, 4096, 2, true, false);
        for (uint64_t i = 1; i <= 100; ++i)
        {
            collector.beginTimerTick(configuration);
            if ((i % 10) == 0)
                collector.recordRebuild(i * 1'000);
            collector.finishTimerTick(i * 1'000'000);
        }

        const auto summary = Collector::summarize(collector.snapshot());
        expectEquals(static_cast<juce::int64>(summary.sampleCount), static_cast<juce::int64>(100));
        expectEquals(static_cast<juce::int64>(summary.rebuildCount), static_cast<juce::int64>(10));
        expectEquals(static_cast<juce::int64>(summary.timerP50Ns), static_cast<juce::int64>(50'000'000));
        expectEquals(static_cast<juce::int64>(summary.timerP95Ns), static_cast<juce::int64>(95'000'000));
        expectEquals(static_cast<juce::int64>(summary.timerP99Ns), static_cast<juce::int64>(99'000'000));
        expectEquals(static_cast<juce::int64>(summary.timerMaxNs), static_cast<juce::int64>(100'000'000));
        expectEquals(static_cast<juce::int64>(summary.rebuildP50Ns), static_cast<juce::int64>(50'000));
        expectEquals(static_cast<juce::int64>(summary.rebuildP95Ns), static_cast<juce::int64>(100'000));
        expectEquals(static_cast<juce::int64>(summary.rebuildP99Ns), static_cast<juce::int64>(100'000));
        expectEquals(static_cast<juce::int64>(summary.rebuildMaxNs), static_cast<juce::int64>(100'000));
    }

    void testDeadlineBoundary()
    {
        beginTest("Deadline miss is strictly greater than 16.667 ms");

        Collector collector;
        const auto configuration = config(48000, 4096, 2, true, false);
        collector.beginTimerTick(configuration);
        collector.finishTimerTick(Collector::deadlineNs);
        collector.beginTimerTick(configuration);
        collector.finishTimerTick(Collector::deadlineNs + 1);

        const auto summary = Collector::summarize(collector.snapshot());
        expectEquals(static_cast<juce::int64>(summary.deadlineMissCount), static_cast<juce::int64>(1));
    }

    void testRingOverwriteIsChronological()
    {
        beginTest("Fixed ring overwrites oldest samples and snapshot restores chronology");

        Collector collector;
        const auto configuration = config(44100, 1024, 0, false, true);
        constexpr uint64_t extra = 7;
        for (uint64_t i = 0; i < Collector::capacity + extra; ++i)
        {
            collector.beginTimerTick(configuration);
            collector.finishTimerTick(i);
        }

        const auto snapshot = collector.snapshot();
        expectEquals(static_cast<juce::int64>(snapshot.totalTicksSeen),
                     static_cast<juce::int64>(Collector::capacity + extra));
        expectEquals(static_cast<juce::int64>(snapshot.overwrittenTickCount), static_cast<juce::int64>(extra));
        expectEquals(static_cast<juce::int64>(snapshot.samples.size()), static_cast<juce::int64>(Collector::capacity));
        expectEquals(static_cast<juce::int64>(snapshot.samples.front().sequence), static_cast<juce::int64>(extra));
        expectEquals(static_cast<juce::int64>(snapshot.samples.front().timerDurationNs), static_cast<juce::int64>(extra));
        expectEquals(static_cast<juce::int64>(snapshot.samples.back().sequence),
                     static_cast<juce::int64>(Collector::capacity + extra - 1));
    }

    void testReset()
    {
        beginTest("Reset clears samples and cumulative counters");

        Collector collector;
        collector.recordRebuild(17);
        collector.beginTimerTick(config(48000, 4096, 2, true, false));
        collector.finishTimerTick(42);
        collector.reset();

        const auto snapshot = collector.snapshot();
        expect(snapshot.samples.empty());
        expectEquals(static_cast<juce::int64>(snapshot.totalTicksSeen), static_cast<juce::int64>(0));
        expectEquals(static_cast<juce::int64>(snapshot.totalRebuildsSeen), static_cast<juce::int64>(0));
        expectEquals(static_cast<juce::int64>(snapshot.totalRebuildDurationNs), static_cast<juce::int64>(0));
    }

    void testWriteDump()
    {
        beginTest("writeDump records p50/p95 and variant off the hot path");

        Collector collector;
        collector.beginTimerTick(config(48000, 4096, 2, true, false));
        collector.recordRebuild(2'000'000);
        collector.finishTimerTick(8'000'000);

        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("aieq-analyzer-ab-dump-test.txt");
        file.deleteFile();
        expect(writeDump(file.getFullPathName().toStdString(), collector.snapshot()),
               "dump file is written");

        const auto text = file.loadFileAsString();
        expect(text.contains("variant " + juce::String(variantName(buildVariant()))));
        expect(text.contains("timer_p50_ms 8"));
        expect(text.contains("rebuild_p50_ms 2"));
        expect(text.contains("ticks 1"));
        file.deleteFile();
    }
};

static AnalyzerABTelemetryTest analyzerABTelemetryTest;
