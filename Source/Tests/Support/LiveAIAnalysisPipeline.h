#pragma once

/**
 * LiveAIAnalysisPipeline — TEST-ONLY deterministic mirror of the shipped
 * EC-001/B4 audio -> PerceptualFrontEnd::rawDb detector representation AND
 * processor-side analysis cadence.
 *
 * analyzeRawFrames() exposes every frontend frame for representation witnesses.
 * analyze() returns only the frames that the live processor would submit to
 * AIEngine at ~10 Hz (before AIEngine's existing internal /3 throttle).
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include "../../AI/PerceptualFrontEnd.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace aieq_test
{

class LiveAIAnalysisPipeline
{
public:
    static constexpr int kFftSize = PerceptualFrontEnd::kFftSize;
    static constexpr int kNumBins = PerceptualFrontEnd::kNumBins;

    explicit LiveAIAnalysisPipeline(double sampleRate, int blockSize = 512)
        : sr(sampleRate), block(blockSize)
    {
    }

    std::vector<std::vector<float>> analyzeRawFrames(const juce::AudioBuffer<float>& audio)
    {
        const int n = audio.getNumSamples();
        const int ch = audio.getNumChannels();

        std::vector<float> mono(static_cast<size_t>(std::max(0, n)), 0.0f);
        for (int c = 0; c < ch; ++c)
        {
            const float* src = audio.getReadPointer(c);
            for (int i = 0; i < n; ++i)
                mono[static_cast<size_t>(i)] += src[i];
        }
        if (ch > 0)
            for (auto& v : mono)
                v /= static_cast<float>(ch);

        PerceptualFrontEnd frontEnd;
        frontEnd.prepare(sr);
        const auto frames = frontEnd.analyzeAll(mono.data(), n);

        std::vector<std::vector<float>> out;
        out.reserve(frames.size());
        for (const auto& frame : frames)
            out.push_back(frame.rawDb);
        return out;
    }

    std::vector<std::vector<float>> analyze(const juce::AudioBuffer<float>& audio)
    {
        const auto raw = analyzeRawFrames(audio);
        std::vector<std::vector<float>> selected;
        selected.reserve(raw.size());

        const auto interval = static_cast<juce::int64>(std::max(
            static_cast<int>(std::lround(sr * 0.1)), block));
        if (interval <= 0)
            return selected;

        juce::int64 frameEnd = kFftSize;
        juce::int64 nextAnalysis = interval;

        for (const auto& frame : raw)
        {
            if (frameEnd >= nextAnalysis)
            {
                selected.push_back(frame);
                do
                    nextAnalysis += interval;
                while (nextAnalysis <= frameEnd);
            }
            frameEnd += PerceptualFrontEnd::kHopSize;
        }
        return selected;
    }

private:
    double sr = 48000.0;
    int block = 512;
};

} // namespace aieq_test
