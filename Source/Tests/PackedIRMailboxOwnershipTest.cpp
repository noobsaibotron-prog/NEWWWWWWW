#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <latch>
#include <thread>

#include "../Core/LatestValueMailbox.h"

/**
 * EC-004 regression: the packed-IR handoff must transfer ownership atomically.
 *
 * The protocol this replaces claimed a buffer with readyIndex.exchange(-1) and
 * pinned it with a separate readingIndex.store(). In the window between those
 * two atomics the builder still saw the slot as reusable, so it could overwrite
 * an IR the audio thread was already copying. Here READY->READING is a single
 * compare-exchange, so a producer can drop an obsolete value but can never
 * mutate one the consumer owns.
 *
 * The payload is deliberately far smaller than the real 16,384-float packed IR:
 * the state machine is what is under test, and a small payload lets the
 * sanitizers run many more ownership transitions in the same time. Production
 * instantiates the same template with the real payload.
 *
 * Name note: the runner only executes tests whose name matches a prefix in
 * TestMain.cpp::isAieqProjectTestName(). "Packed IR Mailbox Ownership" matched
 * none and would have been compiled, linked and silently skipped.
 */
class PackedIRMailboxOwnershipTest : public juce::UnitTest
{
public:
    PackedIRMailboxOwnershipTest()
        : juce::UnitTest("LinearPhase Packed IR Mailbox Ownership", "ThreadSafety") {}

    struct Payload
    {
        std::uint64_t generation = 0;
        std::array<std::uint64_t, 32> signature {};
    };

    static constexpr std::uint64_t kSalt = 0x9E3779B97F4A7C15ull;

    static Payload make(std::uint64_t generation)
    {
        Payload p;
        p.generation = generation;
        for (std::size_t i = 0; i < p.signature.size(); ++i)
            p.signature[i] = generation ^ kSalt ^ static_cast<std::uint64_t>(i * 0x10001u);
        return p;
    }

    static bool intact(const Payload& p)
    {
        if (p.generation == 0)
            return false;
        for (std::size_t i = 0; i < p.signature.size(); ++i)
            if (p.signature[i] != (p.generation ^ kSalt ^ static_cast<std::uint64_t>(i * 0x10001u)))
                return false;
        return true;
    }

    void runTest() override
    {
        beginTest("A -> B -> C -> FINAL converges, and never on a stale state");
        {
            // The drag case, deterministic and readable. Losing intermediate
            // values is allowed — only the newest EQ curve matters. Staying on
            // an old one forever is not: that is a knob that stopped working.
            LatestValueMailbox<Payload, 4> mailbox;

            for (std::uint64_t generation : { 1ull, 2ull, 3ull })
                expect(mailbox.publish(make(generation)),
                       "producer could not claim a slot during the burst");

            auto view = mailbox.acquireLatest();
            expect(static_cast<bool>(view), "no value available after three publications");
            expect(intact(*view.payload), "consumer observed a torn payload");
            expectEquals(static_cast<juce::int64>(view.payload->generation),
                         static_cast<juce::int64>(3),
                         "consumer must land on the newest value, not the oldest queued one");
            mailbox.release(view);

            expect(!mailbox.acquireLatest(),
                   "a consumed generation must not be handed out again");

            expect(mailbox.publish(make(4)), "FINAL publication failed");
            view = mailbox.acquireLatest();
            expect(static_cast<bool>(view), "FINAL value never became visible");
            expectEquals(static_cast<juce::int64>(view.payload->generation),
                         static_cast<juce::int64>(4),
                         "mailbox did not converge to FINAL");
            mailbox.release(view);
        }

        beginTest("READY/READING ownership prevents write-while-read under contention");
        {
            LatestValueMailbox<Payload, 4> mailbox;
            constexpr std::uint64_t publishes = 200000;

            std::latch ready { 2 };
            std::latch start { 1 };
            std::atomic<bool> producerDone { false };
            std::atomic<std::uint64_t> lastSeen { 0 };
            std::atomic<std::uint64_t> corruptReads { 0 };
            std::atomic<std::uint64_t> publishFailures { 0 };

            std::thread producer([&]
            {
                ready.count_down();
                start.wait();
                for (std::uint64_t generation = 1; generation <= publishes; ++generation)
                {
                    if (!mailbox.publish(make(generation)))
                        publishFailures.fetch_add(1, std::memory_order_relaxed);

                    if ((generation & 0x3ffu) == 0)
                        std::this_thread::yield();
                }
                producerDone.store(true, std::memory_order_release);
            });

            std::thread consumer([&]
            {
                ready.count_down();
                start.wait();

                while (!producerDone.load(std::memory_order_acquire)
                       || lastSeen.load(std::memory_order_relaxed) < publishes)
                {
                    auto view = mailbox.acquireLatest();
                    if (!view)
                    {
                        std::this_thread::yield();
                        continue;
                    }

                    if (!intact(*view.payload))
                        corruptReads.fetch_add(1, std::memory_order_relaxed);

                    const auto generation = view.payload->generation;
                    if (generation > lastSeen.load(std::memory_order_relaxed))
                        lastSeen.store(generation, std::memory_order_relaxed);

                    mailbox.release(view);
                }
            });

            ready.wait();
            start.count_down();
            producer.join();
            consumer.join();

            logMessage("  published=" + juce::String(static_cast<juce::int64>(publishes))
                       + " lastConsumed=" + juce::String(static_cast<juce::int64>(lastSeen.load()))
                       + " corrupt=" + juce::String(static_cast<juce::int64>(corruptReads.load()))
                       + " publishFailures=" + juce::String(static_cast<juce::int64>(publishFailures.load())));

            expectEquals(static_cast<juce::int64>(corruptReads.load()), static_cast<juce::int64>(0),
                         "consumer observed a torn or reused payload");
            expectEquals(static_cast<juce::int64>(publishFailures.load()), static_cast<juce::int64>(0),
                         "single producer always finds a FREE or reclaimable READY slot");
            expectEquals(static_cast<juce::int64>(lastSeen.load()), static_cast<juce::int64>(publishes),
                         "latest-wins mailbox must converge to the final publication");
        }
    }
};

static PackedIRMailboxOwnershipTest packedIRMailboxOwnershipTest;
