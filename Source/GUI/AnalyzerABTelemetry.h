#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Dev/test-only telemetry for the analyzer A1/A2 presentation experiment.
 *
 * Recording is a fixed-capacity, allocation-free message-thread operation.
 * snapshot() and summarize() may allocate and therefore must be called outside
 * the measured timer callback. The collector is intentionally not thread-safe:
 * AdvancedSpectrumDisplay records and snapshots it on the JUCE message thread.
 */
namespace aieq::gui::analyzer_ab
{
enum class Variant : uint8_t
{
    unknown = 0,
    A1_LATEST_HOP = 1,
    A2_HOP_LERP = 2
};

constexpr Variant buildVariant() noexcept
{
#if defined(AIEQ_ANALYZER_AB_A1_LATEST_HOP) && AIEQ_ANALYZER_AB_A1_LATEST_HOP
    return Variant::A1_LATEST_HOP;
#else
    return Variant::A2_HOP_LERP;
#endif
}

struct Configuration
{
    Variant variant = buildVariant();
    uint32_t sampleRateHz = 0;
    uint32_t fftSize = 0;
    int8_t resolutionChoice = -1;
    uint16_t timerHz = 0;
    bool visible = false;
    bool frozen = false;
    bool injectedPipeline = false;
    bool glSpectrumActive = false;
};

struct TickSample
{
    uint64_t sequence = 0;
    uint64_t timerDurationNs = 0;
    uint64_t rebuildDurationNs = 0;
    uint16_t rebuildCount = 0;
    Configuration configuration;
};

struct Snapshot
{
    std::vector<TickSample> samples;
    uint64_t totalTicksSeen = 0;
    uint64_t totalRebuildsSeen = 0;
    uint64_t totalRebuildDurationNs = 0;
    uint64_t overwrittenTickCount = 0;
};

struct DurationSummary
{
    uint64_t sampleCount = 0;
    uint64_t rebuildCount = 0;
    uint64_t deadlineMissCount = 0;
    uint64_t timerP50Ns = 0;
    uint64_t timerP95Ns = 0;
    uint64_t timerP99Ns = 0;
    uint64_t timerMaxNs = 0;
    uint64_t rebuildP50Ns = 0;
    uint64_t rebuildP95Ns = 0;
    uint64_t rebuildP99Ns = 0;
    uint64_t rebuildMaxNs = 0;
};

class Collector
{
public:
    static constexpr size_t capacity = 4096;
    static constexpr uint64_t deadlineNs = 16'667'000;

    void beginTimerTick(const Configuration& configuration) noexcept
    {
        currentConfiguration = configuration;
        currentRebuildDurationNs = 0;
        currentRebuildCount = 0;
        timerTickActive = true;
    }

    void recordRebuild(uint64_t durationNs) noexcept
    {
        ++totalRebuildsSeen;
        totalRebuildDurationNs += durationNs;

        if (timerTickActive)
        {
            currentRebuildDurationNs += durationNs;
            if (currentRebuildCount != UINT16_MAX)
                ++currentRebuildCount;
        }
    }

    void finishTimerTick(uint64_t durationNs) noexcept
    {
        if (!timerTickActive)
            return;

        TickSample sample;
        sample.sequence = totalTicksSeen;
        sample.timerDurationNs = durationNs;
        sample.rebuildDurationNs = currentRebuildDurationNs;
        sample.rebuildCount = currentRebuildCount;
        sample.configuration = currentConfiguration;

        ring[writeIndex] = sample;
        writeIndex = (writeIndex + 1) % capacity;
        if (storedCount < capacity)
            ++storedCount;
        ++totalTicksSeen;
        timerTickActive = false;
    }

    Snapshot snapshot() const
    {
        Snapshot result;
        result.samples.reserve(storedCount);
        result.totalTicksSeen = totalTicksSeen;
        result.totalRebuildsSeen = totalRebuildsSeen;
        result.totalRebuildDurationNs = totalRebuildDurationNs;
        result.overwrittenTickCount = totalTicksSeen > storedCount
                                      ? totalTicksSeen - storedCount : 0;

        const size_t first = storedCount == capacity ? writeIndex : 0;
        for (size_t i = 0; i < storedCount; ++i)
            result.samples.push_back(ring[(first + i) % capacity]);
        return result;
    }

    void reset() noexcept
    {
        writeIndex = 0;
        storedCount = 0;
        totalTicksSeen = 0;
        totalRebuildsSeen = 0;
        totalRebuildDurationNs = 0;
        currentRebuildDurationNs = 0;
        currentRebuildCount = 0;
        timerTickActive = false;
    }

    static DurationSummary summarize(const Snapshot& snapshot)
    {
        DurationSummary result;
        result.sampleCount = snapshot.samples.size();
        result.rebuildCount = snapshot.totalRebuildsSeen;

        std::vector<uint64_t> timerDurations;
        std::vector<uint64_t> rebuildDurations;
        timerDurations.reserve(snapshot.samples.size());
        rebuildDurations.reserve(snapshot.samples.size());

        for (const auto& sample : snapshot.samples)
        {
            timerDurations.push_back(sample.timerDurationNs);
            if (sample.timerDurationNs > deadlineNs)
                ++result.deadlineMissCount;
            if (sample.rebuildCount != 0)
                rebuildDurations.push_back(sample.rebuildDurationNs);
        }

        std::sort(timerDurations.begin(), timerDurations.end());
        std::sort(rebuildDurations.begin(), rebuildDurations.end());
        assignQuantiles(timerDurations, result.timerP50Ns, result.timerP95Ns,
                        result.timerP99Ns, result.timerMaxNs);
        assignQuantiles(rebuildDurations, result.rebuildP50Ns, result.rebuildP95Ns,
                        result.rebuildP99Ns, result.rebuildMaxNs);
        return result;
    }

private:
    static uint64_t nearestRank(const std::vector<uint64_t>& sorted, double percentile)
    {
        if (sorted.empty())
            return 0;
        const auto rank = static_cast<size_t>(std::ceil(percentile * static_cast<double>(sorted.size())));
        return sorted[std::min(sorted.size() - 1, std::max<size_t>(1, rank) - 1)];
    }

    static void assignQuantiles(const std::vector<uint64_t>& sorted,
                                uint64_t& p50, uint64_t& p95,
                                uint64_t& p99, uint64_t& maximum)
    {
        p50 = nearestRank(sorted, 0.50);
        p95 = nearestRank(sorted, 0.95);
        p99 = nearestRank(sorted, 0.99);
        maximum = sorted.empty() ? 0 : sorted.back();
    }

    std::array<TickSample, capacity> ring {};
    size_t writeIndex = 0;
    size_t storedCount = 0;
    uint64_t totalTicksSeen = 0;
    uint64_t totalRebuildsSeen = 0;
    uint64_t totalRebuildDurationNs = 0;
    uint64_t currentRebuildDurationNs = 0;
    uint16_t currentRebuildCount = 0;
    Configuration currentConfiguration;
    bool timerTickActive = false;
};

class ScopedTimerTick
{
public:
    ScopedTimerTick(Collector& collectorIn, const Configuration& configuration) noexcept
        : collector(collectorIn), start(Clock::now())
    {
        collector.beginTimerTick(configuration);
    }

    ~ScopedTimerTick() noexcept
    {
        collector.finishTimerTick(elapsedNs(start));
    }

private:
    using Clock = std::chrono::steady_clock;

    static uint64_t elapsedNs(Clock::time_point from) noexcept
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - from).count());
    }

    Collector& collector;
    Clock::time_point start;
};

class ScopedRebuild
{
public:
    explicit ScopedRebuild(Collector& collectorIn) noexcept
        : collector(collectorIn), start(Clock::now()) {}

    ~ScopedRebuild() noexcept
    {
        collector.recordRebuild(elapsedNs(start));
    }

private:
    using Clock = std::chrono::steady_clock;

    static uint64_t elapsedNs(Clock::time_point from) noexcept
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - from).count());
    }

    Collector& collector;
    Clock::time_point start;
};
} // namespace aieq::gui::analyzer_ab
