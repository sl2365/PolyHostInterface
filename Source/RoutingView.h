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
        bool canMoveUp = false;
        bool canMoveDown = false;
    };

    RoutingView();

    void setModules(const juce::Array<ModuleEntry>& newModules);

    std::function<void(int tabIndex)> onMoveUp;
    std::function<void(int tabIndex)> onMoveDown;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ModuleRow final : public juce::Component
    {
    public:
        ModuleRow();

        void setModule(const ModuleEntry& newEntry);

        std::function<void(int tabIndex)> onMoveUp;
        std::function<void(int tabIndex)> onMoveDown;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        ModuleEntry entry;
        juce::Label nameLabel;
        juce::Label typeLabel;
        juce::TextButton bypassButton { "Bypass" };
        juce::TextButton upButton { "Up" };
        juce::TextButton downButton { "Down" };
    };

    void rebuildModuleRows();

    juce::Label titleLabel;
    juce::Label emptyLabel;
    juce::Viewport viewport;
    juce::Component contentComponent;
    juce::OwnedArray<ModuleRow> moduleRows;
    juce::Array<ModuleEntry> modules;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingView)
};
