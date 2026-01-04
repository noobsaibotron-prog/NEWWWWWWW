#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <mutex>

//==============================================================================
/**
 * Parametric EQ Processor - Multi-band parametric equalizer
 * 
 * Features:
 * - Up to 8 fully parametric bands
 * - Multiple filter types per band
 * - Real-time coefficient updates
 * - Magnitude response calculation
 * - Low latency processing
 */
class ParametricEQProcessor
{
public:
    //==============================================================================
    // Filter types
    enum FilterType
    {
        LowCut = 0,     // High-pass filter
        LowShelf,       // Low shelf
        Peak,           // Parametric peak/notch
        HighShelf,      // High shelf
        HighCut,        // Low-pass filter
        Notch,          // Notch filter
        BandPass,       // Band-pass filter
        VintageLowShelf, // Pultec-style low shelf with analog modeling
        VintageHighShelf // Pultec-style high shelf with analog modeling
    };
    
    // Single EQ band
    struct Band
    {
        float frequency = 1000.0f;
        float gain = 0.0f;          // dB
        float q = 1.0f;
        FilterType type = Peak;
        bool enabled = true;
        bool solo = false;
        bool vintageMode = false;  // Enable analog modeling (soft clipping)
        
        // Processing state
        juce::dsp::IIR::Filter<float> filterL;
        juce::dsp::IIR::Filter<float> filterR;
        juce::dsp::IIR::Coefficients<float>::Ptr coefficients;
        
        bool needsUpdate = true;
        
        // Default constructor
        Band() = default;
        
        // Move constructor (required because filters are not copyable)
        Band(Band&&) noexcept = default;
        
        // Move assignment
        Band& operator=(Band&&) noexcept = default;
        
        // Delete copy constructor and assignment (filters are not copyable)
        Band(const Band&) = delete;
        Band& operator=(const Band&) = delete;
    };

    //==============================================================================
    ParametricEQProcessor();
    ~ParametricEQProcessor() = default;

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    
    //==============================================================================
    // Band management
    int addBand(float freq, float gain, float q, int type);
    void removeBand(int index);
    void clearAllBands();
    
    int getNumBands() const { return static_cast<int>(bands.size()); }
    static constexpr int getMaxBands() { return 24; }
    
    //==============================================================================
    // Band parameters
    void setBandFrequency(int index, float freq);
    void setBandGain(int index, float gain);
    void setBandQ(int index, float q);
    void setBandType(int index, int type);
    void setBandEnabled(int index, bool enabled);
    void setBandSolo(int index, bool solo);
    
    void setBandParameters(int index, float freq, float gain, float q, int type);
    
    float getBandFrequency(int index) const;
    float getBandGain(int index) const;
    float getBandQ(int index) const;
    int getBandType(int index) const;
    bool isBandEnabled(int index) const;
    bool isBandSolo(int index) const;
    
    //==============================================================================
    // Magnitude response
    float getMagnitudeForFrequency(float freq, double sampleRate) const;
    void getMagnitudeForFrequencyArray(const float* frequencies, float* magnitudes, 
                                        size_t numPoints, double sampleRate) const;
    
    //==============================================================================
    // Global controls
    void setOutputGain(float gainDB) { outputGain = juce::Decibels::decibelsToGain(gainDB); }
    float getOutputGain() const { return juce::Decibels::gainToDecibels(outputGain); }
    
    void setBypass(bool shouldBypass) { bypassed = shouldBypass; }
    bool isBypassed() const { return bypassed; }
    
    //==============================================================================
    // Get band for external access
    const Band& getBand(int index) const;

private:
    //==============================================================================
    void updateCoefficients(int index);
    void updateAllCoefficients();
    
    juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients(
        FilterType type, float freq, float gain, float q, double sampleRate) const;
    
    //==============================================================================
    std::vector<Band> bands;
    mutable std::mutex bandsMutex;
    
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int numChannels = 2;
    
    float outputGain = 1.0f;
    bool bypassed = false;
    
    // Dummy band for invalid access
    Band dummyBand;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametricEQProcessor)
};
