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

struct SessionWindowData
{
    int width = 900;
    int height = 520;
};

struct SessionData
{
    juce::String name;
    int selectedTab = 0;
    double hostTempoBpm = 120.0;
    bool hostSyncEnabled = false;
    SessionWindowData window;
    juce::Array<SessionTabData> tabs;
};