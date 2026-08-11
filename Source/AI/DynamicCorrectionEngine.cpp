#include "DynamicCorrectionEngine.h"

void DynamicCorrectionEngine::prepare(double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    sr = (sampleRate > 0.0) ? sampleRate : 44100.0;
    channels = juce::jlimit(1, kMaxChannels, numChannels);
    reset();
}

void DynamicCorrectionEngine::reset()
{
    for (auto& slot : slots)
    {
        for (auto& st : slot.detectorState) st = {};
        for (auto& st : slot.applyState) st = {};
        slot.envelope = 0.0f;
        slot.gainDb = 0.0f;
        slot.appliedGainDb = 1.0e9f;
    }
    // Force the currently owned snapshot (if any) to rebuild the audio-owned
    // filter state on the next block. The mailbox itself is deliberately not
    // cleared: reset/prepare must not silently discard an already published
    // correction set.
    lastAppliedSnapshotSequence = 0;
    for (auto& g : currentGainDb)
        g.store(0.0f, std::memory_order_relaxed);
}

void DynamicCorrectionEngine::publishCorrections(const Snapshot& snapshot)
{
    int writeIndex = -1;

    // Prefer a genuinely free slot. If the producer outruns the audio thread,
    // reclaim an unconsumed READY slot. READY->WRITING races safely against
    // the consumer's READY->READING CAS: exactly one side can own the slot.
    for (const auto desired : { SnapshotSlotState::free, SnapshotSlotState::ready })
    {
        for (size_t i = 0; i < snapshotSlots.size(); ++i)
        {
            auto expected = desired;
            if (snapshotSlots[i].state.compare_exchange_strong(
                    expected, SnapshotSlotState::writing,
                    std::memory_order_acquire, std::memory_order_relaxed))
            {
                writeIndex = static_cast<int>(i);
                break;
            }
        }
        if (writeIndex >= 0)
            break;
    }

    // Under the documented single-writer contract this is unreachable: the
    // audio thread can own only one of four slots, leaving at least one FREE or
    // READY slot. Fail closed if the contract is violated by multiple writers.
    if (writeIndex < 0)
    {
        jassertfalse;
        return;
    }

    auto& slot = snapshotSlots[static_cast<size_t>(writeIndex)];
    slot.snapshot = snapshot; // fixed-size struct copy — no allocation
    slot.snapshot.numActive = juce::jlimit(0, kMaxCorrections, snapshot.numActive);
    const auto sequence = nextPublicationSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    slot.sequence.store(sequence, std::memory_order_relaxed);
    slot.state.store(SnapshotSlotState::ready, std::memory_order_release);
}

const DynamicCorrectionEngine::Snapshot*
DynamicCorrectionEngine::acquireLatestSnapshot() noexcept
{
    int candidateIndex = -1;
    uint64_t candidateSequence = currentSnapshotSequence;

    for (size_t i = 0; i < snapshotSlots.size(); ++i)
    {
        auto& slot = snapshotSlots[i];
        if (slot.state.load(std::memory_order_acquire) != SnapshotSlotState::ready)
            continue;

        const auto sequence = slot.sequence.load(std::memory_order_relaxed);
        if (sequence > candidateSequence)
        {
            candidateSequence = sequence;
            candidateIndex = static_cast<int>(i);
        }
    }

    if (candidateIndex >= 0)
    {
        auto& candidate = snapshotSlots[static_cast<size_t>(candidateIndex)];
        auto expected = SnapshotSlotState::ready;
        if (candidate.state.compare_exchange_strong(
                expected, SnapshotSlotState::reading,
                std::memory_order_acquire, std::memory_order_relaxed))
        {
            // The producer may have reclaimed and republished this slot between
            // our scan and CAS. Read the authoritative sequence only after we
            // own it; any newer payload is still safe and preferable.
            const auto acquiredSequence = candidate.sequence.load(std::memory_order_relaxed);
            if (acquiredSequence > currentSnapshotSequence)
            {
                const int previousIndex = audioSnapshotIndex;
                audioSnapshotIndex = candidateIndex;
                currentSnapshotSequence = acquiredSequence;

                if (previousIndex >= 0 && previousIndex != candidateIndex)
                    snapshotSlots[static_cast<size_t>(previousIndex)].state.store(
                        SnapshotSlotState::free, std::memory_order_release);
            }
            else
            {
                candidate.state.store(SnapshotSlotState::free, std::memory_order_release);
            }
        }
    }

    if (audioSnapshotIndex < 0)
        return nullptr;

    return &snapshotSlots[static_cast<size_t>(audioSnapshotIndex)].snapshot;
}

DynamicCorrectionEngine::BiquadCoeffs
DynamicCorrectionEngine::makeBandpass(float freq, float q, double sampleRate) noexcept
{
    BiquadCoeffs c;
    const float omega = juce::jlimit(1.0e-4f, juce::MathConstants<float>::pi * 0.99f,
        2.0f * juce::MathConstants<float>::pi * freq / static_cast<float>(sampleRate));
    const float sinW = std::sin(omega);
    const float cosW = std::cos(omega);
    const float alpha = sinW / (2.0f * juce::jlimit(0.1f, 40.0f, q));

    // RBJ constant-skirt-gain bandpass (peak gain = Q)
    const float a0 = 1.0f + alpha;
    c.b0 = (sinW * 0.5f) / a0;
    c.b1 = 0.0f;
    c.b2 = -(sinW * 0.5f) / a0;
    c.a1 = (-2.0f * cosW) / a0;
    c.a2 = (1.0f - alpha) / a0;
    return c;
}

DynamicCorrectionEngine::BiquadCoeffs
DynamicCorrectionEngine::makePeak(float freq, float q, float gainDb, double sampleRate) noexcept
{
    BiquadCoeffs c;
    const float omega = juce::jlimit(1.0e-4f, juce::MathConstants<float>::pi * 0.99f,
        2.0f * juce::MathConstants<float>::pi * freq / static_cast<float>(sampleRate));
    const float sinW = std::sin(omega);
    const float cosW = std::cos(omega);
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float alpha = sinW / (2.0f * juce::jlimit(0.1f, 40.0f, q));

    const float a0 = 1.0f + alpha / A;
    c.b0 = (1.0f + alpha * A) / a0;
    c.b1 = (-2.0f * cosW) / a0;
    c.b2 = (1.0f - alpha * A) / a0;
    c.a1 = (-2.0f * cosW) / a0;
    c.a2 = (1.0f - alpha / A) / a0;
    return c;
}

void DynamicCorrectionEngine::rebuildSlotFromParams(int slotIndex, const CorrectionParams& params) noexcept
{
    auto& slot = slots[static_cast<size_t>(slotIndex)];
    slot.detectorCoeffs = makeBandpass(params.frequencyHz, params.q, sr);
    for (auto& st : slot.detectorState) st = {};
    // apply filter is rebuilt lazily by the gain-slew logic (appliedGainDb sentinel)
    slot.appliedGainDb = 1.0e9f;
    slot.envelope = 0.0f;
    slot.gainDb = 0.0f;

    const float atkMs = juce::jlimit(0.1f, 500.0f, params.attackMs);
    const float relMs = juce::jlimit(1.0f, 2000.0f, params.releaseMs);
    slot.attackCoeff  = 1.0f - std::exp(-1.0f / (atkMs  * 0.001f * static_cast<float>(sr)));
    slot.releaseCoeff = 1.0f - std::exp(-1.0f / (relMs * 0.001f * static_cast<float>(sr)));
}

void DynamicCorrectionEngine::process(juce::AudioBuffer<float>& buffer) noexcept
{
    if (!enabled.load(std::memory_order_relaxed))
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), channels);
    if (numSamples <= 0 || numChannels <= 0)
        return;

    // Acquire ownership of at most one immutable snapshot for this block.
    const auto* acquiredSnapshot = acquireLatestSnapshot();
    if (acquiredSnapshot == nullptr)
        return;
    const auto& snap = *acquiredSnapshot;

    if (currentSnapshotSequence != lastAppliedSnapshotSequence)
    {
        for (int i = 0; i < snap.numActive; ++i)
            rebuildSlotFromParams(i, snap.corrections[static_cast<size_t>(i)]);
        for (int i = snap.numActive; i < kMaxCorrections; ++i)
            currentGainDb[static_cast<size_t>(i)].store(0.0f, std::memory_order_relaxed);
        lastAppliedSnapshotSequence = currentSnapshotSequence;
        activeVersion.store(snap.version, std::memory_order_relaxed);
    }

    for (int corrIdx = 0; corrIdx < snap.numActive; ++corrIdx)
    {
        const auto& params = snap.corrections[static_cast<size_t>(corrIdx)];
        if (!params.enabled)
            continue;

        auto& slot = slots[static_cast<size_t>(corrIdx)];

        float targetCutDb = 0.0f;

        if (!params.dynamic)
        {
            targetCutDb = -std::abs(params.maxCutDb);
        }
        else
        {
            // ── envelope detection on the band-passed signal (max over channels) ──
            float env = slot.envelope;
            const auto& dc = slot.detectorCoeffs;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* data = buffer.getReadPointer(ch);
                auto& st = slot.detectorState[static_cast<size_t>(ch)];
                float z1 = st.z1, z2 = st.z2;
                for (int i = 0; i < numSamples; ++i)
                {
                    const float x = data[i];
                    const float y = dc.b0 * x + z1;
                    z1 = dc.b1 * x - dc.a1 * y + z2;
                    z2 = dc.b2 * x - dc.a2 * y;
                    const float rect = std::abs(y);
                    const float coeff = rect > env ? slot.attackCoeff : slot.releaseCoeff;
                    env += coeff * (rect - env);
                }
                st.z1 = z1;
                st.z2 = z2;
            }
            // NaN hygiene at the state boundary (AIEngine biquad lesson):
            // a non-finite envelope would latch forever.
            if (!std::isfinite(env))
                env = 0.0f;
            slot.envelope = env;

            const float envDb = juce::Decibels::gainToDecibels(env, -100.0f);
            const float overshootDb = envDb - params.thresholdDb;
            if (overshootDb > 0.0f)
            {
                const float ratio = juce::jmax(1.01f, params.ratio);
                const float cut = overshootDb * (1.0f - 1.0f / ratio);
                targetCutDb = -juce::jmin(cut, std::abs(params.maxCutDb));
            }
        }

        // ── block-rate gain slew (bounded zipper) ──
        const float step = juce::jlimit(-kMaxGainStepDbPerBlock, kMaxGainStepDbPerBlock,
                                        targetCutDb - slot.gainDb);
        slot.gainDb += step;
        if (std::abs(slot.gainDb) < 0.05f)
            slot.gainDb = juce::jmin(0.0f, slot.gainDb);

        currentGainDb[static_cast<size_t>(corrIdx)].store(slot.gainDb, std::memory_order_relaxed);

        if (slot.gainDb > -0.05f)
        {
            // effectively transparent: skip filtering entirely (also lets the
            // apply-filter state relax on the next active block via rebuild)
            continue;
        }

        // rebuild apply filter only when the smoothed gain moved meaningfully
        if (std::abs(slot.gainDb - slot.appliedGainDb) > 0.05f)
        {
            slot.applyCoeffs = makePeak(params.frequencyHz, params.q, slot.gainDb, sr);
            slot.appliedGainDb = slot.gainDb;
        }

        const auto& ac = slot.applyCoeffs;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            auto& st = slot.applyState[static_cast<size_t>(ch)];
            float z1 = st.z1, z2 = st.z2;
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = data[i];
                const float y = ac.b0 * x + z1;
                z1 = ac.b1 * x - ac.a1 * y + z2;
                z2 = ac.b2 * x - ac.a2 * y;
                data[i] = y;
            }
            // NaN hygiene: never let a poisoned biquad state persist
            if (!std::isfinite(z1) || !std::isfinite(z2))
                z1 = z2 = 0.0f;
            st.z1 = z1;
            st.z2 = z2;
        }
    }
}
