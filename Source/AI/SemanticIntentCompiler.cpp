#include "SemanticIntentCompiler.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace AIEQPerceptual
{
namespace
{
struct Alias
{
    const char* phrase;
    SemanticDimension dimension;
    int polarity; // +1 means the phrase names the positive dimension direction.
    float defaultAmount;
};

// Long phrases deliberately precede their shorter relatives. Word-boundary
// checks prevent "bright" from matching inside "brighter".
constexpr std::array<Alias, 87> kAliases {{
    { "low end",       SemanticDimension::Weight,      +1, 0.68f },
    { "low-end",       SemanticDimension::Weight,      +1, 0.68f },
    { "bass weight",   SemanticDimension::Weight,      +1, 0.68f },
    { "high end",      SemanticDimension::Brightness,  +1, 0.62f },
    { "top end",       SemanticDimension::Brightness,  +1, 0.62f },

    { "brighter",      SemanticDimension::Brightness,  +1, 0.68f },
    { "brightness",    SemanticDimension::Brightness,  +1, 0.64f },
    { "brilliance",    SemanticDimension::Brightness,  +1, 0.62f },
    { "sparkle",       SemanticDimension::Brightness,  +1, 0.60f },
    { "bright",        SemanticDimension::Brightness,  +1, 0.64f },
    { "airy",          SemanticDimension::Brightness,  +1, 0.62f },
    { "air",           SemanticDimension::Brightness,  +1, 0.62f },
    { "open",          SemanticDimension::Brightness,  +1, 0.58f },
    { "darker",        SemanticDimension::Brightness,  -1, 0.68f },
    { "darkness",      SemanticDimension::Brightness,  -1, 0.64f },
    { "dull",          SemanticDimension::Brightness,  -1, 0.62f },
    { "dark",          SemanticDimension::Brightness,  -1, 0.64f },
    { "brillante",     SemanticDimension::Brightness,  +1, 0.64f },
    { "brillantezza",  SemanticDimension::Brightness,  +1, 0.64f },
    { "aria",          SemanticDimension::Brightness,  +1, 0.62f },
    { "scuro",         SemanticDimension::Brightness,  -1, 0.64f },

    { "warmer",        SemanticDimension::Warmth,      +1, 0.68f },
    { "warmth",        SemanticDimension::Warmth,      +1, 0.66f },
    { "fuller",        SemanticDimension::Warmth,      +1, 0.64f },
    { "thicker",       SemanticDimension::Warmth,      +1, 0.64f },
    { "richer",        SemanticDimension::Warmth,      +1, 0.60f },
    { "warm",          SemanticDimension::Warmth,      +1, 0.64f },
    { "body",          SemanticDimension::Warmth,      +1, 0.62f },
    { "full",          SemanticDimension::Warmth,      +1, 0.60f },
    { "thick",         SemanticDimension::Warmth,      +1, 0.60f },
    { "thinness",      SemanticDimension::Warmth,      -1, 0.66f },
    { "thinner",       SemanticDimension::Warmth,      -1, 0.68f },
    { "thin",          SemanticDimension::Warmth,      -1, 0.64f },
    { "calore",        SemanticDimension::Warmth,      +1, 0.66f },
    { "caldo",         SemanticDimension::Warmth,      +1, 0.64f },
    { "corpo",         SemanticDimension::Warmth,      +1, 0.62f },
    { "pieno",         SemanticDimension::Warmth,      +1, 0.60f },
    { "sottile",       SemanticDimension::Warmth,      -1, 0.64f },

    { "clearer",       SemanticDimension::Clarity,     +1, 0.68f },
    { "clarity",       SemanticDimension::Clarity,     +1, 0.66f },
    { "cleaner",       SemanticDimension::Clarity,     +1, 0.66f },
    { "definition",    SemanticDimension::Clarity,     +1, 0.60f },
    { "defined",       SemanticDimension::Clarity,     +1, 0.60f },
    { "clear",         SemanticDimension::Clarity,     +1, 0.64f },
    { "clean",         SemanticDimension::Clarity,     +1, 0.62f },
    { "muddiness",     SemanticDimension::Clarity,     -1, 0.70f },
    { "muddier",       SemanticDimension::Clarity,     -1, 0.70f },
    { "muddy",         SemanticDimension::Clarity,     -1, 0.68f },
    { "mud",           SemanticDimension::Clarity,     -1, 0.68f },
    { "chiarezza",     SemanticDimension::Clarity,     +1, 0.66f },
    { "pulito",        SemanticDimension::Clarity,     +1, 0.62f },
    { "definito",      SemanticDimension::Clarity,     +1, 0.60f },
    { "impastato",     SemanticDimension::Clarity,     -1, 0.68f },

    { "presence",      SemanticDimension::Presence,    +1, 0.64f },
    { "present",       SemanticDimension::Presence,    +1, 0.62f },
    { "forward",       SemanticDimension::Presence,    +1, 0.62f },
    { "articulation",  SemanticDimension::Presence,    +1, 0.60f },
    { "recessed",      SemanticDimension::Presence,    -1, 0.62f },
    { "distant",       SemanticDimension::Presence,    -1, 0.58f },
    { "presenza",      SemanticDimension::Presence,    +1, 0.64f },

    { "smoothness",    SemanticDimension::Smoothness,  +1, 0.66f },
    { "smoother",      SemanticDimension::Smoothness,  +1, 0.68f },
    { "smooth",        SemanticDimension::Smoothness,  +1, 0.64f },
    { "silky",         SemanticDimension::Smoothness,  +1, 0.60f },
    { "gentle",        SemanticDimension::Smoothness,  +1, 0.58f },
    { "harshness",     SemanticDimension::Smoothness,  -1, 0.72f },
    { "harsher",       SemanticDimension::Smoothness,  -1, 0.72f },
    { "harsh",         SemanticDimension::Smoothness,  -1, 0.70f },
    { "aspro",         SemanticDimension::Smoothness,  -1, 0.70f },
    { "morbido",       SemanticDimension::Smoothness,  +1, 0.64f },

    { "weight",        SemanticDimension::Weight,      +1, 0.66f },
    { "foundation",    SemanticDimension::Weight,      +1, 0.62f },
    { "sub",           SemanticDimension::Weight,      +1, 0.62f },
    { "peso",          SemanticDimension::Weight,      +1, 0.66f },

    { "punch",         SemanticDimension::Punch,       +1, 0.66f },
    { "attack",        SemanticDimension::Punch,       +1, 0.62f },
    { "snap",          SemanticDimension::Punch,       +1, 0.60f },

    { "tightness",     SemanticDimension::Tightness,   +1, 0.68f },
    { "tighter",       SemanticDimension::Tightness,   +1, 0.70f },
    { "tight",         SemanticDimension::Tightness,   +1, 0.66f },
    { "controlled",    SemanticDimension::Tightness,   +1, 0.60f },
    { "boomy",         SemanticDimension::Tightness,   -1, 0.70f },
    { "boom",          SemanticDimension::Tightness,   -1, 0.68f },
    { "flabby",        SemanticDimension::Tightness,   -1, 0.66f },
    { "loose",         SemanticDimension::Tightness,   -1, 0.60f },
    { "compatto",      SemanticDimension::Tightness,   +1, 0.66f },
    { "gonfio",        SemanticDimension::Tightness,   -1, 0.66f }
}};

struct Span
{
    std::size_t begin = 0;
    std::size_t end = 0;
};

struct TermMatch
{
    const Alias* alias = nullptr;
    std::size_t begin = 0;
    std::size_t end = 0;
};

bool isWordByte(unsigned char c) noexcept
{
    return std::isalnum(c) != 0 || c >= 0x80;
}

bool hasWordBoundary(const std::string& text, std::size_t begin, std::size_t end) noexcept
{
    const bool leftOk = begin == 0 || !isWordByte(static_cast<unsigned char>(text[begin - 1]));
    const bool rightOk = end >= text.size() || !isWordByte(static_cast<unsigned char>(text[end]));
    return leftOk && rightOk;
}

std::string trim(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

void replaceAll(std::string& text, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;

    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos)
    {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string normalize(std::string_view input)
{
    std::string text;
    text.reserve(input.size() + 8);

    for (unsigned char c : input)
    {
        if (c < 0x80)
        {
            const char lower = static_cast<char>(std::tolower(c));
            switch (lower)
            {
                case ',': case ';': case '.': case '!': case '?': case '\n': case '\r':
                    text += " | ";
                    break;
                case '-': case '_': case '/': case '\\': case '\t': case '\'':
                    text.push_back(' ');
                    break;
                default:
                    text.push_back(lower);
                    break;
            }
        }
        else
        {
            // UTF-8 bytes are preserved. ASCII-leading words such as "Più" still
            // normalize correctly because only the leading ASCII byte needs case folding.
            text.push_back(static_cast<char>(c));
        }
    }

    // Contrast connectors create modifier-scope boundaries. "and/e" deliberately
    // remain inside a clause so the nearest local modifier can control each term.
    for (const auto* connector : { " but ", " however ", " ma ", " però ", " pero ", " mentre " })
        replaceAll(text, connector, " | ");

    while (text.find("  ") != std::string::npos)
        replaceAll(text, "  ", " ");

    return trim(text);
}

std::size_t clauseStart(const std::string& text, std::size_t pos) noexcept
{
    if (pos == 0)
        return 0;
    const auto sep = text.rfind('|', pos - 1);
    return sep == std::string::npos ? 0 : sep + 1;
}

std::size_t clauseEnd(const std::string& text, std::size_t pos) noexcept
{
    const auto sep = text.find('|', pos);
    return sep == std::string::npos ? text.size() : sep;
}

bool insideSpan(std::size_t pos, const std::vector<Span>& spans) noexcept
{
    for (const auto& span : spans)
        if (pos >= span.begin && pos < span.end)
            return true;
    return false;
}

std::vector<TermMatch> findTerms(const std::string& text)
{
    std::vector<TermMatch> matches;

    for (const auto& alias : kAliases)
    {
        const std::string phrase(alias.phrase);
        std::size_t pos = 0;
        while ((pos = text.find(phrase, pos)) != std::string::npos)
        {
            const auto end = pos + phrase.size();
            if (hasWordBoundary(text, pos, end))
                matches.push_back({ &alias, pos, end });
            pos = end;
        }
    }

    std::sort(matches.begin(), matches.end(), [](const TermMatch& a, const TermMatch& b)
    {
        if (a.begin != b.begin)
            return a.begin < b.begin;
        return (a.end - a.begin) > (b.end - b.begin);
    });

    // Remove overlaps, preferring the longest match starting at a position.
    std::vector<TermMatch> filtered;
    for (const auto& match : matches)
    {
        bool overlaps = false;
        for (const auto& accepted : filtered)
        {
            if (match.begin < accepted.end && accepted.begin < match.end)
            {
                overlaps = true;
                break;
            }
        }
        if (!overlaps)
            filtered.push_back(match);
    }

    std::sort(filtered.begin(), filtered.end(), [](const TermMatch& a, const TermMatch& b)
    {
        return a.begin < b.begin;
    });
    return filtered;
}

bool containsPhrase(const std::string& text, std::string_view phrase)
{
    const std::string p(phrase);
    std::size_t pos = 0;
    while ((pos = text.find(p, pos)) != std::string::npos)
    {
        if (hasWordBoundary(text, pos, pos + p.size()))
            return true;
        pos += p.size();
    }
    return false;
}

int localModifier(const std::string& text, std::size_t termPos)
{
    const auto begin = clauseStart(text, termPos);
    const auto contextStart = termPos > 48 ? std::max(begin, termPos - 48) : begin;
    const auto context = text.substr(contextStart, termPos - contextStart);

    struct Modifier { const char* phrase; int sign; };
    constexpr std::array<Modifier, 18> modifiers {{
        { "more", +1 }, { "add", +1 }, { "increase", +1 }, { "boost", +1 },
        { "piu", +1 }, { "più", +1 }, { "aumenta", +1 }, { "aumentare", +1 },
        { "less", -1 }, { "reduce", -1 }, { "remove", -1 }, { "cut", -1 },
        { "meno", -1 }, { "riduci", -1 }, { "ridurre", -1 }, { "togli", -1 },
        { "decrease", -1 }, { "lower", -1 }
    }};

    std::size_t bestPos = std::string::npos;
    int bestSign = +1;
    for (const auto& modifier : modifiers)
    {
        const std::string phrase(modifier.phrase);
        std::size_t searchFrom = context.size();
        while (searchFrom > 0)
        {
            const auto pos = context.rfind(phrase, searchFrom - 1);
            if (pos == std::string::npos)
                break;

            const auto end = pos + phrase.size();
            if (hasWordBoundary(context, pos, end))
            {
                if (bestPos == std::string::npos || pos > bestPos)
                {
                    bestPos = pos;
                    bestSign = modifier.sign;
                }
                break; // rfind found the nearest valid occurrence for this phrase
            }

            if (pos == 0)
                break;
            searchFrom = pos;
        }
    }
    return bestSign;
}

float localIntensity(const std::string& text, std::size_t termPos)
{
    const auto begin = clauseStart(text, termPos);
    const auto contextStart = termPos > 48 ? std::max(begin, termPos - 48) : begin;
    const auto context = text.substr(contextStart, termPos - contextStart);

    for (const auto* phrase : { "slightly", "a little", "a bit", "subtle", "subtly",
                               "gently", "leggermente", "un po", "poco" })
        if (containsPhrase(context, phrase))
            return 0.55f;

    for (const auto* phrase : { "much", "very", "a lot", "strongly", "significantly",
                               "molto", "tanto", "parecchio", "decisamente" })
        if (containsPhrase(context, phrase))
            return 1.35f;

    return 1.0f;
}

void mergeGoal(SemanticIntent& intent, SemanticGoal incoming)
{
    incoming.amount = std::clamp(incoming.amount, -1.0f, 1.0f);

    for (auto& existing : intent.goals)
    {
        if (existing.dimension != incoming.dimension)
            continue;

        if (existing.amount * incoming.amount < 0.0f)
            intent.contradictory = true;

        // Same-direction synonyms should not stack into an exaggerated move.
        // Opposing instructions algebraically resolve and mark ambiguity.
        if (existing.amount * incoming.amount >= 0.0f)
        {
            if (std::abs(incoming.amount) > std::abs(existing.amount))
                existing = std::move(incoming);
        }
        else
        {
            existing.amount = std::clamp(existing.amount + incoming.amount, -1.0f, 1.0f);
            existing.confidence = std::min(existing.confidence, incoming.confidence) * 0.75f;
            existing.sourcePhrase += " + " + incoming.sourcePhrase;
        }
        return;
    }

    intent.goals.push_back(std::move(incoming));
}

bool hasConstraint(const SemanticIntent& intent,
                   SemanticDimension dimension,
                   SemanticConstraintKind kind,
                   int direction) noexcept
{
    for (const auto& existing : intent.constraints)
        if (existing.dimension == dimension && existing.kind == kind
            && (kind == SemanticConstraintKind::Preserve || existing.direction == direction))
            return true;
    return false;
}

void detectGoalConstraintConflicts(SemanticIntent& intent) noexcept
{
    for (const auto& goal : intent.goals)
    {
        if (std::abs(goal.amount) < 1.0e-5f)
            continue;

        const int goalDirection = goal.amount > 0.0f ? +1 : -1;
        for (const auto& constraint : intent.constraints)
        {
            if (constraint.dimension != goal.dimension)
                continue;

            const bool conflicts = constraint.kind == SemanticConstraintKind::Preserve
                || (constraint.kind == SemanticConstraintKind::AvoidDirection
                    && constraint.direction == goalDirection);

            if (conflicts)
            {
                intent.goalConstraintConflict = true;
                intent.contradictory = true;
                return;
            }
        }
    }
}

} // namespace

SemanticIntent SemanticIntentCompiler::compile(std::string_view input) const
{
    SemanticIntent intent;
    const std::string text = normalize(input);
    if (text.empty())
        return intent;

    const auto terms = findTerms(text);
    std::vector<Span> constraintSpans;

    struct ConstraintMarker
    {
        const char* phrase;
        SemanticConstraintKind kind;
    };

    constexpr std::array<ConstraintMarker, 9> markers {{
        { "without", SemanticConstraintKind::AvoidDirection },
        { "senza", SemanticConstraintKind::AvoidDirection },
        { "keep", SemanticConstraintKind::Preserve },
        { "preserve", SemanticConstraintKind::Preserve },
        { "don t touch", SemanticConstraintKind::Preserve },
        { "do not touch", SemanticConstraintKind::Preserve },
        { "non toccare", SemanticConstraintKind::Preserve },
        { "mantieni", SemanticConstraintKind::Preserve },
        { "preserva", SemanticConstraintKind::Preserve }
    }};

    for (const auto& marker : markers)
    {
        const std::string phrase(marker.phrase);
        std::size_t markerPos = 0;
        while ((markerPos = text.find(phrase, markerPos)) != std::string::npos)
        {
            if (!hasWordBoundary(text, markerPos, markerPos + phrase.size()))
            {
                markerPos += phrase.size();
                continue;
            }

            const auto end = clauseEnd(text, markerPos + phrase.size());
            const auto tailBegin = markerPos + phrase.size();

            const TermMatch* constrainedTerm = nullptr;
            for (const auto& term : terms)
            {
                if (term.begin >= tailBegin && term.begin < end)
                {
                    constrainedTerm = &term;
                    break;
                }
            }

            if (constrainedTerm != nullptr)
            {
                auto kind = marker.kind;
                const auto tail = text.substr(tailBegin, end - tailBegin);
                if (marker.kind == SemanticConstraintKind::AvoidDirection
                    && (containsPhrase(tail, "lose") || containsPhrase(tail, "losing")
                        || containsPhrase(tail, "loss") || containsPhrase(tail, "perdere")
                        || containsPhrase(tail, "perdendo")))
                    kind = SemanticConstraintKind::Preserve;

                const int direction = constrainedTerm->alias->polarity >= 0 ? +1 : -1;
                if (!hasConstraint(intent, constrainedTerm->alias->dimension, kind, direction))
                {
                    SemanticConstraint constraint;
                    constraint.dimension = constrainedTerm->alias->dimension;
                    constraint.kind = kind;
                    constraint.direction = direction;
                    constraint.confidence = 0.95f;
                    constraint.sourcePhrase = trim(text.substr(
                        markerPos, constrainedTerm->end - markerPos));
                    intent.constraints.push_back(std::move(constraint));
                }

                // Only consume the constraint phrase itself. A later goal in the
                // same grammatical clause remains independently compilable, e.g.
                // "warmer without mud and more air".
                constraintSpans.push_back({ markerPos, constrainedTerm->end });
                intent.hasRecognizedContent = true;
                markerPos = constrainedTerm->end;
            }
            else
            {
                markerPos = end;
            }
        }
    }

    for (const auto& term : terms)
    {
        if (insideSpan(term.begin, constraintSpans))
            continue;

        const int modifier = localModifier(text, term.begin);
        const float intensity = localIntensity(text, term.begin);

        SemanticGoal goal;
        goal.dimension = term.alias->dimension;
        goal.amount = std::clamp(term.alias->defaultAmount
                                 * static_cast<float>(term.alias->polarity)
                                 * static_cast<float>(modifier)
                                 * intensity,
                                 -1.0f, 1.0f);
        goal.confidence = 0.95f;
        goal.sourcePhrase = text.substr(term.begin, term.end - term.begin);
        mergeGoal(intent, std::move(goal));
        intent.hasRecognizedContent = true;
    }

    detectGoalConstraintConflicts(intent);

    if (intent.hasRecognizedContent)
    {
        float confidence = intent.contradictory ? 0.65f : 0.95f;
        if (intent.goals.empty() && !intent.constraints.empty())
            confidence = 0.85f;
        intent.confidence = confidence;
    }

    return intent;
}

} // namespace AIEQPerceptual
