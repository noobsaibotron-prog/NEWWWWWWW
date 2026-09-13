#pragma once
#include <algorithm>
#include <cmath>
#include "EmberTokens.h"

/** Ember Visual Language v1: the one visual reading of each EmberUiState.
    Every value multiplies a base alpha. None of them creates or removes an
    object: ghosts, the reference curve and the inspector keep their own
    visibility rules. */
struct EmberVisualProfile
{
    float grid        = 1.0f;
    float spectrum    = 1.0f;
    float actualCurve = 1.0f;
    float idleNodes   = 1.0f;
    float selected    = 1.0f;
    float ghost       = 1.0f;
    float reference   = 1.0f;
    float inspector   = 1.0f;
    float bar         = 1.0f;
    float meter       = 1.0f;
    float header      = 1.0f;
};

inline EmberVisualProfile emberVisualProfileFor(EmberUiState s) noexcept
{
    // Inspector is 1.00 in Riposo because selection alone decides whether it is shown.
    // Painted by Ember itself: grid, spectrum, curves, nodes, ghost, reference, meter and
    // the bar's intent filament. inspector and header are not consumed yet: JUCE controls
    // stay fully legible, and Component::setAlpha would open a transparency layer per repaint.
    //                                  grid   spectrum curve  idle   selected ghost reference inspector bar meter header
    switch (s)
    {
        case EmberUiState::Riposo: return { 1.00f, 0.82f, 0.96f, 0.78f, 1.00f, 0.00f, 0.00f, 1.00f, 0.48f, 0.72f, 0.62f };
        case EmberUiState::Nodo:   return { 0.82f, 0.60f, 1.00f, 0.48f, 1.00f, 0.00f, 0.00f, 1.00f, 0.42f, 0.58f, 0.52f };
        case EmberUiState::Frase:  return { 0.68f, 0.50f, 0.80f, 0.44f, 0.88f, 1.00f, 0.00f, 0.72f, 1.00f, 0.48f, 0.46f };
        case EmberUiState::Apply:  return { 0.60f, 0.42f, 0.86f, 0.38f, 0.86f, 1.00f, 0.00f, 0.55f, 0.82f, 0.44f, 0.44f };
        case EmberUiState::Match:  return { 0.74f, 0.54f, 0.90f, 0.52f, 0.90f, 0.00f, 1.00f, 0.72f, 0.92f, 0.52f, 0.50f };
    }
    return {};
}

/** Moves every field of `current` a fraction `a` toward `target`.
    Returns the largest distance still left. */
inline float emberApproachProfile(EmberVisualProfile& current, const EmberVisualProfile& target, float a) noexcept
{
    float left = 0.0f;
    const auto step = [&](float& c, float t)
    {
        c += (t - c) * a;
        left = std::max(left, std::abs(t - c));
    };
    step(current.grid, target.grid);
    step(current.spectrum, target.spectrum);
    step(current.actualCurve, target.actualCurve);
    step(current.idleNodes, target.idleNodes);
    step(current.selected, target.selected);
    step(current.ghost, target.ghost);
    step(current.reference, target.reference);
    step(current.inspector, target.inspector);
    step(current.bar, target.bar);
    step(current.meter, target.meter);
    step(current.header, target.header);
    return left;
}
