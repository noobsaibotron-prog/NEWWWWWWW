#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

/**
 * Single-producer / single-consumer latest-value ownership mailbox.
 *
 * The producer writes only slots it owns in WRITING state and publishes with a
 * release transition to READY. The consumer must win READY->READING before
 * dereferencing the payload, then explicitly releases the slot to FREE.
 *
 * READY slots are reclaimable by the producer: if the consumer is behind,
 * intermediate values may be dropped, but a READING slot is never writable.
 * This is appropriate for control data such as a newly built IR where newest
 * state wins and the audio thread must never block.
 *
 * Why ownership rather than paired indices: a protocol that claims a buffer
 * with one atomic and pins it with a second leaves a window between the two in
 * which the producer still sees the slot as free. Here the claim and the pin
 * are the same compare-exchange, so the window does not exist.
 */
template <typename Payload, std::size_t SlotCount = 4>
class LatestValueMailbox
{
    static_assert(SlotCount >= 3, "LatestValueMailbox needs at least three slots");
    static_assert(std::is_trivially_copyable_v<Payload>,
                  "LatestValueMailbox payload must be a fixed-size trivially-copyable value");

public:
    enum class SlotState : std::uint8_t
    {
        free = 0,
        writing,
        ready,
        reading
    };

    struct ReadView
    {
        const Payload* payload = nullptr;
        int slotIndex = -1;
        std::uint64_t sequence = 0;

        explicit operator bool() const noexcept { return payload != nullptr; }
    };

    LatestValueMailbox() = default;

    /** Single producer. Bounded, allocation-free, never waits for consumer. */
    bool publish(const Payload& value) noexcept
    {
        int writeIndex = -1;

        // Prefer FREE. If the producer outruns the consumer, replace an
        // obsolete READY value. READY->WRITING races safely with the
        // consumer's READY->READING CAS; exactly one side wins ownership.
        for (const auto desired : { SlotState::free, SlotState::ready })
        {
            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                auto expected = desired;
                if (slots[i].state.compare_exchange_strong(
                        expected, SlotState::writing,
                        std::memory_order_acquire, std::memory_order_relaxed))
                {
                    writeIndex = static_cast<int>(i);
                    break;
                }
            }
            if (writeIndex >= 0)
                break;
        }

        // Under the documented SPSC contract the consumer can own at most one
        // READING slot and the producer can own at most this one WRITING slot,
        // leaving at least one FREE/READY slot in a four-slot mailbox.
        if (writeIndex < 0)
            return false;

        auto& slot = slots[static_cast<std::size_t>(writeIndex)];
        slot.payload = value;
        const auto seq = nextSequence.fetch_add(1, std::memory_order_relaxed) + 1;
        slot.sequence.store(seq, std::memory_order_relaxed);
        slot.state.store(SlotState::ready, std::memory_order_release);
        return true;
    }

    /** Single consumer. Acquire the newest READY value newer than the last
        successfully acquired sequence. The returned payload remains immutable
        until release(view) is called.

        Retries are bounded because the intended consumer is the audio thread.
        Losing a race to the producer means a newer value is on its way, so
        giving up and returning an empty view costs at most one block of
        latency on a latest-wins channel — whereas an unbounded retry loop in
        an audio callback is a deadline hazard even when it almost always
        terminates immediately. */
    ReadView acquireLatest() noexcept
    {
        constexpr int maxAttempts = 8;

        for (int attempt = 0; attempt < maxAttempts; ++attempt)
        {
            int candidateIndex = -1;
            std::uint64_t candidateSequence = lastAcquiredSequence;

            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                auto& slot = slots[i];
                if (slot.state.load(std::memory_order_acquire) != SlotState::ready)
                    continue;

                const auto seq = slot.sequence.load(std::memory_order_relaxed);
                if (seq > candidateSequence)
                {
                    candidateSequence = seq;
                    candidateIndex = static_cast<int>(i);
                }
            }

            if (candidateIndex < 0)
                return {};

            auto& candidate = slots[static_cast<std::size_t>(candidateIndex)];
            auto expected = SlotState::ready;
            if (!candidate.state.compare_exchange_strong(
                    expected, SlotState::reading,
                    std::memory_order_acquire, std::memory_order_relaxed))
            {
                // Producer reclaimed it between scan and claim. Rescan.
                continue;
            }

            // Sequence is authoritative only after ownership. A producer may
            // have reclaimed/re-published the candidate between scan and CAS;
            // that newer payload is safe and preferable.
            const auto acquiredSequence = candidate.sequence.load(std::memory_order_relaxed);
            if (acquiredSequence <= lastAcquiredSequence)
            {
                candidate.state.store(SlotState::free, std::memory_order_release);
                continue;
            }

            lastAcquiredSequence = acquiredSequence;
            return { &candidate.payload, candidateIndex, acquiredSequence };
        }

        return {};
    }

    /** Single consumer. Release a previously acquired view. */
    void release(ReadView& view) noexcept
    {
        if (view.slotIndex >= 0)
        {
            auto& slot = slots[static_cast<std::size_t>(view.slotIndex)];
            auto expected = SlotState::reading;
            (void) slot.state.compare_exchange_strong(
                expected, SlotState::free,
                std::memory_order_release, std::memory_order_relaxed);
        }
        view = {};
    }

    /** Lifecycle-only reset. Must not race publish/acquire. */
    void reset() noexcept
    {
        for (auto& slot : slots)
        {
            slot.sequence.store(0, std::memory_order_relaxed);
            slot.state.store(SlotState::free, std::memory_order_relaxed);
        }
        nextSequence.store(0, std::memory_order_relaxed);
        lastAcquiredSequence = 0;
    }

    std::uint64_t publishedSequence() const noexcept
    {
        return nextSequence.load(std::memory_order_relaxed);
    }

    std::uint64_t consumedSequence() const noexcept
    {
        return lastAcquiredSequence;
    }

private:
    struct Slot
    {
        Payload payload {};
        std::atomic<std::uint64_t> sequence { 0 };
        std::atomic<SlotState> state { SlotState::free };
    };

    std::array<Slot, SlotCount> slots {};
    std::atomic<std::uint64_t> nextSequence { 0 };
    std::uint64_t lastAcquiredSequence = 0; // consumer-thread only

    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Mailbox sequencing requires lock-free uint64 atomics");
    static_assert(std::atomic<SlotState>::is_always_lock_free,
                  "Mailbox ownership requires lock-free state atomics");
};
