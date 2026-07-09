#pragma once

#include <JuceHeader.h>
#include "ButtonStyling.h"
#include "SessionManager.h"
#include "AppSettings.h"
#include "RoutingView.h"
#include "MacroMappingsView.h"
#include "PointerControl.h"
#include "MidiMonitorWindow.h"

class PolyHostPluginProcessor;

class MainView : public juce::Component,
                 public juce::MenuBarModel,
                 public juce::FileDragAndDropTarget,
                 private juce::Timer
{
public:
    explicit MainView(PolyHostPluginProcessor& processorIn);
    ~MainView() override;

    AppSettings& getAppSettings() { return appSettings; }
    PolyHostPluginProcessor& getProcessor() { return processor; }
    juce::Point<int> getHostedEditorLocalPointFromScreen(juce::Point<int> screenPoint) const;
    juce::Point<int> getHostedEditorScreenPointFromLocal(juce::Point<int> localPoint) const;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void refreshHostedEditorForWindowReopen();
    void clearHostedEditorForWindowClose();
    void recoverCurrentTabAfterWindowReopen();
    void showTemporaryStatusMessage(const juce::String& text);
    bool saveCurrentTabPointerMapToCurrentPresetFile();
    juce::Component* getWindowCenterTarget() const;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

    void setPointerEditGestureActive(bool shouldBeActive);
    bool isPointerEditGestureActive() const;

private:
    class PointerEditOverlayHost;
    class StatusMetersComponent;

    class TabButton final : public juce::Button
    {
    public:
        TabButton(const juce::String& textIn,
                  PluginSlotType typeIn,
                  bool selectedIn,
                  bool needsAttentionIn = false)
            : juce::Button(textIn),
              tabText(textIn),
              tabType(typeIn),
              selected(selectedIn),
              needsAttention(needsAttentionIn)
        {
        }

        void setTabState(PluginSlotType newType, bool isSelected, bool shouldNeedAttention = false)
        {
            tabType = newType;
            selected = isSelected;
            needsAttention = shouldNeedAttention;
            repaint();
        }

        void paintButton(juce::Graphics& g,
                         bool isMouseOverButton,
                         bool isButtonDown) override
        {
            juce::ignoreUnused(isButtonDown);

            auto bounds = getLocalBounds().toFloat();

            auto baseColour = needsAttention ? juce::Colour(0xFFB3261E)
                                             : colourForType(tabType);
            auto fill = selected ? baseColour
                                 : baseColour.darker(0.60f);

            if (isMouseOverButton && ! selected)
                fill = fill.brighter(0.10f);

            juce::Path tabShape;
            const float radius = 5.0f;

            tabShape.startNewSubPath(0.0f, bounds.getBottom());
            tabShape.lineTo(0.0f, radius);
            tabShape.quadraticTo(0.0f, 0.0f, radius, 0.0f);
            tabShape.lineTo(bounds.getWidth() - radius, 0.0f);
            tabShape.quadraticTo(bounds.getWidth(), 0.0f, bounds.getWidth(), radius);
            tabShape.lineTo(bounds.getWidth(), bounds.getBottom());
            tabShape.closeSubPath();

            g.setColour(fill);
            g.fillPath(tabShape);

            g.setColour(fill.darker(0.35f));
            g.strokePath(tabShape, juce::PathStrokeType(1.0f));

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(13.0f,
                                                   selected ? juce::Font::bold
                                                            : juce::Font::plain)));
            g.drawFittedText(tabText,
                             getLocalBounds().reduced(10, 2),
                             juce::Justification::centred,
                             1);
        }

    private:
        static juce::Colour colourForType(PluginSlotType t)
        {
            switch (t)
            {
                case PluginSlotType::Synth: return juce::Colour(0xFF3A7BD5);
                case PluginSlotType::FX:    return juce::Colour(0xFFE67E22);
                case PluginSlotType::Empty: return juce::Colour(0xFF555555);
            }

            return juce::Colour(0xFF555555);
        }

        juce::String tabText;
        PluginSlotType tabType = PluginSlotType::Empty;
        bool selected = false;
        bool needsAttention = false;
    };

    class AddTabButton final : public juce::Button
    {
    public:
        AddTabButton()
            : juce::Button("AddTab")
        {
        }

        void paintButton(juce::Graphics& g,
                         bool isMouseOverButton,
                         bool isButtonDown) override
        {
            auto bounds = getLocalBounds().toFloat();

            auto fill = juce::Colour(0xFF50545C);

            if (! isEnabled())
                fill = fill.darker(0.35f);
            else if (isButtonDown)
                fill = fill.brighter(0.15f);
            else if (isMouseOverButton)
                fill = fill.brighter(0.08f);

            juce::Path tabShape;
            const float radius = 5.0f;

            tabShape.startNewSubPath(0.0f, bounds.getBottom());
            tabShape.lineTo(0.0f, radius);
            tabShape.quadraticTo(0.0f, 0.0f, radius, 0.0f);
            tabShape.lineTo(bounds.getWidth() - radius, 0.0f);
            tabShape.quadraticTo(bounds.getWidth(), 0.0f, bounds.getWidth(), radius);
            tabShape.lineTo(bounds.getWidth(), bounds.getBottom());
            tabShape.closeSubPath();

            g.setColour(fill);
            g.fillPath(tabShape);

            g.setColour(fill.darker(0.35f));
            g.strokePath(tabShape, juce::PathStrokeType(1.0f));

            g.setColour(isEnabled() ? juce::Colours::white
                                    : juce::Colours::lightgrey.withAlpha(0.45f));
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
            g.drawFittedText(getButtonText(),
                             getLocalBounds(),
                             juce::Justification::centred,
                             1);
        }
    };

    class FixedWidthSpacer final : public juce::ToolbarItemComponent
    {
    public:
        FixedWidthSpacer(int itemId, int widthIn)
            : juce::ToolbarItemComponent(itemId, "Spacer", false),
              width(widthIn)
        {
        }

        bool getToolbarItemSizes(int toolbarDepth,
                                 bool isVertical,
                                 int& preferredSize,
                                 int& minSize,
                                 int& maxSize) override
        {
            juce::ignoreUnused(toolbarDepth, isVertical);
            preferredSize = width;
            minSize = width;
            maxSize = width;
            return true;
        }

        void paintButtonArea(juce::Graphics&, int, int, bool, bool) override {}
        void contentAreaChanged(const juce::Rectangle<int>&) override {}

    private:
        int width = 10;
    };

    class ToolbarFactory final : public juce::ToolbarItemFactory
    {
    public:
        explicit ToolbarFactory(MainView& ownerIn)
            : owner(ownerIn)
        {
        }

        enum ItemIds
        {
            routingToggle = 10001,
            spacer = 10002,
            pointerControlEdit = 10003,
            clearSolos = 10004,
            savePreset = 10005,
            savePresetAs = 10006,
            revertPreset = 10007,
            midiPanic = 10008,
            mapLastTouched = 10009,
            showMacroMappings = 10010,
        };

        void getAllToolbarItemIds(juce::Array<int>& ids) override
        {
            ids.add(routingToggle);
            ids.add(spacer);
            ids.add(pointerControlEdit);
            ids.add(spacer);
            ids.add(mapLastTouched);
            ids.add(showMacroMappings);
            ids.add(spacer);
            ids.add(clearSolos);
            ids.add(spacer);
            ids.add(savePreset);
            ids.add(savePresetAs);
            ids.add(revertPreset);
            ids.add(spacer);
            ids.add(midiPanic);
        }

        void getDefaultItemSet(juce::Array<int>& ids) override
        {
            getAllToolbarItemIds(ids);
        }

        juce::ToolbarItemComponent* createItem(int itemId) override
        {
            if (itemId == routingToggle)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::routing(),
                    ButtonStyling::Glyphs::routing(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth());

                button->onClick = [this] { owner.toggleRoutingView(); };
                return button;
            }

            if (itemId == pointerControlEdit)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::pointerControlEditMode(),
                    ButtonStyling::Glyphs::pointerControl(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth(),
                    [this] { return owner.pointerControlEditModeEnabled; });

                button->onClick = [this, button]
                {
                    owner.togglePointerControlEditMode();
                    button->setToggleState(owner.pointerControlEditModeEnabled,
                                           juce::dontSendNotification);
                    button->repaint();
                };

                button->setToggleState(owner.pointerControlEditModeEnabled,
                                       juce::dontSendNotification);
                return button;
            }

            if (itemId == mapLastTouched)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::mapLastTouched(),
                    ButtonStyling::Glyphs::mapLastTouched(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth());

                button->onClick = [this] { owner.mapLastTouchedParameterToMacro(); };
                return button;
            }

            if (itemId == showMacroMappings)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::macroMappings(),
                    ButtonStyling::Glyphs::mappings(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth());

                button->onClick = [this] { owner.showMacroMappingsView(); };
                return button;
            }

            if (itemId == clearSolos)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::clearSolos(),
                    ButtonStyling::Glyphs::clearSolo(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth(),
                    [this] { return ! owner.soloedTabIndices.isEmpty(); });

                button->onClick = [this]
                {
                    owner.clearAllSolos();
                };

                return button;
            }

            if (itemId == savePreset)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::savePreset(),
                    ButtonStyling::Glyphs::save(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth());

                owner.savePresetToolbarButton = button;
                button->onClick = [this] { owner.savePreset(); };
                return button;
            }

            if (itemId == savePresetAs)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::savePresetAs(),
                    ButtonStyling::Glyphs::saveAs(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth());

                owner.savePresetAsToolbarButton = button;
                button->onClick = [this] { owner.savePresetAs(); };
                return button;
            }

            if (itemId == revertPreset)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::revertPreset(),
                    ButtonStyling::Glyphs::revert(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth());

                button->onClick = [this] { owner.reloadCurrentPreset(); };
                return button;
            }

            if (itemId == midiPanic)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::midiPanic(),
                    ButtonStyling::Glyphs::panic(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth(),
                    {},
                    ButtonStyling::destructiveBackground(),
                    1);

                button->onClick = [this] { owner.sendMidiPanic(); };
                return button;
            }

            if (itemId == spacer)
                return new FixedWidthSpacer(itemId, 2);

            return nullptr;
        }

    private:
        MainView& owner;
    };

    class MidiAssignmentsPopup final : public juce::Component,
                                       private juce::Button::Listener
    {
    public:
        MidiAssignmentsPopup(MainView& ownerIn,
                             int tabIndexIn);

        void refresh();
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        void buttonClicked(juce::Button* button) override;

        MainView& owner;
        int tabIndex = -1;

        juce::OwnedArray<juce::ToggleButton> deviceButtons;
        juce::TextButton assignAllButton { "Assign All Enabled" };
        juce::TextButton clearAllButton { "Clear All" };
    };

    enum MenuCommandIds
    {
        commandNewPreset = 2001,
        commandNewTab = 2002,
        commandCloseCurrentTab = 2003,
        commandSavePreset = 2004,
        commandSavePresetAs = 2005,
        commandLoadPreset = 2006,
        commandRefreshMidiDevices = 2007,
        commandMidiPanic = 2008,
        commandMidiMonitor = 2009,
        commandRecentPresetBase = 2100,
        commandDeleteCurrentPreset = 2200,
        commandOpenPresetsFolder = 2201,
        commandLocateMissingPlugins = 2202,
        commandReplacePlugin = 2203,
        commandNewPlugin = 2204,
        commandTabContextNewTab = 2300,
        commandTabContextClearTab = 2301,
        commandTabContextCloseTab = 2302,
        commandTabContextReplacePlugin = 2303,
        commandTabContextReloadPlugin = 2304,
        commandPointerControlSettings = 2400,
        commandEnableDebugLogging = 2500,
        commandEnableAdvancedDebugLogging = 2501,
        commandClearDebugLogOnStartup = 2502,
        commandClearDebugLogNow = 2503,

        commandAutoSaveAfterPluginRepair = 2600,
        commandAddPluginScanFolder = 2601,
        commandShowPluginScanFolders = 2602,
        commandClearPluginScanFolders = 2603,
        commandLocateMissingPluginsNow = 2604,

        commandPresetLoadReport = 2700
    };

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex,
                                    const juce::String& menuName) override;
    void menuItemSelected(int menuItemID,
                          int topLevelMenuIndex) override;
    void timerCallback() override;

    void refreshFromCore();
    void refreshDirtyUiOnly();
    void refreshHostedEditor();
    void clearHostedEditor();
    void loadPluginIntoMainSlot();
    void handlePresetSelection();
    void resizeParentEditorToFitHostedPlugin();
    void rebuildPresetDropdown();
    void rebuildTabButtons();
    void rebuildRoutingView();
    void updateTabScrollButtonState();
    void selectTab(int tabIndex);
    void clearTab(int tabIndex);
    void closeTab(int tabIndex);
    void showTabContextMenuAsync(int tabIndex, juce::Component* anchor);
    void handleTabContextCommand(int commandId, int tabIndex);
    void showMidiAssignmentsPopup(int tabIndex, juce::Component* anchorComponent);
    void showPluginDiagnosticsDialog(int tabIndex);
    void dismissMidiAssignmentsPopup();
    void toggleRoutingView();
    void handleToggleSolo(int tabIndex);
    void clearAllSolos();
    void mapLastTouchedParameterToMacro();
    void showMacroMappingsView();
    void updateSaveButtonColours();
    void syncManualBypassStatesFromCore();
    void applyEffectiveBypassStates();

    void processPendingPointerMidi();
    void refreshPointerControlTarget();
    void showPointerControlSettingsDialog();
    void showAboutDialog();
    void buildAndStorePresetLoadReport(const juce::File& file,
                                       const SessionData& sessionData,
                                       const juce::StringArray& warnings,
                                       bool loadSucceeded,
                                       const juce::String& failureReason);
    void buildAndStoreCurrentSessionPresetLoadReport();
    void showPresetLoadReportDialog(bool manualRequest);

    void setPointerControlEditMode(bool shouldEnable);
    void togglePointerControlEditMode();
    void updatePointerEditOverlay();
    void hidePointerEditOverlay();

    void savePreset();
    void savePresetAs();
    void loadPresetFromFile();
    bool saveSessionToFile(const juce::File& file);
    bool loadSessionFromFile(const juce::File& file);
    void createNewPreset();
    void reloadCurrentPreset();
    void sendMidiPanic();

    bool promptToSaveIfNeeded();
    void loadRecentPreset(int menuItemID);

    void openPresetsFolder();
    void deleteCurrentPreset();
    void replacePlugin();
    void newPlugin();
    void newTab();
    void closeCurrentTab();

    void addPluginScanFolder();
    void showPluginScanFolders();
    void clearPluginScanFolders();
    void locateMissingPluginsNow();
    bool tryRepairMissingPlugin(int tabIndex,
                                const juce::String& pluginName,
                                const juce::String& pluginStateBase64);

    PolyHostPluginProcessor& processor;

    AppSettings appSettings;
    PresetSessionController presetController;
    PresetFileDialogHelper presetFileHelper;
    RecentPresetMenuHelper recentPresetMenuHelper;
    UnsavedChangesHelper unsavedChangesHelper;
    ToolbarFactory toolbarFactory;

    std::unique_ptr<StatusMetersComponent> statusMeters;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    juce::TooltipWindow tooltipWindow { this };
    juce::Toolbar toolbar;
    ButtonStyling::ToolbarIconButton* savePresetToolbarButton = nullptr;
    ButtonStyling::ToolbarIconButton* savePresetAsToolbarButton = nullptr;

    juce::ComboBox presetDropdown;
    juce::OwnedArray<TabButton> tabButtons;
    juce::Component tabButtonsContainer;
    juce::Viewport tabButtonsViewport;
    AddTabButton scrollTabsLeftButton;
    AddTabButton scrollTabsRightButton;
    AddTabButton addTabButton;
    juce::TextButton emptyLoadPluginButton;

    juce::Component editorHolder;
    std::unique_ptr<juce::AudioProcessorEditor> hostedEditor;
    juce::Label contentPlaceholder;
    juce::TextEditor pluginIssueMessageEditor;
    RoutingView routingView;
    MacroMappingsView macroMappingsView;
    juce::Component::SafePointer<juce::CallOutBox> midiAssignmentsCallout;
    juce::Component::SafePointer<juce::Component> midiAssignmentsAnchor;
    int midiAssignmentsTabIndex = -1;
    std::unique_ptr<VstMidiMonitorWindow> midiMonitorWindow;
    bool showingRoutingView = false;
    bool showingMacroMappingsView = false;
    juce::Array<int> soloedTabIndices;
    juce::Array<bool> manualBypassStates;

    PointerControl pointerControl;
    juce::Rectangle<int> lastPointerTargetBounds;
    int lastPointerTargetTabIndex = -1;
    int lastPointerJumpPointCount = -1;
    int lastPointerXccValue = -1;
    int lastPointerYccValue = -1;
    int lastPointerAdjustCcValue = -1;
    int lastPointerToleranceCcValue = -1;
    double lastPointerTabSwitchTimeMs = 0.0;
    juce::String temporaryStatusMessage;
    juce::uint32 temporaryStatusExpiryMs = 0;

    bool pointerControlEditModeEnabled = false;
    bool pointerEditGestureActive = false;
    std::unique_ptr<PointerEditOverlayHost> pointerEditOverlayHost;

    std::unique_ptr<juce::PluginDescription> choosePluginDescriptionForFile(const juce::File& file);
    bool loadPluginFileIntoSelectedTabWithShellChoice(const juce::File& file);
    bool handleDroppedPluginFile(const juce::File& file, int targetTabIndex);
    bool loadDroppedPluginInNewTab(const juce::File& file);
    int promptForDroppedPluginAction(const juce::File& droppedFile, int targetTabIndex) const;

    bool lastKnownDirtyState = false;
    bool lastKnownShowingState = true;
    bool pendingMissingPluginPrompt = false;
    int pendingMissingPluginPromptDelayTicks = 0;
};