#pragma once
#include <JuceHeader.h>

class FileMenuHelper
{
public:
    enum ItemIds
    {
        newPreset = 100,
        newTab = 101,
        closeCurrentTab = 102,
        savePreset = 103,
        savePresetAs = 104,
        loadPreset = 105,
        deleteCurrentPreset = 106,
        locateMissingPlugins = 107,
        quit = 199
    };

    juce::PopupMenu buildFileMenu(const juce::PopupMenu& recentMenu,
                                  bool canLocateMissingPlugins,
                                  int openPresetsFolderItemId) const
    {
        juce::PopupMenu menu;
        menu.addItem(newPreset, "New Preset");
        menu.addSeparator();
        menu.addItem(newTab, "New Tab");
        menu.addItem(closeCurrentTab, "Close Current Tab");
        menu.addSeparator();
        menu.addItem(savePreset, "Save Preset");
        menu.addItem(savePresetAs, "Save Preset As...");
        menu.addItem(loadPreset, "Load Preset");
        menu.addSubMenu("Recent Presets", recentMenu);
        menu.addItem(locateMissingPlugins, "Locate Missing Plugins...",
                     canLocateMissingPlugins);
        menu.addItem(deleteCurrentPreset, "Delete Current Preset");
        menu.addSeparator();
        menu.addItem(openPresetsFolderItemId, "Open Presets Folder");
        menu.addSeparator();
        menu.addItem(quit, "Quit");
        return menu;
    }

    bool isFileCommand(int itemId) const
    {
        switch (itemId)
        {
            case newPreset:
            case newTab:
            case closeCurrentTab:
            case savePreset:
            case savePresetAs:
            case loadPreset:
            case deleteCurrentPreset:
            case locateMissingPlugins:
            case quit:
                return true;
            default:
                return false;
        }
    }
};