#include "SlotModel.h"

SlotModel::SlotModel()
{
}

const juce::String& SlotModel::getSlotName() const
{
    return slotName;
}

void SlotModel::setSlotName(const juce::String& newName)
{
    slotName = newName;
}

bool SlotModel::isPluginLoaded() const
{
    return pluginLoaded;
}

void SlotModel::setPluginLoaded(bool shouldBeLoaded)
{
    pluginLoaded = shouldBeLoaded;
}

juce::String SlotModel::getLoadedPluginName() const
{
    return loadedPluginName;
}

void SlotModel::setLoadedPluginName(const juce::String& name)
{
    loadedPluginName = name;
}

juce::String SlotModel::getPluginPath() const
{
    return pluginPath;
}

void SlotModel::setPluginPath(const juce::String& path)
{
    pluginPath = path;
}

juce::String SlotModel::getLastError() const
{
    return lastError;
}

void SlotModel::setLastError(const juce::String& errorText)
{
    lastError = errorText;
}

void SlotModel::clearPlugin()
{
    pluginLoaded = false;
    loadedPluginName.clear();
    pluginPath.clear();
    lastError.clear();
}

juce::String SlotModel::getStatusText() const
{
    if (pluginLoaded)
        return "Loaded: " + loadedPluginName;

    if (lastError.isNotEmpty())
        return "Error: " + lastError;

    return "Empty";
}