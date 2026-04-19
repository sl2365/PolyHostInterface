#include "MidiEngine.h"

MidiEngine::MidiEngine(juce::AudioDeviceManager& dm) : deviceManager(dm) {}
MidiEngine::~MidiEngine() { closeCurrentDevice(); }

juce::StringArray MidiEngine::getAvailableDeviceNames() const
{
    juce::StringArray names;
    for (auto& info : juce::MidiInput::getAvailableDevices())
        names.add(info.name);
    return names;
}

void MidiEngine::openDevice(const juce::String& deviceIdentifier)
{
    closeCurrentDevice();
    for (auto& info : juce::MidiInput::getAvailableDevices())
    {
        if (info.identifier == deviceIdentifier || info.name == deviceIdentifier)
        {
            midiInput = juce::MidiInput::openDevice(info.identifier, this);
            if (midiInput) { midiInput->start(); deviceManager.setMidiInputDeviceEnabled(info.identifier, true); }
            break;
        }
    }
    sendChangeMessage();
}

void MidiEngine::closeCurrentDevice()
{
    if (midiInput)
    {
        midiInput->stop();
        deviceManager.setMidiInputDeviceEnabled(midiInput->getDeviceInfo().identifier, false);
        midiInput.reset();
        sendChangeMessage();
    }
}

juce::String MidiEngine::getCurrentDeviceName() const { return midiInput ? midiInput->getName() : "(none)"; }

juce::MidiMessage MidiEngine::getLastMessage() const { juce::ScopedLock sl(lock); return lastMessage; }

void MidiEngine::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg) { juce::ScopedLock sl(lock); lastMessage = msg; }
