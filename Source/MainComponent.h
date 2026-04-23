#pragma once
#include <cmath>
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
                            public juce::ToolbarItemFactory,
                            private juce::TextEditor::Listener,
                            private juce::Timer
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

    class TempoControlBackgroundComponent final : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(0.5f);

            g.setColour(juce::Colour(0xFF16213E));
            g.fillRoundedRectangle(r, 8.0f);

            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.drawRoundedRectangle(r, 8.0f, 1.0f);
        }
    };

    class TempoTextEditor final : public juce::TextEditor
    {
    public:
        explicit TempoTextEditor(MainComponent& ownerIn) : owner(ownerIn) {}

        void mouseWheelMove(const juce::MouseEvent& event,
                            const juce::MouseWheelDetails& wheel) override
        {
            if (! isInteractive())
            {
                juce::TextEditor::mouseWheelMove(event, wheel);
                return;
            }

            if (wheel.deltaY == 0.0f)
            {
                juce::TextEditor::mouseWheelMove(event, wheel);
                return;
            }

            const bool fineAdjust = event.mods.isShiftDown();
            const double step = fineAdjust ? 0.1 : 1.0;
            const double direction = (wheel.deltaY > 0.0f) ? 1.0 : -1.0;

            owner.setHostTempoBpm(owner.hostTempoBpm + direction * step);
        }

        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            juce::ignoreUnused(event);
            if (! isInteractive())
                return;
            owner.setHostTempoBpm(owner.defaultTempoBpm);
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (! isInteractive())
                return juce::TextEditor::keyPressed(key);

            if (key == juce::KeyPress(juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0))
            {
                owner.setHostTempoBpm(owner.hostTempoBpm + 0.1);
                return true;
            }

            if (key == juce::KeyPress(juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier, 0))
            {
                owner.setHostTempoBpm(owner.hostTempoBpm - 0.1);
                return true;
            }

            if (key == juce::KeyPress::upKey)
            {
                owner.setHostTempoBpm(owner.hostTempoBpm + 1.0);
                return true;
            }

            if (key == juce::KeyPress::downKey)
            {
                owner.setHostTempoBpm(owner.hostTempoBpm - 1.0);
                return true;
            }

            return juce::TextEditor::keyPressed(key);
        }

        bool isInteractive() const
        {
            return owner.isTempoEditorInteractive();
        }

    private:
        MainComponent& owner;
    };

    class BeatIndicatorComponent final : public juce::Component,
                                         private juce::Timer
    {
    public:
        explicit BeatIndicatorComponent(MainComponent& ownerIn) : owner(ownerIn)
        {
            startTimerHz(30);
        }

        void paint(juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat().reduced(2.0f);
            auto beatWidth = area.getWidth() / 4.0f;

            const auto bpm = owner.hostTempoBpm;
            const auto nowMs = juce::Time::getMillisecondCounterHiRes();
            const auto beatLengthMs = 60000.0 / juce::jmax(1.0, bpm);
            const int beatIndex = (int) std::floor(nowMs / beatLengthMs) % 4;

            for (int i = 0; i < 4; ++i)
            {
                auto r = juce::Rectangle<float>(area.getX() + beatWidth * (float) i + 1.5f,
                                                area.getY() + 2.0f,
                                                beatWidth - 6.0f,
                                                area.getHeight() - 4.0f);

                const bool isActive = (i == beatIndex);

                float pulse = 1.0f;

                if (isActive)
                {
                    auto beatProgress = std::fmod(nowMs, beatLengthMs) / beatLengthMs;
                    pulse = 0.75f + 0.25f * (1.0f - (float) beatProgress);
                }

                auto baseColour = (i == 0) ? juce::Colours::red : juce::Colours::limegreen;
                auto fillColour = isActive ? baseColour.brighter(0.35f * pulse + 0.15f)
                                           : baseColour.darker(0.75f).withAlpha(0.22f);

                auto borderColour = isActive ? baseColour.brighter(0.55f * pulse + 0.25f)
                                             : baseColour.darker(0.5f).withAlpha(0.35f);

                auto highlightColour = isActive ? juce::Colours::white.withAlpha(0.20f + 0.12f * pulse)
                                                : juce::Colours::white.withAlpha(0.06f);

                const float cornerSize = 5.0f;

                g.setColour(fillColour);
                g.fillRoundedRectangle(r, cornerSize);

                auto highlight = r.reduced(2.0f, 2.0f).removeFromTop(r.getHeight() * 0.38f);
                g.setColour(highlightColour);
                g.fillRoundedRectangle(highlight, cornerSize * 0.75f);

                g.setColour(borderColour);
                g.drawRoundedRectangle(r, cornerSize, 1.0f);
            }
        }

    private:
        void timerCallback() override
        {
            repaint();
        }

        MainComponent& owner;
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
        bool bypassed = false;
        juce::File pluginFile;
        juce::MemoryBlock pluginState;
        PluginTabComponent::SlotType type = PluginTabComponent::SlotType::Empty;
        juce::String tabName;
        juce::StringArray midiAssignedDeviceIdentifiers;
    };

    struct MidiTabRoutingState
    {
        int tabIndex = -1;
        bool expanded = true;
        juce::StringArray assignedDeviceIdentifiers;
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
    void syncRoutingToAudioEngine();
    void moveTabEarlier(int tabIndex);
    void moveTabLater(int tabIndex);
    void toggleTabBypass(int tabIndex);
    void updateStatusBarText();
    enum ToolbarItemIds
    {
        toolbarRoutingToggle = 10001
    };
    enum MenuItemIds
    {
        menuHostSyncToggle = 303,
        menuSetFakeHostTempo = 304,
        menuClearFakeHostTempo = 305
    };

    void getAllToolbarItemIds(juce::Array<int>& ids) override;
    void getDefaultItemSet(juce::Array<int>& ids) override;
    juce::ToolbarItemComponent* createItem(int itemId) override;

    // MIDI
    MidiTabRoutingState* getMidiRoutingStateForTab(int tabIndex);
    const MidiTabRoutingState* getMidiRoutingStateForTab(int tabIndex) const;
    void ensureMidiRoutingStateForCurrentTabs();
    void refreshMidiRoutingView();
    void toggleMidiRoutingExpanded(int tabIndex);
    void toggleMidiAssignment(int tabIndex, const juce::String& deviceIdentifier);
    juce::Array<MidiTabRoutingState> midiRoutingStates;
    void showMidiAssignmentsCallout(int tabIndex, juce::Component* anchorComponent);

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

    // Tempo
    bool hostSyncEnabled = false;
    bool isTempoEditorInteractive() const;
    double hostTempoBpm = 120.0;
    double defaultTempoBpm = 120.0;
    double lastDisplayedSyncedTempoBpm = -1.0;
    bool lastDisplayedHostTempoAvailable = false;

    BeatIndicatorComponent beatIndicator { *this };
    TempoControlBackgroundComponent tempoControlBackground;
    juce::TooltipWindow tooltipWindow { this };
    juce::Label tempoLabel;
    TempoTextEditor tempoEditor { *this };
    juce::TextButton tapTempoButton { "Tap" };
    juce::Array<double> tapTimesMs;

    void timerCallback() override;
    void refreshTempoUiFromEngine();
    void updateTempoControlState();
    void setHostTempoBpm(double bpm);
    void updateTempoTooltip();
    void updateTempoUi();
    void registerTapTempo();

    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;
    void textEditorFocusLost(juce::TextEditor& editor) override;
    void commitTempoFromEditor();

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