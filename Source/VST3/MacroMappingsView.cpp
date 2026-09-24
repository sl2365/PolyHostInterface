#include "MacroMappingsView.h"

#include <algorithm>
#include <array>

namespace
{
constexpr auto backgroundColour = 0xFF1B263B;
constexpr auto tableBackgroundColour = 0xFF151A23;
constexpr auto selectedRowColour = 0xFF385A72;
constexpr auto accentAColour = 0xFF57D6D0;
constexpr auto accentBColour = 0xFFFFA24C;
constexpr auto macroColour = 0xFF61D9C7;

juce::Colour colourForTab(int tabIndex)
{
    static constexpr std::array<juce::uint32, 25> palette {
        0xFF2E6F9E, 0xFFA65D2E, 0xFF397D54, 0xFF704F9B, 0xFF9A3F50,
        0xFF2F7F7B, 0xFF8A7028, 0xFF465EAF, 0xFF98503D, 0xFF34735F,
        0xFF7E4F82, 0xFF2B718D, 0xFF64743A, 0xFF964966, 0xFF355C8A,
        0xFF90612F, 0xFF3F7650, 0xFF684C84, 0xFF8D493F, 0xFF357883,
        0xFF7E6B32, 0xFF5665A0, 0xFF92544B, 0xFF3D786D, 0xFF824A70
    };

    const auto index = static_cast<std::size_t>(
        juce::jmax(0, tabIndex) % static_cast<int>(palette.size()));
    return juce::Colour(palette[index]);
}
}

class MacroMappingsView::MappedCell final : public juce::Component
{
public:
    explicit MappedCell(MacroMappingsView& ownerIn) : owner(ownerIn)
    {
        enabledButton.setClickingTogglesState(true);
        enabledButton.setTooltip(
            "Assign this parameter to the next free Macro, or pause/resume its existing Macro");
        enabledButton.setColour(juce::ToggleButton::tickColourId,
                                juce::Colour(macroColour));
        enabledButton.setColour(juce::ToggleButton::tickDisabledColourId,
                                juce::Colour(macroColour).withAlpha(0.35f));
        enabledButton.onClick = [this]
        {
            owner.changeMappingEnabled(entry,
                                       enabledButton.getToggleState());
        };
        addAndMakeVisible(enabledButton);
    }

    void setEntry(const ParameterEntry& newEntry)
    {
        entry = newEntry;
        enabledButton.setToggleState(entry.macroIndex >= 0
                                         && entry.mappingEnabled,
                                     juce::dontSendNotification);
        enabledButton.setTooltip(
            entry.macroIndex < 0
                ? "Assign this parameter to the next free PHI Macro"
                : (entry.mappingEnabled
                       ? "Temporarily pause this Macro mapping without deleting it"
                       : "Resume this preserved Macro mapping"));
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::white.withAlpha(
            owner.showSeqwencerTargets ? 0.12f : 0.24f));
        g.drawVerticalLine(getWidth() - 1, 2.0f,
                           (float) getHeight() - 2.0f);
    }

    void resized() override
    {
        enabledButton.setBounds(
            getLocalBounds().withSizeKeepingCentre(26, getHeight()));
    }

private:
    MacroMappingsView& owner;
    ParameterEntry entry;
    juce::ToggleButton enabledButton;
};

class MacroMappingsView::TargetsCell final : public juce::Component
{
public:
    explicit TargetsCell(MacroMappingsView& ownerIn) : owner(ownerIn)
    {
        for (int lane = 0; lane < 8; ++lane)
        {
            auto button = std::make_unique<juce::ToggleButton>();
            configureButton(
                *button,
                juce::String::charToString(
                    static_cast<juce::juce_wchar>('A' + lane)),
                juce::Colour((lane & 1) == 0
                                 ? accentAColour : accentBColour));
            button->onClick = [this, lane]
            {
                owner.changeTarget(
                    entry, lane,
                    buttons[static_cast<std::size_t>(lane)]->getToggleState());
            };
            addAndMakeVisible(*button);
            buttons[static_cast<std::size_t>(lane)] = std::move(button);
        }
    }

    void setEntry(const ParameterEntry& newEntry, int serialPairMask)
    {
        entry = newEntry;
        for (int lane = 0; lane < 8; ++lane)
        {
            const auto pair = lane / 2;
            const auto serial = (serialPairMask & (1 << pair)) != 0;
            const auto storedLane = serial ? pair * 2 : lane;
            auto* button = buttons[static_cast<std::size_t>(lane)].get();
            button->setToggleState(
                entry.targets[static_cast<std::size_t>(storedLane)],
                juce::dontSendNotification);
            const auto laneName = juce::String::charToString(
                static_cast<juce::juce_wchar>('A' + lane));
            const auto firstName = juce::String::charToString(
                static_cast<juce::juce_wchar>('A' + pair * 2));
            const auto secondName = juce::String::charToString(
                static_cast<juce::juce_wchar>('A' + pair * 2 + 1));
            button->setTooltip(serial
                ? "Shared Seqwencer " + firstName + "/" + secondName
                    + " SERIAL target"
                : "Seqwencer " + laneName + " target");
        }
    }

    void paint(juce::Graphics& g) override
    {
        // Keep the Targets/Macro boundary more distinct than the ordinary
        // table grid so the controls cannot appear to belong to one column.
        g.setColour(juce::Colours::white.withAlpha(0.24f));
        g.drawVerticalLine(getWidth() - 1, 1.0f,
                           (float) getHeight() - 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(4, 1);
        for (int lane = 0; lane < 8; ++lane)
        {
            const auto lanesRemaining = 8 - lane;
            const auto width = lanesRemaining > 1
                ? area.getWidth() / lanesRemaining : area.getWidth();
            buttons[static_cast<std::size_t>(lane)]->setBounds(
                area.removeFromLeft(width));
        }
    }

private:
    static void configureButton(juce::ToggleButton& button,
                                const juce::String& text,
                                juce::Colour accent)
    {
        button.setButtonText(text);
        button.setClickingTogglesState(true);
        button.setColour(juce::ToggleButton::textColourId,
                         juce::Colours::white);
        button.setColour(juce::ToggleButton::tickColourId, accent);
        button.setColour(juce::ToggleButton::tickDisabledColourId,
                         accent.withAlpha(0.35f));
    }

    MacroMappingsView& owner;
    ParameterEntry entry;
    std::array<std::unique_ptr<juce::ToggleButton>, 8> buttons;
};

class MacroMappingsView::MacroCell final : public juce::Component
{
public:
    explicit MacroCell(MacroMappingsView& ownerIn)
        : owner(ownerIn),
          replaceButton(ButtonStyling::Glyphs::replace()),
          deleteButton(ButtonStyling::Glyphs::close(),
                       ButtonStyling::destructiveBackground())
    {
        replaceButton.onClick = [this]
        {
            if (entry.macroIndex >= 0 && owner.onReplaceMapping)
                owner.onReplaceMapping(entry.macroIndex);
        };
        deleteButton.onClick = [this]
        {
            if (entry.macroIndex >= 0)
                owner.confirmDelete(entry);
        };
        addAndMakeVisible(replaceButton);
        addAndMakeVisible(deleteButton);
    }

    void setEntry(const ParameterEntry& newEntry)
    {
        entry = newEntry;
        const auto mapped = entry.macroIndex >= 0;
        replaceButton.setVisible(mapped);
        deleteButton.setVisible(mapped);
        replaceButton.setTooltip(mapped
            ? "Replace Macro "
                + juce::String(entry.macroIndex + 1).paddedLeft('0', 3)
                + " with the last touched parameter"
            : juce::String());
        deleteButton.setTooltip(mapped
            ? "Permanently delete Macro "
                + juce::String(entry.macroIndex + 1).paddedLeft('0', 3)
                + " and its Seqwencer assignments"
            : juce::String());
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (entry.macroIndex >= 0)
        {
            g.setColour(juce::Colour(macroColour));
            g.setFont(juce::Font(juce::FontOptions(12.5f,
                                                   juce::Font::bold)));
            g.drawFittedText(
                juce::String(entry.macroIndex + 1).paddedLeft('0', 3),
                getLocalBounds().withTrimmedRight(60).reduced(5, 1),
                juce::Justification::centred,
                1);
        }

        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawVerticalLine(0, 1.0f,
                           (float) getHeight() - 1.0f);
        g.drawVerticalLine(getWidth() - 1, 2.0f,
                           (float) getHeight() - 2.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(4, 3);
        deleteButton.setBounds(area.removeFromRight(25));
        area.removeFromRight(3);
        replaceButton.setBounds(area.removeFromRight(25));
    }

private:
    MacroMappingsView& owner;
    ParameterEntry entry;
    ButtonStyling::SmallIconButton replaceButton;
    ButtonStyling::SmallIconButton deleteButton;
};

MacroMappingsView::MacroMappingsView()
{
    titleLabel.setText("Macro Mappings", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(22.0f,
                                                    juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    helpLabel.setColour(juce::Label::textColourId,
                        juce::Colours::lightgrey);
    helpLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    helpLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(helpLabel);

    filterEditor.setTextToShowWhenEmpty(
        "Search tabs, plug-ins, parameters or Macros...",
        juce::Colours::grey);
    filterEditor.setColour(juce::TextEditor::backgroundColourId,
                           juce::Colour(tableBackgroundColour));
    filterEditor.setColour(juce::TextEditor::textColourId,
                           juce::Colours::white);
    filterEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colours::white.withAlpha(0.25f));
    filterEditor.onTextChange = [this]
    {
        filterText = filterEditor.getText().trim();
        rebuildFilter();
    };
    addAndMakeVisible(filterEditor);

    assignedOnlyButton.setClickingTogglesState(true);
    assignedOnlyButton.setTooltip(
        "Toggle between every parameter and parameters with a Macro assignment");
    assignedOnlyButton.setColour(
        juce::TextButton::buttonColourId,
        ButtonStyling::defaultBackground());
    assignedOnlyButton.setColour(
        juce::TextButton::buttonOnColourId,
        juce::Colour(macroColour).darker(0.45f));
    assignedOnlyButton.onClick = [this]
    {
        assignedOnly = assignedOnlyButton.getToggleState();
        rebuildFilter();
    };
    addAndMakeVisible(assignedOnlyButton);

    undoButton.setEnabled(false);
    undoButton.onClick = [this]
    {
        if (onUndoLastEdit)
            onUndoLastEdit();
    };
    addAndMakeVisible(undoButton);

    clearAllButton.onClick = [this]
    {
        if (onClearAllMappings)
            onClearAllMappings();
    };
    addAndMakeVisible(clearAllButton);

    parameterTable.setModel(this);
    parameterTable.setRowHeight(28);
    parameterTable.setHeaderHeight(28);
    parameterTable.setOutlineThickness(1);
    parameterTable.setMultipleSelectionEnabled(false);
    parameterTable.setColour(juce::ListBox::backgroundColourId,
                             juce::Colour(tableBackgroundColour));
    parameterTable.setColour(juce::ListBox::outlineColourId,
                             juce::Colours::white.withAlpha(0.18f));

    auto& header = parameterTable.getHeader();
    header.addColumn("Tab", tabColumn, 58, 48, 90);
    header.addColumn("Plugin", pluginColumn, 120, 120, 420);
    header.addColumn("Parameter", parameterColumn, 190, 150, 620);
    header.addColumn("Mapped", mappedColumn, 84, 72, 110);
    header.addColumn("Targets", targetsColumn, 304, 272, 360);
    header.addColumn("Macro", macroColumn, 134, 118, 170);
    header.setPopupMenuActive(false);
    header.setStretchToFitActive(true);
    header.setSortColumnId(tabColumn, true);
    addAndMakeVisible(parameterTable);

    countLabel.setColour(juce::Label::textColourId,
                         juce::Colours::lightgrey);
    countLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(countLabel);

    setParameters({}, false, 0);
}

void MacroMappingsView::setParameters(
    const juce::Array<ParameterEntry>& newParameters,
    bool shouldShowSeqwencerTargets,
    int newSerialPairMask)
{
    allParameters = newParameters;
    serialPairMask = juce::jlimit(0, 15, newSerialPairMask);

    showSeqwencerTargets = shouldShowSeqwencerTargets;
    auto& header = parameterTable.getHeader();
    header.setColumnVisible(targetsColumn, showSeqwencerTargets);

    if (! showSeqwencerTargets && sortColumn == targetsColumn)
    {
        sortColumn = tabColumn;
        sortForwards = true;
        header.setSortColumnId(tabColumn, true);
    }

    helpLabel.setText(
        showSeqwencerTargets
            ? (serialPairMask != 0
                   ? "Mapped pauses or resumes a Macro. A-H are arranged as four pairs; each pair in SERIAL mirrors its two target buttons. Replace keeps the Macro number and X deletes it."
                   : "Mapped pauses or resumes a Macro. A-H are independent Seqwencer targets arranged as four pairs. Replace keeps the Macro number and X deletes it.")
            : "Mapped assigns the next free Macro or pauses/resumes it. Replace keeps the Macro number; X permanently deletes the assignment.",
        juce::dontSendNotification);

    clearAllButton.setEnabled(
        std::any_of(allParameters.begin(), allParameters.end(),
                    [](const ParameterEntry& entry)
                    {
                        return entry.macroIndex >= 0;
                    }));

    rebuildFilter();
}

void MacroMappingsView::setFilterText(const juce::String& newFilterText)
{
    const auto trimmed = newFilterText.trim();
    if (filterEditor.getText() != trimmed)
        filterEditor.setText(trimmed, juce::dontSendNotification);
    filterText = trimmed;
    rebuildFilter();
}

void MacroMappingsView::setUndoAvailable(bool shouldBeAvailable)
{
    undoButton.setEnabled(shouldBeAvailable);
}

int MacroMappingsView::getNumRows()
{
    return filteredParameterIndices.size();
}

const MacroMappingsView::ParameterEntry*
MacroMappingsView::getEntryForRow(int rowNumber) const
{
    if (! juce::isPositiveAndBelow(rowNumber,
                                   filteredParameterIndices.size()))
        return nullptr;

    const auto entryIndex = filteredParameterIndices[rowNumber];
    return juce::isPositiveAndBelow(entryIndex, allParameters.size())
        ? &allParameters.getReference(entryIndex)
        : nullptr;
}

void MacroMappingsView::paintRowBackground(juce::Graphics& g,
                                           int rowNumber,
                                           int width,
                                           int height,
                                           bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour(selectedRowColour));
    else if ((rowNumber & 1) != 0)
        g.fillAll(juce::Colours::white.withAlpha(0.025f));

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(height - 1, 0.0f, (float) width);
}

void MacroMappingsView::paintCell(juce::Graphics& g,
                                  int rowNumber,
                                  int columnId,
                                  int width,
                                  int height,
                                  bool rowIsSelected)
{
    const auto* entry = getEntryForRow(rowNumber);
    if (entry == nullptr
        || columnId == mappedColumn
        || columnId == targetsColumn
        || columnId == macroColumn)
        return;

    juce::String cellText;
    auto justification = juce::Justification::centredLeft;

    if (columnId == tabColumn)
    {
        cellText = juce::String(entry->tabIndex + 1);
        justification = juce::Justification::centred;
    }
    else if (columnId == pluginColumn)
    {
        cellText = entry->pluginName;
    }
    else if (columnId == parameterColumn)
    {
        cellText = entry->parameterName;
    }

    const auto tabTextColour = colourForTab(entry->tabIndex)
        .interpolatedWith(juce::Colours::white,
                          rowIsSelected ? 0.55f : 0.35f);
    g.setColour(tabTextColour);
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawFittedText(cellText,
                     juce::Rectangle<int>(0, 0, width, height).reduced(8, 1),
                     justification,
                     1);

    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawVerticalLine(width - 1, 2.0f, (float) height - 2.0f);
}

juce::Component* MacroMappingsView::refreshComponentForCell(
    int rowNumber,
    int columnId,
    bool,
    juce::Component* existingComponentToUpdate)
{
    const auto* entry = getEntryForRow(rowNumber);
    if (entry == nullptr)
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    if (columnId == mappedColumn)
    {
        auto* cell = dynamic_cast<MappedCell*>(existingComponentToUpdate);
        if (cell == nullptr)
        {
            delete existingComponentToUpdate;
            cell = new MappedCell(*this);
        }
        cell->setEntry(*entry);
        return cell;
    }

    if (columnId == targetsColumn && showSeqwencerTargets)
    {
        auto* cell = dynamic_cast<TargetsCell*>(existingComponentToUpdate);
        if (cell == nullptr)
        {
            delete existingComponentToUpdate;
            cell = new TargetsCell(*this);
        }
        cell->setEntry(*entry, serialPairMask);
        return cell;
    }

    if (columnId == macroColumn)
    {
        auto* cell = dynamic_cast<MacroCell*>(existingComponentToUpdate);
        if (cell == nullptr)
        {
            delete existingComponentToUpdate;
            cell = new MacroCell(*this);
        }
        cell->setEntry(*entry);
        return cell;
    }

    delete existingComponentToUpdate;
    return nullptr;
}

void MacroMappingsView::sortOrderChanged(int newSortColumnId,
                                         bool isForwards)
{
    if (newSortColumnId < tabColumn || newSortColumnId > macroColumn
        || (! showSeqwencerTargets && newSortColumnId == targetsColumn))
        return;

    sortColumn = newSortColumnId;
    sortForwards = isForwards;
    rebuildFilter();
}

bool MacroMappingsView::matchesFilter(const ParameterEntry& entry) const
{
    if (assignedOnly && entry.macroIndex < 0)
        return false;

    if (filterText.isEmpty())
        return true;

    auto searchable = juce::String(entry.tabIndex + 1)
        + " " + entry.tabName
        + " " + entry.pluginName
        + " " + entry.parameterName;

    if (entry.macroIndex >= 0)
    {
        searchable += " macro "
            + juce::String(entry.macroIndex + 1).paddedLeft('0', 3)
            + (entry.mappingEnabled ? " mapped enabled" : " paused disabled");
    }

    return searchable.toLowerCase().contains(filterText.toLowerCase());
}

int MacroMappingsView::compareEntries(const ParameterEntry& first,
                                      const ParameterEntry& second) const
{
    auto result = 0;

    if (sortColumn == tabColumn)
        result = first.tabIndex - second.tabIndex;
    else if (sortColumn == pluginColumn)
        result = first.pluginName.compareNatural(second.pluginName);
    else if (sortColumn == parameterColumn)
        result = first.parameterName.compareNatural(second.parameterName);
    else if (sortColumn == mappedColumn)
    {
        const auto firstState = first.macroIndex < 0
            ? 0 : (first.mappingEnabled ? 2 : 1);
        const auto secondState = second.macroIndex < 0
            ? 0 : (second.mappingEnabled ? 2 : 1);
        result = firstState - secondState;
    }
    else if (sortColumn == targetsColumn)
    {
        const auto visibleMask = [this](const ParameterEntry& entry)
        {
            auto mask = 0;
            for (int lane = 0; lane < 8; ++lane)
            {
                const auto pair = lane / 2;
                const auto storedLane = (serialPairMask & (1 << pair)) != 0
                    ? pair * 2 : lane;
                if (entry.targets[static_cast<std::size_t>(storedLane)])
                    mask |= 1 << lane;
            }
            return mask;
        };
        const auto firstMask = visibleMask(first);
        const auto secondMask = visibleMask(second);
        result = firstMask - secondMask;
    }
    else if (sortColumn == macroColumn)
    {
        const auto firstMacro = first.macroIndex >= 0
            ? first.macroIndex : 1000;
        const auto secondMacro = second.macroIndex >= 0
            ? second.macroIndex : 1000;
        result = firstMacro - secondMacro;
    }

    if (result == 0)
        result = first.tabIndex - second.tabIndex;
    if (result == 0)
        result = first.pluginName.compareNatural(second.pluginName);
    if (result == 0)
        result = first.parameterName.compareNatural(second.parameterName);
    if (result == 0)
        result = first.parameterIndex - second.parameterIndex;

    return sortForwards ? result : -result;
}

void MacroMappingsView::rebuildFilter()
{
    filteredParameterIndices.clearQuick();

    for (int index = 0; index < allParameters.size(); ++index)
    {
        if (matchesFilter(allParameters.getReference(index)))
            filteredParameterIndices.add(index);
    }

    std::stable_sort(filteredParameterIndices.begin(),
                     filteredParameterIndices.end(),
                     [this](int firstIndex, int secondIndex)
                     {
                         return compareEntries(
                                    allParameters.getReference(firstIndex),
                                    allParameters.getReference(secondIndex)) < 0;
                     });

    const auto filteredCount = filteredParameterIndices.size();
    const auto totalCount = allParameters.size();
    countLabel.setText(
        filteredCount == totalCount
            ? juce::String(totalCount)
                + (totalCount == 1 ? " parameter" : " parameters")
            : juce::String(filteredCount) + " of " + juce::String(totalCount)
                + " parameters",
        juce::dontSendNotification);

    parameterTable.updateContent();
    parameterTable.repaint();
    repaint();
}

void MacroMappingsView::changeMappingEnabled(const ParameterEntry& entry,
                                             bool enabled)
{
    if (! onSetMappingEnabled)
        return;

    juce::String errorMessage;
    if (! onSetMappingEnabled(entry, enabled, errorMessage))
    {
        showError(errorMessage.isNotEmpty()
                      ? errorMessage
                      : "PHI could not change that Macro mapping.");
        parameterTable.updateContent();
    }
}

void MacroMappingsView::changeTarget(const ParameterEntry& entry,
                                     int lane,
                                     bool assigned)
{
    if (! onSetSeqwencerTarget)
        return;

    juce::String errorMessage;
    if (! onSetSeqwencerTarget(entry, lane, assigned, errorMessage))
    {
        showError(errorMessage.isNotEmpty()
                      ? errorMessage
                      : "PHI could not change that Seqwencer target.");
        parameterTable.updateContent();
    }
}

void MacroMappingsView::confirmDelete(const ParameterEntry& entry)
{
    if (entry.macroIndex < 0 || ! onDeleteMapping)
        return;

    juce::Component::SafePointer<MacroMappingsView> safeThis(this);
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Delete Macro Mapping",
        "Delete Macro "
            + juce::String(entry.macroIndex + 1).paddedLeft('0', 3)
            + " from " + entry.pluginName + " / "
            + entry.parameterName + "?\n\nThis also removes its "
              "Seqwencer A and B assignments.",
        "Delete",
        "Cancel",
        nullptr,
        juce::ModalCallbackFunction::create(
            [safeThis, macroIndex = entry.macroIndex](int result)
            {
                if (safeThis != nullptr && result != 0
                    && safeThis->onDeleteMapping)
                    safeThis->onDeleteMapping(macroIndex);
            }));
}

void MacroMappingsView::showError(const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        "Macro Mappings",
        message);
}

void MacroMappingsView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(backgroundColour));

    if (filteredParameterIndices.isEmpty())
    {
        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.drawFittedText(
            allParameters.isEmpty()
                ? "No automatable parameters are available. Load a synth or effect into PHI first."
                : "No parameters match this search.",
            parameterTable.getBounds().reduced(20),
            juce::Justification::centred,
            3);
    }
}

void MacroMappingsView::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto topRow = area.removeFromTop(34);
    clearAllButton.setBounds(
        topRow.removeFromRight(74).withSizeKeepingCentre(74, 24));
    topRow.removeFromRight(8);
    undoButton.setBounds(
        topRow.removeFromRight(58).withSizeKeepingCentre(58, 24));
    topRow.removeFromRight(8);
    assignedOnlyButton.setBounds(
        topRow.removeFromRight(106).withSizeKeepingCentre(106, 24));
    topRow.removeFromRight(8);
    filterEditor.setBounds(
        topRow.removeFromRight(270).withSizeKeepingCentre(270, 26));
    topRow.removeFromRight(12);
    titleLabel.setBounds(topRow);

    area.removeFromTop(5);
    helpLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(9);

    auto footer = area.removeFromBottom(24);
    countLabel.setBounds(footer);
    area.removeFromBottom(7);
    parameterTable.setBounds(area);
}
