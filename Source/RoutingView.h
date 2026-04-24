#pragma once
#include <JuceHeader.h>
#include "PluginTabComponent.h"

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
    };

    RoutingView();

    void setModules(const juce::Array<ModuleEntry>& newModules);

    std::function<void(int tabIndex)> onMoveUp;
    std::function<void(int tabIndex)> onMoveDown;
    std::function<void(int tabIndex)> onToggleBypass;
    std::function<void(int tabIndex)> onSelectTab;
    std::function<void(int tabIndex, juce::Component* anchorComponent)> onShowMidiAssignments;
    std::function<void()> onRefreshMidiDevices;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ModuleRow final : public juce::Component
    {
    public:
        class TypeBadgeButton final : public juce::TextButton
        {
        public:
            void paintButton(juce::Graphics& g,
                             bool isMouseOverButton,
                             bool isButtonDown) override
            {
                auto area = getLocalBounds().toFloat().reduced(0.5f);

                auto baseColour = findColour(juce::TextButton::buttonColourId);

                if (isButtonDown)
                    baseColour = baseColour.darker(0.15f);
                else if (isMouseOverButton)
                    baseColour = baseColour.brighter(0.15f);

                g.setColour(baseColour);
                g.fillRoundedRectangle(area, 8.0f);

                g.setColour(juce::Colours::white.withAlpha(0.20f));
                g.drawRoundedRectangle(area, 8.0f, 1.0f);

                g.setColour(findColour(juce::TextButton::textColourOffId));
                g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
                g.drawFittedText(getButtonText(),
                                 getLocalBounds().reduced(6, 2),
                                 juce::Justification::centred,
                                 1);
            }
        };
        ModuleRow();

        void setModule(const ModuleEntry& newEntry);

        std::function<void(int tabIndex)> onMoveUp;
        std::function<void(int tabIndex)> onMoveDown;
        std::function<void(int tabIndex)> onToggleBypass;
        std::function<void(int tabIndex)> onSelectTab;
        std::function<void(int tabIndex, juce::Component* anchorComponent)> onShowMidiAssignments;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        ModuleEntry entry;
        juce::Label nameLabel;
        TypeBadgeButton typeButton;
        juce::TextButton midiButton { "MIDI" };
        juce::TextButton bypassButton { "Bypass" };
        juce::TextButton upButton { "Up" };
        juce::TextButton downButton { "Down" };
    };

    void rebuildModuleRows();

    juce::Label titleLabel;
    juce::TextButton refreshMidiButton { "Refresh MIDI" };
    juce::Label emptyLabel;
    juce::Viewport viewport;
    juce::Component contentComponent;
    juce::OwnedArray<ModuleRow> moduleRows;
    juce::Array<ModuleEntry> modules;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingView)
};