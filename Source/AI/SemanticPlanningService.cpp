#include "SemanticPlanningService.h"

#include "SemanticPlanner.h"

namespace AIEQPerceptual
{

namespace
{
/** Classify a finished plan. The mapping mirrors the distinctions the panel
    already made when it planned synchronously, so the async path cannot quietly
    change what the user is told. */
SemanticPlanningStatus classify(const SemanticPlan& plan) noexcept
{
    if (!plan.intent.hasRecognizedContent)
        return SemanticPlanningStatus::UnknownIntent;

    if (plan.intent.contradictory)
        return SemanticPlanningStatus::ContradictoryIntent;

    // Intent was understood but planning could not produce a usable plan. That
    // is a failure of the planner, not of the user, and is reported as such.
    if (!plan.valid || !plan.fit.valid)
        return SemanticPlanningStatus::InternalError;

    if (plan.fit.bands.empty())
        return SemanticPlanningStatus::NoSafeMove;

    return SemanticPlanningStatus::Ready;
}
} // namespace

const char* semanticPlanningStatusName(SemanticPlanningStatus status) noexcept
{
    switch (status)
    {
        case SemanticPlanningStatus::Ready:               return "Ready";
        case SemanticPlanningStatus::UnknownIntent:       return "UnknownIntent";
        case SemanticPlanningStatus::ContradictoryIntent: return "ContradictoryIntent";
        case SemanticPlanningStatus::NoSafeMove:          return "NoSafeMove";
        case SemanticPlanningStatus::Cancelled:           return "Cancelled";
        case SemanticPlanningStatus::InternalError:       return "InternalError";
    }
    return "Unknown";
}

SemanticPlanningService::SemanticPlanningService()
    : juce::Thread("AIEQ Semantic Planner")
{
}

SemanticPlanningService::~SemanticPlanningService()
{
    stop();
}

void SemanticPlanningService::start()
{
    if (!isThreadRunning())
        startThread(juce::Thread::Priority::normal);
}

void SemanticPlanningService::stop()
{
    if (!isThreadRunning())
        return;

    signalThreadShouldExit();
    wakeUp.signal();

    // Wait without a deadline on purpose. The unit of work is one bounded fit
    // (tens of milliseconds, measured), so there is nothing to time out against;
    // a timeout here would only trade a guaranteed join for a race against
    // whatever the owner destroys next.
    stopThread(-1);
}

std::uint64_t SemanticPlanningService::submit(std::string text, float intensity,
                                              double sampleRate, const SpectralContext& context,
                                              std::vector<SemanticProtectedRange> protectedRanges)
{
    // Bump first: anything already in flight is stale from this instant, whether
    // or not the worker has noticed yet.
    const auto gen = generation.fetch_add(1, std::memory_order_acq_rel) + 1;

    {
        const std::lock_guard<std::mutex> lock(mailboxMutex);
        pendingRequest = SemanticPlanningRequest { gen, std::move(text), intensity,
                                                   sampleRate, context,
                                                   std::move(protectedRanges) };
        completedResult.reset(); // an older answer must not survive a newer question
    }

    planningInFlight.store(true, std::memory_order_release);
    wakeUp.signal();
    return gen;
}

std::uint64_t SemanticPlanningService::invalidate()
{
    const auto gen = generation.fetch_add(1, std::memory_order_acq_rel) + 1;

    {
        const std::lock_guard<std::mutex> lock(mailboxMutex);
        pendingRequest.reset();
        completedResult.reset();
    }

    planningInFlight.store(false, std::memory_order_release);
    return gen;
}

std::optional<SemanticPlanningResult> SemanticPlanningService::takeCurrentResult()
{
    const std::lock_guard<std::mutex> lock(mailboxMutex);
    if (!completedResult.has_value())
        return std::nullopt;

    // The staleness rule lives here, once, so that no caller can forget it.
    if (completedResult->generation != generation.load(std::memory_order_acquire))
    {
        completedResult.reset();
        return std::nullopt;
    }

    auto result = std::move(*completedResult);
    completedResult.reset();
    return result;
}

bool SemanticPlanningService::waitUntilQuiescent(int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounter()
                        + static_cast<juce::uint32>(juce::jmax(0, timeoutMs));

    for (;;)
    {
        bool busy = planningInFlight.load(std::memory_order_acquire);
        if (!busy)
        {
            const std::lock_guard<std::mutex> lock(mailboxMutex);
            busy = pendingRequest.has_value();
        }

        if (!busy)
            return true;

        if (juce::Time::getMillisecondCounter() >= deadline)
            return false;

        juce::Thread::sleep(1);
    }
}

void SemanticPlanningService::run()
{
    while (!threadShouldExit())
    {
        SemanticPlanningRequest request;

        {
            const std::lock_guard<std::mutex> lock(mailboxMutex);
            if (pendingRequest.has_value())
            {
                request = std::move(*pendingRequest);
                pendingRequest.reset();
            }
        }

        if (request.generation == 0)
        {
            // Nothing to do. Wake on submit(), or periodically so that an exit
            // request is never missed if a signal and a wait interleave badly.
            wakeUp.wait(50);
            continue;
        }

        // Computed outside the lock: this is the expensive part, and holding the
        // mailbox here would make submit() block the message thread — which is
        // the whole problem T3.2 exists to remove.
        // Everything the plan depends on came in with the request. The worker
        // touches no processor, no APVTS, no accumulator and no live front-end
        // state - which is what lets the epoch rule below be the whole of the
        // staleness story.
        SemanticPlan plan = SemanticPlanner().plan(request.text,
                                                   request.sampleRate,
                                                   request.intensity,
                                                   request.spectralContext,
                                                   request.protectedRanges);

        if (threadShouldExit())
            return;

        {
            const std::lock_guard<std::mutex> lock(mailboxMutex);

            // Publish only while still current. A superseded request is dropped
            // silently: the newer one is already pending and will answer.
            if (request.generation == generation.load(std::memory_order_acquire))
            {
                SemanticPlanningResult result;
                result.generation = request.generation;
                result.status = classify(plan);
                result.plan = std::move(plan);
                completedResult = std::move(result);
            }
        }

        // Only clear the busy flag when no newer request is already waiting,
        // otherwise a caller could observe "idle" between two queued fits.
        {
            const std::lock_guard<std::mutex> lock(mailboxMutex);
            if (!pendingRequest.has_value())
                planningInFlight.store(false, std::memory_order_release);
        }
    }
}

} // namespace AIEQPerceptual
