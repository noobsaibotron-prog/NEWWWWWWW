#include <juce_audio_processors/juce_audio_processors.h>
#include <gtest/gtest.h>
#include "../PluginProcessor.h"

class RecallDeterminismTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<AIEqualizerAudioProcessor>();
    }

    std::unique_ptr<AIEqualizerAudioProcessor> processor;
};

TEST_F(RecallDeterminismTest, StateSaveLoadIdentity) {
    auto& apvts = processor->getAPVTS();
    
    // 1. Set a complex state
    apvts.getParameter("masterGain")->setValueNotifyingHost(0.75f);
    apvts.getParameter("lowShelfGain")->setValueNotifyingHost(0.25f);
    apvts.getParameter("highShelfFreq")->setValueNotifyingHost(0.85f);
    apvts.getParameter("msMode")->setValueNotifyingHost(1.0f); // Mid/Side mode
    
    // 2. Save state to MemoryBlock
    juce::MemoryBlock savedState;
    processor->getStateInformation(savedState);
    
    // 3. Modify parameters to different values
    apvts.getParameter("masterGain")->setValueNotifyingHost(0.1f);
    apvts.getParameter("msMode")->setValueNotifyingHost(0.0f);
    
    // 4. Reload state
    processor->setStateInformation(savedState.getData(), (int)savedState.getSize());
    
    // 5. Verify identity
    EXPECT_NEAR(apvts.getParameter("masterGain")->getValue(), 0.75f, 1e-5f);
    EXPECT_NEAR(apvts.getParameter("lowShelfGain")->getValue(), 0.25f, 1e-5f);
    EXPECT_NEAR(apvts.getParameter("highShelfFreq")->getValue(), 0.85f, 1e-5f);
    EXPECT_EQ(apvts.getParameter("msMode")->getValue(), 1.0f);
}

TEST_F(RecallDeterminismTest, SEMANTIC_STATE_CONSISTENCY) {
    // Test that AI-driven semantic states are recalled correctly
    // This assumes AIEngine state is serialized in the APVTS or separate block
    // Implementation depends on how AIEngine state is handled in PluginProcessor
}
