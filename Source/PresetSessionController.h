#pragma once
#include <JuceHeader.h>
#include "AppSettings.h"
#include "SessionDocument.h"

class PresetSessionController
{
public:
    PresetSessionController(AppSettings& settingsIn, SessionDocument& documentIn)
        : settings(settingsIn), document(documentIn)
    {
    }

    const juce::File& getCurrentFile() const
    {
        return document.getCurrentPresetFile();
    }

    bool hasCurrentFile() const
    {
        return document.getCurrentPresetFile().existsAsFile();
    }

    void setCurrentFile(const juce::File& file)
    {
        document.setCurrentPresetFile(file);
    }

    void clearCurrentFile()
    {
        document.setCurrentPresetFile({});
    }

    void rememberSavedFile(const juce::File& file)
    {
        document.setCurrentPresetFile(file);
        settings.setLastPresetPath(file.getFullPathName());
        settings.addRecentPresetPath(file.getFullPathName());
        document.markClean();
    }

    void rememberLoadedFile(const juce::File& file)
    {
        document.setCurrentPresetFile(file);
        settings.setLastPresetPath(file.getFullPathName());
        settings.addRecentPresetPath(file.getFullPathName());
        document.markClean();
    }

    void clearForNewPreset()
    {
        document.setCurrentPresetFile({});
        document.markClean();
    }

    bool isCurrentFile(const juce::File& file) const
    {
        return document.getCurrentPresetFile() == file;
    }

    juce::String getCurrentFileDisplayName() const
    {
        if (document.getCurrentPresetFile().existsAsFile())
            return document.getCurrentPresetFile().getFileNameWithoutExtension();

        return {};
    }

    juce::StringArray getRecentPresetPaths() const
    {
        return settings.getRecentPresetPaths();
    }

    void forgetFile(const juce::File& file)
    {
        juce::ignoreUnused(file);

        if (document.getCurrentPresetFile() == file)
            document.setCurrentPresetFile({});
    }

    void removeMissingRecentPresetPaths()
    {
        settings.removeMissingRecentPresetPaths();
    }

private:
    AppSettings& settings;
    SessionDocument& document;
};