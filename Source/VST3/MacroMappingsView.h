#pragma once

#include <JuceHeader.h>

#include "ButtonStyling.h"

class MacroMappingsView final : public juce::Component,
                                private juce::TableListBoxModel
{
public:
    struct ParameterEntry
    {
        int tabIndex = -1;
        juce::String tabName;
        juce::String pluginName;
        int parameterIndex = -1;
        juce::String parameterName;
        int macroIndex = -1;
        bool mappingEnabled = false;
        bool targetA = false;
        bool targetB = false;
    };

    MacroMappingsView();

    void setParameters(const juce::Array<ParameterEntry>& newParameters,
                       bool shouldShowSeqwencerTargets,
                       bool isSerialMode);
    void setFilterText(const juce::String& newFilterText);
    void setUndoAvailable(bool shouldBeAvailable);

    std::function<bool(const ParameterEntry& entry,
                       bool enabled,
                       juce::String& errorMessage)> onSetMappingEnabled;
    std::function<bool(const ParameterEntry& entry,
                       int lane,
                       bool assigned,
                       juce::String& errorMessage)> onSetSeqwencerTarget;
    std::function<void(int macroIndex)> onDeleteMapping;
    std::function<void(int macroIndex)> onReplaceMapping;
    std::function<void()> onUndoLastEdit;
    std::function<void()> onClearAllMappings;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum ColumnIds
    {
        tabColumn = 1,
        pluginColumn,
        parameterColumn,
        mappedColumn,
        targetsColumn,
        macroColumn
    };

    class MappedCell;
    class TargetsCell;
    class MacroCell;

    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g,
                            int rowNumber,
                            int width,
                            int height,
                            bool rowIsSelected) override;
    void paintCell(juce::Graphics& g,
                   int rowNumber,
                   int columnId,
                   int width,
                   int height,
                   bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(
        int rowNumber,
        int columnId,
        bool rowIsSelected,
        juce::Component* existingComponentToUpdate) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;

    const ParameterEntry* getEntryForRow(int rowNumber) const;
    bool matchesFilter(const ParameterEntry& entry) const;
    int compareEntries(const ParameterEntry& first,
                       const ParameterEntry& second) const;
    void rebuildFilter();
    void changeMappingEnabled(const ParameterEntry& entry, bool enabled);
    void changeTarget(const ParameterEntry& entry, int lane, bool assigned);
    void confirmDelete(const ParameterEntry& entry);
    void showError(const juce::String& message);

    juce::Label titleLabel;
    juce::Label helpLabel;
    juce::TextEditor filterEditor;
    juce::TextButton assignedOnlyButton { "Assigned Only" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton clearAllButton { "Clear All" };
    juce::TableListBox parameterTable;
    juce::Label countLabel;

    juce::Array<ParameterEntry> allParameters;
    juce::Array<int> filteredParameterIndices;
    juce::String filterText;
    bool assignedOnly = false;
    bool showSeqwencerTargets = false;
    bool serialMode = false;
    int sortColumn = tabColumn;
    bool sortForwards = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroMappingsView)
};
