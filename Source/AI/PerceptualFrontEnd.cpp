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

    // P2C3 — LF (8192) path.
    lfFft = std::make_unique<juce::dsp::FFT>(kLfFftOrder);
    lfWindow = std::make_unique<juce::dsp::WindowingFunction<float>>(
        static_cast<size_t>(kLfFftSize),
        juce::dsp::WindowingFunction<float>::hann);
    lfPending.assign(static_cast<size_t>(kLfFftSize) * 4, 0.0f);
    lfFftData.assign(static_cast<size_t>(kLfFftSize) * 2, 0.0f);
    lfOverlap.assign(static_cast<size_t>(kLfHopSize), 0.0f);
    lfRawDb.assign(static_cast<size_t>(kLfNumBins), kMinDb);

    buildBandTables();
    work.bandDb.assign(bandCentersHz.size(), kMinDb);
    work.bandDbFused.assign(bandCentersHz.size(), kMinDb);
    work.salienceDb.assign(bandCentersHz.size(), kMinDb);
    lfBandDb.assign(bandCentersHz.size(), kMinDb);
    prevRawDb.assign(static_cast<size_t>(kNumBins), kMinDb);

    isPrepared = true;
    reset();
}

void PerceptualFrontEnd::reset()
{
    pendingCount = 0;
    primed = false;
    std::fill(overlap.begin(), overlap.end(), 0.0f);
    lfPendingCount = 0;
    lfPrimed = false;
    lfHasFrame = false;
    std::fill(lfOverlap.begin(), lfOverlap.end(), 0.0f);
    std::fill(lfBandDb.begin(), lfBandDb.end(), kMinDb);
    hasPrevRaw = false;
    frameCount = 0;
    totalNs = 0;
    maxNs = 0;
    lfFrameCount = 0;
    lfTotalNs = 0;
    lfMaxNs = 0;
}

float PerceptualFrontEnd::interpLoudnessWeightDb(float hz)
{
    // Fixed ISO-226-like equal-loudness weighting (~75 phon), anchor table
    // interpolated in log-frequency. APPROXIMATION for salience ranking, not
    // metrology: it makes "equally loud-sounding" bands comparable, so a -30 dB
    // band at 3 kHz can outrank a -25 dB band at 40 Hz, as it does to the ear.
    struct Anchor { float hz, wDb; };
    static constexpr Anchor table[] = {
        {   20.0f, -31.0f }, {   31.5f, -26.0f }, {   63.0f, -16.5f },
        {  125.0f,  -9.5f }, {  250.0f,  -4.5f }, {  500.0f,  -1.5f },
        { 1000.0f,   0.0f }, { 2000.0f,   1.5f }, { 3150.0f,   3.0f },
        { 4000.0f,   3.5f }, { 6300.0f,   1.5f }, { 8000.0f,   0.0f },
        {10000.0f,  -1.5f }, {12500.0f,  -3.0f }, {16000.0f,  -7.0f },
        {20000.0f, -12.0f }
    };
    constexpr int n = static_cast<int>(std::size(table));
    if (hz <= table[0].hz)      return table[0].wDb;
    if (hz >= table[n - 1].hz)  return table[n - 1].wDb;
    for (int i = 1; i < n; ++i)
    {
        if (hz <= table[i].hz)
        {
            const float t = (std::log2(hz) - std::log2(table[i - 1].hz))
                          / (std::log2(table[i].hz) - std::log2(table[i - 1].hz));
            return table[i - 1].wDb + t * (table[i].wDb - table[i - 1].wDb);
        }
    }
    return 0.0f;
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

    // P2C3 — equal-loudness weight per band.
    bandLoudnessWeightDb.resize(nb);
    for (size_t b = 0; b < nb; ++b)
        bandLoudnessWeightDb[b] = interpLoudnessWeightDb(bandCentersHz[b]);

    // P2C3 — LF-band mapping on the 8192 grid (bands below kLfCutoverHz only).
    const float lfBinHz = static_cast<float>(sr) / static_cast<float>(kLfFftSize);
    numLfBands = 0;
    while (numLfBands < static_cast<int>(nb)
           && bandCentersHz[static_cast<size_t>(numLfBands)] < kLfCutoverHz)
        ++numLfBands;

    lfBandBinStart.assign(static_cast<size_t>(numLfBands), 1);
    lfBandBinEnd.assign(static_cast<size_t>(numLfBands), 1);
    lfBandWeights.assign(static_cast<size_t>(numLfBands), {});
    lfBandWeightSum.assign(static_cast<size_t>(numLfBands), 1.0f);

    for (int bi = 0; bi < numLfBands; ++bi)
    {
        const size_t b = static_cast<size_t>(bi);
        const float fc = bandCentersHz[b];
        const float fl = (b > 0)      ? bandCentersHz[b - 1] : fc / step;
        const float fh = (b + 1 < nb) ? bandCentersHz[b + 1] : fc * step;

        const int loBin = juce::jlimit(1, kLfNumBins - 1, static_cast<int>(std::floor(fl / lfBinHz)));
        const int hiBin = juce::jlimit(1, kLfNumBins - 1, static_cast<int>(std::ceil (fh / lfBinHz)));
        lfBandBinStart[b] = loBin;
        lfBandBinEnd[b]   = hiBin;

        auto& w = lfBandWeights[b];
        w.assign(static_cast<size_t>(hiBin - loBin + 1), 0.0f);
        float sum = 0.0f;
        const float logFl = std::log2(fl), logFc = std::log2(fc), logFh = std::log2(fh);
        for (int bin = loBin; bin <= hiBin; ++bin)
        {
            const float f = static_cast<float>(bin) * lfBinHz;
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
        if (sum <= 0.0f)
        {
            const int nearest = juce::jlimit(loBin, hiBin,
                static_cast<int>(std::round(fc / lfBinHz)));
            w[static_cast<size_t>(nearest - loBin)] = 1.0f;
            sum = 1.0f;
        }
        lfBandWeightSum[b] = sum;
    }
}

void PerceptualFrontEnd::pushMono(const float* samples, int numSamples,
                                  const std::function<void(const Frame&)>& onFrame)
{
    // prepare() is mandatory: without it `pending` is empty and the loop below
    // could spin forever (take == 0, consumed never advances). Hard no-op + assert.
    if (!isPrepared || pending.empty() || samples == nullptr || numSamples <= 0)
    {
        jassert(isPrepared && "PerceptualFrontEnd::pushMono called before prepare()");
        return;
    }

    // P2C3: the LF (8192) path consumes the same mono stream in parallel.
    pushLfMono(samples, numSamples);

    int consumed = 0;
    while (consumed < numSamples)
    {
        const int space = static_cast<int>(pending.size()) - pendingCount;
        const int take  = juce::jmin(space, numSamples - consumed);
        if (take <= 0)
        {
            // Defensive: accumulation buffer full and no frame produced — cannot
            // happen with the sizes set in prepare(), but never spin.
            jassertfalse;
            return;
        }
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

void PerceptualFrontEnd::pushLfMono(const float* samples, int numSamples)
{
    int consumed = 0;
    while (consumed < numSamples)
    {
        const int space = static_cast<int>(lfPending.size()) - lfPendingCount;
        const int take  = juce::jmin(space, numSamples - consumed);
        if (take <= 0) { jassertfalse; return; }
        std::copy(samples + consumed, samples + consumed + take,
                  lfPending.begin() + lfPendingCount);
        lfPendingCount += take;
        consumed += take;

        while ((!lfPrimed && lfPendingCount >= kLfFftSize)
               || (lfPrimed && lfPendingCount >= kLfHopSize))
        {
            processOneLfFrame();
        }
    }
}

void PerceptualFrontEnd::processOneLfFrame()
{
    const auto t0 = juce::Time::getHighResolutionTicks();

    if (!lfPrimed)
    {
        std::copy(lfPending.begin(), lfPending.begin() + kLfFftSize, lfFftData.begin());
        std::copy(lfPending.begin() + kLfHopSize, lfPending.begin() + kLfFftSize, lfOverlap.begin());
        std::copy(lfPending.begin() + kLfFftSize, lfPending.begin() + lfPendingCount, lfPending.begin());
        lfPendingCount -= kLfFftSize;
        lfPrimed = true;
    }
    else
    {
        std::copy(lfOverlap.begin(), lfOverlap.end(), lfFftData.begin());
        std::copy(lfPending.begin(), lfPending.begin() + kLfHopSize, lfFftData.begin() + kLfHopSize);
        std::copy(lfFftData.begin() + kLfHopSize, lfFftData.begin() + kLfFftSize, lfOverlap.begin());
        std::copy(lfPending.begin() + kLfHopSize, lfPending.begin() + lfPendingCount, lfPending.begin());
        lfPendingCount -= kLfHopSize;
    }

    std::fill(lfFftData.begin() + kLfFftSize, lfFftData.end(), 0.0f);
    lfWindow->multiplyWithWindowingTable(lfFftData.data(), static_cast<size_t>(kLfFftSize));
    lfFft->performFrequencyOnlyForwardTransform(lfFftData.data());

    for (int i = 0; i < kLfNumBins; ++i)
    {
        float mag = lfFftData[static_cast<size_t>(i)] / static_cast<float>(kLfFftSize);
        mag = std::max(mag, 1.0e-10f);
        lfRawDb[static_cast<size_t>(i)] =
            juce::jlimit(kMinDb, kMaxDb, 20.0f * std::log10(mag));
    }

    for (int bi = 0; bi < numLfBands; ++bi)
    {
        const size_t b = static_cast<size_t>(bi);
        const auto& w = lfBandWeights[b];
        float acc = 0.0f;
        for (int bin = lfBandBinStart[b]; bin <= lfBandBinEnd[b]; ++bin)
            acc += lfRawDb[static_cast<size_t>(bin)] * w[static_cast<size_t>(bin - lfBandBinStart[b])];
        lfBandDb[b] = acc / lfBandWeightSum[b];
    }
    lfHasFrame = true;

    const auto t1 = juce::Time::getHighResolutionTicks();
    const auto ns = juce::int64(juce::Time::highResolutionTicksToSeconds(t1 - t0) * 1.0e9);
    ++lfFrameCount;
    lfTotalNs += ns;
    lfMaxNs = std::max(lfMaxNs, ns);
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

    // P2C3 — spectral flux: mean positive per-bin dB delta vs the previous RAW
    // frame (possible at all only because frames are unsmoothed). ~0 on steady
    // material; spikes on transients/onsets. Computed BEFORE prevRawDb update.
    if (hasPrevRaw)
    {
        float acc = 0.0f;
        for (int i = 0; i < kNumBins; ++i)
        {
            const float d = work.rawDb[static_cast<size_t>(i)] - prevRawDb[static_cast<size_t>(i)];
            if (d > 0.0f)
                acc += d;
        }
        work.fluxDb = acc / static_cast<float>(kNumBins);
    }
    else
    {
        work.fluxDb = 0.0f; // first frame: no reference
    }
    std::copy(work.rawDb.begin(), work.rawDb.end(), prevRawDb.begin());
    hasPrevRaw = true;

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

    // P2C3 — LF fusion + equal-loudness salience. LF bands use the latest 8192
    // spectrum (twice the LF resolution); the LF frame lags by at most one LF
    // hop (~85 ms) — acceptable for band STATISTICS, documented for consumers.
    work.lfValid = lfHasFrame;
    for (size_t b = 0; b < nb; ++b)
    {
        const bool useLf = lfHasFrame && static_cast<int>(b) < numLfBands;
        work.bandDbFused[b] = useLf ? lfBandDb[b] : work.bandDb[b];
        work.salienceDb[b]  = work.bandDbFused[b] + bandLoudnessWeightDb[b];
    }

    const auto t1 = juce::Time::getHighResolutionTicks();
    const auto ns = juce::int64(juce::Time::highResolutionTicksToSeconds(t1 - t0) * 1.0e9);
    ++frameCount;
    totalNs += ns;
    maxNs = std::max(maxNs, ns);

    if (onFrame)
        onFrame(work);
}
