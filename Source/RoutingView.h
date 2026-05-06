#pragma once
#include <JuceHeader.h>
#include "PluginTabComponent.h"
#include "ButtonStyling.h"

class RoutingView final : public juce::Component
{
public:
    struct ModuleEntry
    {
        int tabIndex = -1;
        juce::String name;
        PluginTabComponent::SlotType type = PluginTabComponent::SlotType::Empty;
        bool isBypassed = false;
        bool canMoveUp = false;
        bool canMoveDown = false;
        int midiAssignmentCount = 0;
        juce::String routingTooltip;

        bool pointerUseTabSettings = false;
        int pointerJumpFilter = 12;
        int pointerMaxStepSize = 4;
        int pointerStepMultiplier = 1;
    };

    RoutingView();

    void setModules(const juce::Array<ModuleEntry>& newModules);

    std::function<void(int tabIndex)> onMoveUp;
    std::function<void(int tabIndex)> onMoveDown;
    std::function<void(int tabIndex)> onToggleBypass;
    std::function<void(int tabIndex)> onSelectTab;
    std::function<void(int tabIndex)> onCloseTab;
    std::function<void(int tabIndex, juce::Component* anchorComponent)> onShowMidiAssignments;
    std::function<void()> onRefreshMidiDevices;

    std::function<void(int tabIndex, bool shouldUse)> onPointerUseTabSettingsChanged;
    std::function<void(int tabIndex, int value)> onPointerJumpFilterChanged;
    std::function<void(int tabIndex, int value)> onPointerMaxStepSizeChanged;
    std::function<void(int tabIndex, int value)> onPointerStepMultiplierChanged;
    std::function<void(int tabIndex)> onPointerResetRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class NumberBox final : public juce::TextEditor
    {
    public:
        NumberBox() = default;

        void setRange(int minValueIn, int maxValueIn)
        {
            minValue = minValueIn;
            maxValue = maxValueIn;
        }

        void setValue(int newValue)
        {
            value = juce::jlimit(minValue, maxValue, newValue);
            setText(juce::String(value), juce::dontSendNotification);
        }

        int getValue() const
        {
            return value;
        }

        std::function<void(int)> onValueChanged;

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::upKey)
            {
                applyValue(value + 1);
                return true;
            }

            if (key == juce::KeyPress::downKey)
            {
                applyValue(value - 1);
                return true;
            }

            return juce::TextEditor::keyPressed(key);
        }

        void mouseWheelMove(const juce::MouseEvent& event,
                            const juce::MouseWheelDetails& wheel) override
        {
            juce::ignoreUnused(event);

            if (wheel.deltaY > 0.0f)
                applyValue(value + 1);
            else if (wheel.deltaY < 0.0f)
                applyValue(value - 1);
        }

        void focusLost(FocusChangeType cause) override
        {
            juce::TextEditor::focusLost(cause);
            applyTextValue();
        }

    private:
        void applyTextValue()
        {
            auto text = getText().trim();

            if (text.isEmpty())
            {
                setText(juce::String(value), juce::dontSendNotification);
                return;
            }

            applyValue(text.getIntValue());
        }

        void applyValue(int newValue)
        {
            const int clamped = juce::jlimit(minValue, maxValue, newValue);

            value = clamped;
            setText(juce::String(value), juce::dontSendNotification);

            if (onValueChanged)
                onValueChanged(value);
        }

        int minValue = 0;
        int maxValue = 127;
        int value = 0;
    };

    class ModuleRow final : public juce::Component
    {
    public:
        ModuleRow();
        ~ModuleRow() override;

        void setModule(const ModuleEntry& newEntry);

        std::function<void(int tabIndex)> onMoveUp;
        std::function<void(int tabIndex)> onMoveDown;
        std::function<void(int tabIndex)> onToggleBypass;
        std::function<void(int tabIndex)> onSelectTab;
        std::function<void(int tabIndex)> onCloseTab;
        std::function<void(int tabIndex, juce::Component* anchorComponent)> onShowMidiAssignments;

        std::function<void(int tabIndex, bool shouldUse)> onPointerUseTabSettingsChanged;
        std::function<void(int tabIndex, int value)> onPointerJumpFilterChanged;
        std::function<void(int tabIndex, int value)> onPointerMaxStepSizeChanged;
        std::function<void(int tabIndex, int value)> onPointerStepMultiplierChanged;
        std::function<void(int tabIndex)> onPointerResetRequested;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        ModuleEntry entry;

        ButtonStyling::RoundedTextButtonLookAndFeel roundedButtonLookAndFeel { ButtonStyling::defaultCornerRadius() };

        juce::Label nameLabel;
        ButtonStyling::TypeBadgeButton typeButton;
        ButtonStyling::SmallIconButton closeButton { ButtonStyling::Glyphs::close() };
        juce::TextButton midiButton { ButtonStyling::Labels::midi() };
        ButtonStyling::StatusIconButton bypassButton
        {
            ButtonStyling::Glyphs::activeTick(),
            ButtonStyling::Glyphs::bypassCross(),
            ButtonStyling::bypassActiveBackground(),
            ButtonStyling::bypassInactiveBackground()
        };
        ButtonStyling::SmallIconButton upButton { ButtonStyling::Glyphs::arrowUp() };
        ButtonStyling::SmallIconButton downButton { ButtonStyling::Glyphs::arrowDown() };
        ButtonStyling::SmallIconButton infoButton { ButtonStyling::Glyphs::info() };

        ButtonStyling::StatusIconButton pointerOverrideButton {
            ButtonStyling::Glyphs::pointerTabSettings(),
            ButtonStyling::Glyphs::pointerTabSettings(),
            ButtonStyling::bypassActiveBackground(),
            ButtonStyling::defaultBackground(),
            ButtonStyling::defaultCornerRadius(),
            ButtonStyling::defaultIconSize(),
            1
        };
        bool pointerOverrideActive = false;
        juce::Label jumpFilterLabel;
        juce::Label maxStepLabel;
        juce::Label multiplierLabel;
        NumberBox jumpFilterBox;
        NumberBox maxStepBox;
        NumberBox multiplierBox;
        ButtonStyling::SmallIconButton resetPointerButton { ButtonStyling::Glyphs::reset() };
    };

    void rebuildModuleRows();

    juce::Label titleLabel;
    juce::Label midiHelpLabel;
    juce::TextButton refreshMidiButton { "Refresh MIDI" };
    juce::Label emptyLabel;
    juce::Viewport viewport;
    juce::Component contentComponent;
    juce::OwnedArray<ModuleRow> moduleRows;
    juce::Array<ModuleEntry> modules;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingView)
};