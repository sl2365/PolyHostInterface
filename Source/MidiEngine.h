#pragma once
#include <JuceHeader.h>

class MidiEngine : private juce::MidiInputCallback,
                   public juce::ChangeBroadcaster
{
public:
    struct IncomingMidiMessage
    {
        juce::String deviceIdentifier;
        juce::String deviceName;
        juce::MidiMessage message;
    };

    explicit MidiEngine(juce::AudioDeviceManager& deviceManager);
    ~MidiEngine() override;

    juce::Array<juce::MidiDeviceInfo> getAvailableDevices() const;
    juce::StringArray getAvailableDeviceNames() const;

    void openDevice(const juce::String& deviceIdentifier);
    void closeDevice(const juce::String& deviceIdentifier);
    void closeAllDevices();

    bool isDeviceOpen(const juce::String& deviceIdentifier) const;
    juce::StringArray getOpenDeviceIdentifiers() const;
    juce::StringArray getOpenDeviceNames() const;

    juce::String getCurrentDeviceName() const;
    juce::MidiMessage getLastMessage() const;

    juce::Array<IncomingMidiMessage> popPendingMessages();

    std::function<void(const IncomingMidiMessage&)> onMidiMessageReceived;

private:
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    juce::AudioDeviceManager& deviceManager;
    juce::OwnedArray<juce::MidiInput> openMidiInputs;
    juce::MidiMessage lastMessage;
    juce::Array<IncomingMidiMessage> pendingMessages;
    mutable juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEngine)
};