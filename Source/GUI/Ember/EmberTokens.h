#pragma once
#include <juce_graphics/juce_graphics.h>

namespace EmberTokens
{
    inline const juce::Colour bg       { 0xff0A0B0E };
    inline const juce::Colour raised   { 0xff1A1B1F };
    inline const juce::Colour sunken   { 0xff07080A };
    inline const juce::Colour hairline { 0xff2A2B31 };
    inline const juce::Colour text     { 0xffF4F4F2 };
    inline const juce::Colour dim      { 0xff8A8B90 };
    inline const juce::Colour mute     { 0xff55565C };
    inline const juce::Colour intent   { 0xffFF9D2E };
    inline const juce::Colour gold     { 0xffFFD27A };
    inline const juce::Colour cyan     { 0xff2ED6E6 };
    inline const juce::Colour ref      { 0xff3AA8B8 };
    inline const juce::Colour meter    { 0xff2EC8D4 };
    inline const juce::Colour grid     { 0x0FF4F4F2 }; // ~0.06 alpha white
    inline const juce::Colour glass    { 0xE6121318 };

    // Opacity
    constexpr float alphaSpectrumFillTop    = 0.14f;
    constexpr float alphaSpectrumFillBottom = 0.02f;
    constexpr float alphaSpectrumLine       = 0.72f;
    constexpr float alphaGhostCurve         = 0.85f;
    constexpr float alphaGhostNode          = 0.95f;
    constexpr float alphaGhostReadout       = 0.92f;
    constexpr float alphaMatchCurve         = 0.55f;
    constexpr float alphaSelectedRing       = 0.35f;
    constexpr float alphaClimate            = 0.85f;
    constexpr float alphaMeterFill          = 0.85f;
    constexpr float alphaMeterPeak          = 0.55f;
    constexpr float alphaIntentFilament     = 0.85f;
    constexpr float alphaTextSelection      = 0.35f;
    // Stroke width, px at 100% scale
    constexpr float strokeSpectrum     = 1.1f;
    constexpr float strokeFactCurve    = 1.6f;
    constexpr float strokeGhostCurve   = 1.2f;
    constexpr float strokeMatchCurve   = 1.2f;
    constexpr float strokeSelectedRing = 1.0f;
    constexpr float strokeHairline     = 1.0f;
    // Corner radius
    constexpr float radiusInspector = 6.0f;
    // Motion
    constexpr double motionApplyMs    = 280.0;
    constexpr float  motionBarEaseSec = 0.220f;

    constexpr int headerH = 36;
    constexpr int barRiposoH = 20;
    constexpr int barActiveH = 68;
    constexpr int overflowW = 280;
    constexpr int meterW = 32;
    constexpr int padL = 44;
    constexpr int padR = 12;
    constexpr int padT = 14;
    constexpr int padB = 22;
    constexpr int climateH = 8;
    constexpr int inspectorW = 168;

    constexpr float curveMinDb = -12.0f;
    constexpr float curveMaxDb =  12.0f;
    constexpr float specMinDb  = -48.0f;
    constexpr float specMaxDb  =   6.0f;
    constexpr float minHz = 20.0f;
    constexpr float maxHz = 20000.0f;
}

enum class EmberUiState
{
    Riposo,
    Frase,
    Apply,
    Nodo,
    Match
};

struct EmberGhostBand
{
    juce::String chip;
    int type = 2; // Peak
    float hz = 1000.0f;
    float db = 0.0f;
    float q = 1.0f;
};
