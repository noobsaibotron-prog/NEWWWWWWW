#include "SemanticEQEngine.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#if defined(AIEQ_ENABLE_TORCH)
#include <torch/torch.h>
#endif

namespace
{
    constexpr float kEmbeddingSimilarityThreshold = 0.7f;
    constexpr auto* kDefaultGloveFilename = "glove.6B.50d.txt";

    std::string toLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }
}

#if defined(AIEQ_ENABLE_TORCH)
struct SemanticEQEngine::Seq2SeqDecoder : torch::nn::Module
{
    static constexpr int hiddenSize = 64;

    Seq2SeqDecoder()
    {
        lstm = register_module(
            "lstm",
            torch::nn::LSTM(torch::nn::LSTMOptions(SemanticEQEngine::embeddingDimension, hiddenSize)
                                .batch_first(true)));
        head = register_module(
            "head",
            torch::nn::Linear(hiddenSize, SemanticEQEngine::numQualities));
    }

    std::vector<float> infer(const std::vector<std::vector<float>>& sequence) const
    {
        if (sequence.empty())
            return {};

        const auto seqLen = static_cast<int64_t>(sequence.size());
        auto input = torch::zeros({ 1, seqLen, SemanticEQEngine::embeddingDimension });
        for (int64_t t = 0; t < seqLen; ++t)
        {
            const auto& step = sequence[static_cast<size_t>(t)];
            for (int64_t j = 0; j < SemanticEQEngine::embeddingDimension && j < static_cast<int64_t>(step.size()); ++j)
                input[0][t][j] = step[static_cast<size_t>(j)];
        }

        auto lstmOut = lstm->forward(input);
        auto outputSeq = std::get<0>(lstmOut);               // [1, seqLen, hidden]
        auto lastStep = outputSeq.select(1, seqLen - 1);     // [1, hidden]
        auto logits = head->forward(lastStep);               // [1, numQualities]
        auto probs = torch::softmax(logits, 1).squeeze(0);   // [numQualities]

        std::vector<float> out(SemanticEQEngine::numQualities, 0.0f);
        if (probs.numel() == SemanticEQEngine::numQualities)
        {
            auto contiguous = probs.to(torch::kCPU).contiguous();
            const float* ptr = contiguous.data_ptr<float>();
            for (int i = 0; i < SemanticEQEngine::numQualities; ++i)
                out[static_cast<size_t>(i)] = ptr[i];
        }
        return out;
    }

    torch::nn::LSTM lstm;
    torch::nn::Linear head;
};
#else
struct SemanticEQEngine::Seq2SeqDecoder
{
    Seq2SeqDecoder() = default;
    std::vector<float> infer(const std::vector<std::vector<float>>& /*sequence*/) const { return {}; }
};
#endif

//==============================================================================
SemanticEQEngine::SemanticEQEngine()
{
    initialize();
}

SemanticEQEngine::~SemanticEQEngine() = default;

void SemanticEQEngine::initialize()
{
    if (isInitialized) return;
    
    initializeQualityDefinitions();
    initializeLanguageMappings();
    loadEmbeddings();
    seq2seqDecoder = std::make_unique<Seq2SeqDecoder>();
    
    // Initialize learned offsets to neutral
    for (auto& offset : learnedOffsets)
    {
        offset.frequencyMult = 1.0f;
        offset.gainMult = 1.0f;
        offset.qMult = 1.0f;
        offset.sampleCount = 0;
    }
    
    currentState.reset();
    isInitialized = true;
}

void SemanticEQEngine::loadEmbeddings()
{
    embeddings.clear();
    embeddingsLoaded = false;

    juce::File gloveFile = juce::File::getCurrentWorkingDirectory()
                               .getChildFile(kDefaultGloveFilename);

    if (!gloveFile.existsAsFile())
    {
        const auto executableDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                       .getParentDirectory();
        gloveFile = executableDir.getChildFile(kDefaultGloveFilename);
    }

    if (!gloveFile.existsAsFile())
        return;

    std::ifstream file(gloveFile.getFullPathName().toStdString());
    if (!file.is_open())
        return;

    std::string word;
    while (file >> word)
    {
        std::vector<float> vec(embeddingDimension);
        for (auto& v : vec)
        {
            if (!(file >> v))
            {
                vec.clear();
                break;
            }
        }

        if (vec.size() == embeddingDimension)
        {
            embeddings[toLowerAscii(word)] = std::move(vec);
        }
    }

    embeddingsLoaded = !embeddings.empty();
}

void SemanticEQEngine::reset()
{
    currentState.reset();
    morphProgress = 1.0f;
}

//==============================================================================
void SemanticEQEngine::initializeQualityDefinitions()
{
    auto& defs = qualityDefinitions;
    
    //--------------------------------------------------------------------------
    // AIR - High frequency presence, openness
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Air)];
        d.quality = SemanticQuality::Air;
        d.name = "Air";
        d.nameIT = "Aria";
        d.description = "Adds openness and sparkle to high frequencies (8-16 kHz)";
        d.bands = {
            { 10000.0f, 0.7f, 4.0f, 3 },   // High shelf boost
            { 14000.0f, 1.2f, 2.5f, 2 },   // Peak at 14k
        };
        d.complementary = {
            { SemanticQuality::Clarity, 0.2f },
        };
        d.opposite = SemanticQuality::Darkness;
        d.affectedByBrightness = true;
    }
    
    //--------------------------------------------------------------------------
    // BRILLIANCE - Sparkle and shimmer
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Brilliance)];
        d.quality = SemanticQuality::Brilliance;
        d.name = "Brilliance";
        d.nameIT = "Brillantezza";
        d.description = "Crystal clear highs with shimmer (6-12 kHz)";
        d.bands = {
            { 8000.0f, 1.0f, 3.5f, 2 },
            { 12000.0f, 0.8f, 3.0f, 2 },
        };
        d.opposite = SemanticQuality::Softness;
        d.affectedByBrightness = true;
    }
    
    //--------------------------------------------------------------------------
    // PRESENCE - Forward, cutting through the mix
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Presence)];
        d.quality = SemanticQuality::Presence;
        d.name = "Presence";
        d.nameIT = "Presenza";
        d.description = "Makes sound more forward and present (3-6 kHz)";
        d.bands = {
            { 3500.0f, 1.5f, 4.0f, 2 },
            { 5000.0f, 1.2f, 2.5f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Punch, 0.15f },
        };
        d.opposite = SemanticQuality::Smoothness;
    }
    
    //--------------------------------------------------------------------------
    // SIZZLE - Crispy highs
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Sizzle)];
        d.quality = SemanticQuality::Sizzle;
        d.name = "Sizzle";
        d.nameIT = "Frizzante";
        d.description = "Crispy, exciting high frequency content (8-12 kHz)";
        d.bands = {
            { 9000.0f, 2.0f, 3.0f, 2 },
            { 11000.0f, 1.8f, 2.5f, 2 },
        };
        d.opposite = SemanticQuality::Softness;
        d.affectedByBrightness = true;
    }
    
    //--------------------------------------------------------------------------
    // WARMTH - Fullness and analog feel
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Warmth)];
        d.quality = SemanticQuality::Warmth;
        d.name = "Warmth";
        d.nameIT = "Calore";
        d.description = "Adds fullness and analog warmth (100-300 Hz)";
        d.bands = {
            { 180.0f, 0.8f, 3.5f, 1 },    // Low shelf
            { 250.0f, 1.0f, 2.0f, 2 },    // Peak
        };
        d.complementary = {
            { SemanticQuality::Smoothness, 0.1f },
        };
        d.opposite = SemanticQuality::Tightness;
        d.affectedByBass = true;
    }
    
    //--------------------------------------------------------------------------
    // BODY - Weight and substance
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Body)];
        d.quality = SemanticQuality::Body;
        d.name = "Body";
        d.nameIT = "Corpo";
        d.description = "Adds weight and substance to sound (200-500 Hz)";
        d.bands = {
            { 280.0f, 0.9f, 3.0f, 2 },
            { 400.0f, 1.0f, 2.5f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Warmth, 0.2f },
        };
        d.opposite = SemanticQuality::Clarity;
        d.affectedByBass = true;
    }
    
    //--------------------------------------------------------------------------
    // THICKNESS - Dense and heavy
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Thickness)];
        d.quality = SemanticQuality::Thickness;
        d.name = "Thickness";
        d.nameIT = "Spessore";
        d.description = "Makes sound dense and heavy (150-400 Hz)";
        d.bands = {
            { 200.0f, 0.7f, 4.0f, 2 },
            { 350.0f, 0.9f, 3.0f, 2 },
        };
        d.opposite = SemanticQuality::Clarity;
        d.affectedByBass = true;
    }
    
    //--------------------------------------------------------------------------
    // RICHNESS - Complex harmonics
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Richness)];
        d.quality = SemanticQuality::Richness;
        d.name = "Richness";
        d.nameIT = "Ricchezza";
        d.description = "Enhances harmonic complexity (200-800 Hz)";
        d.bands = {
            { 300.0f, 0.6f, 2.5f, 2 },
            { 600.0f, 0.8f, 2.0f, 2 },
            { 1200.0f, 1.0f, 1.5f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Warmth, 0.15f },
            { SemanticQuality::Presence, 0.1f },
        };
    }
    
    //--------------------------------------------------------------------------
    // PUNCH - Attack and impact
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Punch)];
        d.quality = SemanticQuality::Punch;
        d.name = "Punch";
        d.nameIT = "Punch";
        d.description = "Adds attack and percussive impact (2-5 kHz)";
        d.bands = {
            { 2500.0f, 2.0f, 3.5f, 2 },
            { 4000.0f, 1.8f, 2.5f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Attack, 0.3f },
        };
        d.opposite = SemanticQuality::Smoothness;
    }
    
    //--------------------------------------------------------------------------
    // BITE - Aggression and edge
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Bite)];
        d.quality = SemanticQuality::Bite;
        d.name = "Bite";
        d.nameIT = "Mordente";
        d.description = "Adds aggression and edge (1-3 kHz)";
        d.bands = {
            { 1500.0f, 2.2f, 3.0f, 2 },
            { 2500.0f, 1.8f, 2.5f, 2 },
        };
        d.opposite = SemanticQuality::Gentleness;
    }
    
    //--------------------------------------------------------------------------
    // SNAP - Transient definition
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Snap)];
        d.quality = SemanticQuality::Snap;
        d.name = "Snap";
        d.nameIT = "Scatto";
        d.description = "Enhances transient definition (3-6 kHz)";
        d.bands = {
            { 4000.0f, 2.5f, 2.5f, 2 },
            { 6000.0f, 2.0f, 2.0f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Punch, 0.2f },
        };
    }
    
    //--------------------------------------------------------------------------
    // ATTACK - Broad transient enhancement
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Attack)];
        d.quality = SemanticQuality::Attack;
        d.name = "Attack";
        d.nameIT = "Attacco";
        d.description = "Enhances all transients and attack";
        d.bands = {
            { 2000.0f, 1.5f, 2.0f, 2 },
            { 4000.0f, 1.2f, 2.5f, 2 },
            { 8000.0f, 0.8f, 1.5f, 2 },
        };
    }
    
    //--------------------------------------------------------------------------
    // SMOOTHNESS - Reduce harshness
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Smoothness)];
        d.quality = SemanticQuality::Smoothness;
        d.name = "Smoothness";
        d.nameIT = "Morbidezza";
        d.description = "Reduces harshness and aggression (2-4 kHz cut)";
        d.bands = {
            { 2500.0f, 1.5f, -3.0f, 2 },
            { 3500.0f, 1.2f, -2.5f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Warmth, 0.1f },
        };
        d.opposite = SemanticQuality::Punch;
    }
    
    //--------------------------------------------------------------------------
    // SOFTNESS - Roll off highs
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Softness)];
        d.quality = SemanticQuality::Softness;
        d.name = "Softness";
        d.nameIT = "Dolcezza";
        d.description = "Gentle high frequency rolloff";
        d.bands = {
            { 8000.0f, 0.7f, -3.0f, 3 },   // High shelf cut
        };
        d.opposite = SemanticQuality::Brilliance;
    }
    
    //--------------------------------------------------------------------------
    // DARKNESS - Reduce highs, enhance lows
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Darkness)];
        d.quality = SemanticQuality::Darkness;
        d.name = "Darkness";
        d.nameIT = "Oscurità";
        d.description = "Dark, moody character with reduced highs";
        d.bands = {
            { 150.0f, 0.6f, 2.0f, 1 },     // Low shelf boost
            { 6000.0f, 0.7f, -4.0f, 3 },   // High shelf cut
        };
        d.opposite = SemanticQuality::Air;
    }
    
    //--------------------------------------------------------------------------
    // GENTLENESS - Smooth everything
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Gentleness)];
        d.quality = SemanticQuality::Gentleness;
        d.name = "Gentleness";
        d.nameIT = "Delicatezza";
        d.description = "Overall gentle, non-aggressive sound";
        d.bands = {
            { 2000.0f, 0.8f, -2.0f, 2 },
            { 4000.0f, 1.0f, -2.5f, 2 },
            { 8000.0f, 0.7f, -1.5f, 3 },
        };
        d.opposite = SemanticQuality::Bite;
    }
    
    //--------------------------------------------------------------------------
    // WEIGHT - Sub presence
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Weight)];
        d.quality = SemanticQuality::Weight;
        d.name = "Weight";
        d.nameIT = "Peso";
        d.description = "Deep sub-bass weight (40-100 Hz)";
        d.bands = {
            { 60.0f, 0.8f, 4.0f, 1 },      // Low shelf
            { 80.0f, 1.2f, 3.0f, 2 },
        };
        d.affectedByBass = true;
    }
    
    //--------------------------------------------------------------------------
    // FOUNDATION - Bass foundation
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Foundation)];
        d.quality = SemanticQuality::Foundation;
        d.name = "Foundation";
        d.nameIT = "Fondamento";
        d.description = "Solid bass foundation (60-150 Hz)";
        d.bands = {
            { 80.0f, 0.9f, 3.0f, 2 },
            { 120.0f, 1.0f, 2.5f, 2 },
        };
        d.affectedByBass = true;
    }
    
    //--------------------------------------------------------------------------
    // RUMBLE - Sub-bass
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Rumble)];
        d.quality = SemanticQuality::Rumble;
        d.name = "Rumble";
        d.nameIT = "Rombo";
        d.description = "Deep sub-bass rumble (20-60 Hz)";
        d.bands = {
            { 35.0f, 0.7f, 5.0f, 1 },
            { 50.0f, 1.0f, 3.5f, 2 },
        };
        d.affectedByBass = true;
    }
    
    //--------------------------------------------------------------------------
    // TIGHTNESS - Reduce boom
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Tightness)];
        d.quality = SemanticQuality::Tightness;
        d.name = "Tightness";
        d.nameIT = "Compattezza";
        d.description = "Tight, controlled low end (reduce 100-200 Hz)";
        d.bands = {
            { 120.0f, 1.5f, -3.0f, 2 },
            { 180.0f, 1.2f, -2.5f, 2 },
        };
        d.opposite = SemanticQuality::Warmth;
    }
    
    //--------------------------------------------------------------------------
    // CLARITY - Enhance mids, reduce mud
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Clarity)];
        d.quality = SemanticQuality::Clarity;
        d.name = "Clarity";
        d.nameIT = "Chiarezza";
        d.description = "Crystal clear definition, reduced muddiness";
        d.bands = {
            { 250.0f, 1.2f, -2.5f, 2 },    // Cut mud
            { 400.0f, 1.0f, -2.0f, 2 },    // Cut boxy
            { 3000.0f, 1.0f, 2.0f, 2 },    // Boost presence
        };
        d.opposite = SemanticQuality::Body;
    }
    
    //--------------------------------------------------------------------------
    // DEFINITION - Articulation
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Definition)];
        d.quality = SemanticQuality::Definition;
        d.name = "Definition";
        d.nameIT = "Definizione";
        d.description = "Enhanced articulation and detail (1-4 kHz)";
        d.bands = {
            { 1500.0f, 1.5f, 2.0f, 2 },
            { 3000.0f, 1.2f, 2.5f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Clarity, 0.2f },
        };
    }
    
    //--------------------------------------------------------------------------
    // FOCUS - Narrow boosts
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Focus)];
        d.quality = SemanticQuality::Focus;
        d.name = "Focus";
        d.nameIT = "Focus";
        d.description = "Focused, laser-precise tonal character";
        d.bands = {
            { 2000.0f, 3.0f, 2.0f, 2 },    // Narrow Q
        };
    }
    
    //--------------------------------------------------------------------------
    // WIDTH - Stereo and frequency spread
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Width)];
        d.quality = SemanticQuality::Width;
        d.name = "Width";
        d.nameIT = "Ampiezza";
        d.description = "Broader frequency spread";
        d.bands = {
            { 200.0f, 0.5f, 1.5f, 2 },
            { 4000.0f, 0.5f, 1.5f, 2 },
            { 10000.0f, 0.5f, 1.5f, 2 },
        };
    }
    
    //--------------------------------------------------------------------------
    // VINTAGE - Analog-style curve
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Vintage)];
        d.quality = SemanticQuality::Vintage;
        d.name = "Vintage";
        d.nameIT = "Vintage";
        d.description = "Classic analog warmth with rolled highs";
        d.bands = {
            { 100.0f, 0.6f, 2.0f, 1 },
            { 300.0f, 0.8f, 1.5f, 2 },
            { 10000.0f, 0.7f, -2.5f, 3 },
        };
        d.complementary = {
            { SemanticQuality::Warmth, 0.3f },
            { SemanticQuality::Softness, 0.2f },
        };
    }
    
    //--------------------------------------------------------------------------
    // MODERN - Clean, hi-fi
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Modern)];
        d.quality = SemanticQuality::Modern;
        d.name = "Modern";
        d.nameIT = "Moderno";
        d.description = "Clean, hi-fi modern sound";
        d.bands = {
            { 60.0f, 1.0f, 2.0f, 2 },
            { 4000.0f, 1.2f, 1.5f, 2 },
            { 12000.0f, 0.8f, 2.0f, 3 },
        };
        d.complementary = {
            { SemanticQuality::Clarity, 0.2f },
            { SemanticQuality::Air, 0.15f },
        };
    }
    
    //--------------------------------------------------------------------------
    // AGGRESSIVE - Strong presence + punch
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Aggressive)];
        d.quality = SemanticQuality::Aggressive;
        d.name = "Aggressive";
        d.nameIT = "Aggressivo";
        d.description = "In-your-face, aggressive character";
        d.bands = {
            { 2000.0f, 1.5f, 3.0f, 2 },
            { 4000.0f, 1.2f, 3.5f, 2 },
            { 80.0f, 1.0f, 2.5f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Punch, 0.4f },
            { SemanticQuality::Bite, 0.3f },
        };
        d.opposite = SemanticQuality::Smooth;
    }
    
    //--------------------------------------------------------------------------
    // SMOOTH - Gentle, rolled off
    //--------------------------------------------------------------------------
    {
        auto& d = defs[static_cast<int>(SemanticQuality::Smooth)];
        d.quality = SemanticQuality::Smooth;
        d.name = "Smooth";
        d.nameIT = "Liscio";
        d.description = "Smooth, easy-listening character";
        d.bands = {
            { 2500.0f, 1.0f, -2.0f, 2 },
            { 5000.0f, 0.8f, -1.5f, 2 },
            { 250.0f, 0.7f, 1.5f, 2 },
        };
        d.complementary = {
            { SemanticQuality::Warmth, 0.2f },
            { SemanticQuality::Smoothness, 0.3f },
        };
        d.opposite = SemanticQuality::Aggressive;
    }
}

//==============================================================================
void SemanticEQEngine::initializeLanguageMappings()
{
    // English mappings
    auto& en = languageMappings["en"];
    en = {
        { { "air", "airy", "open", "spacious" }, SemanticQuality::Air, 0.7f, false },
        { { "brilliance", "brilliant", "sparkle", "sparkly", "shimmer" }, SemanticQuality::Brilliance, 0.7f, false },
        { { "presence", "present", "forward", "upfront" }, SemanticQuality::Presence, 0.6f, false },
        { { "warm", "warmth", "warmer", "analog" }, SemanticQuality::Warmth, 0.7f, false },
        { { "body", "full", "fuller", "weight" }, SemanticQuality::Body, 0.6f, false },
        { { "punch", "punchy", "impact", "hit" }, SemanticQuality::Punch, 0.7f, false },
        { { "bite", "edge", "edgy", "aggressive" }, SemanticQuality::Bite, 0.6f, false },
        { { "smooth", "smoother", "soft", "gentle" }, SemanticQuality::Smoothness, 0.6f, false },
        { { "clarity", "clear", "clearer", "clean" }, SemanticQuality::Clarity, 0.7f, false },
        { { "dark", "darker", "darkness" }, SemanticQuality::Darkness, 0.6f, false },
        { { "bright", "brighter" }, SemanticQuality::Brilliance, 0.5f, false },
        { { "tight", "tighter", "controlled" }, SemanticQuality::Tightness, 0.6f, false },
        { { "vintage", "retro", "classic" }, SemanticQuality::Vintage, 0.7f, false },
        { { "modern", "contemporary", "hi-fi" }, SemanticQuality::Modern, 0.7f, false },
        { { "bass", "bassy", "low", "sub" }, SemanticQuality::Weight, 0.6f, false },
        { { "thin", "thinner" }, SemanticQuality::Body, -0.5f, true },
        { { "muddy", "mud" }, SemanticQuality::Clarity, 0.7f, false },
        { { "harsh", "harshness" }, SemanticQuality::Smoothness, 0.7f, false },
    };
    
    // Italian mappings
    auto& it = languageMappings["it"];
    it = {
        { { "aria", "arioso", "aperto", "spazioso" }, SemanticQuality::Air, 0.7f, false },
        { { "brillante", "brillantezza", "scintillante" }, SemanticQuality::Brilliance, 0.7f, false },
        { { "presenza", "presente", "avanti" }, SemanticQuality::Presence, 0.6f, false },
        { { "caldo", "calore", "caldezza" }, SemanticQuality::Warmth, 0.7f, false },
        { { "corpo", "pieno", "peso" }, SemanticQuality::Body, 0.6f, false },
        { { "punch", "impatto", "colpo" }, SemanticQuality::Punch, 0.7f, false },
        { { "mordente", "aggressivo" }, SemanticQuality::Bite, 0.6f, false },
        { { "morbido", "dolce", "delicato" }, SemanticQuality::Smoothness, 0.6f, false },
        { { "chiaro", "chiarezza", "pulito" }, SemanticQuality::Clarity, 0.7f, false },
        { { "scuro", "oscuro", "oscurita" }, SemanticQuality::Darkness, 0.6f, false },
        { { "luminoso" }, SemanticQuality::Brilliance, 0.5f, false },
        { { "compatto", "controllato" }, SemanticQuality::Tightness, 0.6f, false },
        { { "vintage", "retro", "classico" }, SemanticQuality::Vintage, 0.7f, false },
        { { "moderno", "contemporaneo" }, SemanticQuality::Modern, 0.7f, false },
        { { "basso", "sub" }, SemanticQuality::Weight, 0.6f, false },
        { { "sottile", "magro" }, SemanticQuality::Body, -0.5f, true },
        { { "fangoso" }, SemanticQuality::Clarity, 0.7f, false },
        { { "aspro", "duro" }, SemanticQuality::Smoothness, 0.7f, false },
    };
}

//==============================================================================
std::vector<SemanticEQEngine::SemanticEQAdjustment> 
SemanticEQEngine::generateEQFromState(const std::vector<float>& spectrum,
                                       double sampleRate) const
{
    std::vector<SemanticEQAdjustment> adjustments;
    
    // Generate adjustments for each non-zero quality
    for (int i = 0; i < numQualities; ++i)
    {
        float amount = currentState.qualities[i];
        if (std::abs(amount) > 0.01f)
        {
            auto quality = static_cast<SemanticQuality>(i);
            auto qualityAdj = generateEQForQuality(quality, amount, spectrum, sampleRate);
            adjustments.insert(adjustments.end(), qualityAdj.begin(), qualityAdj.end());
        }
    }
    
    // Merge overlapping frequency bands: when two adjustments target frequencies
    // within 1/3 octave of each other, combine them into a single adjustment
    // (weighted average of freq/gain/Q by absolute gain magnitude).
    if (adjustments.size() > 1)
    {
        std::sort(adjustments.begin(), adjustments.end(),
                  [](const auto& a, const auto& b) { return a.frequency < b.frequency; });

        std::vector<SemanticEQAdjustment> merged;
        merged.reserve(adjustments.size());
        merged.push_back(adjustments[0]);

        for (size_t i = 1; i < adjustments.size(); ++i)
        {
            auto& prev = merged.back();
            const auto& curr = adjustments[i];

            // Within 1/3 octave?
            const float ratio = curr.frequency / (prev.frequency + 1e-6f);
            if (ratio < std::pow(2.0f, 1.0f / 3.0f))
            {
                // Weighted merge by absolute gain
                const float wPrev = std::abs(prev.gain) + 0.01f;
                const float wCurr = std::abs(curr.gain) + 0.01f;
                const float wTotal = wPrev + wCurr;
                prev.frequency = (prev.frequency * wPrev + curr.frequency * wCurr) / wTotal;
                prev.gain    = (prev.gain * wPrev + curr.gain * wCurr) / wTotal;
                prev.q         = (prev.q * wPrev + curr.q * wCurr) / wTotal;
            }
            else
            {
                merged.push_back(curr);
            }
        }

        return merged;
    }

    return adjustments;
}

std::vector<SemanticEQEngine::SemanticEQAdjustment> 
SemanticEQEngine::generateEQForQuality(SemanticQuality quality,
                                        float amount,
                                        const std::vector<float>& spectrum,
                                        double sampleRate) const
{
    std::vector<SemanticEQAdjustment> adjustments;
    
    // BOUNDS CHECK: Ensure quality is valid
    const int qualityIdx = static_cast<int>(quality);
    if (qualityIdx < 0 || qualityIdx >= numQualities)
        return adjustments;  // Return empty for invalid quality
    
    const auto& def = qualityDefinitions[static_cast<size_t>(qualityIdx)];
    
    // Scale by global intensity
    float scaledAmount = amount * globalIntensity;
    
    // Generate adjustment for each frequency band in the definition
    for (const auto& band : def.bands)
    {
        SemanticEQAdjustment adj;
        adj.frequency = band.centerFreq;
        adj.q = band.q;
        adj.gain = band.maxGain * scaledAmount;
        adj.filterType = band.filterType;
        adj.enabled = true;
        adj.sourceQuality = quality;
        adj.confidence = 1.0f;
        adj.description = def.name + " adjustment at " + 
                         juce::String(static_cast<int>(band.centerFreq)) + " Hz";
        
        // Apply context-aware adjustments if enabled
        if (contextAwareEnabled && !spectrum.empty())
        {
            adj = adjustForContext(adj, spectrum, sampleRate);
        }
        
        // Apply learned offsets from user preferences
        adj = applyLearnedOffsets(adj, quality);
        
        adjustments.push_back(adj);
    }
    
    // Add complementary adjustments
    for (const auto& comp : def.complementary)
    {
        // BOUNDS CHECK: Ensure complementary quality is valid
        const int compIdx = static_cast<int>(comp.quality);
        if (compIdx < 0 || compIdx >= numQualities)
            continue;  // Skip invalid complementary quality
            
        float compAmount = amount * comp.factor;
        const auto& compDef = qualityDefinitions[static_cast<size_t>(compIdx)];
        
        for (const auto& band : compDef.bands)
        {
            SemanticEQAdjustment adj;
            adj.frequency = band.centerFreq;
            adj.q = band.q;
            adj.gain = band.maxGain * compAmount * globalIntensity;
            adj.filterType = band.filterType;
            adj.enabled = true;
            adj.sourceQuality = comp.quality;
            adj.confidence = 0.7f;  // Lower confidence for complementary
            adj.description = "Complementary " + compDef.name;
            
            if (std::abs(adj.gain) > 0.1f)
                adjustments.push_back(adj);
        }
    }
    
    return adjustments;
}

float SemanticEQEngine::cosineSimilarity(const std::vector<float>& a,
                                         const std::vector<float>& b) const
{
    if (a.empty() || b.empty() || a.size() != b.size())
        return 0.0f;

    float dot = 0.0f;
    float magA = 0.0f;
    float magB = 0.0f;

    for (size_t i = 0; i < a.size(); ++i)
    {
        dot += a[i] * b[i];
        magA += a[i] * a[i];
        magB += b[i] * b[i];
    }

    if (magA == 0.0f || magB == 0.0f)
        return 0.0f;

    return dot / (std::sqrt(magA) * std::sqrt(magB));
}

void SemanticEQEngine::addOrUpdateParsedResult(
    std::vector<std::pair<SemanticQuality, float>>& results,
    SemanticQuality quality,
    float amount) const
{
    for (auto& existing : results)
    {
        if (existing.first == quality)
        {
            existing.second = amount;
            return;
        }
    }

    results.push_back({ quality, amount });
}

std::vector<float> SemanticEQEngine::runSeq2Seq(
    const std::vector<std::vector<float>>& tokenEmbeddings) const
{
    if (!seq2seqDecoder)
        return {};

    return seq2seqDecoder->infer(tokenEmbeddings);
}

//==============================================================================
std::vector<std::pair<SemanticEQEngine::SemanticQuality, float>> 
SemanticEQEngine::parseNaturalLanguage(const juce::String& input) const
{
    std::vector<std::pair<SemanticQuality, float>> results;
    
    static juce::PerformanceCounter parsingCounter("SemanticParsing", 200);
    struct CounterScope
    {
        juce::PerformanceCounter& pc;
        CounterScope(juce::PerformanceCounter& p) : pc(p) { pc.start(); }
        ~CounterScope() { pc.stop(); }
    } counterScope(parsingCounter);
    
    const juce::String lowInput = input.toLowerCase();
    
    // Check for "more" or "less" modifiers
    const bool hasMore = lowInput.contains("more") || lowInput.contains("piu") || 
                         lowInput.contains("più") || lowInput.contains("add");
    const bool hasLess = lowInput.contains("less") || lowInput.contains("meno") || 
                         lowInput.contains("reduce") || lowInput.contains("remove");
    
    juce::ignoreUnused(hasMore);
    const float modifier = hasLess ? -1.0f : 1.0f;
    
    auto addResult = [&](SemanticQuality quality, float amount)
    {
        addOrUpdateParsedResult(results, quality, amount);
    };
    
    // Search explicit keywords first (fast path)
    for (const auto& langPair : languageMappings)
    {
        for (const auto& mapping : langPair.second)
        {
            for (const auto& keyword : mapping.keywords)
            {
                if (lowInput.contains(keyword))
                {
                    float amount = mapping.defaultAmount * modifier;
                    if (mapping.isNegative)
                        amount = -amount;
                    
                    addResult(mapping.quality, amount);
                    break;
                }
            }
        }
    }
    
    // Semantic fallback using GloVe embeddings
    if (embeddingsLoaded)
    {
        juce::StringArray tokens;
        tokens.addTokens(lowInput, " ,.;:-_!?/\\\n\t", "");
        tokens.trim();
        tokens.removeEmptyStrings();

        std::vector<std::vector<float>> tokenEmbeddings;
        tokenEmbeddings.reserve(static_cast<size_t>(tokens.size()));

        juce::String langKey = currentLanguage.isNotEmpty() ? currentLanguage.toLowerCase()
                                                            : "en";
        auto mappingIt = languageMappings.find(langKey);
        if (mappingIt == languageMappings.end() && !languageMappings.empty())
            mappingIt = languageMappings.begin();

        if (mappingIt != languageMappings.end())
        {
            for (const auto& token : tokens)
            {
                const auto tokenEmbedding = embeddings.find(token.toStdString());
                if (tokenEmbedding == embeddings.end())
                    continue;

                tokenEmbeddings.push_back(tokenEmbedding->second);

                for (const auto& mapping : mappingIt->second)
                {
                    for (const auto& keyword : mapping.keywords)
                    {
                        const auto keywordEmbedding = embeddings.find(toLowerAscii(keyword.toStdString()));
                        if (keywordEmbedding == embeddings.end())
                            continue;

                        const float similarity = cosineSimilarity(tokenEmbedding->second,
                                                                  keywordEmbedding->second);
                        if (similarity >= kEmbeddingSimilarityThreshold)
                        {
                            float amount = mapping.defaultAmount * modifier;
                            if (mapping.isNegative)
                                amount = -amount;

                            addResult(mapping.quality, amount);
                            break;
                        }
                    }
                }
            }
        }

        // Optional seq2seq decoder (Torch C++ if enabled)
        const auto seqOutputs = runSeq2Seq(tokenEmbeddings);
        if (seqOutputs.size() == static_cast<size_t>(numQualities))
        {
            for (int i = 0; i < numQualities; ++i)
            {
                const float prob = seqOutputs[static_cast<size_t>(i)];
                // Map probability to [-1, 1] around neutral 0.5
                float amount = juce::jlimit(-1.0f, 1.0f, (prob - 0.5f) * 2.0f);
                if (std::abs(amount) > 0.1f)
                    addResult(static_cast<SemanticQuality>(i), amount);
            }
        }
    }
    
    return results;
}

//==============================================================================
void SemanticEQEngine::setQuality(SemanticQuality quality, float amount)
{
    currentState.setQuality(quality, amount);
    
    // Handle opposite qualities
    // BOUNDS CHECK
    const int qIdx = static_cast<int>(quality);
    if (qIdx < 0 || qIdx >= numQualities)
        return;
    const auto& def = qualityDefinitions[static_cast<size_t>(qIdx)];
    if (def.opposite != SemanticQuality::NumQualities && std::abs(amount) > 0.3f)
    {
        // Reduce opposite quality
        float currentOpposite = currentState.getQuality(def.opposite);
        if ((amount > 0 && currentOpposite > 0) || (amount < 0 && currentOpposite < 0))
        {
            currentState.setQuality(def.opposite, currentOpposite * 0.5f);
        }
    }
}

float SemanticEQEngine::getQuality(SemanticQuality quality) const
{
    return currentState.getQuality(quality);
}

//==============================================================================
void SemanticEQEngine::morphToState(const SemanticState& target, float time_ms)
{
    morphStartState = currentState;
    morphTargetState = target;
    morphDuration = time_ms;
    morphProgress = 0.0f;
}

void SemanticEQEngine::updateMorph(float deltaTime_ms)
{
    if (morphProgress >= 1.0f) return;
    
    morphProgress += deltaTime_ms / morphDuration;
    morphProgress = juce::jmin(1.0f, morphProgress);
    
    // Smooth interpolation (ease in-out)
    float t = morphProgress;
    float smoothT = t * t * (3.0f - 2.0f * t);
    
    // Interpolate all qualities
    for (int i = 0; i < numQualities; ++i)
    {
        float start = morphStartState.qualities[i];
        float end = morphTargetState.qualities[i];
        currentState.qualities[i] = start + (end - start) * smoothT;
    }
}

//==============================================================================
void SemanticEQEngine::learnFromUserAdjustment(SemanticQuality quality, 
                                                float originalAmount,
                                                float userFreq, 
                                                float userGain, 
                                                float userQ)
{
    // BOUNDS CHECK
    const int qIdx = static_cast<int>(quality);
    if (qIdx < 0 || qIdx >= numQualities)
        return;
        
    const auto& def = qualityDefinitions[static_cast<size_t>(qIdx)];
    auto& offset = learnedOffsets[static_cast<size_t>(qIdx)];
    
    if (def.bands.empty()) return;
    
    // Calculate what we expected vs what user did
    // Use primary band as reference
    const auto& primaryBand = def.bands[0];
    float expectedGain = primaryBand.maxGain * originalAmount * globalIntensity;
    
    // Learning rate decreases as we have more samples
    float learningRate = 0.2f / (1.0f + offset.sampleCount * 0.1f);
    
    // Update frequency multiplier
    if (primaryBand.centerFreq > 0)
    {
        float freqRatio = userFreq / primaryBand.centerFreq;
        offset.frequencyMult = offset.frequencyMult * (1.0f - learningRate) + 
                               freqRatio * learningRate;
    }
    
    // Update gain multiplier
    if (std::abs(expectedGain) > 0.1f)
    {
        float gainRatio = userGain / expectedGain;
        gainRatio = juce::jlimit(0.5f, 2.0f, gainRatio);
        offset.gainMult = offset.gainMult * (1.0f - learningRate) + 
                          gainRatio * learningRate;
    }
    
    // Update Q multiplier
    if (primaryBand.q > 0)
    {
        float qRatio = userQ / primaryBand.q;
        qRatio = juce::jlimit(0.3f, 3.0f, qRatio);
        offset.qMult = offset.qMult * (1.0f - learningRate) + 
                       qRatio * learningRate;
    }
    
    offset.sampleCount++;
}

SemanticEQEngine::LearnedOffset 
SemanticEQEngine::getLearnedOffset(SemanticQuality quality) const
{
    // BOUNDS CHECK
    const int qIdx = static_cast<int>(quality);
    if (qIdx < 0 || qIdx >= numQualities)
        return LearnedOffset{};  // Return default
    return learnedOffsets[static_cast<size_t>(qIdx)];
}

void SemanticEQEngine::setLearnedOffset(SemanticQuality quality, 
                                         const LearnedOffset& offset)
{
    // BOUNDS CHECK
    const int qIdx = static_cast<int>(quality);
    if (qIdx < 0 || qIdx >= numQualities)
        return;
    learnedOffsets[static_cast<size_t>(qIdx)] = offset;
}

//==============================================================================
SemanticEQEngine::SemanticEQAdjustment 
SemanticEQEngine::adjustForContext(const SemanticEQAdjustment& base,
                                    const std::vector<float>& spectrum,
                                    double sampleRate) const
{
    SemanticEQAdjustment adj = base;
    
    // BOUNDS CHECK
    const int qIdx = static_cast<int>(base.sourceQuality);
    if (qIdx < 0 || qIdx >= numQualities)
        return adj;  // Return unadjusted
        
    const auto& def = qualityDefinitions[static_cast<size_t>(qIdx)];
    
    // Analyze current spectrum
    float bassProportion = analyzeBassProportion(spectrum, sampleRate);
    float brightness = analyzeBrightness(spectrum, sampleRate);
    
    // Adjust based on definition flags
    if (def.affectedByBass)
    {
        // If there's already lots of bass, reduce bass-related boosts
        if (bassProportion > 0.6f && adj.gain > 0 && adj.frequency < 300.0f)
        {
            adj.gain *= 0.7f;
        }
        // If bass is lacking, boost more
        else if (bassProportion < 0.3f && adj.gain > 0 && adj.frequency < 300.0f)
        {
            adj.gain *= 1.2f;
        }
    }
    
    if (def.affectedByBrightness)
    {
        // If already bright, reduce high-freq boosts
        if (brightness > 0.6f && adj.gain > 0 && adj.frequency > 5000.0f)
        {
            adj.gain *= 0.7f;
        }
        // If dull, boost more
        else if (brightness < 0.3f && adj.gain > 0 && adj.frequency > 5000.0f)
        {
            adj.gain *= 1.3f;
        }
    }
    
    return adj;
}

SemanticEQEngine::SemanticEQAdjustment 
SemanticEQEngine::applyLearnedOffsets(const SemanticEQAdjustment& base,
                                       SemanticQuality quality) const
{
    // BOUNDS CHECK: Ensure quality is valid
    const int qIdx = static_cast<int>(quality);
    if (qIdx < 0 || qIdx >= numQualities)
        return base;
    
    const auto& offset = learnedOffsets[static_cast<size_t>(qIdx)];
    
    // Only apply if we have enough samples
    if (offset.sampleCount < 3) return base;
    
    SemanticEQAdjustment adj = base;
    adj.frequency *= offset.frequencyMult;
    adj.gain *= offset.gainMult;
    adj.q *= offset.qMult;
    
    // Clamp to reasonable values
    adj.frequency = juce::jlimit(20.0f, 20000.0f, adj.frequency);
    adj.gain = juce::jlimit(-12.0f, 12.0f, adj.gain);
    adj.q = juce::jlimit(0.1f, 10.0f, adj.q);
    
    return adj;
}

//==============================================================================
float SemanticEQEngine::analyzeBassProportion(const std::vector<float>& spectrum,
                                               double sampleRate) const
{
    if (spectrum.empty()) return 0.5f;
    
    float binWidth = static_cast<float>(sampleRate) / (spectrum.size() * 2);
    int lowCutBin = static_cast<int>(200.0f / binWidth);
    int totalBins = static_cast<int>(spectrum.size());
    
    float bassEnergy = 0.0f;
    float totalEnergy = 0.0f;
    
    for (int i = 0; i < totalBins && i < static_cast<int>(spectrum.size()); ++i)
    {
        float energy = std::pow(10.0f, spectrum[i] / 20.0f);
        totalEnergy += energy;
        if (i < lowCutBin)
            bassEnergy += energy;
    }
    
    return totalEnergy > 0 ? bassEnergy / totalEnergy : 0.5f;
}

float SemanticEQEngine::analyzeBrightness(const std::vector<float>& spectrum,
                                           double sampleRate) const
{
    if (spectrum.empty()) return 0.5f;
    
    float binWidth = static_cast<float>(sampleRate) / (spectrum.size() * 2);
    int highCutBin = static_cast<int>(4000.0f / binWidth);
    int totalBins = static_cast<int>(spectrum.size());
    
    float highEnergy = 0.0f;
    float totalEnergy = 0.0f;
    
    for (int i = 0; i < totalBins && i < static_cast<int>(spectrum.size()); ++i)
    {
        float energy = std::pow(10.0f, spectrum[i] / 20.0f);
        totalEnergy += energy;
        if (i > highCutBin)
            highEnergy += energy;
    }
    
    return totalEnergy > 0 ? highEnergy / totalEnergy : 0.5f;
}

float SemanticEQEngine::analyzeMidContent(const std::vector<float>& spectrum,
                                           double sampleRate) const
{
    if (spectrum.empty()) return 0.5f;
    
    float binWidth = static_cast<float>(sampleRate) / (spectrum.size() * 2);
    int lowBin = static_cast<int>(400.0f / binWidth);
    int highBin = static_cast<int>(4000.0f / binWidth);
    int totalBins = static_cast<int>(spectrum.size());
    
    float midEnergy = 0.0f;
    float totalEnergy = 0.0f;
    
    for (int i = 0; i < totalBins && i < static_cast<int>(spectrum.size()); ++i)
    {
        float energy = std::pow(10.0f, spectrum[i] / 20.0f);
        totalEnergy += energy;
        if (i >= lowBin && i <= highBin)
            midEnergy += energy;
    }
    
    return totalEnergy > 0 ? midEnergy / totalEnergy : 0.5f;
}

//==============================================================================
juce::String SemanticEQEngine::getQualityName(SemanticQuality quality)
{
    switch (quality)
    {
        case SemanticQuality::Air:          return "Air";
        case SemanticQuality::Brilliance:   return "Brilliance";
        case SemanticQuality::Presence:     return "Presence";
        case SemanticQuality::Sizzle:       return "Sizzle";
        case SemanticQuality::Warmth:       return "Warmth";
        case SemanticQuality::Body:         return "Body";
        case SemanticQuality::Thickness:    return "Thickness";
        case SemanticQuality::Richness:     return "Richness";
        case SemanticQuality::Punch:        return "Punch";
        case SemanticQuality::Bite:         return "Bite";
        case SemanticQuality::Snap:         return "Snap";
        case SemanticQuality::Attack:       return "Attack";
        case SemanticQuality::Smoothness:   return "Smoothness";
        case SemanticQuality::Softness:     return "Softness";
        case SemanticQuality::Darkness:     return "Darkness";
        case SemanticQuality::Gentleness:   return "Gentleness";
        case SemanticQuality::Weight:       return "Weight";
        case SemanticQuality::Foundation:   return "Foundation";
        case SemanticQuality::Rumble:       return "Rumble";
        case SemanticQuality::Tightness:    return "Tightness";
        case SemanticQuality::Clarity:      return "Clarity";
        case SemanticQuality::Definition:   return "Definition";
        case SemanticQuality::Focus:        return "Focus";
        case SemanticQuality::Width:        return "Width";
        case SemanticQuality::Vintage:      return "Vintage";
        case SemanticQuality::Modern:       return "Modern";
        case SemanticQuality::Aggressive:   return "Aggressive";
        case SemanticQuality::Smooth:       return "Smooth";
        default:                            return "Unknown";
    }
}

juce::String SemanticEQEngine::getQualityNameLocalized(SemanticQuality quality,
                                                        const juce::String& language)
{
    // Localization is English-only for v1.0. The quality names ("Warm", "Bright",
    // "Air", etc.) are industry-standard English audio terms recognized globally.
    // Full i18n will be added in a future release via JUCE's LocalisedStrings.
    juce::ignoreUnused(language);
    return getQualityName(quality);
}

juce::String SemanticEQEngine::getQualityDescription(SemanticQuality quality)
{
    switch (quality)
    {
        case SemanticQuality::Air:          return "High frequency openness and sparkle";
        case SemanticQuality::Warmth:       return "Low-mid fullness and analog feel";
        case SemanticQuality::Punch:        return "Attack and percussive impact";
        case SemanticQuality::Clarity:      return "Definition and reduced muddiness";
        case SemanticQuality::Smoothness:   return "Reduced harshness and aggression";
        case SemanticQuality::Body:         return "Weight and substance";
        default:                            return "";
    }
}

juce::StringArray SemanticEQEngine::getAllQualityNames()
{
    juce::StringArray names;
    for (int i = 0; i < numQualities; ++i)
    {
        names.add(getQualityName(static_cast<SemanticQuality>(i)));
    }
    return names;
}

const SemanticEQEngine::QualityDefinition& 
SemanticEQEngine::getQualityDefinition(SemanticQuality quality) const
{
    // BOUNDS CHECK - return static empty definition for invalid quality
    static const QualityDefinition emptyDef{};
    const int qIdx = static_cast<int>(quality);
    if (qIdx < 0 || qIdx >= numQualities)
        return emptyDef;
    return qualityDefinitions[static_cast<size_t>(qIdx)];
}

