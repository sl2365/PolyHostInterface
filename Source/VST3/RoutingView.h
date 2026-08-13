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

        class AdjustMethodEditor final : public juce::TextEditor
        {
        public:
            std::function<void()> onValueChanged;

            AdjustMethodEditor()
            {
                setReadOnly(true);
                setMultiLine(false);
                setColour(juce::TextEditor::backgroundColourId, ButtonStyling::defaultBackground());
                setColour(juce::TextEditor::textColourId, juce::Colours::white);
                setColour(juce::TextEditor::outlineColourId, juce::Colours::lightgrey.withAlpha(0.35f));
                setJustification(juce::Justification::centred);
                applyFontToAllText(juce::Font(juce::FontOptions(12.0f)));
            }

            void setMethodOverride(int value)
            {
                methodOverride = juce::jlimit(0, 2, value);
                setText(toDisplayString(methodOverride), juce::dontSendNotification);
            }

            int getMethodOverride() const
            {
                return methodOverride;
            }

            void mouseWheelMove(const juce::MouseEvent& event,
                                const juce::MouseWheelDetails& wheel) override
            {
                juce::ignoreUnused(event);

                if (wheel.deltaY > 0.0f)
                    setMethodOverride((methodOverride + 1) % 3);
                else if (wheel.deltaY < 0.0f)
                    setMethodOverride((methodOverride + 2) % 3);

                if (onValueChanged)
                    onValueChanged();
            }

        private:
            static juce::String toDisplayString(int value)
            {
                switch (value)
                {
                    case 1: return "Scroll";
                    case 2: return "Drag";
                    case 0:
                    default: return "Global";
                }
            }

            int methodOverride = 0;
        };

        ModuleEntry entry;
        ButtonStyling::RoundedTextButtonLookAndFeel roundedButtonLookAndFeel { ButtonStyling::defaultCornerRadius() };
        DragHandle dragHandle;
        juce::Label nameLabel;
        ButtonStyling::TypeBadgeButton typeButton;
        juce::Label adjustLabel;
        AdjustMethodEditor adjustMethodEditor;
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
