#pragma once
#include <JuceHeader.h>

class DebugLog
{
public:
    enum class Level
    {
        Basic,
        Advanced
    };

    static void setEnabled(bool shouldEnable);
    static bool isEnabled();

    static void setAdvancedEnabled(bool shouldEnable);
    static bool isAdvancedEnabled();

    static juce::File getLogFile();
    static void clear();

    static void write(const juce::String& message);
    static void writeAdvanced(const juce::String& message);

private:
    static bool enabled;
    static bool advancedEnabled;
    static juce::CriticalSection lock;
};