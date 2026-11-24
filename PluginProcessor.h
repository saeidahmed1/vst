#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class TashreeqAudioProcessor : public juce::AudioProcessor
{
public:
    enum class RoutingMode
    {
        PolyMPE = 0,
        MonoSingleCh = 1
    };

    TashreeqAudioProcessor();
    ~TashreeqAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tashreeq VST"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void setNoteCents(int pitchClass, float cents);
    float getNoteCents(int pitchClass) const;

    void setDepth(float newDepthPercent);
    float getDepth() const { return depthPercent.load(); }

    void setPitchBendRange(int semitones);
    int getPitchBendRange() const { return pitchBendRange.load(); }

    void setRoutingMode(RoutingMode mode);
    RoutingMode getRoutingMode() const { return static_cast<RoutingMode>(routingMode.load()); }

    void applyMaqamPreset(int maqamIndex, int rootNote, float quarterToneCents);
    void resetAllCents();

    bool loadHostedVST3(const juce::File& pluginFile, juce::String& errorOut);
    void unloadHostedVST3();
    bool hasHostedPlugin() const;
    juce::String getHostedPluginName() const;
    juce::AudioPluginInstance* getHostedPluginInstance() const { return hostedPlugin.get(); }

    juce::CriticalSection& getPluginLock() { return pluginLock; }

private:
    int calculatePitchBendValue(float cents) const;

    std::array<std::atomic<float>, 12> noteCents;
    std::atomic<float> depthPercent { 100.0f };
    std::atomic<int> pitchBendRange { 2 };
    std::atomic<int> routingMode { static_cast<int>(RoutingMode::PolyMPE) };

    std::array<int, 128> activeNoteChannel;
    int nextMpeChannel = 2;

    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> hostedPlugin;
    juce::PluginDescription hostedDescription;
    juce::String hostedPluginPath;
    juce::CriticalSection pluginLock;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TashreeqAudioProcessor)
};
