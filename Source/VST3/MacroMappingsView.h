#pragma once
#include <JuceHeader.h>
#include "SessionManager.h"
#include "ButtonStyling.h"

class MacroMappingsView final : public juce::Component,
                                private juce::Timer
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
    };

    MacroMappingsView();

    void setMappings(const juce::Array<MappingEntry>& newMappings);
    void setFilterText(const juce::String& newFilterText);
    void setUndoAvailable(bool shouldBeAvailable);

    std::function<void(int macroIndex)> onDeleteMapping;
    std::function<void(int fromMacroIndex, int toMacroIndex)> onMoveMapping;
    std::function<void(int macroIndex)> onReplaceMapping;
    std::function<void()> onUndoLastEdit;
    std::function<void()> onClearAllMappings;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

private:
    class SortHeaderButton final : public juce::TextButton
    {
    public:
        enum class Direction
        {
            none,
            ascending,
            descending
        };

        explicit SortHeaderButton(const juce::String& text);

        void setDirection(Direction newDirection);
        void paintButton(juce::Graphics& g,
                         bool isMouseOverButton,
                         bool isButtonDown) override;

    private:
        Direction direction = Direction::none;
    };

    class MappingRow final : public juce::Component
    {
    public:
        MappingRow();

        void setMapping(const MappingEntry& newEntry);
        void setReorderingEnabled(bool shouldBeEnabled);

        std::function<void(int macroIndex)> onDeleteMapping;
        std::function<void(int macroIndex, juce::Point<int> screenPosition)> onDragStarted;
        std::function<void(int macroIndex, juce::Point<int> screenPosition)> onDragMoved;
        std::function<void(int macroIndex, juce::Point<int> screenPosition)> onDragEnded;
        std::function<void(int macroIndex)> onReplace;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        class DragHandle final : public juce::Component,
                                 public juce::SettableTooltipClient
        {
        public:
            DragHandle();
            void setReorderingEnabled(bool shouldBeEnabled);

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
            bool reorderingEnabled = true;
        };

        MappingEntry entry;

        DragHandle dragHandle;
        juce::Label macroLabel;
        juce::Label tabLabel;
        juce::Label pluginLabel;
        juce::Label parameterLabel;
        ButtonStyling::SmallIconButton replaceButton { ButtonStyling::Glyphs::replace() };
        ButtonStyling::SmallIconButton deleteButton { ButtonStyling::Glyphs::close(),
                                                      ButtonStyling::destructiveBackground() };
    };

    enum class SortColumn
    {
        macro,
        tab,
        plugin,
        parameter
    };

    void rebuildRows();
    bool matchesFilter(const MappingEntry& entry) const;
    void cycleSort(SortColumn column);
    void sortFilteredMappings();
    void updateSortHeaderButtons();
    void beginMappingDrag(int macroIndex, juce::Point<int> screenPosition);
    void updateMappingDrag(int macroIndex, juce::Point<int> screenPosition);
    void endMappingDrag(int macroIndex, juce::Point<int> screenPosition);
    void autoScrollForDrag(juce::Point<int> screenPosition);
    int getDropInsertionIndex(juce::Point<int> screenPosition) const;
    int getDropIndicatorContentY() const;
    int findFilteredMappingIndex(int macroIndex) const;
    void timerCallback() override;

    juce::Label titleLabel;
    juce::Label helpLabel;
    juce::Label filterLabel;
    juce::TextEditor filterEditor;

    juce::Label macroHeaderLabel;
    SortHeaderButton tabHeaderButton { "Tab" };
    SortHeaderButton pluginHeaderButton { "Plugin" };
    SortHeaderButton parameterHeaderButton { "Parameter" };

    juce::TextButton undoButton { "Undo" };
    juce::TextButton clearAllButton { "Clear All" };
    juce::Label emptyLabel;

    juce::Viewport viewport;
    juce::Component contentComponent;
    juce::OwnedArray<MappingRow> mappingRows;

    juce::Array<MappingEntry> allMappings;
    juce::Array<MappingEntry> filteredMappings;
    juce::Array<MappingEntry> deferredMappings;
    juce::String filterText;
    SortColumn sortColumn = SortColumn::macro;
    bool sortAscending = true;
    int draggedMacroIndex = -1;
    int dropInsertionIndex = -1;
    juce::Point<int> lastDragScreenPosition;
    bool hasDeferredMappings = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroMappingsView)
};
