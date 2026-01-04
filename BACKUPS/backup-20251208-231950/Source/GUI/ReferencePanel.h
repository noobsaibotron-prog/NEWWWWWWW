#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "ModernLookAndFeel.h"

//==============================================================================
class ReferencePanel : public juce::Component
{
public:
    explicit ReferencePanel(AIEqualizerAudioProcessor&) {}
    void paint(juce::Graphics&) override {}
    void resized() override {}
};
