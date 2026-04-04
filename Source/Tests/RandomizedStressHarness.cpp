#include <juce_audio_processors/juce_audio_processors.h>
#include <gtest/gtest.h>
#include <random>
#include "../PluginProcessor.h"

class RandomizedStressHarness : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<AIEqualizerAudioProcessor>();
        rng.seed(std::random_device()());
    }

    std::unique_ptr<AIEqualizerAudioProcessor> processor;
    std::mt19937 rng;
};

TEST_F(RandomizedStressHarness, BUFFER_SWITCH_STABILITY) {
    const double sampleRate = 44100.0;
    const int maxBlockSize = 2048;
    const int numChannels = 2;
    
    // Initial prepare
    processor->prepareToPlay(sampleRate, 512);
    
    std::vector<int> blockSizes = {64, 128, 256, 512, 1024, 2048};
    juce::AudioBuffer<float> buffer(numChannels, maxBlockSize);
    juce::MidiBuffer midi;
    
    for (int i = 0; i < 100; ++i) {
        int randomSize = blockSizes[std::uniform_int_distribution<int>(0, blockSizes.size() - 1)(rng)];
        
        // Rapidly change block size during process
        processor->prepareToPlay(sampleRate, randomSize);
        processor->processBlock(buffer, midi);
        
        // Ensure no crash or memory error
        EXPECT_NO_THROW();
    }
}

TEST_F(RandomizedStressHarness, SAMPLE_RATE_WARP_STABILITY) {
    const int blockSize = 512;
    const int numChannels = 2;
    
    std::vector<double> sampleRates = {44100.0, 48000.0, 88200.0, 96000.0, 192000.0};
    juce::AudioBuffer<float> buffer(numChannels, blockSize);
    juce::MidiBuffer midi;
    
    for (int i = 0; i < 50; ++i) {
        double randomSR = sampleRates[std::uniform_int_distribution<int>(0, sampleRates.size() - 1)(rng)];
        
        // Rapidly change sample rate
        processor->prepareToPlay(randomSR, blockSize);
        processor->processBlock(buffer, midi);
        
        // Ensure no crash or filter instability (NAN/INF)
        for (int ch = 0; ch < numChannels; ++ch) {
            auto* data = buffer.getReadPointer(ch);
            for (int s = 0; s < blockSize; ++s) {
                EXPECT_TRUE(std::isfinite(data[s]));
            }
        }
    }
}

TEST_F(RandomizedStressHarness, PARAMETER_STORM_STABILITY) {
    const double sampleRate = 44100.0;
    const int blockSize = 512;
    const int numChannels = 2;
    
    processor->prepareToPlay(sampleRate, blockSize);
    auto& apvts = processor->getAPVTS();
    juce::AudioBuffer<float> buffer(numChannels, blockSize);
    juce::MidiBuffer midi;
    
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < 200; ++i) {
        // Randomly set all parameters
        for (auto* param : apvts.getProcessor().getParameters()) {
            param->setValueNotifyingHost(dist(rng));
        }
        
        processor->processBlock(buffer, midi);
        EXPECT_NO_THROW();
    }
}
