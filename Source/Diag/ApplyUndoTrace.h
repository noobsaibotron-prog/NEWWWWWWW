#pragma once

/*  DIAGNOSTIC ONLY — Apply/Undo host-parameter trace for the Live experiment.

    Three channels, one global sequence counter, so the file gives a total
    order of everything the host and the plugin exchanged:

      SENT  b|p|e   plugin -> host  beginEdit / performEdit / endEdit
                    (message thread; JuceVST3EditController)
      RECV  c       host -> plugin  Param::setNormalized on the controller
                    (message thread; also records whether the host reports
                    "playing", because then the processor is NOT updated here)
                    flags bit0 = host playing; bit1 = SELF: the wrapper's own
                    EditController::setParamNormalized right before its
                    performEdit (JUCE "Cubase" workaround), NOT a host write
      RECV  q       host -> plugin  IParameterChanges drained in process()
                    (AUDIO THREAD: lock-free SPSC ring only, no allocation,
                    no mutex, no I/O; overflow is counted, never blocks)

    A message-thread timer drains both rings to a text file. Records carry the
    VST3 param id only; the file starts with a PARAM table id -> JUCE paramID
    so no string work happens on a hot path.

    Not part of the product: lives on the diag branch, never merged.           */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cstdint>

#ifndef EMBER_DIAG_SHA
 #define EMBER_DIAG_SHA "unknown"
#endif

namespace EmberDiag
{
enum class Kind : std::uint8_t { SentBegin = 1, SentPerform, SentEnd, RecvCtrl, RecvProc };

struct Record
{
    std::uint32_t seq;
    std::uint32_t paramId;
    float value;
    std::uint32_t tMs;
    std::uint8_t kind;
    std::uint8_t flags;       // bit0: host reported playing (RecvCtrl only)
    std::uint16_t offset;     // sample offset inside the block (RecvProc only)
};

/** Single-producer / single-consumer ring: one producer thread, drained by
    the message thread. Capacity is a power of two, fixed at compile time. */
template <std::size_t CapacityPow2>
class Ring
{
public:
    static_assert ((CapacityPow2 & (CapacityPow2 - 1)) == 0, "power of two");

    bool push (const Record& r) noexcept
    {
        const auto w = write.load (std::memory_order_relaxed);
        const auto rd = read.load (std::memory_order_acquire);
        if (w - rd >= CapacityPow2) { dropped.fetch_add (1, std::memory_order_relaxed); return false; }
        slots[w & (CapacityPow2 - 1)] = r;
        write.store (w + 1, std::memory_order_release);
        return true;
    }

    bool pop (Record& r) noexcept
    {
        const auto rd = read.load (std::memory_order_relaxed);
        if (rd == write.load (std::memory_order_acquire)) return false;
        r = slots[rd & (CapacityPow2 - 1)];
        read.store (rd + 1, std::memory_order_release);
        return true;
    }

    std::uint32_t droppedCount() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    std::array<Record, CapacityPow2> slots {};
    std::atomic<std::uint32_t> write { 0 }, read { 0 }, dropped { 0 };
};

class Trace final : private juce::Timer
{
public:
    static Trace& get() { static Trace t; return t; }

    /** Message thread. Idempotent. Writes the header and the id -> name table. */
    void start (const juce::AudioProcessor& processor,
                const std::function<std::uint32_t (int)>& vstIdForIndex)
    {
        if (started.exchange (true)) return;
        const auto dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                            .getChildFile ("Library/Logs/EmberCore");
        dir.createDirectory();
        file = dir.getChildFile ("apply-undo-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".log");
        out.reset (new juce::FileOutputStream (file));
        line ("HEADER diag_sha=" EMBER_DIAG_SHA " build=" __DATE__ " " __TIME__
              " plugin=" JucePlugin_Name " cols=seq,t_ms,chan,kind,id,value,flags,offset");
        const auto& params = processor.getParameters();
        for (int i = 0; i < params.size(); ++i)
        {
            juce::String id = "index" + juce::String (i);
            if (auto* withId = dynamic_cast<const juce::AudioProcessorParameterWithID*> (params[i]))
                id = withId->paramID;
            line ("PARAM " + juce::String ((juce::int64) vstIdForIndex (i)) + " " + id + " \"" + params[i]->getName (48) + "\"");
        }
        t0 = juce::Time::getMillisecondCounter();
        startTimer (10);
    }

    // --- producers ---------------------------------------------------------
    void sent (Kind k, std::uint32_t id, double v) noexcept          { msgRing.push (make (k, id, v, 0, 0)); }
    void recvCtrl (std::uint32_t id, double v, bool playing) noexcept
    {
        const std::uint8_t flags = (std::uint8_t) ((playing ? 1 : 0) | (selfWrite ? 2 : 0));
        msgRing.push (make (Kind::RecvCtrl, id, v, flags, 0));
    }

    /** Message thread: brackets the wrapper's own setParamNormalized call. */
    struct ScopedSelfWrite
    {
        ScopedSelfWrite() noexcept  { get().selfWrite = true; }
        ~ScopedSelfWrite() noexcept { get().selfWrite = false; }
    };
    void recvProc (std::uint32_t id, double v, int offset) noexcept   { audioRing.push (make (Kind::RecvProc, id, v, 0, (std::uint16_t) juce::jlimit (0, 65535, offset))); }

private:
    Trace() = default;
    ~Trace() override { stopTimer(); drain(); }

    Record make (Kind k, std::uint32_t id, double v, std::uint8_t flags, std::uint16_t offset) noexcept
    {
        Record r;
        r.seq = seq.fetch_add (1, std::memory_order_relaxed);
        r.paramId = id;
        r.value = (float) v;
        r.tMs = juce::Time::getMillisecondCounter() - t0;   // monotonic, no allocation
        r.kind = (std::uint8_t) k;
        r.flags = flags;
        r.offset = offset;
        return r;
    }

    void timerCallback() override { drain(); }

    void drain()
    {
        if (out == nullptr) return;
        Record r;
        // both rings, then re-sort by seq for the file: merge is cheap because
        // each ring is already in seq order and traffic is tiny.
        std::array<Record, 8192> buf; std::size_t n = 0;
        while (n < buf.size() && msgRing.pop (r)) buf[n++] = r;
        while (n < buf.size() && audioRing.pop (r)) buf[n++] = r;
        std::sort (buf.begin(), buf.begin() + (long) n, [] (const Record& a, const Record& b) { return a.seq < b.seq; });
        for (std::size_t i = 0; i < n; ++i) write (buf[i]);
        const auto d = audioRing.droppedCount();
        if (d != lastDropped) { line ("DROPPED audio_ring=" + juce::String ((int) d)); lastDropped = d; }
        out->flush();
    }

    void write (const Record& r)
    {
        static const char* kinds[] = { "?", "SENT b", "SENT p", "SENT e", "RECV c", "RECV q" };
        const char* k = r.kind >= 1 && r.kind <= 5 ? kinds[r.kind] : kinds[0];
        line (juce::String ((int) r.seq) + " " + juce::String ((int) r.tMs) + " " + k + " "
              + juce::String ((juce::int64) r.paramId) + " " + juce::String (r.value, 6)
              + " " + juce::String ((int) r.flags) + " " + juce::String ((int) r.offset));
    }

    void line (const juce::String& s) { if (out != nullptr) { out->writeText (s + "\n", false, false, nullptr); } }

    std::atomic<bool> started { false };
    bool selfWrite = false;   // message thread only
    std::atomic<std::uint32_t> seq { 0 };
    std::uint32_t t0 = 0, lastDropped = 0;
    Ring<4096> msgRing, audioRing;
    juce::File file;
    std::unique_ptr<juce::FileOutputStream> out;
};
} // namespace EmberDiag
