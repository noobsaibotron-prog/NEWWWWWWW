# JUCE Component Patterns for Design-to-Code

## Standard Component Structure

Every visual component generated from a mockup follows this pattern:

```cpp
class MyComponent : public juce::Component, private juce::Timer
{
public:
    MyComponent()
    {
        // Timer only if animated (ember trails, particles, breath line)
        // startTimerHz(60);
    }

    void paint(juce::Graphics& g) override
    {
        // 1. Background fill
        // 2. Static elements (grid, axes)
        // 3. Dynamic elements (spectrum, curves)
        // 4. Overlay elements (markers, tooltips)
    }

    void resized() override
    {
        // Use proportional bounds — NEVER hardcode pixel values
        auto area = getLocalBounds();
        // topBar = area.removeFromTop(proportionOfHeight(0.06f));
    }

private:
    // Use timerCallback only for animated elements
    void timerCallback() override { repaint(); }
};
```

## Proportional Layout Pattern

All positions and sizes from the mockup are expressed as percentages of the parent component:

```cpp
void resized() override
{
    auto bounds = getLocalBounds().toFloat();

    // Top bar: 6% height
    auto topBar = bounds.removeFromTop(bounds.getHeight() * 0.06f);

    // Bottom bar: 12% height
    auto bottomBar = bounds.removeFromBottom(bounds.getHeight() * 0.12f);

    // Right panel (when expanded): 35% width
    auto rightPanel = bounds.removeFromRight(bounds.getWidth() * 0.35f);

    // Remaining = main EQ display
    eqDisplay = bounds;
}
```

## Paint Layer Order

Render from back to front:

```cpp
void paint(juce::Graphics& g) override
{
    paintBackground(g);       // Layer 0: bg gradient
    paintGrid(g);             // Layer 1: frequency grid
    paintSpectrum(g);         // Layer 2: spectrum analyzer
    paintLavaStrip(g);        // Layer 3: heatmap
    paintGhostCurve(g);       // Layer 4: AI suggestion
    paintEQCurve(g);          // Layer 5: user curve (hero)
    paintMarkers(g);          // Layer 6: resonance markers
    paintEmberTrails(g);      // Layer 7: particle trails
    paintHeatParticles(g);    // Layer 8: rising particles
    paintTooltip(g);          // Layer 9: hover tooltip
}
```

## Glow Effect Pattern

For elements with glow (EQ curve, markers, lava strip):

```cpp
void paintGlow(juce::Graphics& g, juce::Path& path,
               juce::Colour colour, float glowRadius, float lineWidth)
{
    // Outer glow (multiple passes with decreasing opacity)
    for (int i = 3; i >= 1; --i)
    {
        float alpha = 0.05f * i;
        float width = lineWidth + glowRadius * i;
        g.setColour(colour.withAlpha(alpha));
        g.strokePath(path, juce::PathStrokeType(width,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Core line
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(lineWidth,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
```

## Gradient Spectrum Pattern

For the molten lava spectrum:

```cpp
void paintSpectrum(juce::Graphics& g, const juce::Rectangle<float>& area,
                   const float* magnitudes, int numBins)
{
    juce::Path spectrumPath;
    spectrumPath.startNewSubPath(area.getX(), area.getBottom());

    for (int i = 0; i < numBins; ++i)
    {
        float x = juce::jmap((float)i, 0.f, (float)numBins,
                             area.getX(), area.getRight());
        float y = juce::jmap(magnitudes[i], 0.f, 1.f,
                             area.getBottom(), area.getY());
        spectrumPath.lineTo(x, y);
    }

    spectrumPath.lineTo(area.getRight(), area.getBottom());
    spectrumPath.closeSubPath();

    // Vertical gradient fill
    juce::ColourGradient gradient(
        tokens.accent1.withAlpha(0.8f), area.getX(), area.getY(),     // top: crimson
        tokens.bgDeep,                  area.getX(), area.getBottom(), // bottom: dark
        false);
    gradient.addColour(0.3, tokens.accent2.withAlpha(0.6f));  // rose
    gradient.addColour(0.6, tokens.accent1.withAlpha(0.4f));  // crimson
    gradient.addColour(0.85, juce::Colour(0xFF5A1020).withAlpha(0.3f)); // dark crimson

    g.setGradientFill(gradient);
    g.fillPath(spectrumPath);
}
```

## Collapsible Panel Pattern

For the AI Detail Drawer:

```cpp
class AIDrawerComponent : public juce::Component
{
public:
    void setExpanded(bool shouldExpand)
    {
        targetWidth = shouldExpand ? expandedWidth : collapsedWidth;
        startTimerHz(60);
    }

    void timerCallback()
    {
        currentWidth += (targetWidth - currentWidth) * 0.15f; // ease-out
        if (std::abs(currentWidth - targetWidth) < 0.5f)
        {
            currentWidth = targetWidth;
            stopTimer();
        }
        getParentComponent()->resized(); // trigger parent relayout
    }

    float getCurrentWidth() const { return currentWidth; }

private:
    float collapsedWidth = 20.f;  // pixels
    float expandedWidth = 0.35f;  // proportion of parent
    float currentWidth = 20.f;
    float targetWidth = 20.f;
};
```

## Filament Knob Pattern

Custom LookAndFeel for the vacuum tube knob:

```cpp
void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                      float sliderPos, float startAngle, float endAngle,
                      juce::Slider& slider) override
{
    auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(2.f);
    auto centre = bounds.getCentre();
    auto radius = bounds.getWidth() * 0.5f;

    // 1. Outer ring (satin copper)
    juce::ColourGradient copperGrad(
        juce::Colour(0xFFC0A8B0), centre.x, bounds.getY(),
        juce::Colour(0xFF8A7078), centre.x, bounds.getBottom(), false);
    g.setGradientFill(copperGrad);
    g.fillEllipse(bounds);

    // 2. Inner glass window (smoked glass)
    auto innerBounds = bounds.reduced(radius * 0.35f);
    g.setColour(juce::Colour(0xCC1C1016)); // 80% opacity
    g.fillEllipse(innerBounds);

    // 3. Filament indicator
    float angle = startAngle + sliderPos * (endAngle - startAngle);
    auto filamentEnd = centre.getPointOnCircumference(
        innerBounds.getWidth() * 0.4f, angle);

    // Filament glow
    g.setColour(tokens.accent1.withAlpha(0.3f));
    g.drawLine(juce::Line<float>(centre, filamentEnd), 4.f);

    // Filament core
    g.setColour(tokens.accent1);
    g.drawLine(juce::Line<float>(centre, filamentEnd), 1.5f);
}
```
