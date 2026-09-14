#pragma once
#include <cmath>
#include <regex>
#include "EmberTokens.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace EmberPhrase
{

/** Lower-case words of a phrase. ASCII letters and digits belong to a word. An apostrophe (ASCII ' or
    U+2019) between two word characters stays in the word, normalised to ': "vocal's" and "vocal’s" are one
    word. Every other character (space, punctuation, brackets, quotes, an apostrophe at a word edge, any other
    non-ASCII code point) is a boundary, whatever the locale. The vocabulary below is ASCII English. */
inline juce::StringArray words(const juce::String& text)
{
    const auto isWordChar = [](juce::juce_wchar c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    };

    juce::StringArray out;
    juce::String current;
    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const juce::juce_wchar c = *p;
        if (isWordChar(c))
        {
            current += juce::CharacterFunctions::toLowerCase(c);
            continue;
        }

        auto next = p;
        ++next;
        const bool apostrophe = c == '\'' || c == (juce::juce_wchar) 0x2019;
        if (apostrophe && current.isNotEmpty() && ! next.isEmpty() && isWordChar(*next))
        {
            current += "'";
            continue;
        }

        if (current.isNotEmpty())
        {
            out.add(current);
            current.clear();
        }
    }
    if (current.isNotEmpty())
        out.add(current);
    return out;
}

/** Whole-word match against an explicit vocabulary: "muddy" hits because it is listed, "hair" never hits "air". */
inline bool tokenHit(const juce::String& text, const juce::StringArray& keys)
{
    for (const auto& w : words(text))
        if (keys.contains(w))
            return true;
    return false;
}

inline juce::String formatHzChip(float hz)
{
    if (hz >= 1000.0f)
    {
        const float k = hz / 1000.0f;
        if (std::abs(k - std::round(k)) < 0.05f)
            return juce::String((int) std::round(k)) + "k";
        return juce::String(k, 1) + "k";
    }
    return juce::String((int) std::round(hz));
}

inline juce::String formatDbChip(float db)
{
    juce::String s;
    if (db > 0.0f)
        s = "+";
    else if (db < 0.0f)
        s = juce::CharPointer_UTF8("\xe2\x88\x92"); // −
    s += juce::String(std::abs(db), 1);
    return s;
}

inline int inferAbsoluteType(const juce::String& text) noexcept
{
    if (text.contains("low shelf") || text.contains("lowshelf"))
        return 1; // LowShelf
    if (text.contains("high shelf") || text.contains("highshelf"))
        return 3; // HighShelf
    if (text.contains("low cut") || text.contains("lowcut") || text.contains("highpass") || text.contains("high pass"))
        return 0; // LowCut
    if (text.contains("high cut") || text.contains("highcut") || text.contains("lowpass") || text.contains("low pass"))
        return 4; // HighCut
    if (text.contains("notch"))
        return 5;
    if (text.contains("bandpass") || text.contains("band pass"))
        return 6;
    // peak / bell / default absolute
    return 2;
}

inline const char* typeChipStem(int type) noexcept
{
    switch (type)
    {
        case 0: return "LC";
        case 1: return "LS";
        case 3: return "HS";
        case 4: return "HC";
        case 5: return "Notch";
        case 6: return "BP";
        default: return "Peak";
    }
}

/** Absolute numeric phrase: "peak at 2khz with -4db ... 2.5 q".
    Same vector feeds amber ghosts and Apply (via setGhosts → getEffectiveGhosts). */
inline std::vector<EmberGhostBand> parseAbsolute(const juce::String& text)
{
    // Require an explicit frequency unit so bare numbers (e.g. Q) are not mistaken for Hz.
    static const std::regex freqRe(R"((\d+(?:\.\d+)?)\s*(khz|kh|k|hz)\b)",
                                   std::regex_constants::icase);
    static const std::regex dbRe(R"(([+-]?\d+(?:\.\d+)?)\s*db\b)",
                                 std::regex_constants::icase);
    // "q 2.5" / "q=2.5" / "2.5 q" / "q2.5"
    static const std::regex qRe(R"((?:\bq\s*[:=]?\s*(\d+(?:\.\d+)?))|(?:(\d+(?:\.\d+)?)\s*q\b))",
                                std::regex_constants::icase);

    const std::string s = text.toStdString();
    std::smatch fm, dm, qm;
    if (! std::regex_search(s, fm, freqRe))
        return {};
    if (! std::regex_search(s, dm, dbRe))
        return {};

    float hz = 0.0f;
    try { hz = std::stof(fm[1].str()); }
    catch (...) { return {}; }
    const auto unit = juce::String(fm[2].str()).toLowerCase();
    if (unit == "khz" || unit == "kh" || unit == "k")
        hz *= 1000.0f;
    hz = juce::jlimit(20.0f, 20000.0f, hz);

    float db = 0.0f;
    try { db = std::stof(dm[1].str()); }
    catch (...) { return {}; }
    db = juce::jlimit(-24.0f, 24.0f, db);

    float q = 1.0f;
    if (std::regex_search(s, qm, qRe))
    {
        const auto qStr = qm[1].matched ? qm[1].str() : qm[2].str();
        try
        {
            q = std::stof(qStr);
            q = juce::jlimit(0.1f, 10.0f, q);
        }
        catch (...) { q = 1.0f; }
    }

    const int type = inferAbsoluteType(text);

    EmberGhostBand g;
    g.type = type;
    g.hz = hz;
    g.db = db;
    g.q = q;
    // Chip mirrors on-screen amber readout (DoD: Peak 2000 / −4.0 / 2.50).
    g.chip = juce::String(typeChipStem(type)) + " "
           + formatHzChip(hz) + "/" + formatDbChip(db);

    std::vector<EmberGhostBand> out;
    out.push_back(g);
    return out;
}

inline std::vector<EmberGhostBand> parse(const juce::String& raw)
{
    auto text = raw.trim().toLowerCase();
    if (text.isEmpty())
        return {};

    // Absolute numeric phrases win — one parse path for preview + Apply.
    auto absolute = parseAbsolute(text);
    if (! absolute.empty())
        return absolute;

    enum class ClausePolarity { Default, Boost, Cut };

    auto splitClauses = [](const juce::String& phrase)
    {
        auto normalized = phrase.replaceCharacter(',', '|').replaceCharacter(';', '|');
        normalized = normalized.replace(" and ", " | ")
                               .replace(" but ", " | ")
                               .replace(" then ", " | ");
        auto clauses = juce::StringArray::fromTokens(normalized, "|", "");
        clauses.trim();
        clauses.removeEmptyStrings();
        return clauses;
    };

    auto polarityForClause = [](const juce::String& clause)
    {
        for (const auto& w : words(clause))
        {
            if (w == "less" || w == "cut" || w == "tame")
                return ClausePolarity::Cut;
            if (w == "more" || w == "add" || w == "boost")
                return ClausePolarity::Boost;
        }
        return ClausePolarity::Default;
    };

    const auto clauses = splitClauses(text);

    struct Rule
    {
        juce::StringArray keys;
        const char* chip;
        int type;
        float hz, db, q;
    };

    const Rule rules[] = {
        // Explicit vocabulary, whole words only, no roots: a word that merely starts with or contains
        // a key (hair, airport, mudguard, brighton, pierce, messy, clearly) never fires.
        { juce::StringArray{ "air", "airy", "airier", "airiest", "airiness",
                             "bright", "brighter", "brightest", "brightness", "brighten",
                             "open", "openness", "sparkle", "sparkly", "sparkling" },        "air+",    3, 11200.f,  2.4f, 0.85f },
        { juce::StringArray{ "warm", "warmer", "warmest", "warmth", "warming",
                             "body", "bodied", "round", "rounder", "rounded", "roundness" }, "warmth+", 1,   180.f,  1.8f, 0.72f },
        { juce::StringArray{ "harsh", "harsher", "harshest", "harshness",
                             "sibilance", "sibilant", "sibilants", "essy", "piercing" },     "harsh-",  2,  6500.f, -2.8f, 1.35f },
        { juce::StringArray{ "mud", "muddy", "muddier", "muddiest", "muddiness",
                             "box", "boxy", "boxier", "boxiness" },                          "mud-",    2,   280.f, -2.2f, 1.10f },
        { juce::StringArray{ "punch", "punches", "punchy", "punchier", "punchiest", "punchiness",
                             "thump", "thumpy", "thumping", "attack" },                      "punch+",  2,    95.f,  1.7f, 0.90f },
        { juce::StringArray{ "clear", "clearer", "clearest", "clarity", "presence" },        "clear+",  2,  3200.f,  1.4f, 0.95f },
    };

    std::vector<EmberGhostBand> out;
    for (const auto& rule : rules)
    {
        if (out.size() >= 2)
            break;
        juce::String matchedClause;
        for (const auto& clause : clauses)
        {
            if (tokenHit(clause, rule.keys))
            {
                matchedClause = clause;
                break;
            }
        }
        if (matchedClause.isEmpty())
            continue;

        EmberGhostBand g;
        g.chip = rule.chip;
        g.type = rule.type;
        g.hz = rule.hz;
        g.db = rule.db;
        g.q = rule.q;
        const auto polarity = polarityForClause(matchedClause);
        if (polarity == ClausePolarity::Cut)
            g.db = -std::abs(g.db);
        else if (polarity == ClausePolarity::Boost)
            g.db = std::abs(g.db);
        out.push_back(g);
    }
    return out;
}

} // namespace EmberPhrase
