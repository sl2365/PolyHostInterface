#pragma once
#include <JuceHeader.h>

class SessionDocument
{
public:
    const juce::File& getCurrentPresetFile() const { return currentPresetFile; }
    void setCurrentPresetFile(const juce::File& file) { currentPresetFile = file; }

    bool hasCurrentPresetFile() const { return currentPresetFile.existsAsFile(); }

    juce::String getDisplayName() const
    {
        if (currentPresetFile.existsAsFile())
            return currentPresetFile.getFileNameWithoutExtension();

        return "Untitled";
    }

    bool isDirty() const { return dirty; }
    void markDirty() { dirty = true; }
    void markClean() { dirty = false; }

    juce::String buildWindowTitle() const
    {
        juce::String title = "PolyHost - " + getDisplayName();

        if (dirty)
            title += " *";

        return title;
    }

    void clear()
    {
        currentPresetFile = {};
        dirty = false;
    }

private:
    juce::File currentPresetFile;
    bool dirty = false;
};