#include "PerceptualFrontEnd.h"

void PerceptualFrontEnd::prepare(double sampleRate)
{
    sr = sampleRate;
    fft = std::make_unique<juce::dsp::FFT>(kFftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
        static_cast<size_t>(kFftSize),
        juce::dsp::WindowingFunction<float>::hann);

    pending.assign(static_cast<size_t>(kFftSize) * 4, 0.0f); // headroom: 2 hops + slack
    fftData.assign(static_cast<size_t>(kFftSize) * 2, 0.0f);
    overlap.assign(static_cast<size_t>(kHopSize), 0.0f);
    work.rawDb.assign(static_cast<size_t>(kNumBins), kMinDb);

    buildBandTables();
    work.bandDb.assign(bandCentersHz.size(), kMinDb);
    reset();
}

void PerceptualFrontEnd::reset()
{
    pendingCount = 0;
    primed = false;
    std::fill(overlap.begin(), overlap.end(), 0.0f);
    frameCount = 0;
    totalNs = 0;
    maxNs = 0;
}

void PerceptualFrontEnd::buildBandTables()
{
    bandCentersHz.clear();
    bandBinStart.clear();
    bandBinEnd.clear();
    bandWeights.clear();
    bandWeightSum.clear();

    const float nyquist = static_cast<float>(sr) * 0.5f;
    const float binHz   = static_cast<float>(sr) / static_cast<float>(kFftSize);
    const float step    = std::pow(2.0f, 1.0f / static_cast<float>(kBandsPerOctave));

    // Band centers: 12/octave from kMinBandHz up to just below Nyquist.
    for (float fc = kMinBandHz; fc < nyquist / step; fc *= step)
        bandCentersHz.push_back(fc);

    const size_t nb = bandCentersHz.size();
    bandBinStart.resize(nb);
    bandBinEnd.resize(nb);
    bandWeights.resize(nb);
    bandWeightSum.resize(nb);

    // Triangular weights on the LOG-frequency axis between neighboring centers
    // (same construction pattern as MLEngine::extractMelBands, log-f instead of mel).
    for (size_t b = 0; b < nb; ++b)
    {
        const float fc = bandCentersHz[b];
        const float fl = (b > 0)      ? bandCentersHz[b - 1] : fc / step;
        const float fh = (b + 1 < nb) ? bandCentersHz[b + 1] : juce::jmin(fc * step, nyquist);

        const int loBin = juce::jlimit(1, kNumBins - 1, static_cast<int>(std::floor(fl / binHz)));
        const int hiBin = juce::jlimit(1, kNumBins - 1, static_cast<int>(std::ceil (fh / binHz)));
        bandBinStart[b] = loBin;
        bandBinEnd[b]   = hiBin;

        auto& w = bandWeights[b];
        w.assign(static_cast<size_t>(hiBin - loBin + 1), 0.0f);
        float sum = 0.0f;
        const float logFl = std::log2(fl), logFc = std::log2(fc), logFh = std::log2(fh);
        for (int bin = loBin; bin <= hiBin; ++bin)
        {
            const float f = static_cast<float>(bin) * binHz;
            if (f <= 0.0f) continue;
            const float lf = std::log2(f);
            float weight = 0.0f;
            if (lf >= logFl && lf <= logFc && logFc > logFl)
                weight = (lf - logFl) / (logFc - logFl);
            else if (lf > logFc && lf <= logFh && logFh > logFc)
                weight = (logFh - lf) / (logFh - logFc);
            w[static_cast<size_t>(bin - loBin)] = weight;
            sum += weight;
        }
        // Narrow LF bands can cover <2 bins: fall back to nearest-bin pickup so no
        // band is ever empty.
        if (sum <= 0.0f)
        {
            const int nearest = juce::jlimit(loBin, hiBin,
                static_cast<int>(std::round(fc / binHz)));
            w[static_cast<size_t>(nearest - loBin)] = 1.0f;
            sum = 1.0f;
        }
        bandWeightSum[b] = sum;
    }
}

void PerceptualFrontEnd::pushMono(const float* samples, int numSamples,
                                  const std::function<void(const Frame&)>& onFrame)
{
    int consumed = 0;
    while (consumed < numSamples)
    {
        const int space = static_cast<int>(pending.size()) - pendingCount;
        const int take  = juce::jmin(space, numSamples - consumed);
        std::copy(samples + consumed, samples + consumed + take,
                  pending.begin() + pendingCount);
        pendingCount += take;
        consumed += take;

        // Produce as many frames as the accumulated samples allow.
        while ((!primed && pendingCount >= kFftSize)
               || (primed && pendingCount >= kHopSize))
        {
            processOneFrame(onFrame);
        }
    }
}

std::vector<PerceptualFrontEnd::Frame> PerceptualFrontEnd::analyzeAll(const float* samples,
                                                                      int numSamples)
{
    std::vector<Frame> out;
    pushMono(samples, numSamples, [&out](const Frame& f) { out.push_back(f); });
    return out;
}

void PerceptualFrontEnd::processOneFrame(const std::function<void(const Frame&)>& onFrame)
{
    const auto t0 = juce::Time::getHighResolutionTicks();

    if (!primed)
    {
        std::copy(pending.begin(), pending.begin() + kFftSize, fftData.begin());
        std::copy(pending.begin() + kHopSize, pending.begin() + kFftSize, overlap.begin());
        // Compact the consumed kFftSize samples out of the pending buffer.
        std::copy(pending.begin() + kFftSize, pending.begin() + pendingCount, pending.begin());
        pendingCount -= kFftSize;
        primed = true;
    }
    else
    {
        std::copy(overlap.begin(), overlap.end(), fftData.begin());
        std::copy(pending.begin(), pending.begin() + kHopSize, fftData.begin() + kHopSize);
        std::copy(fftData.begin() + kHopSize, fftData.begin() + kFftSize, overlap.begin());
        std::copy(pending.begin() + kHopSize, pending.begin() + pendingCount, pending.begin());
        pendingCount -= kHopSize;
    }

    std::fill(fftData.begin() + kFftSize, fftData.end(), 0.0f);
    window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(kFftSize));
    fft->performFrequencyOnlyForwardTransform(fftData.data());

    // RAW dB bins: same normalization/clamp as the legacy path, NO ballistics.
    for (int i = 0; i < kNumBins; ++i)
    {
        float mag = fftData[static_cast<size_t>(i)] / static_cast<float>(kFftSize);
        mag = std::max(mag, 1.0e-10f);
        work.rawDb[static_cast<size_t>(i)] =
            juce::jlimit(kMinDb, kMaxDb, 20.0f * std::log10(mag));
    }

    // Log-band energies (weighted mean of the dB bins — perceptual axis).
    const size_t nb = bandCentersHz.size();
    for (size_t b = 0; b < nb; ++b)
    {
        const auto& w = bandWeights[b];
        float acc = 0.0f;
        for (int bin = bandBinStart[b]; bin <= bandBinEnd[b]; ++bin)
            acc += work.rawDb[static_cast<size_t>(bin)] * w[static_cast<size_t>(bin - bandBinStart[b])];
        work.bandDb[b] = acc / bandWeightSum[b];
    }

    const auto t1 = juce::Time::getHighResolutionTicks();
    const auto ns = juce::int64(juce::Time::highResolutionTicksToSeconds(t1 - t0) * 1.0e9);
    ++frameCount;
    totalNs += ns;
    maxNs = std::max(maxNs, ns);

    if (onFrame)
        onFrame(work);
}
