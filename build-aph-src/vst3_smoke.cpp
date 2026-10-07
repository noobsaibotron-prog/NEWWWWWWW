#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <iostream>

/** Headless VST3 load smoke: opens the diagnostic bundle long enough for ApplyUndoTrace HEADER+PARAM. */
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::AudioPluginFormatManager formats;
    formats.addFormat (new juce::VST3PluginFormat());

    const juce::File bundle (
        juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile ("Library/Audio/Plug-Ins/VST3/EMBER CORE ApplyUndo.vst3"));

    if (! bundle.isDirectory())
    {
        std::cerr << "missing bundle: " << bundle.getFullPathName() << "\n";
        return 2;
    }

    juce::OwnedArray<juce::PluginDescription> types;
    for (int i = 0; i < formats.getNumFormats(); ++i)
        if (auto* f = formats.getFormat (i))
            f->findAllTypesForFile (types, bundle.getFullPathName());

    if (types.isEmpty())
    {
        std::cerr << "no plugin types found in bundle\n";
        return 3;
    }

    juce::String err;
    auto instance = formats.createPluginInstance (*types[0], 48000.0, 512, err);
    if (instance == nullptr)
    {
        std::cerr << "createPluginInstance failed: " << err << "\n";
        return 4;
    }

    instance->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (juce::jmax (2, instance->getTotalNumOutputChannels()), 512);
    juce::MidiBuffer midi;
    for (int n = 0; n < 20; ++n)
        instance->processBlock (buffer, midi);

    // HEADER/PARAM are written synchronously in Trace::start on load.
    // Give the 10 ms drain timer a moment, then tear down.
    juce::Thread::sleep (200);

    instance->releaseResources();
    instance.reset();
    juce::Thread::sleep (100);

    std::cout << "smoke load ok: " << types[0]->name << "\n";
    return 0;
}
