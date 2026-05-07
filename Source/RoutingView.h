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

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
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