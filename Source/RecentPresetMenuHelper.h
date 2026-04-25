#pragma once
#include <JuceHeader.h>
#include "PresetSessionController.h"

class RecentPresetMenuHelper
{
public:
    explicit RecentPresetMenuHelper(PresetSessionController& controllerIn)
        : controller(controllerIn)
    {
    }

    juce::PopupMenu buildRecentPresetMenu(int baseItemId) const
    {
        juce::PopupMenu menu;
        auto recentPresetPaths = controller.getRecentPresetPaths();

        int visibleIndex = 0;

        for (int i = 0; i < recentPresetPaths.size(); ++i)
        {
            juce::File presetFile(recentPresetPaths[i]);

            if (!presetFile.existsAsFile())
                continue;

            menu.addItem(baseItemId + visibleIndex,
                         presetFile.getFileNameWithoutExtension());
            ++visibleIndex;
        }

        if (visibleIndex == 0)
            menu.addItem(baseItemId, "(No Recent Presets)", false, false);

        return menu;
    }

    bool isRecentPresetItemId(int itemId, int baseItemId) const
    {
        return itemId >= baseItemId && itemId < baseItemId + 100;
    }

    juce::File getRecentPresetFileForItemId(int itemId, int baseItemId) const
    {
        auto recentPresetPaths = controller.getRecentPresetPaths();

        juce::Array<juce::File> validFiles;
        for (int i = 0; i < recentPresetPaths.size(); ++i)
        {
            juce::File presetFile(recentPresetPaths[i]);

            if (presetFile.existsAsFile())
                validFiles.add(presetFile);
        }

        int index = itemId - baseItemId;

        if (juce::isPositiveAndBelow(index, validFiles.size()))
            return validFiles.getReference(index);

        return {};
    }

private:
    PresetSessionController& controller;
};