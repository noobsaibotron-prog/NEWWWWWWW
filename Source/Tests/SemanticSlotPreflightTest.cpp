#include <juce_core/juce_core.h>

#include "AI/SemanticSlotPreflight.h"

namespace
{
using namespace EmberSemantic;

class SemanticSlotPreflightTest final : public juce::UnitTest
{
public:
    SemanticSlotPreflightTest()
        : juce::UnitTest("Semantic Slot Preflight", "AI-Diag") {}

    void runTest() override
    {
        beginTest("Allocates only disabled non-solo slots, highest first");
        {
            const std::vector<SlotAvailability> slots {
                { true, false }, { true, false }, { false, false }, { false, false }
            };
            const std::vector<RequestedSemanticSlot> requests { { 0, 0 }, { 0, 1 } };
            const auto r = preflightSemanticSlots(slots, requests, {});
            expect(r.complete());
            expectEquals(r.resolvedSlots[0], 3);
            expectEquals(r.resolvedSlots[1], 2);
        }

        beginTest("Fails closed when complete plan cannot fit");
        {
            const std::vector<SlotAvailability> slots {
                { true, false }, { true, false }, { false, false }
            };
            const std::vector<RequestedSemanticSlot> requests { { 1, 0 }, { 1, 1 } };
            const auto r = preflightSemanticSlots(slots, requests, {});
            expect(!r.complete());
            expectEquals(r.resolvableCount, 1);
        }

        beginTest("Keeps an untouched existing semantic assignment");
        {
            const std::vector<SlotAvailability> slots {
                { true, false }, { true, false }, { true, false }
            };
            const std::vector<RequestedSemanticSlot> requests { { 2, 0 } };
            const std::vector<ExistingSemanticSlot> existing {
                { 2, 0, 1, true, true, { false, false } }
            };
            const auto r = preflightSemanticSlots(slots, requests, existing);
            expect(r.complete());
            expectEquals(r.resolvedSlots[0], 1);
        }

        beginTest("Virtually releases obsolete semantic ownership before allocation");
        {
            const std::vector<SlotAvailability> slots {
                { true, false }, { true, false }, { true, false }
            };
            const std::vector<RequestedSemanticSlot> requests { { 3, 0 } };
            const std::vector<ExistingSemanticSlot> existing {
                { 2, 0, 2, true, true, { false, false } }
            };
            const auto r = preflightSemanticSlots(slots, requests, existing);
            expect(r.complete());
            expectEquals(r.resolvedSlots[0], 2);
        }

        beginTest("Manual takeover is unavailable while enabled");
        {
            const std::vector<SlotAvailability> slots {
                { true, false }, { true, false }, { true, false }
            };
            const std::vector<RequestedSemanticSlot> requests { { 3, 0 } };
            const std::vector<ExistingSemanticSlot> existing {
                { 2, 0, 2, false, true, { false, false } }
            };
            const auto r = preflightSemanticSlots(slots, requests, existing);
            expect(!r.complete());
        }

        beginTest("Malformed request remains unresolved");
        {
            const std::vector<SlotAvailability> slots { { false, false } };
            const std::vector<RequestedSemanticSlot> requests { { -1, -1 } };
            const auto r = preflightSemanticSlots(slots, requests, {});
            expect(!r.complete());
            expectEquals(r.resolvableCount, 0);
            expectEquals(r.resolvedSlots[0], -1);
        }
    }
};

static SemanticSlotPreflightTest gSemanticSlotPreflightTest;
} // namespace
