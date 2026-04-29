#pragma once
#include <cmath>
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "MidiEngine.h"
#include "PluginTabComponent.h"
#include "AppSettings.h"
#include "MidiMonitorWindow.h"
#include "RoutingView.h"
#include "SessionData.h"
#include "SessionDocument.h"
#include "SessionManager.h"
#include "StandaloneTempoSupport.h"
#include "PresetSessionController.h"
#include "PresetFileDialogHelper.h"
#include "RecentPresetMenuHelper.h"
#include "FileMenuHelper.h"
#include "UnsavedChangesHelper.h"
#include "PresetNamePromptHelper.h"

namespace
{
    class StatusMetersComponent final : public juce::Component,
                                        private juce::Timer
    {
    public:
        explicit StatusMetersComponent(AudioEngine& engineIn)
            : engine(engineIn)
        {
            startTimerHz(24);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF0F3460));

            auto area = getLocalBounds().reduced(4, 4);

            constexpr int rowHeight = 7;
            constexpr int rowGap = 3;

            drawMeterRow(g, area.removeFromTop(rowHeight), "IN",
                         juce::jmax(engine.getInputMeterLevelL(), engine.getInputMeterLevelR()));

            area.removeFromTop(rowGap);

            drawMeterRow(g, area.removeFromTop(rowHeight), "OUT",
                         juce::jmax(engine.getOutputMeterLevelL(), engine.getOutputMeterLevelR()));
        }

    private:
        void timerCallback() override
        {
            repaint();
        }

        void drawMeterRow(juce::Graphics& g,
                          juce::Rectangle<int> row,
                          const juce::String& label,
                          float level)
        {
            auto labelArea = row.removeFromLeft(24);

            auto meterArea = row;
            const int meterHeight = 6;
            meterArea = meterArea.withSizeKeepingCentre(meterArea.getWidth(), meterHeight);

            g.setColour(juce::Colours::lightgrey.withAlpha(0.85f));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
            g.drawText(label, labelArea, juce::Justification::centredLeft);

            g.setColour(juce::Colour(0xFF0F3460).darker(0.35f));
            g.fillRect(meterArea);

            const auto innerMeterArea = meterArea.reduced(1, 1);

            const float clamped = juce::jlimit(0.0f, 1.0f, level);
            auto fillWidth = (int) std::round((float) innerMeterArea.getWidth() * clamped);

            if (fillWidth > 0)
            {
                auto fill = innerMeterArea.withWidth(fillWidth);

                juce::ColourGradient meterGradient(
                    juce::Colour(0xFF42D96B), (float) innerMeterArea.getX(),     (float) innerMeterArea.getCentreY(),
                    juce::Colour(0xFFFF4545), (float) innerMeterArea.getRight(), (float) innerMeterArea.getCentreY(),
                    false);

                meterGradient.clearColours();
                meterGradient.addColour(0.00, juce::Colour(0xFF42D96B)); // green
                meterGradient.addColour(0.55, juce::Colour(0xFF42D96B)); // stay green for most of meter
                meterGradient.addColour(0.72, juce::Colour(0xFFE6E64A)); // yellow-green transition
                meterGradient.addColour(0.84, juce::Colour(0xFFFFB347)); // amber
                meterGradient.addColour(0.92, juce::Colour(0xFFFF7A3A)); // orange-red transition
                meterGradient.addColour(0.97, juce::Colour(0xFFFF4A3A)); // red
                meterGradient.addColour(1.00, juce::Colour(0xFFFF3B3B)); // full red

                g.setGradientFill(meterGradient);
                g.fillRect(fill);
            }

            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawRect(meterArea, 1);
        }

        AudioEngine& engine;
    };
}

class MainComponent final : public juce::Component,
                            public juce::MenuBarModel,
                            public juce::FileDragAndDropTarget,
                            private juce::ChangeListener,
                            public juce::ToolbarItemFactory
{
public:
    static constexpr int kMaxSynthTabs = 2;
    static constexpr int kMaxFxTabs    = 8;

    MainComponent();
    ~MainComponent() override;

    void performInitialSessionLoad(bool shouldLoadLastPreset);
    void loadPluginFromFile(const juce::File& file);
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void resized() override;
    void paint(juce::Graphics& g) override;
    bool requestQuit();

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int index, const juce::String& name) override;
    void menuItemSelected(int itemId, int menuIndex) override;

    AppSettings& getSettings() { return settings; }

private:
    class PresetComboBox final : public juce::ComboBox
    {
    public:
        std::function<void()> onBeforePopup;

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (onBeforePopup)
                onBeforePopup();

            juce::ComboBox::mouseDown(event);
        }
    };

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

    class StyledToolbarButton final : public juce::ToolbarButton
    {
    public:
        enum class ContentType
        {
            IconGlyph,
            TextLabel
        };

        bool isVisuallyActive() const
        {
            return isActiveProvider ? isActiveProvider() : false;
        }

        StyledToolbarButton(int itemId,
                            const juce::String& tooltipText,
                            const juce::String& contentText,
                            ContentType contentTypeIn,
                            int preferredWidthIn,
                            std::function<bool()> isActiveProviderIn = {},
                            juce::Colour baseColourIn = juce::Colour(0xFF3C3C3C))
            : juce::ToolbarButton(itemId, "", nullptr, nullptr),
              tooltip(tooltipText),
              content(contentText),
              contentType(contentTypeIn),
              preferredWidth(preferredWidthIn),
              isActiveProvider(std::move(isActiveProviderIn)),
              baseColour(baseColourIn)
        {
            setTooltip(tooltip);
            setWantsKeyboardFocus(false);
        }

        bool getToolbarItemSizes(int toolbarDepth, bool isVertical,
                                 int& preferredSizeOut, int& minSize, int& maxSize) override
        {
            juce::ignoreUnused(toolbarDepth, isVertical);
            preferredSizeOut = preferredWidth;
            minSize = preferredWidth;
            maxSize = preferredWidth;
            return true;
        }

        void paintButtonArea(juce::Graphics& g,
                             int width, int height,
                             bool isMouseOver, bool isMouseDown) override
        {
            auto area = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(2.0f, 4.0f);
            // Toolbar buttons common background colour
            auto base = baseColour;

            if (isVisuallyActive())
                base = juce::Colour(0xFF3A7BD5);
            else if (isMouseDown)
                base = base.darker(0.15f);
            else if (isMouseOver)
                base = base.brighter(0.12f);

            g.setColour(base);
            g.fillRoundedRectangle(area, 4.0f);

            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.drawRoundedRectangle(area, 4.0f, 1.0f);
        }

        void contentAreaChanged(const juce::Rectangle<int>& newArea) override
        {
            contentArea = newArea;
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();
            auto isOver = isMouseOver(true);
            auto isDown = isMouseButtonDown();

            paintButtonArea(g, bounds.getWidth(), bounds.getHeight(), isOver, isDown);

            auto drawArea = contentArea.isEmpty() ? bounds : contentArea;

            if (getItemId() == MainComponent::toolbarMidiPanic)
                drawArea = drawArea.translated(0, 1);
            g.setColour(isVisuallyActive() ? juce::Colours::white
                                           : juce::Colours::lightgrey);

            if (contentType == ContentType::IconGlyph)
            {
               #if JUCE_WINDOWS
                juce::Font font(juce::FontOptions("Segoe Fluent Icons", 18.0f, juce::Font::plain));
               #else
                juce::Font font(juce::FontOptions(18.0f));
               #endif
                g.setFont(font);
            }
            else
            {
                g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
            }

            g.drawFittedText(content,
                             drawArea.reduced(4, 1),
                             juce::Justification::centred,
                             1);
        }

    private:
        juce::String tooltip;
        juce::String content;
        ContentType contentType;
        int preferredWidth = 32;
        juce::Rectangle<int> contentArea;
        std::function<bool()> isActiveProvider;
        juce::Colour baseColour;
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

    class AddTabButton final : public juce::Button
    {
    public:
        AddTabButton() : juce::Button("Add Tab") {}

        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
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
        bool hasSavedWindowBounds = false;
        int savedWindowWidth = 0;
        int savedWindowHeight = 0;
        juce::File pluginFile;
        juce::MemoryBlock pluginState;
        juce::PluginDescription pluginDescription;
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
    bool handleDroppedPluginFile(const juce::File& file, int targetTabIndex);
    void browseAndLoadPluginInCurrentTab();
    void browseAndLoadPluginInNewTab();
    int promptForDroppedPluginAction(const juce::File& droppedFile, int targetTabIndex) const;
    bool loadDroppedPluginInNewTab(const juce::File& file);
    void configurePluginTabComponent(PluginTabComponent& tabComponent);
    int getLoadedPluginTabCount() const;
    int countTabsOfType(PluginTabComponent::SlotType type) const;
    bool shouldSuppressDirtyForSinglePluginQuickOpen() const;
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
    void autoAssignGlobalMidiToTabIfAppropriate(int tabIndex);
    void assignEnabledGlobalMidiDevicesToTab(int tabIndex);
    void sendMidiPanic();
    void moveTabEarlier(int tabIndex);
    void moveTabLater(int tabIndex);
    void toggleTabBypass(int tabIndex);
    void updateStatusBarText();
    void showAboutDialog();
    enum ToolbarItemIds
    {
        toolbarRoutingToggle          = 10001,
        toolbarRefitWindow            = 10002,
        toolbarSavePreset             = 10003,
        toolbarSavePresetAs           = 10004,
        toolbarSpacer                 = 10005,
        toolbarRevertPreset           = 10006,
        toolbarMidiPanic              = 10007,
        toolbarSaveTabWindowSize      = 10008,
        toolbarClearTabWindowSize     = 10009
    };
    enum MenuItemIds
    {
        menuRecentPresetBase = 2000,
        menuOpenPresetsFolder = 2101,
    };

    void getAllToolbarItemIds(juce::Array<int>& ids) override;
    void getDefaultItemSet(juce::Array<int>& ids) override;
    juce::ToolbarItemComponent* createItem(int itemId) override;
    static juce::String getToolbarIconGlyph(int itemId);

    // MIDI
    MidiTabRoutingState* getMidiRoutingStateForTab(int tabIndex);
    const MidiTabRoutingState* getMidiRoutingStateForTab(int tabIndex) const;
    void ensureMidiRoutingStateForCurrentTabs();
    void refreshMidiRoutingView();
    void toggleMidiRoutingExpanded(int tabIndex);
    void toggleMidiAssignment(int tabIndex, const juce::String& deviceIdentifier);
    void assignAllEnabledMidiDevicesToTab(int tabIndex);
    void clearMidiAssignmentsForTab(int tabIndex);
    juce::Array<MidiTabRoutingState> midiRoutingStates;
    void showMidiAssignmentsCallout(int tabIndex, juce::Component* anchorComponent);
    void refreshMidiDevices();

    // Preset/session helpers
    void newPreset();
    void savePreset();
    void savePresetAs();
    void loadPreset();
    void deletePreset();
    bool confirmRevertCurrentPreset() const;
    void revertCurrentPreset();
    void clearAllPlugins();
    bool writePresetToFile(const juce::File& file);
    void updateWindowTitle();
    void rebuildTabsFromPresetXml(const juce::XmlElement& presetXml);
    bool maybeSaveChanges();
    void markSessionDirty();
    void markSessionClean();

    void showPresetLoadErrors(const juce::StringArray& errors);
    bool loadPresetFromFile(const juce::File& file);
    void refreshPresetLists();
    void refreshPresetDropdown();
    void updatePresetDropdownDisplayText();
    void loadPresetFromDropdown();
    void resizeWindowToFitCurrentTab();
    void saveCurrentWindowSizeForCurrentTab();
    void clearSavedWindowSizeForCurrentTab();
    void handleCurrentTabChanged();

    SessionData buildSessionData() const;
    void applySessionData(const SessionData& session,
                          juce::StringArray& loadErrors,
                          juce::Array<MissingPluginEntry>& missingPlugins);

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
    StandaloneTempoSupport standaloneTempoSupport { audioEngine };

    juce::TooltipWindow tooltipWindow { this };
    std::unique_ptr<juce::Component> tempoStrip;

    // Plugin scan folder actions
    void addPluginScanFolder();
    void showPluginScanFolders();
    void clearPluginScanFolders();

    SessionDocument sessionDocument;
    AppSettings settings;
    PresetSessionController presetSessionController { settings, sessionDocument };
    PresetFileDialogHelper presetFileDialogHelper { settings, presetSessionController };
    RecentPresetMenuHelper recentPresetMenuHelper { presetSessionController };
    FileMenuHelper fileMenuHelper;
    UnsavedChangesHelper unsavedChangesHelper;
    PresetNamePromptHelper presetNamePromptHelper;
    AudioEngine audioEngine;
    MidiEngine midiEngine { audioEngine.getDeviceManager() };
    bool saveCurrentSessionOrSaveAs();
    void handleSuccessfulPresetSave(const juce::File& file);

    juce::MenuBarComponent menuBar { this };
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    AddTabButton addTabButton;
    TabBarLookAndFeel tabBarLookAndFeel;
    TabBarMouseListener tabBarMouseListener { *this };
    juce::Label statusBar;
    std::unique_ptr<StatusMetersComponent> statusMeters;
    juce::Toolbar toolbar;
    RoutingView routingView;
    bool showingRoutingView = false;
    std::unique_ptr<MidiMonitorWindow> midiMonitorWindow;
    juce::File lastPluginRepairDirectory;
    bool isLoadingPreset = false;
    double ignoreDirtyChangesUntilMs = 0.0;
    bool isSessionDirty = false;
    bool suppressDirtyForSinglePluginQuickOpen = false;
    juce::Array<MissingPluginEntry> unresolvedMissingPlugins;
    PresetComboBox presetDropdown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};