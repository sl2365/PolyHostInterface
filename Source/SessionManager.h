#pragma once
#include <JuceHeader.h>
#include "AppSettings.h"

enum class PluginSlotType
{
    Empty,
    Synth,
    FX
};

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
        juce::String title = "PolyHostInterface - " + getDisplayName();

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

    bool hasSavedFile() const
    {
        return getCurrentFile().existsAsFile();
    }

    void handleSuccessfulSave(const juce::File& file)
    {
        rememberSavedFile(file);
        document.markClean();
    }

    void clearCurrentSessionReference()
    {
        document.setCurrentPresetFile({});
        settings.setLastPresetPath({});
        document.markClean();
    }

private:
    AppSettings& settings;
    SessionDocument& document;
};

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

            if (lastPresetFile.existsAsFile())
                return lastPresetFile.getParentDirectory();
        }

        return AppSettings::getPresetsDirectory();
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

class PresetNamePromptHelper
{
public:
    juce::String promptForPresetName(const juce::String& initialName) const
    {
        juce::AlertWindow alert("Save Preset As",
                                "Enter a preset name:",
                                juce::AlertWindow::NoIcon);

        alert.addTextEditor("name", initialName, "Preset Name:");
        alert.addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        if (alert.runModalLoop() != 1)
            return {};

        return alert.getTextEditorContents("name").trim();
    }
};

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
        replacePlugin = 108,
        newPlugin = 109,
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
        menu.addItem(replacePlugin, "Replace Plugin...");
        menu.addItem(newPlugin, "New Plugin...");
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
            case replacePlugin:
            case newPlugin:
            case quit:
                return true;
            default:
                return false;
        }
    }
};

class UnsavedChangesHelper
{
public:
    enum class Decision
    {
        cancel,
        discard,
        save
    };

    Decision promptToSaveChanges() const
    {
        juce::AlertWindow alert("Unsaved Changes",
                                "Save changes to the current preset before continuing?",
                                juce::AlertWindow::WarningIcon);

        alert.addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert.addButton("Discard", 2);
        alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        auto result = alert.runModalLoop();

        if (result == 1)
            return Decision::save;

        if (result == 2)
            return Decision::discard;

        return Decision::cancel;
    }
};

class SessionManager
{
public:
    static bool saveSessionToFile(const SessionData& session, const juce::File& file);
    static bool loadSessionFromFile(const juce::File& file,
                                    SessionData& session,
                                    juce::StringArray& warnings);
};
