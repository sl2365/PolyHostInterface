#pragma once
#include <JuceHeader.h>
#include "SessionManager.h"
#include "ButtonStyling.h"

class MacroMappingsView final : public juce::Component
{
public:
    struct MappingEntry
    {
        int macroIndex = -1;
        juce::String label;
        int tabIndex = -1;
        juce::String pluginName;
        int parameterIndex = -1;
        juce::String parameterName;
        bool enabled = true;
        bool canMoveUp = false;
        bool canMoveDown = false;
    };

    MacroMappingsView();

    void setMappings(const juce::Array<MappingEntry>& newMappings);
    void setFilterText(const juce::String& newFilterText);
    void setUndoAvailable(bool shouldBeAvailable);

    std::function<void(int macroIndex)> onDeleteMapping;
    std::function<void(int macroIndex)> onMoveMappingUp;
    std::function<void(int macroIndex)> onMoveMappingDown;
    std::function<void(int macroIndex)> onReplaceMapping;
    std::function<void()> onUndoLastEdit;
    std::function<void()> onClearAllMappings;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class MappingRow final : public juce::Component
    {
    public:
        MappingRow();

        void setMapping(const MappingEntry& newEntry);

        std::function<void(int macroIndex)> onDeleteMapping;
        std::function<void(int macroIndex)> onMoveUp;
        std::function<void(int macroIndex)> onMoveDown;
        std::function<void(int macroIndex)> onReplace;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        MappingEntry entry;

        juce::Label macroLabel;
        juce::Label tabLabel;
        juce::Label pluginLabel;
        juce::Label parameterLabel;
        ButtonStyling::SmallIconButton upButton { ButtonStyling::Glyphs::arrowUp() };
        ButtonStyling::SmallIconButton downButton { ButtonStyling::Glyphs::arrowDown() };
        ButtonStyling::SmallIconButton replaceButton { ButtonStyling::Glyphs::replace() };
        ButtonStyling::SmallIconButton deleteButton { ButtonStyling::Glyphs::close(),
                                                      ButtonStyling::destructiveBackground() };
    };

    void rebuildRows();
    bool matchesFilter(const MappingEntry& entry) const;

    juce::Label titleLabel;
    juce::Label helpLabel;
    juce::Label filterLabel;
    juce::TextEditor filterEditor;

    juce::Label macroHeaderLabel;
    juce::Label tabHeaderLabel;
    juce::Label pluginHeaderLabel;
    juce::Label parameterHeaderLabel;

    juce::TextButton undoButton { "Undo" };
    juce::TextButton clearAllButton { "Clear All" };
    juce::Label emptyLabel;

    juce::Viewport viewport;
    juce::Component contentComponent;
    juce::OwnedArray<MappingRow> mappingRows;

    juce::Array<MappingEntry> allMappings;
    juce::Array<MappingEntry> filteredMappings;
    juce::String filterText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroMappingsView)
};