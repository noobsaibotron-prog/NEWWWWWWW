# Design Token C++ Template

## Token Namespace Structure

Every Design Token Sheet from Fase 3 maps to a C++ namespace:

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace DesignTokens
{
    // ═══════════════════════════════════════════
    // Theme: [THEME_NAME]
    // Generated from: Product Design Specialist
    // Version: [TOKEN_SHEET_VERSION]
    // ═══════════════════════════════════════════

    struct Theme
    {
        // ── Backgrounds ──
        juce::Colour bgDeep;
        juce::Colour bgSurface;
        juce::Colour bgElevated;

        // ── Borders ──
        juce::Colour borderSubtle;

        // ── Text ──
        juce::Colour textPrimary;
        juce::Colour textSecondary;
        juce::Colour textMuted;

        // ── Accents ──
        juce::Colour accent1;    // Firma (primary action, signature)
        juce::Colour accent2;    // Glow (secondary, hover states)
        juce::Colour accent3;    // Attenzione (warnings, attention)

        // ── Specialty (theme-specific) ──
        juce::Colour spectrumLow;
        juce::Colour spectrumMid;
        juce::Colour spectrumHigh;
        juce::Colour spectrumPeak;
        juce::Colour heatmapCold;
        juce::Colour heatmapHot;
        juce::Colour heatmapCritical;
        juce::Colour knobBody;
        juce::Colour knobFilament;
        juce::Colour knobGlass;
    };

    // ── Predefined Themes ──

    inline Theme deepCrimson()
    {
        return {
            // Backgrounds
            juce::Colour(0xFF060406),  // bgDeep
            juce::Colour(0xFF120A0E),  // bgSurface
            juce::Colour(0xFF1C1016),  // bgElevated
            // Borders
            juce::Colour(0xFF2A1820),  // borderSubtle
            // Text
            juce::Colour(0xFFF2E4E8),  // textPrimary
            juce::Colour(0xFFB08088),  // textSecondary
            juce::Colour(0xFF6A4850),  // textMuted
            // Accents
            juce::Colour(0xFFCC2244),  // accent1 — crimson firma
            juce::Colour(0xFFE85070),  // accent2 — rose glow
            juce::Colour(0xFFD4A853),  // accent3 — gold attention
            // Spectrum
            juce::Colour(0xFF1A0610),  // spectrumLow
            juce::Colour(0xFF5A1020),  // spectrumMid
            juce::Colour(0xFFCC2244),  // spectrumHigh
            juce::Colour(0xFFFF6030),  // spectrumPeak
            // Heatmap (Lava Strip)
            juce::Colour(0xFF3A0818),  // heatmapCold
            juce::Colour(0xFFCC2244),  // heatmapHot
            juce::Colour(0xFFFFD4AA),  // heatmapCritical
            // Knobs
            juce::Colour(0xFFC0A8B0),  // knobBody (satin copper)
            juce::Colour(0xFFCC2244),  // knobFilament
            juce::Colour(0xCC1C1016),  // knobGlass (80% opacity)
        };
    }

    inline Theme emeraldNight()
    {
        return {
            // Backgrounds
            juce::Colour(0xFF040806),  // bgDeep
            juce::Colour(0xFF0A120E),  // bgSurface
            juce::Colour(0xFF101C16),  // bgElevated
            // Borders
            juce::Colour(0xFF182A20),  // borderSubtle
            // Text
            juce::Colour(0xFFE4F2E8),  // textPrimary
            juce::Colour(0xFF80B088),  // textSecondary
            juce::Colour(0xFF486A50),  // textMuted
            // Accents
            juce::Colour(0xFF22CC66),  // accent1 — emerald firma
            juce::Colour(0xFF50E8A0),  // accent2 — mint glow
            juce::Colour(0xFFD4A853),  // accent3 — gold attention
            // Spectrum
            juce::Colour(0xFF061A10),  // spectrumLow
            juce::Colour(0xFF105A30),  // spectrumMid
            juce::Colour(0xFF22CC66),  // spectrumHigh
            juce::Colour(0xFF60FF90),  // spectrumPeak
            // Heatmap (Bioluminescence)
            juce::Colour(0xFF083A18),  // heatmapCold
            juce::Colour(0xFF22CC66),  // heatmapHot
            juce::Colour(0xFFAAFFD4),  // heatmapCritical
            // Knobs
            juce::Colour(0xFFA8C0B0),  // knobBody (jade)
            juce::Colour(0xFF22CC66),  // knobFilament
            juce::Colour(0xCC101C16),  // knobGlass
        };
    }

    // ── Sizing Tokens ──

    struct Sizing
    {
        static constexpr float topBarHeight      = 0.06f;  // 6% of total
        static constexpr float bottomBarHeight   = 0.12f;  // 12% of total
        static constexpr float panelCollapsed    = 20.f;   // pixels
        static constexpr float panelExpanded     = 0.35f;  // 35% of total width
        static constexpr float lavaStripHeight   = 16.f;   // pixels
        static constexpr float heroKnobSize      = 52.f;   // pixels
        static constexpr float secondaryKnobSize = 38.f;   // pixels
        static constexpr float markerRadius      = 6.f;    // pixels
        static constexpr float markerGlowRadius  = 10.f;   // pixels
        static constexpr float curveThickness    = 2.f;    // pixels
        static constexpr float curveGlowRadius   = 2.5f;   // pixels
        static constexpr float ghostCurveAlpha   = 0.35f;  // 25-45% variable
    };

    // ── Animation Tokens ──

    struct Animation
    {
        static constexpr int   frameRate         = 60;     // Hz
        static constexpr float drawerEasing      = 0.15f;  // ease-out factor
        static constexpr float markerHoverMs     = 200.f;  // ms
        static constexpr float applyFixMs        = 500.f;  // ms
        static constexpr float fixAllStaggerMs   = 120.f;  // ms between each
        static constexpr float breathLineMs      = 2000.f; // ms cycle
        static constexpr int   particleCount     = 8;      // max per frame
        static constexpr float particleSpeed     = 0.5f;   // px/frame
        static constexpr int   emberTrailDots    = 3;      // per marker
    };
}
```

## Theme Switching

Runtime theme switching via pointer:

```cpp
class ThemeManager
{
public:
    void setTheme(const DesignTokens::Theme& newTheme)
    {
        currentTheme = newTheme;
        // Notify all components to repaint
    }

    const DesignTokens::Theme& getTheme() const { return currentTheme; }

private:
    DesignTokens::Theme currentTheme = DesignTokens::deepCrimson();
};
```
