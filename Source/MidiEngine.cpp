#include "MidiEngine.h"

MidiEngine::MidiEngine(juce::AudioDeviceManager& dm) : deviceManager(dm) {}
MidiEngine::~MidiEngine() { closeAllDevices(); }

juce::Array<juce::MidiDeviceInfo> MidiEngine::getAvailableDevices() const
{
    return juce::MidiInput::getAvailableDevices();
}

juce::StringArray MidiEngine::getAvailableDeviceNames() const
{
    juce::StringArray names;
    for (auto& info : juce::MidiInput::getAvailableDevices())
        names.add(info.name);
    return names;
}

void MidiEngine::openDevice(const juce::String& deviceIdentifier)
{
    if (isDeviceOpen(deviceIdentifier))
        return;

    for (auto& info : juce::MidiInput::getAvailableDevices())
    {
        if (info.identifier == deviceIdentifier || info.name == deviceIdentifier)
        {
            auto input = juce::MidiInput::openDevice(info.identifier, this);
            if (input)
            {
                input->start();
                deviceManager.setMidiInputDeviceEnabled(info.identifier, true);
                openMidiInputs.add(input.release());
                sendChangeMessage();
            }
            return;
        }
    }
}

void MidiEngine::closeDevice(const juce::String& deviceIdentifier)
{
    for (int i = openMidiInputs.size(); --i >= 0;)
    {
        auto* input = openMidiInputs[i];
        auto info = input->getDeviceInfo();

        if (info.identifier == deviceIdentifier || info.name == deviceIdentifier)
        {
            input->stop();
            deviceManager.setMidiInputDeviceEnabled(info.identifier, false);
            openMidiInputs.remove(i);
            sendChangeMessage();
            return;
        }
    }
}

void MidiEngine::closeAllDevices()
{
    bool changed = false;

    for (int i = openMidiInputs.size(); --i >= 0;)
    {
        auto* input = openMidiInputs[i];
        auto info = input->getDeviceInfo();

        input->stop();
        deviceManager.setMidiInputDeviceEnabled(info.identifier, false);
        openMidiInputs.remove(i);
        changed = true;
    }

    if (changed)
        sendChangeMessage();
}

bool MidiEngine::isDeviceOpen(const juce::String& deviceIdentifier) const
{
    for (auto* input : openMidiInputs)
    {
        auto info = input->getDeviceInfo();
        if (info.identifier == deviceIdentifier || info.name == deviceIdentifier)
            return true;
    }

    return false;
}

juce::StringArray MidiEngine::getOpenDeviceIdentifiers() const
{
    juce::StringArray ids;

    for (auto* input : openMidiInputs)
        ids.add(input->getDeviceInfo().identifier);

    return ids;
}

juce::StringArray MidiEngine::getOpenDeviceNames() const
{
    juce::StringArray names;

    for (auto* input : openMidiInputs)
        names.add(input->getDeviceInfo().name);

    return names;
}

juce::String MidiEngine::getCurrentDeviceName() const
{
    auto names = getOpenDeviceNames();
    if (names.isEmpty())
        return "(none)";

    return names.joinIntoString(", ");
}

juce::MidiMessage MidiEngine::getLastMessage() const
{
    juce::ScopedLock sl(lock);
    return lastMessage;
}

juce::Array<MidiEngine::IncomingMidiMessage> MidiEngine::popPendingMessages()
{
    juce::ScopedLock sl(lock);
    auto result = pendingMessages;
    pendingMessages.clear();
    return result;
}

void MidiEngine::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& msg)
{
    IncomingMidiMessage incoming;
    incoming.message = msg;

    if (source != nullptr)
    {
        auto info = source->getDeviceInfo();
        incoming.deviceIdentifier = info.identifier;
        incoming.deviceName = info.name;
    }

    {
        juce::ScopedLock sl(lock);
        lastMessage = msg;
        pendingMessages.add(incoming);

        if (pendingMessages.size() > 512)
            pendingMessages.removeRange(0, pendingMessages.size() - 512);
    }

    if (onMidiMessageReceived)
        onMidiMessageReceived(incoming);
}