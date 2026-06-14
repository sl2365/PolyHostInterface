#pragma once

#include <JuceHeader.h>
#include "ButtonStyling.h"
#include "SessionManager.h"
#include "AppSettings.h"
#include "RoutingView.h"
#include "PointerControl.h"

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
    void refreshHostedEditorForWindowReopen();
    void clearHostedEditorForWindowClose();
    void recoverCurrentTabAfterWindowReopen();
    void showTemporaryStatusMessage(const juce::String& text);

private:
    class PointerEditOverlayWindow;

    class TabButton final : public juce::Button
    {
    public:
        TabButton(const juce::String& textIn,
                  PluginSlotType typeIn,
                  bool selectedIn)
            : juce::Button(textIn),
              tabText(textIn),
              tabType(typeIn),
              selected(selectedIn)
        {
        }

        void setTabState(PluginSlotType newType, bool isSelected)
        {
            tabType = newType;
            selected = isSelected;
            repaint();
        }

        void paintButton(juce::Graphics& g,
                         bool isMouseOverButton,
                         bool isButtonDown) override
        {
            juce::ignoreUnused(isButtonDown);

            auto bounds = getLocalBounds().toFloat();

            auto baseColour = colourForType(tabType);
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

            if (isButtonDown)
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

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
            g.drawFittedText("+",
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
            savePreset = 10004,
            savePresetAs = 10005,
            revertPreset = 10006,
            midiPanic = 10007
        };

        void getAllToolbarItemIds(juce::Array<int>& ids) override
        {
            ids.add(routingToggle);
            ids.add(spacer);
            ids.add(pointerControlEdit);
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
                    "Pointer Control Edit Mode",
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

            if (itemId == savePreset)
            {
                auto* button = new ButtonStyling::ToolbarIconButton(
                    itemId,
                    ButtonStyling::Tooltips::savePreset(),
                    ButtonStyling::Glyphs::save(),
                    ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
                    ButtonStyling::defaultButtonWidth());

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
        commandClearDebugLogNow = 2503
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
    void selectTab(int tabIndex);
    void clearTab(int tabIndex);
    void closeTab(int tabIndex);
    void showTabContextMenuAsync(int tabIndex, juce::Component* anchor);
    void handleTabContextCommand(int commandId, int tabIndex);
    void showMidiAssignmentsPopup(int tabIndex, juce::Component* anchorComponent);
    void dismissMidiAssignmentsPopup();
    void toggleRoutingView();

    void processPendingPointerMidi();
    void refreshPointerControlTarget();
    void showPointerControlSettingsDialog();
    void showAboutDialog();

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

    PolyHostPluginProcessor& processor;

    AppSettings appSettings;
    PresetSessionController presetController;
    PresetFileDialogHelper presetFileHelper;
    RecentPresetMenuHelper recentPresetMenuHelper;
    UnsavedChangesHelper unsavedChangesHelper;
    ToolbarFactory toolbarFactory;

    std::unique_ptr<juce::MenuBarComponent> menuBar;
    juce::Toolbar toolbar;

    juce::ComboBox presetDropdown;
    juce::OwnedArray<TabButton> tabButtons;
    AddTabButton addTabButton;
    juce::TextButton emptyLoadPluginButton;

    juce::Component editorHolder;
    std::unique_ptr<juce::AudioProcessorEditor> hostedEditor;
    juce::Label contentPlaceholder;
    RoutingView routingView;
    juce::Component::SafePointer<juce::CallOutBox> midiAssignmentsCallout;
    juce::Component::SafePointer<juce::Component> midiAssignmentsAnchor;
    int midiAssignmentsTabIndex = -1;
    bool showingRoutingView = false;

    PointerControl pointerControl;
    juce::Rectangle<int> lastPointerTargetBounds;
    int lastPointerTargetTabIndex = -1;
    int lastPointerJumpPointCount = -1;
    int lastPointerXccValue = -1;
    int lastPointerYccValue = -1;
    int lastPointerAdjustCcValue = -1;
    int lastPointerToleranceCcValue = -1;
    juce::String temporaryStatusMessage;
    juce::uint32 temporaryStatusExpiryMs = 0;

    bool pointerControlEditModeEnabled = false;
    std::unique_ptr<PointerEditOverlayWindow> pointerEditOverlayWindow;

    bool handleDroppedPluginFile(const juce::File& file, int targetTabIndex);
    bool loadDroppedPluginInNewTab(const juce::File& file);
    int promptForDroppedPluginAction(const juce::File& droppedFile, int targetTabIndex) const;

    bool lastKnownDirtyState = false;
    bool lastKnownShowingState = true;
};