#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace EmberSemantic
{

/**
 * Pure, allocation-owning description of the message-thread semantic band
 * allocation problem. It intentionally knows nothing about JUCE/APVTS or
 * BandState: PluginProcessor snapshots those into these booleans before any
 * mutation. This makes typed-plan atomicity independently testable.
 */
struct SlotAvailability
{
    bool enabled = false;
    bool solo = false;
};

struct RequestedSemanticSlot
{
    int group = -1;
    int ordinal = -1;
};

struct ExistingSemanticSlot
{
    int group = -1;
    int ordinal = -1;
    int slot = -1;

    // True only when the current physical band still equals the last state
    // written by Semantic. False means user/manual takeover.
    bool stillSemantic = false;

    // If an obsolete owned slot is released, this is the state that would be
    // restored before the new plan allocates its bands.
    bool hasSnapshot = false;
    SlotAvailability originalState {};
};

struct SemanticSlotPreflightResult
{
    std::vector<int> resolvedSlots;
    int resolvableCount = 0;

    [[nodiscard]] bool complete() const noexcept
    {
        return resolvableCount == static_cast<int>(resolvedSlots.size());
    }
};

[[nodiscard]] inline SemanticSlotPreflightResult preflightSemanticSlots(
    const std::vector<SlotAvailability>& currentSlots,
    const std::vector<RequestedSemanticSlot>& requested,
    const std::vector<ExistingSemanticSlot>& existing)
{
    SemanticSlotPreflightResult result;
    result.resolvedSlots.assign(requested.size(), -1);

    if (currentSlots.empty() || requested.empty())
        return result;

    auto virtualStates = currentSlots;
    std::vector<bool> virtualClaimed(currentSlots.size(), false);

    auto isRequestedKey = [&](int group, int ordinal) noexcept
    {
        return std::any_of(requested.begin(), requested.end(),
                           [&](const RequestedSemanticSlot& request)
                           {
                               return request.group == group && request.ordinal == ordinal;
                           });
    };

    // Simulate reconciliation first. Desired, untouched semantic assignments
    // reserve their current slot. Obsolete untouched assignments virtually
    // restore the pre-semantic snapshot and can then be reused by the new plan.
    // Manual takeovers are left exactly as the current slot snapshot describes.
    for (const auto& assignment : existing)
    {
        if (!assignment.stillSemantic || assignment.slot < 0
            || assignment.slot >= static_cast<int>(currentSlots.size()))
            continue;

        const auto slot = static_cast<std::size_t>(assignment.slot);
        if (isRequestedKey(assignment.group, assignment.ordinal))
        {
            virtualClaimed[slot] = true;
        }
        else if (assignment.hasSnapshot)
        {
            virtualStates[slot] = assignment.originalState;
        }
    }

    // Resolve in request order for deterministic behavior. Existing untouched
    // ownership for the same stable (group, ordinal) key wins. Otherwise choose
    // the highest-numbered physically disabled/non-solo slot, matching the
    // product allocator's historic preference for keeping lower slots manual.
    for (std::size_t i = 0; i < requested.size(); ++i)
    {
        const auto& request = requested[i];
        if (request.group < 0 || request.ordinal < 0)
            continue; // malformed/unmapped semantic request remains unresolved

        const auto existingIt = std::find_if(existing.begin(), existing.end(),
            [&](const ExistingSemanticSlot& assignment)
            {
                return assignment.group == request.group
                    && assignment.ordinal == request.ordinal
                    && assignment.stillSemantic
                    && assignment.slot >= 0
                    && assignment.slot < static_cast<int>(currentSlots.size());
            });

        if (existingIt != existing.end())
        {
            result.resolvedSlots[i] = existingIt->slot;
            ++result.resolvableCount;
            continue;
        }

        int chosen = -1;
        for (int slot = static_cast<int>(virtualStates.size()) - 1; slot >= 0; --slot)
        {
            const auto index = static_cast<std::size_t>(slot);
            if (virtualClaimed[index])
                continue;

            const auto& state = virtualStates[index];
            if (!state.enabled && !state.solo)
            {
                chosen = slot;
                virtualClaimed[index] = true;
                break;
            }
        }

        result.resolvedSlots[i] = chosen;
        if (chosen >= 0)
            ++result.resolvableCount;
    }

    return result;
}

} // namespace EmberSemantic
