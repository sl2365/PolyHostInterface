#include "MacroMappingsView.h"

MacroMappingsView::MappingRow::MappingRow()
{
    auto configureLabel = [](juce::Label& label, juce::Justification justification)
    {
        label.setJustificationType(justification);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
    };

    configureLabel(macroLabel, juce::Justification::centredLeft);
    configureLabel(tabLabel, juce::Justification::centredLeft);
    configureLabel(pluginLabel, juce::Justification::centredLeft);
    configureLabel(parameterLabel, juce::Justification::centredLeft);

    addAndMakeVisible(macroLabel);
    addAndMakeVisible(tabLabel);
    addAndMakeVisible(pluginLabel);
    addAndMakeVisible(parameterLabel);
    addAndMakeVisible(upButton);
    addAndMakeVisible(downButton);
    addAndMakeVisible(replaceButton);
    addAndMakeVisible(deleteButton);

    upButton.setTooltip("Move this mapping up");
    downButton.setTooltip("Move this mapping down");
    replaceButton.setTooltip("Replace this macro target with the\ncurrent last touched parameter");
    deleteButton.setTooltip("Delete this macro mapping");

    upButton.onClick = [this]
    {
        if (onMoveUp)
            onMoveUp(entry.macroIndex);
    };

    downButton.onClick = [this]
    {
        if (onMoveDown)
            onMoveDown(entry.macroIndex);
    };

    replaceButton.onClick = [this]
    {
        if (onReplace)
            onReplace(entry.macroIndex);
    };

    deleteButton.onClick = [this]
    {
        if (onDeleteMapping)
            onDeleteMapping(entry.macroIndex);
    };
}

void MacroMappingsView::MappingRow::setMapping(const MappingEntry& newEntry)
{
    entry = newEntry;

    macroLabel.setText("Macro " + juce::String(entry.macroIndex + 1).paddedLeft('0', 3),
                       juce::dontSendNotification);

    tabLabel.setText(entry.tabIndex >= 0 ? ("Tab " + juce::String(entry.tabIndex + 1)) : "-",
                     juce::dontSendNotification);

    pluginLabel.setText(entry.pluginName.isNotEmpty() ? entry.pluginName : "-",
                        juce::dontSendNotification);

    parameterLabel.setText(entry.parameterName.isNotEmpty()
                               ? entry.parameterName
                               : (entry.parameterIndex >= 0
                                      ? ("Parameter " + juce::String(entry.parameterIndex))
                                      : "-"),
                           juce::dontSendNotification);

    upButton.setEnabled(entry.canMoveUp);
    downButton.setEnabled(entry.canMoveDown);

    repaint();
}

void MacroMappingsView::MappingRow::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xFF243B55));
    g.fillRoundedRectangle(area.reduced(1.0f), 8.0f);

    g.setColour(juce::Colour(0xFF3A506B));
    g.drawRoundedRectangle(area.reduced(1.0f), 8.0f, 1.2f);
}

void MacroMappingsView::MappingRow::resized()
{
    auto area = getLocalBounds().reduced(10);

    auto deleteArea = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    deleteArea = deleteArea.withSizeKeepingCentre(deleteArea.getWidth(),
                                                  ButtonStyling::defaultButtonHeight());
    deleteButton.setBounds(deleteArea);

    area.removeFromRight(6);

    auto replaceArea = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    replaceArea = replaceArea.withSizeKeepingCentre(replaceArea.getWidth(),
                                                    ButtonStyling::defaultButtonHeight());
    replaceButton.setBounds(replaceArea);

    area.removeFromRight(6);

    auto downArea = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    downArea = downArea.withSizeKeepingCentre(downArea.getWidth(),
                                              ButtonStyling::defaultButtonHeight());
    downButton.setBounds(downArea);

    area.removeFromRight(6);

    auto upArea = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    upArea = upArea.withSizeKeepingCentre(upArea.getWidth(),
                                          ButtonStyling::defaultButtonHeight());
    upButton.setBounds(upArea);

    area.removeFromRight(10);

    macroLabel.setBounds(area.removeFromLeft(100));
    area.removeFromLeft(10);

    tabLabel.setBounds(area.removeFromLeft(70));
    area.removeFromLeft(10);

    pluginLabel.setBounds(area.removeFromLeft(220));
    area.removeFromLeft(10);

    parameterLabel.setBounds(area);
}

MacroMappingsView::MacroMappingsView()
{
    titleLabel.setText("Macro Mappings", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    helpLabel.setText("These are global PolyHost macro mappings. Use Macro 001-128 in your DAW, ignore the MIDI CC entries.",
                      juce::dontSendNotification);
    helpLabel.setJustificationType(juce::Justification::centredLeft);
    helpLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    helpLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    addAndMakeVisible(helpLabel);

    filterLabel.setText("Filter:", juce::dontSendNotification);
    filterLabel.setJustificationType(juce::Justification::centredLeft);
    filterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(filterLabel);

    filterEditor.setTextToShowWhenEmpty("Search by macro, tab, plugin, or parameter", juce::Colours::grey);
    filterEditor.onTextChange = [this]
    {
        setFilterText(filterEditor.getText());
    };
    addAndMakeVisible(filterEditor);

    auto configureHeaderLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        label.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        addAndMakeVisible(label);
    };

    configureHeaderLabel(macroHeaderLabel, "Macro");
    configureHeaderLabel(tabHeaderLabel, "Tab");
    configureHeaderLabel(pluginHeaderLabel, "Plugin");
    configureHeaderLabel(parameterHeaderLabel, "Parameter");

    undoButton.onClick = [this]
    {
        if (onUndoLastEdit)
            onUndoLastEdit();
    };
    undoButton.setEnabled(false);
    addAndMakeVisible(undoButton);

    clearAllButton.onClick = [this]
    {
        if (onClearAllMappings)
            onClearAllMappings();
    };
    addAndMakeVisible(clearAllButton);

    emptyLabel.setText("No macro mappings.\n\nAdjust a hosted parameter, then click the toolbar 'Map Last Touched' button.",
                       juce::dontSendNotification);
    emptyLabel.setJustificationType(juce::Justification::centred);
    emptyLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(emptyLabel);

    viewport.setViewedComponent(&contentComponent, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void MacroMappingsView::setMappings(const juce::Array<MappingEntry>& newMappings)
{
    allMappings = newMappings;
    rebuildRows();
    resized();
}

void MacroMappingsView::setFilterText(const juce::String& newFilterText)
{
    filterText = newFilterText.trim();
    rebuildRows();
    resized();
}

void MacroMappingsView::setUndoAvailable(bool shouldBeAvailable)
{
    undoButton.setEnabled(shouldBeAvailable);
}

bool MacroMappingsView::matchesFilter(const MappingEntry& entry) const
{
    if (filterText.isEmpty())
        return true;

    const auto needle = filterText.toLowerCase();

    const juce::String haystack =
        ("macro " + juce::String(entry.macroIndex + 1)
         + " " + entry.label
         + " tab " + juce::String(entry.tabIndex + 1)
         + " " + entry.pluginName
         + " " + entry.parameterName).toLowerCase();

    return haystack.contains(needle);
}

void MacroMappingsView::rebuildRows()
{
    filteredMappings.clear();

    for (auto& entry : allMappings)
    {
        if (matchesFilter(entry))
            filteredMappings.add(entry);
    }

    mappingRows.clear();
    contentComponent.removeAllChildren();

    for (auto& mapping : filteredMappings)
    {
        auto* row = mappingRows.add(new MappingRow());
        row->setMapping(mapping);

        row->onDeleteMapping = [this](int macroIndex)
        {
            if (onDeleteMapping)
                onDeleteMapping(macroIndex);
        };

        row->onMoveUp = [this](int macroIndex)
        {
            if (onMoveMappingUp)
                onMoveMappingUp(macroIndex);
        };

        row->onMoveDown = [this](int macroIndex)
        {
            if (onMoveMappingDown)
                onMoveMappingDown(macroIndex);
        };

        row->onReplace = [this](int macroIndex)
        {
            if (onReplaceMapping)
                onReplaceMapping(macroIndex);
        };

        contentComponent.addAndMakeVisible(row);
    }

    const bool hasMappings = ! filteredMappings.isEmpty();
    viewport.setVisible(hasMappings);
    emptyLabel.setVisible(! hasMappings);

    macroHeaderLabel.setVisible(hasMappings);
    tabHeaderLabel.setVisible(hasMappings);
    pluginHeaderLabel.setVisible(hasMappings);
    parameterHeaderLabel.setVisible(hasMappings);
}

void MacroMappingsView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1B263B));
}

void MacroMappingsView::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto topRow = area.removeFromTop(34);

    auto clearAllArea = topRow.removeFromRight(90);
    clearAllButton.setBounds(clearAllArea.withSizeKeepingCentre(90, 22));

    topRow.removeFromRight(8);

    auto undoArea = topRow.removeFromRight(70);
    undoButton.setBounds(undoArea.withSizeKeepingCentre(70, 22));

    topRow.removeFromRight(8);

    auto editorArea = topRow.removeFromRight(280);
    filterEditor.setBounds(editorArea.withSizeKeepingCentre(editorArea.getWidth(), 23));

    topRow.removeFromRight(8);

    auto labelArea = topRow.removeFromRight(45);
    filterLabel.setBounds(labelArea);

    topRow.removeFromRight(16);

    titleLabel.setBounds(topRow);

    area.removeFromTop(6);
    helpLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(10);

    auto headerRow = area.removeFromTop(22);
    headerRow.removeFromLeft(10);

    macroHeaderLabel.setBounds(headerRow.removeFromLeft(100));
    headerRow.removeFromLeft(10);

    tabHeaderLabel.setBounds(headerRow.removeFromLeft(70));
    headerRow.removeFromLeft(10);

    pluginHeaderLabel.setBounds(headerRow.removeFromLeft(220));
    headerRow.removeFromLeft(10);

    parameterHeaderLabel.setBounds(headerRow);

    area.removeFromTop(6);

    emptyLabel.setBounds(area);
    viewport.setBounds(area);

    auto contentArea = viewport.getLocalBounds();
    int y = 0;
    constexpr int rowHeight = 52;
    constexpr int rowGap = 8;

    for (auto* row : mappingRows)
    {
        row->setBounds(0, y, juce::jmax(0, contentArea.getWidth() - 12), rowHeight);
        y += rowHeight + rowGap;
    }

    contentComponent.setSize(juce::jmax(0, contentArea.getWidth() - 12),
                             juce::jmax(y, viewport.getHeight()));
}