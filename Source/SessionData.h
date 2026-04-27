#pragma once
#include <JuceHeader.h>
#include "SessionTypes.h"

struct SessionPluginData
{
    juce::String pluginName;
    juce::String pluginPath;
    juce::String pluginPathRelative;
    juce::String pluginPathDriveFlexible;
    juce::String pluginStateBase64;
    juce::String pluginFormatName;
    bool isInstrument = false;
    juce::String pluginManufacturer;
    juce::String pluginVersion;
};

struct SessionTabData
{
    int index = -1;
    PluginSlotType type = PluginSlotType::Empty;
    juce::String tabName;
    bool bypassed = false;
    juce::StringArray midiAssignedDeviceIdentifiers;
    bool hasPlugin = false;
    SessionPluginData plugin;
};

struct SessionData
{
    juce::String name;
    double hostTempoBpm = 120.0;
    juce::Array<SessionTabData> tabs;
};
