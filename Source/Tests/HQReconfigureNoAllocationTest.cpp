#include <juce_core/juce_core.h>
#include "DSP/DynamicEQProcessor.h"
#include "DSP/ParametricEQProcessor.h"

class HQReconfigureNoAllocationTest : public juce::UnitTest
{
public:
    HQReconfigureNoAllocationTest()
        : juce::UnitTest("HQ Reconfigure No Allocation", "DSP") {}

    void runTest() override
    {
        beginTest("DynamicEQ reconfigures within preallocated capacity");

        DynamicEQProcessor dyn;
        dyn.setLookahead(5.0f);
        dyn.prepare(192000.0, 8192, 2);

        const int dryCapacity = dyn.getDryBufferCapacityForTests();
        const int lookaheadCapacity = dyn.getLookaheadBufferCapacityForTests();

        expect(dyn.reconfigureNoAllocation(96000.0, 4096, 2));
        expectEquals(dyn.getDryBufferCapacityForTests(), dryCapacity);
        expectEquals(dyn.getLookaheadBufferCapacityForTests(), lookaheadCapacity);
        expectEquals(dyn.getLookaheadSamplesForTests(), 480);

        expect(dyn.reconfigureNoAllocation(192000.0, 8192, 2));
        expectEquals(dyn.getDryBufferCapacityForTests(), dryCapacity);
        expectEquals(dyn.getLookaheadBufferCapacityForTests(), lookaheadCapacity);
        expectEquals(dyn.getLookaheadSamplesForTests(), 960);

        beginTest("DynamicEQ rejects insufficient capacity without mutating state");

        DynamicEQProcessor small;
        small.setLookahead(3.0f);
        small.prepare(48000.0, 512, 2);
        const int smallDryCapacity = small.getDryBufferCapacityForTests();
        const int smallLookaheadCapacity = small.getLookaheadBufferCapacityForTests();
        const int smallLookaheadSamples = small.getLookaheadSamplesForTests();

        expect(!small.canReconfigureWithoutAllocation(192000.0, 8192, 2));
        expect(!small.reconfigureNoAllocation(192000.0, 8192, 2));
        expectEquals(small.getDryBufferCapacityForTests(), smallDryCapacity);
        expectEquals(small.getLookaheadBufferCapacityForTests(), smallLookaheadCapacity);
        expectEquals(small.getLookaheadSamplesForTests(), smallLookaheadSamples);

        beginTest("ParametricEQ exposes no-allocation retune contract");

        ParametricEQProcessor eq;
        eq.prepare(48000.0, 512, 2);
        expect(eq.reconfigureNoAllocation(96000.0, 1024, 2));
    }
};

static HQReconfigureNoAllocationTest hqReconfigureNoAllocationTest;
