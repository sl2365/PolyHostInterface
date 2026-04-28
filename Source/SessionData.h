#pragma once
#include <JuceHeader.h>
#include "SessionTypes.h"

struct SessionPluginData
{
    juce::String pluginName;
    juce::String pluginDescriptiveName;
    juce::String pluginPath;
    juce::String pluginPathRelative;
    juce::String pluginPathDriveFlexible;
    juce::String pluginStateBase64;
    juce::String pluginFormatName;
    juce::String pluginFileOrIdentifier;
    int pluginUniqueId = 0;
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
    bool hasSavedWindowBounds = false;
    int savedWindowWidth = 0;
    int savedWindowHeight = 0;
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