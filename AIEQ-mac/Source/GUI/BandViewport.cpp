#include "BandViewport.h"

BandViewport::BandViewport(juce::AudioProcessorValueTreeState& apvts, int maxBands)
{
    setScrollBarsShown(false, false, true, false);
#if JUCE_VERSION >= 0x070000
    setScrollOnDragMode(ScrollOnDragMode::all);
#else
    setScrollOnDragEnabled(true); // Compatibility with older JUCE versions
#endif
    setWantsKeyboardFocus(false);
    container = std::make_unique<BandContainer>(apvts, maxBands);
    setViewedComponent(container.get(), false);
}

void BandViewport::setNumBands(int num)
{
    if (container)
    {
        container->setNumBands(num);
        // Keep current scroll position but force the viewport to recompute visible area
        const int vx = getViewPositionX();
        const int vy = getViewPositionY();
        setViewPosition(vx, vy);
        auto* vc = getViewedComponent();
        if (vc)
            setViewPosition(std::min(vx, std::max(0, vc->getWidth() - getWidth())),
                            std::min(vy, std::max(0, vc->getHeight() - getHeight())));
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
    const int panelH = std::max(1, getHeight());
    int x = 0;
    for (auto* p : panels)
    {
        p->setBounds(x, 0, panelW, panelH);
        x += panelW;
    }
    setSize(std::max(x, panelW * std::max(1, numBands)), panelH);
}
