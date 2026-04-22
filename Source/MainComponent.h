#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "MidiEngine.h"
#include "PluginTabComponent.h"
#include "AppSettings.h"
#include "MidiMonitorWindow.h"
#include "RoutingView.h"

class MainComponent final : public juce::Component,
                            public juce::MenuBarModel,
                            private juce::ChangeListener,
                            public juce::ToolbarItemFactory
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
    class TabBarMouseListener final : public juce::MouseListener
    {
    public:
        explicit TabBarMouseListener(MainComponent& ownerIn) : owner(ownerIn) {}

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (!e.mods.isRightButtonDown())
                return;

            auto& tabBar = owner.tabs.getTabbedButtonBar();

            for (int i = 0; i < tabBar.getNumTabs(); ++i)
            {
                if (auto* button = tabBar.getTabButton(i))
                {
                    auto boundsInBar = button->getBounds();

                    if (boundsInBar.contains(e.getEventRelativeTo(&tabBar).position.toInt()))
                    {
                        owner.showTabContextMenu(i);
                        return;
                    }
                }
            }
        }

    private:
        MainComponent& owner;
    };

    class TabBarLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawTabButton(juce::TabBarButton& button,
                           juce::Graphics& g,
                           bool isMouseOver,
                           bool /*isMouseDown*/) override
        {
            auto area = button.getLocalBounds().toFloat().reduced(2.0f, 2.0f);

            auto activeColour = button.getTabBackgroundColour();
            auto colour = button.isFrontTab() ? activeColour.brighter(0.1f)
                                              : activeColour.darker(0.7f);

            if (isMouseOver)
                colour = colour.brighter(0.1f);

            const float cornerSize = 6.0f;

            juce::Path tabShape;
            tabShape.startNewSubPath(area.getBottomLeft());
            tabShape.lineTo(area.getX(), area.getY() + cornerSize);
            tabShape.quadraticTo(area.getX(), area.getY(),
                                 area.getX() + cornerSize, area.getY());
            tabShape.lineTo(area.getRight() - cornerSize, area.getY());
            tabShape.quadraticTo(area.getRight(), area.getY(),
                                 area.getRight(), area.getY() + cornerSize);
            tabShape.lineTo(area.getRight(), area.getBottom());
            tabShape.closeSubPath();

            g.setColour(colour);
            g.fillPath(tabShape);

            g.setColour(button.isFrontTab() ? juce::Colours::white
                                            : juce::Colours::lightgrey);

            g.setFont(juce::Font(juce::FontOptions(
                14.0f,
                button.isFrontTab() ? juce::Font::bold : juce::Font::plain)));

            g.drawFittedText(button.getButtonText(),
                             button.getLocalBounds().reduced(10, 2),
                             juce::Justification::centred,
                             1);
        }
    };

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

    struct TabSnapshot
    {
        bool hasPlugin = false;
        juce::File pluginFile;
        juce::MemoryBlock pluginState;
        PluginTabComponent::SlotType type = PluginTabComponent::SlotType::Empty;
        juce::String tabName;
    };

    // Core UI / tab helpers
    void addEmptyTab();
    void refreshTabAppearance(int tabIndex);
    int countTabsOfType(PluginTabComponent::SlotType type) const;
    PluginTabComponent* getTabComponent(int tabIndex) const;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void closeCurrentTab();
    void showTabContextMenu(int tabIndex);
    bool confirmCloseTab(int tabIndex) const;
    bool confirmClearTab(int tabIndex) const;
    void setRoutingViewVisible(bool shouldShow);
    void toggleRoutingView();
    void refreshRoutingView();
    void moveTabEarlier(int tabIndex);
    void moveTabLater(int tabIndex);
    enum ToolbarItemIds
    {
        toolbarRoutingToggle = 10001
    };

    void getAllToolbarItemIds(juce::Array<int>& ids) override;
    void getDefaultItemSet(juce::Array<int>& ids) override;
    juce::ToolbarItemComponent* createItem(int itemId) override;

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
    juce::Array<juce::File> collectPluginFilesInFolder(const juce::File& folder,
                                                       bool recursive) const;
    int getPluginMatchScore(const MissingPluginEntry& entry,
                            const juce::PluginDescription& desc) const;
    juce::String getExpectedPluginFileName(const MissingPluginEntry& entry) const;
    void showMissingPluginRepairResult(const juce::StringArray& restored,
                                       const juce::StringArray& failed,
                                       const juce::StringArray& skipped);
    juce::File browseForReplacementPlugin(const MissingPluginEntry& entry,
                                          const juce::File& startDir) const;
    bool applyReplacementPlugin(const MissingPluginEntry& entry,
                                const juce::File& replacementFile);
    juce::File findReplacementPluginFile(const MissingPluginEntry& entry,
                                         const juce::File& startDir);
    juce::String buildMissingPluginRepairSummary(const juce::StringArray& restored,
                                                 const juce::StringArray& failed,
                                                 const juce::StringArray& skipped) const;

    // Plugin scan folder actions
    void addPluginScanFolder();
    void showPluginScanFolders();
    void clearPluginScanFolders();

    AppSettings settings;
    AudioEngine audioEngine;
    MidiEngine midiEngine { audioEngine.getDeviceManager() };

    juce::MenuBarComponent menuBar { this };
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    TabBarLookAndFeel tabBarLookAndFeel;
    TabBarMouseListener tabBarMouseListener { *this };
    juce::Label statusBar;
    juce::Toolbar toolbar;
    RoutingView routingView;
    bool showingRoutingView = false;
    std::unique_ptr<MidiMonitorWindow> midiMonitorWindow;
    juce::File currentPresetFile;
    juce::File lastPluginRepairDirectory;
    bool isLoadingPreset = false;
    bool isSessionDirty = false;
    juce::Array<MissingPluginEntry> unresolvedMissingPlugins;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};