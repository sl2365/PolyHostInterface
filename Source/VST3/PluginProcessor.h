#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "PluginCore.h"

class PolyHostPluginProcessor : public juce::AudioProcessor
{
public:
    PolyHostPluginProcessor();
    ~PolyHostPluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    struct PointerMidiEvent
    {
        int controllerNumber = -1;
        int controllerValue = 0;
        bool valid = false;
    };

    bool popNextPointerMidiEvent(PointerMidiEvent& dest);
    void pushPointerMidiEvent(int controllerNumber, int controllerValue);

    PluginCore& getCore();

private:
    PluginCore core;

    static constexpr int pointerMidiQueueSize = 128;
    PointerMidiEvent pointerMidiQueue[pointerMidiQueueSize];
    std::atomic<int> pointerMidiWriteIndex { 0 };
    std::atomic<int> pointerMidiReadIndex { 0 };
};