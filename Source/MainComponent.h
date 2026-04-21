#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "MidiEngine.h"
#include "PluginTabComponent.h"
#include "AppSettings.h"
#include "MidiMonitorWindow.h"

class MainComponent final : public juce::Component,
                            public juce::MenuBarModel,
                            private juce::ChangeListener
{
public:
    static constexpr int kMaxSynthTabs = 2;
    static constexpr int kMaxFxTabs    = 8;

    MainComponent();
    ~MainComponent() override;

    void loadPluginFromFile(const juce::File& file);
    void resized() override;
    void paint(juce::Graphics& g) override;
    bool requestQuit();

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int index, const juce::String& name) override;
    void menuItemSelected(int itemId, int menuIndex) override;

    AppSettings& getSettings() { return settings; }

private:
    struct MissingPluginEntry
    {
        int tabIndex = -1;
        PluginTabComponent::SlotType slotType = PluginTabComponent::SlotType::Empty;
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

    struct PluginReplacementCandidate
    {
        juce::File file;
        juce::PluginDescription desc;
        int score = 0;
    };

    // Core UI / tab helpers
    void addEmptyTab();
    void refreshTabAppearance(int tabIndex);
    int countTabsOfType(PluginTabComponent::SlotType type) const;
    PluginTabComponent* getTabComponent(int tabIndex) const;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Preset/session helpers
    void newPreset();
    void savePreset();
    void savePresetAs();
    void loadPreset();
    void deletePreset();
    void clearAllPlugins();
    bool writePresetToFile(const juce::File& file);
    void updateWindowTitle();
    void rebuildTabsFromPresetXml(const juce::XmlElement& presetXml);
    bool maybeSaveChanges();
    void markSessionDirty();
    void markSessionClean();
    void showPresetLoadErrors(const juce::StringArray& errors);

    // Missing plugin repair
    void locateMissingPlugins();
    bool promptToLocateMissingPlugins(const juce::Array<MissingPluginEntry>& missingPlugins);
    int promptForMissingPluginRepairAction(const MissingPluginEntry& entry,
                                           int currentIndex,
                                           int totalCount) const;
    bool locateMissingPlugin(MissingPluginEntry& entry);
    bool confirmReplacementPlugin(const MissingPluginEntry& entry, const juce::File& replacementFile);
    bool scanPluginFile(const juce::File& pluginFile, juce::PluginDescription& desc) const;
    bool validateReplacementPluginCompatibility(const MissingPluginEntry& entry,
                                                const juce::File& replacementFile);
    juce::File tryAutoLocateReplacement(const MissingPluginEntry& entry) const;
    juce::File tryAutoLocateReplacementByMetadata(const MissingPluginEntry& entry) const;
    juce::Array<PluginReplacementCandidate> findReplacementCandidates(const MissingPluginEntry& entry) const;
    juce::File chooseReplacementCandidate(const MissingPluginEntry& entry,
                                          const juce::Array<PluginReplacementCandidate>& candidates);
    int getPluginMatchScore(const MissingPluginEntry& entry,
                            const juce::PluginDescription& desc) const;
    juce::String getExpectedPluginFileName(const MissingPluginEntry& entry) const;
    void showMissingPluginRepairResult(const juce::StringArray& restored,
                                       const juce::StringArray& failed,
                                       const juce::StringArray& skipped);

    // Plugin scan folder actions
    void addPluginScanFolder();
    void showPluginScanFolders();
    void clearPluginScanFolders();

    AppSettings settings;
    AudioEngine audioEngine;
    MidiEngine midiEngine { audioEngine.getDeviceManager() };

    juce::MenuBarComponent menuBar { this };
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Label statusBar;
    std::unique_ptr<MidiMonitorWindow> midiMonitorWindow;
    juce::File currentPresetFile;
    juce::File lastPluginRepairDirectory;
    bool isLoadingPreset = false;
    bool isSessionDirty = false;
    juce::Array<MissingPluginEntry> unresolvedMissingPlugins;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};