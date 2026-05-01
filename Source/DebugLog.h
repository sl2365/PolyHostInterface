#pragma once
#include <JuceHeader.h>

class DebugLog
{
public:
    static void setEnabled(bool shouldEnable);
    static bool isEnabled();

    static juce::File getLogFile();
    static void clear();
    static void write(const juce::String& message);

private:
    static bool enabled;
    static juce::CriticalSection lock;
};