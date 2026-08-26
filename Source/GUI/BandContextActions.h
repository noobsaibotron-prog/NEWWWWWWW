#pragma once

#include "../PluginProcessor.h"
#include "BandRadialMenu.h"

/**
 * Processor-side band context commands shared by the radial menu, the classic
 * popup, and integration tests. GUI-only side effects (selection, repaint)
 * stay in AdvancedSpectrumDisplay.
 */
namespace aieq::gui
{
[[nodiscard]] inline bool applyBandContextCommand (
    AIEqualizerAudioProcessor& processor,
    int bandIndex,
    BandRadialMenu::Command command)
{
    if (bandIndex < 0 || bandIndex >= processor.getNumActiveBands())
        return false;

    auto state = processor.getBandState (bandIndex);
    switch (command.type)
    {
        case BandRadialMenu::CommandType::setFilterType:
            if (command.value < 0 || command.value > 6)
                return false;
            if (state.type == command.value)
                return false;
            state.type = command.value;
            processor.setBandState (bandIndex, state);
            return true;

        case BandRadialMenu::CommandType::toggleEnabled:
            state.enabled = ! state.enabled;
            processor.setBandState (bandIndex, state);
            return true;

        case BandRadialMenu::CommandType::toggleSolo:
            state.solo = ! state.solo;
            processor.setBandState (bandIndex, state);
            return true;

        case BandRadialMenu::CommandType::resetGain:
            if (state.gain == 0.0f)
                return false;
            state.gain = 0.0f;
            processor.setBandState (bandIndex, state);
            return true;

        case BandRadialMenu::CommandType::resetBand:
            state.gain = 0.0f;
            state.q = 1.0f;
            state.type = 2;
            state.solo = false;
            processor.setBandState (bandIndex, state);
            return true;

        case BandRadialMenu::CommandType::deleteBand:
            state.enabled = false;
            state.gain = 0.0f;
            state.q = 1.0f;
            processor.setBandState (bandIndex, state);
            return true;
    }

    return false;
}
} // namespace aieq::gui
