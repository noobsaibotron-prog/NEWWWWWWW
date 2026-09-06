#pragma once
#include <cmath>
#include "EmberTokens.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace EmberPhrase
{

inline bool tokenHit(const juce::String& text, const juce::StringArray& keys)
{
    auto words = juce::StringArray::fromTokens(text, " \t\n\r,.;:!?/+-", "");
    for (auto& w : words)
    {
        w = w.trim().toLowerCase();
        for (const auto& k : keys)
            if (w.contains(k) || w == k)
                return true;
    }
    // also allow substring match on full phrase for short stems
    for (const auto& k : keys)
        if (text.contains(k))
            return true;
    return false;
}

inline std::vector<EmberGhostBand> parse(const juce::String& raw)
{
    auto text = raw.trim().toLowerCase();
    if (text.isEmpty())
        return {};

    const bool invert = text.contains("less") || text.contains("cut") || text.contains("tame");

    struct Rule
    {
        juce::StringArray keys;
        const char* chip;
        int type;
        float hz, db, q;
    };

    const Rule rules[] = {
        { juce::StringArray{ "air", "bright", "open", "sparkle" }, "air+", 3, 11200.f,  2.4f, 0.85f },
        { juce::StringArray{ "warm", "warmer", "body", "round" },  "warmth+", 1, 180.f, 1.8f, 0.72f },
        { juce::StringArray{ "harsh", "sibil", "essy", "pierc" },  "harsh-", 2, 6500.f, -2.8f, 1.35f },
        { juce::StringArray{ "mud", "boxy", "box" },               "mud-", 2, 280.f, -2.2f, 1.10f },
        { juce::StringArray{ "punch", "thump", "attack" },         "punch+", 2, 95.f, 1.7f, 0.90f },
        { juce::StringArray{ "clear", "clarity", "presence" },     "clear+", 2, 3200.f, 1.4f, 0.95f },
    };

    std::vector<EmberGhostBand> out;
    for (const auto& rule : rules)
    {
        if (out.size() >= 2)
            break;
        if (! tokenHit(text, rule.keys))
            continue;

        EmberGhostBand g;
        g.chip = rule.chip;
        g.type = rule.type;
        g.hz = rule.hz;
        g.db = rule.db;
        g.q = rule.q;
        if (invert && g.db > 0.0f)
            g.db = -g.db;
        out.push_back(g);
    }
    return out;
}

} // namespace EmberPhrase
