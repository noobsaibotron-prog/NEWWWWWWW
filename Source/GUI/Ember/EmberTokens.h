#pragma once
#include <juce_graphics/juce_graphics.h>

namespace EmberTokens
{
    // Carbon hierarchy
    inline const juce::Colour bg       { 0xff090A0B };
    inline const juce::Colour chrome   { 0xff070809 };
    inline const juce::Colour raised   { 0xff101214 };
    inline const juce::Colour sunken   { 0xff060708 };
    inline const juce::Colour hairline { 0xff25292E };

    // Semantic colours
    inline const juce::Colour state     { 0xffE8EAEC }; // committed DSP truth
    inline const juce::Colour stateHi   { 0xffF6F7F8 }; // tiny selected highlights only
    inline const juce::Colour dim       { 0xff858B91 };
    inline const juce::Colour mute      { 0xff50565C };
    inline const juce::Colour signal    { 0xff56C7C6 }; // live measurement
    inline const juce::Colour intent    { 0xffD49A32 }; // pending semantic intent, never decoration
    inline const juce::Colour reference { 0xff6E8A9A }; // comparison / memory

    // Aliases, so existing call sites keep their names
    inline const juce::Colour& text  = state;
    inline const juce::Colour& cyan  = signal;
    inline const juce::Colour& meter = signal;
    inline const juce::Colour& ref   = reference;

    // Grid
    constexpr float gridPrimaryAlpha   = 0.070f;
    constexpr float gridSecondaryAlpha = 0.032f;
    constexpr float gridZeroAlpha      = 0.105f;
    // Spectrum
    constexpr float spectrumStrokeWidth  = 0.95f;
    constexpr float spectrumLineAlpha    = 0.58f;
    constexpr float spectrumFillTopAlpha = 0.105f;
    constexpr float spectrumFillBotAlpha = 0.012f;
    // Curves
    constexpr float actualCurveWidth    = 1.15f;
    constexpr float actualCurveAlpha    = 0.96f;
    constexpr float intentCurveWidth    = 1.30f;
    constexpr float intentCurveAlpha    = 0.90f;
    constexpr float intentNodeAlpha     = 0.95f;
    constexpr float intentReadoutAlpha  = 0.92f;
    constexpr float referenceCurveWidth = 0.90f;
    constexpr float referenceCurveAlpha = 0.60f;
    constexpr float referenceDashLength = 2.0f;
    constexpr float referenceGapLength  = 3.0f;
    // Legibility floors on the final alpha, after the state profile
    constexpr float actualCurveMinAlpha  = 0.90f;
    constexpr float spectrumLineMinAlpha = 0.40f;
    // Nodes: drawn size only, the hit radius stays in EmberGraph
    constexpr float nodeIdleRadius        = 3.25f;
    constexpr float nodeSelectedRadius    = 4.25f;
    constexpr float nodeSelectedRingR     = 8.25f;
    constexpr float nodeSelectedRingWidth = 1.0f;
    constexpr float nodeSelectedRingAlpha = 0.70f;
    // Surfaces
    constexpr float inspectorSurfaceAlpha = 0.94f;
    constexpr float inspectorBorderAlpha  = 0.70f;
    constexpr float radiusInspector       = 6.0f;
    constexpr float strokeHairline        = 1.0f;
    // Other opacity
    constexpr float alphaClimate        = 0.85f;
    constexpr float alphaMeterFill      = 0.85f;
    constexpr float alphaMeterPeak      = 0.55f;
    constexpr float alphaIntentFilament = 0.85f;
    constexpr float alphaTextSelection  = 0.35f;
    // Motion
    constexpr double motionApplyMs       = 280.0;
    constexpr float  motionBarEaseSec    = 0.220f;
    constexpr float  visualFocusMs       = 160.0f;
    constexpr float  visualContextMs     = 220.0f;
    constexpr float  visualCommitMs      = 320.0f; // reserved; Apply timing is unchanged
    constexpr float  visualProfileTauSec = 0.180f;

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
