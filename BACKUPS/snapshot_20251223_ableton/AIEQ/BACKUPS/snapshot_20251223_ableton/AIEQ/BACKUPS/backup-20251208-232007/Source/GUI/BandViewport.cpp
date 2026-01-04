#include "BandViewport.h"

BandViewport::BandViewport(juce::AudioProcessorValueTreeState& apvts, int maxBands)
{
    setScrollBarsShown(false, false, true, false);
    setScrollOnDragEnabled(true);
    setWantsKeyboardFocus(false);
    container = std::make_unique<BandContainer>(apvts, maxBands);
    setViewedComponent(container.get(), false);
}

void BandViewport::setNumBands(int num)
{
    if (container)
    {
        container->setNumBands(num);
        container->resized();
        updateVisibleArea();
    }
}

//==============================================================================
BandViewport::BandContainer::BandContainer(juce::AudioProcessorValueTreeState& tree, int mb)
    : apvts(tree), maxBands(mb)
{
}

void BandViewport::BandContainer::setNumBands(int num)
{
    numBands = juce::jlimit(1, maxBands, num);
    while (panels.size() < numBands)
    {
        auto* p = new BandControlPanel(panels.size(), apvts);
        panels.add(p);
        addAndMakeVisible(p);
    }
    while (panels.size() > numBands)
    {
        panels.removeLast();
    }
    resized();
}

void BandViewport::BandContainer::resized()
{
    const int panelW = 160;
    const int panelH = getHeight();
    int x = 0;
    for (auto* p : panels)
    {
        p->setBounds(x, 0, panelW, panelH);
        x += panelW;
    }
    setSize(std::max(x, getWidth()), panelH);
}
