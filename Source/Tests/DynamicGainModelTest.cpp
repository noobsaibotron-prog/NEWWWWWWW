#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../DSP/DynamicGainModel.h"
#include <array>
#include <cmath>

class DynamicGainModelTest final : public juce::UnitTest
{
public:
    DynamicGainModelTest()
        : juce::UnitTest("Dynamic gain model authority", "AIEQ-DSP") {}

    void runTest() override
    {
        testHardKneeActionTriggerMatrix();
        testSoftKneeContinuityAndSlope();
        testRangeAndEffectiveGainClamps();
        testLinkedDetectorDefinitions();
        testDetectorTimeConstant();
        testFiniteGuards();
    }

private:
    using Action = AIEQDSP::DynamicGainModel::Action;
    using Trigger = AIEQDSP::DynamicGainModel::TriggerSide;
    using Detection = AIEQDSP::DynamicGainModel::DetectionMode;

    void testHardKneeActionTriggerMatrix()
    {
        beginTest("Hard-knee Action x Trigger signs and magnitudes");

        const auto evaluate = [](Action action, Trigger trigger, double level)
        {
            return AIEQDSP::DynamicGainModel::evaluate(
                level, 0.0, action, trigger, -20.0, 4.0, 0.0, 48.0);
        };

        expectWithinAbsoluteError(evaluate(Action::Compress, Trigger::Above, -10.0).dynamicGainDb,
                                  -7.5, 1.0e-12);
        expectWithinAbsoluteError(evaluate(Action::Compress, Trigger::Below, -30.0).dynamicGainDb,
                                  7.5, 1.0e-12);
        expectWithinAbsoluteError(evaluate(Action::Expand, Trigger::Above, -10.0).dynamicGainDb,
                                  30.0, 1.0e-12);
        expectWithinAbsoluteError(evaluate(Action::Expand, Trigger::Below, -30.0).dynamicGainDb,
                                  -30.0, 1.0e-12);

        expectWithinAbsoluteError(evaluate(Action::Compress, Trigger::Above, -30.0).dynamicGainDb,
                                  0.0, 1.0e-12);
        expectWithinAbsoluteError(evaluate(Action::Expand, Trigger::Below, -10.0).dynamicGainDb,
                                  0.0, 1.0e-12);
    }

    void testSoftKneeContinuityAndSlope()
    {
        beginTest("Soft-knee phi functions are C1 at both boundaries");

        constexpr double width = 8.0;
        constexpr double h = 1.0e-5;
        for (const double boundary : { -4.0, 4.0 })
        {
            const double aLeft = AIEQDSP::DynamicGainModel::phiAbove(boundary - h, width);
            const double aRight = AIEQDSP::DynamicGainModel::phiAbove(boundary + h, width);
            const double bLeft = AIEQDSP::DynamicGainModel::phiBelow(boundary - h, width);
            const double bRight = AIEQDSP::DynamicGainModel::phiBelow(boundary + h, width);
            expect(std::abs(aRight - aLeft) < 3.0e-5);
            expect(std::abs(bRight - bLeft) < 3.0e-5);
        }

        const auto derivative = [h](auto function, double x)
        {
            return (function(x + h) - function(x - h)) / (2.0 * h);
        };
        const auto above = [width](double x) { return AIEQDSP::DynamicGainModel::phiAbove(x, width); };
        const auto below = [width](double x) { return AIEQDSP::DynamicGainModel::phiBelow(x, width); };
        expectWithinAbsoluteError(derivative(above, -4.0), 0.0, 2.0e-5);
        expectWithinAbsoluteError(derivative(above, 4.0), 1.0, 2.0e-5);
        expectWithinAbsoluteError(derivative(below, -4.0), 1.0, 2.0e-5);
        expectWithinAbsoluteError(derivative(below, 4.0), 0.0, 2.0e-5);
    }

    void testRangeAndEffectiveGainClamps()
    {
        beginTest("Range and effective-band gain clamps are ordered correctly");

        const auto rangeLimited = AIEQDSP::DynamicGainModel::evaluate(
            0.0, 0.0, Action::Expand, Trigger::Above, -60.0, 20.0, 0.0, 12.0);
        expectWithinAbsoluteError(rangeLimited.dynamicGainDb, 12.0, 1.0e-12);
        expectWithinAbsoluteError(rangeLimited.effectiveBandGainDb, 12.0, 1.0e-12);

        const auto headroomLimited = AIEQDSP::DynamicGainModel::evaluate(
            0.0, 30.0, Action::Expand, Trigger::Above, -60.0, 20.0, 0.0, 48.0);
        expectWithinAbsoluteError(headroomLimited.dynamicGainDb, 6.0, 1.0e-12);
        expectWithinAbsoluteError(headroomLimited.effectiveBandGainDb, 36.0, 1.0e-12);
    }

    void testLinkedDetectorDefinitions()
    {
        beginTest("Linked Peak uses amplitude and linked RMS uses power");

        expectWithinAbsoluteError(AIEQDSP::DynamicGainModel::detectorInput(
                                      -0.25, 0.75, 2, Detection::Peak),
                                  0.75, 1.0e-12);
        expectWithinAbsoluteError(AIEQDSP::DynamicGainModel::detectorInput(
                                      0.5, 1.0, 2, Detection::RMS),
                                  0.625, 1.0e-12);
        expectWithinAbsoluteError(AIEQDSP::DynamicGainModel::detectorInput(
                                      -0.5, 99.0, 1, Detection::RMS),
                                  0.25, 1.0e-12);
    }

    void testDetectorTimeConstant()
    {
        beginTest("Detector reaches 1-exp(-1) after one time constant");

        constexpr double sampleRate = 48000.0;
        constexpr double timeMs = 10.0;
        const auto coefficient = AIEQDSP::DynamicGainModel::smoothingCoefficient(timeMs, sampleRate);
        double envelope = 0.0;
        const int samples = static_cast<int>(timeMs * 0.001 * sampleRate);
        for (int i = 0; i < samples; ++i)
            envelope = AIEQDSP::DynamicGainModel::advanceEnvelope(
                envelope, 1.0, coefficient, coefficient);

        expectWithinAbsoluteError(envelope, 1.0 - std::exp(-1.0), 1.0e-12);
        expectWithinAbsoluteError(AIEQDSP::DynamicGainModel::envelopeToDb(1.0, Detection::Peak),
                                  0.0, 1.0e-12);
        expectWithinAbsoluteError(AIEQDSP::DynamicGainModel::envelopeToDb(1.0, Detection::RMS),
                                  0.0, 1.0e-12);
    }

    void testFiniteGuards()
    {
        beginTest("Non-finite detector and transfer inputs fail safe");

        const auto nonFinite = std::numeric_limits<double>::quiet_NaN();
        expectWithinAbsoluteError(AIEQDSP::DynamicGainModel::detectorInput(
                                      nonFinite, nonFinite, 2, Detection::Peak),
                                  0.0, 1.0e-12);
        expectWithinAbsoluteError(AIEQDSP::DynamicGainModel::envelopeToDb(
                                      nonFinite, Detection::RMS),
                                  -160.0, 1.0e-12);

        const auto result = AIEQDSP::DynamicGainModel::evaluate(
            nonFinite, nonFinite, Action::Compress, Trigger::Above,
            nonFinite, nonFinite, nonFinite, nonFinite);
        expect(std::isfinite(result.dynamicGainDb));
        expect(std::isfinite(result.effectiveBandGainDb));
    }
};

static DynamicGainModelTest dynamicGainModelTest;

#endif
