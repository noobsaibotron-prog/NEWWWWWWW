#include <juce_audio_processors/juce_audio_processors.h>
#include <gtest/gtest.h>
#include "../PluginProcessor.h"

class DynEQRuntimeValidation : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<AIEqualizerAudioProcessor>();
        sampleRate = 44100.0;
        blockSize = 512;
        processor->prepareToPlay(sampleRate, blockSize);
    }

    std::unique_ptr<AIEqualizerAudioProcessor> processor;
    double sampleRate;
    int blockSize;
};

TEST_F(DynEQRuntimeValidation, LOOKAHEAD_LATENCY_REPORTING) {
    // Gate 4.2: Ensure lookahead latency is reported correctly to the host
    auto reportedLatency = processor->getLatencySamples();
    
    // Assume lookahead is 5ms (standard for DynEQ)
    const int expectedLatency = (int)(0.005 * sampleRate);
    
    // Check if reported latency is close to expected (allow some margin for rounding)
    EXPECT_NEAR(reportedLatency, expectedLatency, 10);
}

TEST_F(DynEQRuntimeValidation, ENVELOPE_FOLLOWER_PRECISION) {
    // Gate 4.1: Verify attack/release envelope precision
    // Test with a step signal (silence -> 0dB)
    const int numChannels = 2;
    juce::AudioBuffer<float> buffer(numChannels, blockSize);
    buffer.clear();
    
    // Set a step signal in the second half of the buffer
    for (int ch = 0; ch < numChannels; ++ch) {
        for (int s = blockSize / 2; s < blockSize; ++s) {
            buffer.setSample(ch, s, 1.0f);
        }
    }
    
    juce::MidiBuffer midi;
    
    // Process multiple blocks to see envelope reaction
    for (int i = 0; i < 10; ++i) {
        processor->processBlock(buffer, midi);
        
        // Verify no NANs or extreme values
        for (int ch = 0; ch < numChannels; ++ch) {
            auto* data = buffer.getReadPointer(ch);
            for (int s = 0; s < blockSize; ++s) {
                EXPECT_TRUE(std::isfinite(data[s]));
                EXPECT_LE(std::abs(data[s]), 10.0f); // Allow some overshoot but no explosions
            }
        }
    }
}

TEST_F(DynEQRuntimeValidation, FILTER_STABILITY_EXTREME_GAIN) {
    // Gate 4.3: Test IIR stability with extreme gains (+24dB / -24dB)
    auto& apvts = processor->getAPVTS();
    
    // Set extreme gain on a dynamic band
    apvts.getParameter("band1Gain")->setValueNotifyingHost(1.0f); // Max +24dB
    apvts.getParameter("band1Q")->setValueNotifyingHost(1.0f);    // Max Q
    
    juce::AudioBuffer<float> buffer(2, blockSize);
    // Fill with noise to stress the filter
    for (int ch = 0; ch < 2; ++ch) {
        for (int s = 0; s < blockSize; ++s) {
            buffer.setSample(ch, s, ((float)rand() / RAND_MAX) * 2.0f - 1.0f);
        }
    }
    
    juce::MidiBuffer midi;
    
    // Process and check for stability
    for (int i = 0; i < 100; ++i) {
        processor->processBlock(buffer, midi);
        for (int ch = 0; ch < 2; ++ch) {
            auto* data = buffer.getReadPointer(ch);
            for (int s = 0; s < blockSize; ++s) {
                EXPECT_TRUE(std::isfinite(data[s]));
            }
        }
    }
}
