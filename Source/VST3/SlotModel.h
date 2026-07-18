#pragma once
#include <JuceHeader.h>

class SlotModel
{
public:
    SlotModel();

    const juce::String& getSlotName() const;
    void setSlotName(const juce::String& newName);

    bool isPluginLoaded() const;
    void setPluginLoaded(bool shouldBeLoaded);

    juce::String getLoadedPluginName() const;
    void setLoadedPluginName(const juce::String& name);

    juce::String getPluginPath() const;
    void setPluginPath(const juce::String& path);

    juce::String getLastError() const;
    void setLastError(const juce::String& errorText);

    void clearPlugin();

    juce::String getStatusText() const;

private:
    juce::String slotName { "Main Slot" };
    bool pluginLoaded = false;
    juce::String loadedPluginName;
    juce::String pluginPath;
    juce::String lastError;
};