#pragma once
#include <JuceHeader.h>
#include "SessionManager.h"
#include "ButtonStyling.h"

class RoutingView final : public juce::Component,
                          private juce::Timer
{
public:
    struct ModuleEntry
    {
        int tabIndex = -1;
        juce::String name;
        PluginSlotType type = PluginSlotType::Empty;
        bool isBypassed = false;
        bool isSoloed = false;
        bool isMutedBySolo = false;
        float outputGainDb = 0.0f;
        int midiAssignmentCount = 0;
        juce::String midiAssignmentsTooltip;
        juce::String routingTooltip;
        int pointerAdjustMethodOverride = 0;
        bool needsAttention = false;
        bool isMissingPlugin = false;
        juce::String attentionMessage;
    };

    RoutingView();

    void setModules(const juce::Array<ModuleEntry>& newModules);

    std::function<void(int fromTabIndex, int toTabIndex)> onMove;
    std::function<void(int tabIndex)> onToggleBypass;
    std::function<void(int tabIndex)> onToggleSolo;
    std::function<void(int tabIndex, float gainDb)> onSetOutputGainDb;
    std::function<void(int tabIndex)> onSelectTab;
    std::function<void(int tabIndex)> onCloseTab;
    std::function<void(int tabIndex, juce::Component* anchorComponent)> onShowMidiAssignments;
    std::function<void(int tabIndex, juce::Component* anchorComponent)> onShowPluginInfo;
    std::function<void()> onRefreshMidiDevices;
    std::function<void(int tabIndex, int methodOverride)> onSetPointerAdjustMethodOverride;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

private:
    class ModuleRow final : public juce::Component
    {
    public:
        ModuleRow();
        ~ModuleRow() override;

        void setModule(const ModuleEntry& newEntry);

        std::function<void(int tabIndex, juce::Point<int> screenPosition)> onDragStarted;
        std::function<void(int tabIndex, juce::Point<int> screenPosition)> onDragMoved;
        std::function<void(int tabIndex, juce::Point<int> screenPosition)> onDragEnded;
        std::function<void(int tabIndex)> onToggleBypass;
        std::function<void(int tabIndex)> onToggleSolo;
        std::function<void(int tabIndex, float gainDb)> onSetOutputGainDb;
        std::function<void(int tabIndex)> onSelectTab;
        std::function<void(int tabIndex)> onCloseTab;
        std::function<void(int tabIndex, juce::Component* anchorComponent)> onShowMidiAssignments;
        std::function<void(int tabIndex, juce::Component* anchorComponent)> onShowPluginInfo;
        std::function<void(int tabIndex, int methodOverride)> onSetPointerAdjustMethodOverride;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        class DragHandle final : public juce::Component,
                                 public juce::SettableTooltipClient
        {
        public:
            DragHandle();

            std::function<void(juce::Point<int> screenPosition)> onDragStarted;
            std::function<void(juce::Point<int> screenPosition)> onDragMoved;
            std::function<void(juce::Point<int> screenPosition)> onDragEnded;

            void paint(juce::Graphics& g) override;
            void mouseDown(const juce::MouseEvent& event) override;
            void mouseDrag(const juce::MouseEvent& event) override;
            void mouseUp(const juce::MouseEvent& event) override;
            void mouseEnter(const juce::MouseEvent&) override { repaint(); }
            void mouseExit(const juce::MouseEvent&) override { repaint(); }

        private:
            bool dragStarted = false;
        };

        class VolumeKnobLookAndFeel final : public juce::LookAndFeel_V4
        {
        public:
            void drawRotarySlider(juce::Graphics& g,
                                  int x,
                                  int y,
                                  int width,
                                  int height,
                                  float sliderPosition,
                                  float rotaryStartAngle,
                                  float rotaryEndAngle,
                                  juce::Slider& slider) override;
        };

        void updateVolumeValueLabel();
        void updateAdjustMethodValueLabel();
        static double adjustMethodToKnobValue(int methodOverride);
        static int knobValueToAdjustMethod(double knobValue);

        ModuleEntry entry;
        ButtonStyling::RoundedTextButtonLookAndFeel roundedButtonLookAndFeel { ButtonStyling::defaultCornerRadius() };
        VolumeKnobLookAndFeel volumeKnobLookAndFeel;
        DragHandle dragHandle;
        juce::Label nameLabel;
        ButtonStyling::TypeBadgeButton typeButton;
        juce::Label volumeLabel;
        juce::Slider volumeSlider;
        juce::Label volumeValueLabel;
        juce::Label adjustLabel;
        juce::Slider adjustMethodSlider;
        juce::Label adjustMethodValueLabel;
        ButtonStyling::SmallIconButton closeButton { ButtonStyling::Glyphs::close() };
        juce::TextButton midiButton { "MIDI Ch" };
        ButtonStyling::StatusIconButton bypassButton
        {
            ButtonStyling::Glyphs::activeTick(),
            ButtonStyling::Glyphs::bypassCross(),
            ButtonStyling::bypassActiveBackground(),
            ButtonStyling::bypassInactiveBackground()
        };
        ButtonStyling::StatusIconButton soloButton
        {
            ButtonStyling::Glyphs::solo(),
            ButtonStyling::Glyphs::solo(),
            juce::Colour(0xFFB8860B),
            ButtonStyling::defaultBackground()
        };
        ButtonStyling::SmallIconButton infoButton { ButtonStyling::Glyphs::info() };
    };

    void rebuildModuleRows();
    void beginModuleDrag(int tabIndex, juce::Point<int> screenPosition);
    void updateModuleDrag(int tabIndex, juce::Point<int> screenPosition);
    void endModuleDrag(int tabIndex, juce::Point<int> screenPosition);
    void autoScrollForDrag(juce::Point<int> screenPosition);
    int getDropInsertionIndex(juce::Point<int> screenPosition) const;
    int getDropIndicatorContentY() const;
    void timerCallback() override;

    juce::Label titleLabel;
    juce::Label midiHelpLabel;
    juce::TextButton refreshMidiButton { "Refresh MIDI" };
    juce::Label emptyLabel;
    juce::Viewport viewport;
    juce::Component contentComponent;
    juce::OwnedArray<ModuleRow> moduleRows;
    juce::Array<ModuleEntry> modules;
    juce::Array<ModuleEntry> deferredModules;
    int draggedTabIndex = -1;
    int dropInsertionIndex = -1;
    juce::Point<int> lastDragScreenPosition;
    bool hasDeferredModules = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingView)
};
