#include "DebugLog.h"

bool DebugLog::enabled = true;
bool DebugLog::advancedEnabled = false;
juce::CriticalSection DebugLog::lock;

juce::File DebugLog::getLogFile()
{
    auto settingsDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getChildFile("Settings");

    settingsDir.createDirectory();
    return settingsDir.getChildFile("polyhost.log");
}

void DebugLog::setEnabled(bool shouldEnable)
{
    const juce::ScopedLock sl(lock);
    enabled = shouldEnable;
}

bool DebugLog::isEnabled()
{
    const juce::ScopedLock sl(lock);
    return enabled;
}

void DebugLog::setAdvancedEnabled(bool shouldEnable)
{
    const juce::ScopedLock sl(lock);
    advancedEnabled = shouldEnable;
}

bool DebugLog::isAdvancedEnabled()
{
    const juce::ScopedLock sl(lock);
    return advancedEnabled;
}

void DebugLog::clear()
{
    const juce::ScopedLock sl(lock);
    getLogFile().deleteFile();
}

void DebugLog::write(const juce::String& message)
{
    const juce::ScopedLock sl(lock);

    if (!enabled)
        return;

    auto line = juce::Time::getCurrentTime().toString(true, true)
              + " | "
              + message
              + "\n";

    auto file = getLogFile();

    if (!file.existsAsFile())
        file.create();

    file.appendText(line, false, false, "\n");
}

void DebugLog::writeAdvanced(const juce::String& message)
{
    const juce::ScopedLock sl(lock);

    if (!enabled || !advancedEnabled)
        return;

    auto line = juce::Time::getCurrentTime().toString(true, true)
              + " | [ADV] "
              + message
              + "\n";

    auto file = getLogFile();

    if (!file.existsAsFile())
        file.create();

    file.appendText(line, false, false, "\n");
}