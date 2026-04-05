# Visual Decomposition Template

## Step 1: Zone Map

Divide the mockup into zones. Standard audio plugin zones:

```
┌─────────────────────────────────────────────┐
│                 TOP BAR (6%)                │
├──────────────────────────┬──────────────────┤
│                          │   RIGHT PANEL    │
│     MAIN DISPLAY         │   (collapsed:    │
│     (variable %)         │    20px /        │
│                          │    expanded:     │
│                          │    35%)          │
│                          │                  │
├──────────────────────────┴──────────────────┤
│               BOTTOM BAR (12%)              │
└─────────────────────────────────────────────┘
```

## Step 2: Element Inventory

For each zone, catalog every visible element:

### Template Table

| ID | Zone | Element | JUCE Type | X% | Y% | W% | H% | Colour (hex) | Material | Interactive? | Animation? |
|----|------|---------|-----------|----|----|----|----|-------------|----------|-------------|------------|
| E01 | TopBar | Save Button | TextButton | 12 | 1 | 5 | 4 | border:#B08088 | flat | click | none |
| E02 | TopBar | AI Badge | Component | 88 | 1 | 3 | 4 | fill:#CC2244 | glow | click | breathe |
| E03 | Main | EQ Curve | Path | 0 | 8 | 65 | 72 | stroke:#F2E4E8 | glow | drag | realtime |
| E04 | Main | Spectrum | Path+Fill | 0 | 8 | 65 | 72 | gradient | lava | none | realtime |
| E05 | Main | Lava Strip | Component | 0 | 78 | 65 | 2 | gradient | magma | none | realtime |
| E06 | Main | Ghost Curve | Path | 0 | 8 | 65 | 72 | #E85070@35% | filament | none | realtime |
| E07 | Main | Marker | Component | var | var | 12px | 12px | #D4A853 | glow | hover+drag | hover |
| E08 | Main | Ember Trail | paint | var | var | 1px | var | grad | particle | none | continuous |
| E09 | Bottom | Hero Knob | Slider | var | 88 | 52px | 52px | copper | filament | drag | rotate |

### Categorization Rules

**Static elements** (paint once, cache):
- Background, grid lines, axis labels, zone borders

**Reactive elements** (repaint on data change):
- Spectrum, EQ curve, ghost curve, markers, lava strip

**Animated elements** (timer-driven):
- Ember trails, heat particles, breath line, AI badge glow

**Interactive elements** (mouse-driven):
- Knobs, markers (drag), buttons (click), panel (expand/collapse), markers (hover → tooltip)

## Step 3: Component Tree

Group elements into JUCE Components. Rules:
1. Elements that repaint together → same Component
2. Elements with different repaint rates → separate Components
3. Overlapping elements → use paint layer order, same Component
4. Interactive elements → own Component (for mouse handling)

## Step 4: Dependency Map

For each element, identify data dependencies:

| Element | Data Source | Update Rate | Bridge Type |
|---------|------------|-------------|-------------|
| Spectrum | FFT magnitudes | every audio block | std::atomic array |
| EQ Curve | Band parameters | on user change | ValueTree |
| Ghost Curve | AI suggestion | on analysis complete | async message |
| Markers | AI resonances | on analysis complete | async message |
| Lava Strip | AI confidence map | on analysis complete | async message |
| Knob values | Band parameters | on user change | Slider::Listener |
| Level Meter | Peak/RMS | every audio block | std::atomic |

## Step 5: Code Generation Checklist

Before generating code, verify:

- [ ] Every element has a JUCE type assigned
- [ ] Every colour has a hex value from the Design Token Sheet
- [ ] Every position is expressed as percentage (no hardcoded pixels except icon sizes)
- [ ] Every animated element has a timer strategy
- [ ] Every interactive element has a mouse handler strategy
- [ ] The component tree avoids unnecessary nesting (max 3 levels)
- [ ] The paint layer order matches the visual stacking in the mockup
- [ ] Theme-dependent colours use the Theme struct, not hardcoded values
