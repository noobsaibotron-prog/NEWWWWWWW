#pragma once

// T3.2 — asynchronous Semantic planning.
//
// Why this exists: the fitter costs 32-42 ms on a real Mac (44.1/48/96 kHz,
// measured), and SemanticControlPanel used to call it synchronously from the
// message thread. That is 2-3 dropped GUI frames on every PLAN. The algorithm is
// fine; only its location was wrong.
//
// Design is a LATEST-REQUEST-WINS MAILBOX, deliberately not a FIFO. The user
// does not want a backlog of every phrase they typed, they want the newest
// intent. One pending slot and one completed slot make the staleness rule
// trivial to state and to verify: a result is publishable only while its
// generation is still the current one.
//
// Ownership contract, which the worker must never violate:
//   message thread  -> captures an immutable request (text/intensity/sampleRate)
//   worker          -> SemanticPlanner only; no PluginProcessor, no APVTS, no
//                      SemanticEQEngine, no JUCE component
//   message thread  -> consumes a current-generation result, previews, applies
//
// SemanticPlanner::plan() is const and stateless (it default-constructs per
// call), so the worker needs nothing from the engine beyond the three values
// already carried in the request. That is also the shape T5 wants: the
// SpectralContext snapshot is taken on the message thread BEFORE enqueue and
// travels inside the request, so the worker never reaches into live analysis.

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include <juce_core/juce_core.h>

#include "SemanticPlan.h"

namespace AIEQPerceptual
{

/** Everything the worker needs, captured on the message thread. Immutable by
    convention: once submitted it is never edited, only replaced. */
struct SemanticPlanningRequest
{
    std::uint64_t generation = 0;
    std::string   text;
    float         intensity = 1.0f;
    double        sampleRate = 44100.0;

    // T5 extension point. The snapshot is taken before enqueue, never pulled by
    // the worker:
    // std::optional<SpectralContext> contextSnapshot;
};

/** An unusable command is still a valid planning OUTCOME. The UI has to be able
    to tell "I did not understand you" from "you contradicted yourself" from
    "there is nothing safe to do here" — collapsing them into an empty optional
    throws away exactly the information the user needs. */
enum class SemanticPlanningStatus
{
    Ready,               ///< plan.fit.bands is non-empty and applicable
    UnknownIntent,       ///< nothing recognisable in the text
    ContradictoryIntent, ///< goal fights a protection, or direction is ambiguous
    NoSafeMove,          ///< understood, but the safe plan is empty
    Cancelled,           ///< superseded before publication
    InternalError        ///< intent recognised but planning produced an invalid plan
};

struct SemanticPlanningResult
{
    std::uint64_t          generation = 0;
    SemanticPlanningStatus status = SemanticPlanningStatus::InternalError;
    SemanticPlan           plan;
};

/** UX state is owned by the UI, not inferred from worker internals. Deriving it
    from "is the thread busy" plus "is the result slot empty" produces states
    that are briefly wrong during handover; an explicit value cannot. */
enum class SemanticPlanningUiState
{
    Idle,
    Planning,
    Ready
};

class SemanticPlanningService final : private juce::Thread
{
public:
    SemanticPlanningService();
    ~SemanticPlanningService() override;

    SemanticPlanningService(const SemanticPlanningService&) = delete;
    SemanticPlanningService& operator=(const SemanticPlanningService&) = delete;

    /** Idempotent. The worker is not started by the constructor so that a test
        or a headless host can construct the service without a thread. */
    void start();

    /** Idempotent, and joins. Safe to call from the destructor of the owner
        before any object the worker could observe is torn down. */
    void stop();

    /** Replaces any pending request and supersedes any in-flight one. Returns
        the generation the caller must quote to recognise its own result. */
    std::uint64_t submit(std::string text, float intensity, double sampleRate);

    /** Every event that changes planning inputs must call this — not merely
        clear a pending plan. Bumping the epoch is what makes an in-flight
        result unpublishable; clearing a slot does not. */
    std::uint64_t invalidate();

    [[nodiscard]] std::uint64_t currentGeneration() const noexcept
    {
        return generation.load(std::memory_order_acquire);
    }

    /** Message thread. Hands over the completed result if and only if it still
        belongs to the current generation, and clears the slot. A stale result
        is dropped here rather than at the call site, so no caller can forget. */
    [[nodiscard]] std::optional<SemanticPlanningResult> takeCurrentResult();

    /** True between submit() and publication/supersession. Test/diagnostic. */
    [[nodiscard]] bool isPlanning() const noexcept
    {
        return planningInFlight.load(std::memory_order_acquire);
    }

    /** Test support: bounded wait for the worker to go quiet. Returns false on
        timeout so a test fails loudly instead of racing. */
    bool waitUntilQuiescent(int timeoutMs);

private:
    void run() override;

    mutable std::mutex                    mailboxMutex;
    std::optional<SemanticPlanningRequest> pendingRequest;   // latest wins
    std::optional<SemanticPlanningResult>  completedResult;  // latest wins

    std::atomic<std::uint64_t> generation { 0 };
    std::atomic<bool>          planningInFlight { false };
    juce::WaitableEvent        wakeUp { false };
};

[[nodiscard]] const char* semanticPlanningStatusName(SemanticPlanningStatus status) noexcept;

} // namespace AIEQPerceptual
