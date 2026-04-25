#pragma once
#include <JuceHeader.h>
#include "SessionData.h"

class SessionManager
{
public:
    static bool saveSessionToFile(const SessionData& session, const juce::File& file);
    static bool loadSessionFromFile(const juce::File& file,
                                    SessionData& session,
                                    juce::StringArray& warnings);
};