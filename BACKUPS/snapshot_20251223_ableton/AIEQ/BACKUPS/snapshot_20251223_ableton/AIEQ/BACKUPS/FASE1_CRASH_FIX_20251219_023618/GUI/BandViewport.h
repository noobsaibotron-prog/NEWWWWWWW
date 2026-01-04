#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "BandControlPanel.h"

class BandViewport : public juce::Viewport
{
public:
    BandViewport(juce::AudioProcessorValueTreeState& apvts, int maxBands);
    void setNumBands(int numBands);

private:
    class BandContainer : public juce::Component
    {
    public:
        BandContainer(juce::AudioProcessorValueTreeState& apvts, int maxBands);
        void setNumBands(int numBands);
        void resized() override;

    private:
        juce::AudioProcessorValueTreeState& apvts;
        int maxBands = 0;
        int numBands = 0;
        juce::OwnedArray<BandControlPanel> panels;
    };

    std::unique_ptr<BandContainer> container;
};
