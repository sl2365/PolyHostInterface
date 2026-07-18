#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "PluginCore.h"

class PolyHostPluginProcessor : public juce::AudioProcessor,
                                private juce::AsyncUpdater,
                                private juce::Timer
{
public:
    class MacroParameter final : public juce::AudioProcessorParameter
    {
    public:
        MacroParameter(PolyHostPluginProcessor& ownerIn, int macroIndexIn);

        float getValue() const override;
        void setValue(float newValue) override;
        float getDefaultValue() const override;
        juce::String getName(int maximumStringLength) const override;
        juce::String getLabel() const override;
        juce::String getText(float value, int maximumStringLength) const override;
        float getValueForText(const juce::String& text) const override;
        bool isAutomatable() const override;
        bool isDiscrete() const override;
        bool isBoolean() const override;
        int getNumSteps() const override;
        bool isOrientationInverted() const override;
        bool isMetaParameter() const override;
        juce::AudioProcessorParameter::Category getCategory() const override;

    private:
        PolyHostPluginProcessor& owner;
        int macroIndex = -1;
    };

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

    struct MidiMonitorEvent
    {
        juce::MidiMessage message;
        juce::String sourceName;
        bool valid = false;
    };

    bool popNextPointerMidiEvent(PointerMidiEvent& dest);
    void pushPointerMidiEvent(int controllerNumber, int controllerValue);

    juce::Array<MidiMonitorEvent> popPendingMidiMonitorEvents();
    void pushMidiMonitorEvent(
        const juce::MidiMessage& message);

    PluginCore& getCore();

    class StandaloneAudioExtension
    {
    public:
        virtual ~StandaloneAudioExtension() = default;

        virtual void prepareToPlay(double sampleRate,
                                   int samplesPerBlock) = 0;

        virtual void processOutputBlock(
            juce::AudioBuffer<float>& buffer) = 0;

        virtual void releaseResources() = 0;
    };

    void setStandaloneAudioExtension(
        StandaloneAudioExtension* extension);

private:
    void initialiseMacroParameters();

    void setMacroParameterValue(int macroIndex,
                                float normalizedValue);

    void queuePointerAutomationHostNotification(
        int macroIndex,
        float normalizedValue);

    void handleAsyncUpdate() override;
    void timerCallback() override;

    void notifyHostOfPointerAutomation(
        int macroIndex,
        float normalizedValue);

    void endPointerAutomationGesture();

    StandaloneAudioExtension* standaloneAudioExtension = nullptr;
    PluginCore core;
    std::vector<MacroParameter*> macroParameters;

    std::atomic<int> pendingPointerAutomationMacroIndex { -1 };
    std::atomic<float> pendingPointerAutomationValue { 0.0f };
    std::atomic<juce::uint32> pendingPointerAutomationSequence { 0 };

    juce::uint32 lastHandledPointerAutomationSequence = 0;
    std::atomic<int> pointerAutomationNotificationMacroIndex { -1 };
    int activePointerAutomationMacroIndex = -1;
    juce::uint32 lastPointerAutomationChangeMs = 0;

    static constexpr int pointerMidiQueueSize = 128;
    PointerMidiEvent pointerMidiQueue[pointerMidiQueueSize];
    std::atomic<int> pointerMidiWriteIndex { 0 };
    std::atomic<int> pointerMidiReadIndex { 0 };

    static constexpr int midiMonitorMaxMessageBytes = 1024;
    static constexpr int midiMonitorQueueSize = 512;

    struct RawMidiMonitorEvent
    {
        std::array<
            juce::uint8,
            midiMonitorMaxMessageBytes> data {};

        int dataSize = 0;
        double timeStamp = 0.0;
    };

    RawMidiMonitorEvent
        midiMonitorQueue[midiMonitorQueueSize];

    std::atomic<int> midiMonitorWriteIndex { 0 };
    std::atomic<int> midiMonitorReadIndex { 0 };
};