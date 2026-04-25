#pragma once
#include <JuceHeader.h>
#include "AppSettings.h"
#include "PresetSessionController.h"

class PresetFileDialogHelper
{
public:
    PresetFileDialogHelper(AppSettings& settingsIn, PresetSessionController& controllerIn)
        : settings(settingsIn), controller(controllerIn)
    {
    }

    juce::File getDefaultPresetDirectory() const
    {
        auto lastPresetPath = settings.getLastPresetPath().trim();

        if (lastPresetPath.isNotEmpty())
        {
            juce::File lastPresetFile(lastPresetPath);

            if (lastPresetFile.exists())
                return lastPresetFile.getParentDirectory();
        }

        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    }

    juce::String getSuggestedSaveName() const
    {
        if (controller.getCurrentFile().existsAsFile())
            return controller.getCurrentFile().getFileNameWithoutExtension();

        return {};
    }

    juce::File withXmlExtension(const juce::File& file) const
    {
        if (file.hasFileExtension("xml"))
            return file;

        return file.withFileExtension(".xml");
    }

    juce::File buildSaveFileForPresetName(const juce::String& presetName) const
    {
        return getDefaultPresetDirectory().getChildFile(presetName).withFileExtension(".xml");
    }

private:
    AppSettings& settings;
    PresetSessionController& controller;
};