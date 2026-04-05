#pragma once
/**
 * DesignTokens.h — Premium Visual Rebranding Foundation
 *
 * Centralizes ALL visual tokens (colors, sizing, animation timing)
 * for the entire plugin UI. Supports runtime theme switching between
 * Deep Crimson and Emerald Night.
 *
 * Usage:
 *   auto& theme = ThemeManager::getInstance().getTheme();
 *   g.setColour(theme.bgPrimary);
 *
 * Architecture:
 *   - Theme struct: immutable color/sizing/animation values
 *   - ThemeManager: singleton, owns current theme, notifies listeners
 *   - Two pre-built themes: Deep Crimson, Emerald Night
 *   - All colors previously hardcoded in ModernLookAndFeel, PluginEditor,
 *     and AdvancedSpectrumDisplay are mapped to Theme roles
 */

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <mutex>

//==============================================================================
// Theme — Immutable set of visual tokens
//==============================================================================
struct Theme
{
    juce::String name;

    // ── Background System ──────────────────────────────────────────────────
    juce::Colour bgPrimary;        // Main background (deepest)
    juce::Colour bgSecondary;      // Panels, sidebars
    juce::Colour bgTertiary;       // Cards, elevated surfaces
    juce::Colour bgSurface;        // Interactive surface (buttons, inputs)
    juce::Colour bgHover;          // Hover state for surfaces

    // ── Text System ────────────────────────────────────────────────────────
    juce::Colour textPrimary;      // Main text (high contrast)
    juce::Colour textSecondary;    // Labels, captions
    juce::Colour textMuted;        // Disabled, placeholder
    juce::Colour textAccent;       // Highlighted text (matches accent)

    // ── Accent System ──────────────────────────────────────────────────────
    juce::Colour accent;           // Primary accent (signature color)
    juce::Colour accentBright;     // Brighter variant (hover, active)
    juce::Colour accentDim;        // Dimmer variant (inactive, subtle)
    juce::Colour accentGlow;       // Glow/emission color (for knobs, curves)

    // ── Semantic Colors ────────────────────────────────────────────────────
    juce::Colour warning;          // Attention (amber/gold)
    juce::Colour critical;         // Critical issues (was red CRITICAL)
    juce::Colour success;          // Applied/approved
    juce::Colour info;             // Informational

    // ── Spectrum System ────────────────────────────────────────────────────
    // GPU fill gradient (3-stop: bottom → mid → top)
    juce::Colour spectrumFillBottom;   // Deepest (near silence)
    juce::Colour spectrumFillMid;      // Mid energy
    juce::Colour spectrumFillTop;      // Peak energy (brightest)
    // GPU line
    juce::Colour spectrumLineColor;    // Spectrum outline
    // Post-EQ spectrum
    juce::Colour spectrumPostFillTop;
    juce::Colour spectrumPostFillBottom;
    juce::Colour spectrumPostLineColor;

    // ── Lava Strip (Heatmap) ───────────────────────────────────────────────
    juce::Colour lavaStripCold;        // Low energy zone
    juce::Colour lavaStripMid;         // Medium energy
    juce::Colour lavaStripHot;         // High energy (hot spot)
    float lavaStripHeight = 16.0f;     // Strip height in pixels

    // ── Ghost Curve (AI Suggestion) ────────────────────────────────────────
    juce::Colour ghostCurveColor;      // AI suggestion curve
    float ghostCurveAlpha = 0.35f;     // Base opacity
    float ghostCurveDash = 6.0f;       // Dash length

    // ── Ember/Bio Markers (AI Detection) ───────────────────────────────────
    juce::Colour markerCore;           // Inner marker color
    juce::Colour markerGlow;           // Outer glow
    juce::Colour markerApproved;       // Applied correction marker

    // ── EQ Curve ───────────────────────────────────────────────────────────
    juce::Colour eqCurveColor;         // Main EQ curve line
    juce::Colour eqCurveGlow;          // Glow layers
    juce::Colour eqCurveFill;          // Fill under curve

    // ── Knob System ────────────────────────────────────────────────────────
    juce::Colour knobBody;             // Main knob body
    juce::Colour knobRing;             // Outer ring
    juce::Colour knobIndicator;        // Value arc / filament
    juce::Colour knobIndicatorGlow;    // Indicator glow
    juce::Colour knobNotch;            // Grip notches

    // ── Band Colors (Monochromatic) ────────────────────────────────────────
    // Instead of rainbow, use intensity variations of the accent
    juce::Colour bandColors[12];

    // ── Borders & Dividers ─────────────────────────────────────────────────
    juce::Colour border;               // Subtle borders
    juce::Colour borderActive;         // Active/focused border
    juce::Colour divider;              // Panel dividers

    // ── Grid ───────────────────────────────────────────────────────────────
    juce::Colour gridMajor;            // Major grid lines
    juce::Colour gridMinor;            // Minor grid lines
    juce::Colour gridLabel;            // Frequency/dB labels
    juce::Colour gridLabelMajor;       // Major labels (100, 1k, 10k)

    // ── Toolbar ────────────────────────────────────────────────────────────
    juce::Colour toolbarBtnBg;         // Button background
    juce::Colour toolbarBtnBgActive;   // Active toggle
    juce::Colour toolbarBtnText;       // Button text
    juce::Colour toolbarBtnTextActive; // Active text

    // ── Tooltip ────────────────────────────────────────────────────────────
    juce::Colour tooltipBg;            // Tooltip background
    juce::Colour tooltipBorder;        // Tooltip border
    juce::Colour tooltipText;          // Tooltip text

    // ── Dynamic EQ Overlay ─────────────────────────────────────────────────
    juce::Colour dynOverlayFill;       // GR area fill
    juce::Colour dynOverlayCurve;      // Dynamic curve line

    // ── Header Bar ─────────────────────────────────────────────────────────
    juce::Colour headerBg;
    juce::Colour headerText;
    juce::Colour headerAccent;         // PRE/POST toggle active

    // ── AI Panel ───────────────────────────────────────────────────────────
    juce::Colour aiPanelBg;
    juce::Colour aiPanelCardBg;
    juce::Colour aiPanelCardBorder;
    juce::Colour aiPanelCardBorderCritical;
    juce::Colour aiPanelBadge;
    juce::Colour aiPanelBadgeText;
};

//==============================================================================
// Pre-built Themes
//==============================================================================
namespace Themes
{

inline Theme deepCrimson()
{
    Theme t;
    t.name = "Deep Crimson";

    // Background — obsidian with warm undertone
    t.bgPrimary        = juce::Colour (0xFF0A0810);
    t.bgSecondary      = juce::Colour (0xFF12101A);
    t.bgTertiary       = juce::Colour (0xFF1A1622);
    t.bgSurface        = juce::Colour (0xFF221E2A);
    t.bgHover          = juce::Colour (0xFF2A2434);

    // Text
    t.textPrimary      = juce::Colour (0xFFE8E0F0);
    t.textSecondary    = juce::Colour (0xFFB0A8C0);
    t.textMuted        = juce::Colour (0xFF706880);
    t.textAccent       = juce::Colour (0xFFFF6B6B);

    // Accent — cremisi
    t.accent           = juce::Colour (0xFFDC143C);
    t.accentBright     = juce::Colour (0xFFFF2D55);
    t.accentDim        = juce::Colour (0xFF8B0A1E);
    t.accentGlow       = juce::Colour (0xFFFF4466);

    // Semantic
    t.warning          = juce::Colour (0xFFD4A843);
    t.critical         = juce::Colour (0xFFDC143C);
    t.success          = juce::Colour (0xFF4CAF50);
    t.info             = juce::Colour (0xFF64B5F6);

    // Spectrum — lava gradient
    t.spectrumFillBottom   = juce::Colour (0x088B0000);  // burgundy, nearly transparent
    t.spectrumFillMid      = juce::Colour (0x40DC143C);  // cremisi, semi-transparent
    t.spectrumFillTop      = juce::Colour (0x66FF6B4A);  // rosa incandescente
    t.spectrumLineColor    = juce::Colour (0xCCFF8A70);  // bright warm line
    t.spectrumPostFillTop      = juce::Colour (0x44D4A843);
    t.spectrumPostFillBottom   = juce::Colour (0x08D4A843);
    t.spectrumPostLineColor    = juce::Colour (0xCCFFD700);

    // Lava Strip
    t.lavaStripCold    = juce::Colour (0xFF1A0A10);  // near-black burgundy
    t.lavaStripMid     = juce::Colour (0xFFDC143C);  // cremisi
    t.lavaStripHot     = juce::Colour (0xFFFFAA44);  // oro incandescente
    t.lavaStripHeight  = 16.0f;

    // Ghost Curve
    t.ghostCurveColor  = juce::Colour (0xFFFF6B6B);  // cremisi chiaro
    t.ghostCurveAlpha  = 0.35f;
    t.ghostCurveDash   = 6.0f;

    // Ember Markers
    t.markerCore       = juce::Colour (0xFFFF4444);  // brace ardente
    t.markerGlow       = juce::Colour (0xFFFF6B4A);  // glow caldo
    t.markerApproved   = juce::Colour (0xFF4CAF50);  // verde approvato

    // EQ Curve — warm white with cremisi glow
    t.eqCurveColor     = juce::Colour (0xFFF0E8E0);  // warm white
    t.eqCurveGlow      = juce::Colour (0xFFDC143C);  // cremisi glow
    t.eqCurveFill      = juce::Colour (0xFFDC143C);  // cremisi fill

    // Knob — rame spazzolato
    t.knobBody         = juce::Colour (0xFF2A2030);
    t.knobRing         = juce::Colour (0xFFB87333);  // copper
    t.knobIndicator    = juce::Colour (0xFFDC143C);  // cremisi filament
    t.knobIndicatorGlow = juce::Colour (0xFFFF4466);
    t.knobNotch        = juce::Colour (0xFF8B6914);  // dark copper

    // Band colors — monochromatic cremisi with intensity variations
    t.bandColors[0]  = juce::Colour (0xFFFF6B6B);  // light crimson
    t.bandColors[1]  = juce::Colour (0xFFDC143C);  // crimson
    t.bandColors[2]  = juce::Colour (0xFFFF4466);  // hot pink crimson
    t.bandColors[3]  = juce::Colour (0xFFE8445A);  // rose crimson
    t.bandColors[4]  = juce::Colour (0xFFCC1133);  // deep crimson
    t.bandColors[5]  = juce::Colour (0xFFFF8A70);  // salmon crimson
    t.bandColors[6]  = juce::Colour (0xFFD4556B);  // muted crimson
    t.bandColors[7]  = juce::Colour (0xFFFF5577);  // vivid crimson
    t.bandColors[8]  = juce::Colour (0xFFBB2244);  // dark crimson
    t.bandColors[9]  = juce::Colour (0xFFFF7788);  // pink crimson
    t.bandColors[10] = juce::Colour (0xFFAA1133);  // wine crimson
    t.bandColors[11] = juce::Colour (0xFFFF9999);  // pale crimson

    // Borders
    t.border           = juce::Colour (0xFF2A2234);
    t.borderActive     = juce::Colour (0xFFDC143C);
    t.divider          = juce::Colour (0xFF1E1828);

    // Grid
    t.gridMajor        = juce::Colour (0xFF2A2234);
    t.gridMinor        = juce::Colour (0xFF1E1828);
    t.gridLabel        = juce::Colour (0xFF504860);
    t.gridLabelMajor   = juce::Colour (0xFF706880);

    // Toolbar
    t.toolbarBtnBg         = juce::Colour (0x40201828);
    t.toolbarBtnBgActive   = juce::Colour (0x80DC143C);
    t.toolbarBtnText       = juce::Colour (0xBBB0A8C0);
    t.toolbarBtnTextActive = juce::Colours::white;

    // Tooltip — vetro affumicato
    t.tooltipBg        = juce::Colour (0xF01A1622);
    t.tooltipBorder    = juce::Colour (0xFFDC143C);
    t.tooltipText      = juce::Colour (0xFFE8E0F0);

    // Dynamic EQ overlay
    t.dynOverlayFill   = juce::Colour (0xFFD4A843);  // warm amber
    t.dynOverlayCurve  = juce::Colours::white;

    // Header
    t.headerBg         = juce::Colour (0xFF0E0C14);
    t.headerText       = juce::Colour (0xFFB0A8C0);
    t.headerAccent     = juce::Colour (0xFFDC143C);

    // AI Panel
    t.aiPanelBg                = juce::Colour (0xFF100E18);
    t.aiPanelCardBg            = juce::Colour (0xFF1A1622);
    t.aiPanelCardBorder        = juce::Colour (0xFFD4A843);  // gold attention
    t.aiPanelCardBorderCritical = juce::Colour (0xFFDC143C); // cremisi critical
    t.aiPanelBadge             = juce::Colour (0xFFDC143C);
    t.aiPanelBadgeText         = juce::Colours::white;

    return t;
}

inline Theme emeraldNight()
{
    Theme t;
    t.name = "Emerald Night";

    // Background — deep forest black
    t.bgPrimary        = juce::Colour (0xFF080C0A);
    t.bgSecondary      = juce::Colour (0xFF0E1410);
    t.bgTertiary       = juce::Colour (0xFF141C16);
    t.bgSurface        = juce::Colour (0xFF1C2620);
    t.bgHover          = juce::Colour (0xFF243028);

    // Text
    t.textPrimary      = juce::Colour (0xFFE0F0E8);
    t.textSecondary    = juce::Colour (0xFFA8C0B0);
    t.textMuted        = juce::Colour (0xFF688070);
    t.textAccent       = juce::Colour (0xFF00E896);

    // Accent — smeraldo bioluminescente
    t.accent           = juce::Colour (0xFF00C896);
    t.accentBright     = juce::Colour (0xFF00FFB4);
    t.accentDim        = juce::Colour (0xFF006B50);
    t.accentGlow       = juce::Colour (0xFF00E8A0);

    // Semantic
    t.warning          = juce::Colour (0xFFD4A843);
    t.critical         = juce::Colour (0xFFFF6B6B);
    t.success          = juce::Colour (0xFF00E896);
    t.info             = juce::Colour (0xFF64D5F6);

    // Spectrum — bioluminescence gradient
    t.spectrumFillBottom   = juce::Colour (0x08004D40);  // deep teal, nearly transparent
    t.spectrumFillMid      = juce::Colour (0x4000C896);  // emerald, semi-transparent
    t.spectrumFillTop      = juce::Colour (0x6600FFB4);  // bright mint
    t.spectrumLineColor    = juce::Colour (0xCC80FFD4);  // bright emerald line
    t.spectrumPostFillTop      = juce::Colour (0x44D4A843);
    t.spectrumPostFillBottom   = juce::Colour (0x08D4A843);
    t.spectrumPostLineColor    = juce::Colour (0xCCFFD700);

    // Lava Strip → Bio Strip
    t.lavaStripCold    = juce::Colour (0xFF0A1410);  // near-black forest
    t.lavaStripMid     = juce::Colour (0xFF00C896);  // emerald
    t.lavaStripHot     = juce::Colour (0xFF80FFD4);  // bright mint
    t.lavaStripHeight  = 16.0f;

    // Ghost Curve
    t.ghostCurveColor  = juce::Colour (0xFF00E896);
    t.ghostCurveAlpha  = 0.35f;
    t.ghostCurveDash   = 6.0f;

    // Bio Markers
    t.markerCore       = juce::Colour (0xFF00E896);
    t.markerGlow       = juce::Colour (0xFF00FFB4);
    t.markerApproved   = juce::Colour (0xFF80FFD4);

    // EQ Curve — cool white with emerald glow
    t.eqCurveColor     = juce::Colour (0xFFE0F0F0);
    t.eqCurveGlow      = juce::Colour (0xFF00C896);
    t.eqCurveFill      = juce::Colour (0xFF00C896);

    // Knob — jade
    t.knobBody         = juce::Colour (0xFF1C2620);
    t.knobRing         = juce::Colour (0xFF4A8070);  // jade
    t.knobIndicator    = juce::Colour (0xFF00C896);  // emerald filament
    t.knobIndicatorGlow = juce::Colour (0xFF00E8A0);
    t.knobNotch        = juce::Colour (0xFF2A4A3A);  // dark jade

    // Band colors — monochromatic emerald
    t.bandColors[0]  = juce::Colour (0xFF00E896);
    t.bandColors[1]  = juce::Colour (0xFF00C896);
    t.bandColors[2]  = juce::Colour (0xFF00FFB4);
    t.bandColors[3]  = juce::Colour (0xFF20D8A0);
    t.bandColors[4]  = juce::Colour (0xFF00A878);
    t.bandColors[5]  = juce::Colour (0xFF40F0B0);
    t.bandColors[6]  = juce::Colour (0xFF00B888);
    t.bandColors[7]  = juce::Colour (0xFF60FFD0);
    t.bandColors[8]  = juce::Colour (0xFF008868);
    t.bandColors[9]  = juce::Colour (0xFF80FFD4);
    t.bandColors[10] = juce::Colour (0xFF006850);
    t.bandColors[11] = juce::Colour (0xFFA0FFE0);

    // Borders
    t.border           = juce::Colour (0xFF1C2A22);
    t.borderActive     = juce::Colour (0xFF00C896);
    t.divider          = juce::Colour (0xFF142018);

    // Grid
    t.gridMajor        = juce::Colour (0xFF1C2A22);
    t.gridMinor        = juce::Colour (0xFF142018);
    t.gridLabel        = juce::Colour (0xFF486050);
    t.gridLabelMajor   = juce::Colour (0xFF688070);

    // Toolbar
    t.toolbarBtnBg         = juce::Colour (0x400E1810);
    t.toolbarBtnBgActive   = juce::Colour (0x8000C896);
    t.toolbarBtnText       = juce::Colour (0xBBA8C0B0);
    t.toolbarBtnTextActive = juce::Colours::white;

    // Tooltip
    t.tooltipBg        = juce::Colour (0xF0141C16);
    t.tooltipBorder    = juce::Colour (0xFF00C896);
    t.tooltipText      = juce::Colour (0xFFE0F0E8);

    // Dynamic EQ overlay
    t.dynOverlayFill   = juce::Colour (0xFFD4A843);
    t.dynOverlayCurve  = juce::Colours::white;

    // Header
    t.headerBg         = juce::Colour (0xFF0A100C);
    t.headerText       = juce::Colour (0xFFA8C0B0);
    t.headerAccent     = juce::Colour (0xFF00C896);

    // AI Panel
    t.aiPanelBg                = juce::Colour (0xFF0C120E);
    t.aiPanelCardBg            = juce::Colour (0xFF141C16);
    t.aiPanelCardBorder        = juce::Colour (0xFFD4A843);
    t.aiPanelCardBorderCritical = juce::Colour (0xFFFF6B6B);
    t.aiPanelBadge             = juce::Colour (0xFF00C896);
    t.aiPanelBadgeText         = juce::Colours::white;

    return t;
}

} // namespace Themes

//==============================================================================
// ThemeManager — Singleton for runtime theme switching
//==============================================================================
class ThemeManager
{
public:
    static ThemeManager& getInstance()
    {
        static ThemeManager instance;
        return instance;
    }

    enum class ThemeID { DeepCrimson, EmeraldNight };

    const Theme& getTheme() const noexcept { return currentTheme; }
    ThemeID getCurrentThemeID() const noexcept { return currentID; }

    void setTheme (ThemeID id)
    {
        std::lock_guard<std::mutex> lock (themeMutex);
        currentID = id;
        switch (id)
        {
            case ThemeID::DeepCrimson:  currentTheme = Themes::deepCrimson();  break;
            case ThemeID::EmeraldNight: currentTheme = Themes::emeraldNight(); break;
        }
        notifyListeners();
    }

    // Listener interface for components that need to repaint on theme change
    using Listener = std::function<void()>;

    void addListener (Listener l)
    {
        std::lock_guard<std::mutex> lock (themeMutex);
        listeners.push_back (std::move (l));
    }

    // Convenience: get band color with wrapping
    juce::Colour getBandColor (int index) const
    {
        return currentTheme.bandColors[index % 12];
    }

private:
    ThemeManager() : currentTheme (Themes::deepCrimson()), currentID (ThemeID::DeepCrimson) {}

    void notifyListeners()
    {
        for (auto& l : listeners)
            if (l) l();
    }

    Theme currentTheme;
    ThemeID currentID;
    std::vector<Listener> listeners;
    std::mutex themeMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThemeManager)
};
