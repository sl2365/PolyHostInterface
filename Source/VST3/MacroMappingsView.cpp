#include "MacroMappingsView.h"
#include <algorithm>

MacroMappingsView::SortHeaderButton::SortHeaderButton(
    const juce::String& text)
    : juce::TextButton(text)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void MacroMappingsView::SortHeaderButton::setDirection(
    Direction newDirection)
{
    if (direction == newDirection)
        return;

    direction = newDirection;
    repaint();
}

void MacroMappingsView::SortHeaderButton::paintButton(
    juce::Graphics& g,
    bool isMouseOverButton,
    bool isButtonDown)
{
    auto bounds = getLocalBounds();

    if (isMouseOverButton || isButtonDown)
    {
        g.setColour(
            juce::Colour(0xFF3A506B).withAlpha(
                isButtonDown ? 0.65f : 0.40f));
        g.fillRoundedRectangle(
            bounds.toFloat().reduced(1.0f),
            3.0f);
    }

    const bool isActive = direction != Direction::none;
    const auto font =
        juce::Font(juce::FontOptions(13.0f, juce::Font::bold));

    g.setFont(font);
    g.setColour(
        isActive
            ? juce::Colour(0xFFB9DAFF)
            : juce::Colour(0xFF79B8FF));
    g.drawText(getButtonText(),
               bounds,
               juce::Justification::centredLeft,
               true);

    if (! isActive)
        return;

    const float arrowCentreX = juce::jmin(
        (float) bounds.getRight() - 5.0f,
        (float) bounds.getX()
            + (float) getButtonText().length() * 7.0f
            + 7.0f);
    const float arrowCentreY =
        (float) bounds.getCentreY();

    juce::Path arrow;

    if (direction == Direction::ascending)
    {
        arrow.startNewSubPath(
            arrowCentreX,
            arrowCentreY - 3.0f);
        arrow.lineTo(
            arrowCentreX - 3.5f,
            arrowCentreY + 2.5f);
        arrow.lineTo(
            arrowCentreX + 3.5f,
            arrowCentreY + 2.5f);
    }
    else
    {
        arrow.startNewSubPath(
            arrowCentreX - 3.5f,
            arrowCentreY - 2.5f);
        arrow.lineTo(
            arrowCentreX + 3.5f,
            arrowCentreY - 2.5f);
        arrow.lineTo(
            arrowCentreX,
            arrowCentreY + 3.0f);
    }

    arrow.closeSubPath();
    g.fillPath(arrow);
}

MacroMappingsView::MappingRow::DragHandle::DragHandle()
{
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setTooltip("Drag to reorder this mapping");
}

void MacroMappingsView::MappingRow::DragHandle::setReorderingEnabled(
    bool shouldBeEnabled)
{
    reorderingEnabled = shouldBeEnabled;
    dragStarted = false;

    setMouseCursor(
        reorderingEnabled
            ? juce::MouseCursor::DraggingHandCursor
            : juce::MouseCursor::NormalCursor);

    setTooltip(
        reorderingEnabled
            ? "Drag to reorder this mapping"
            : "Return to Macro order before reordering mappings");

    repaint();
}

void MacroMappingsView::MappingRow::DragHandle::paint(juce::Graphics& g)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const auto colour = ! reorderingEnabled
                            ? juce::Colours::lightgrey.withAlpha(0.25f)
                            : (dragStarted || isMouseOverOrDragging()
                                   ? juce::Colour(0xFF79B8FF)
                                   : juce::Colours::lightgrey.withAlpha(0.70f));

    g.setColour(colour);

    constexpr float dotSize = 3.0f;
    constexpr float xOffset = 3.25f;
    constexpr float ySpacing = 6.0f;

    for (int column = -1; column <= 1; column += 2)
    {
        for (int row = -1; row <= 1; ++row)
        {
            g.fillEllipse(
                centre.x + (float) column * xOffset - dotSize * 0.5f,
                centre.y + (float) row * ySpacing - dotSize * 0.5f,
                dotSize,
                dotSize);
        }
    }
}

void MacroMappingsView::MappingRow::DragHandle::mouseDown(
    const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (! reorderingEnabled)
        return;

    dragStarted = false;
    repaint();
}

void MacroMappingsView::MappingRow::DragHandle::mouseDrag(
    const juce::MouseEvent& event)
{
    if (! reorderingEnabled
        || ! event.mods.isLeftButtonDown())
        return;

    const auto screenPosition = event.getScreenPosition();

    if (! dragStarted
        && event.getDistanceFromDragStart() >= 4)
    {
        dragStarted = true;

        if (onDragStarted)
            onDragStarted(screenPosition);
    }

    if (dragStarted && onDragMoved)
        onDragMoved(screenPosition);

    repaint();
}

void MacroMappingsView::MappingRow::DragHandle::mouseUp(
    const juce::MouseEvent& event)
{
    if (! reorderingEnabled)
        return;

    if (dragStarted && onDragEnded)
        onDragEnded(event.getScreenPosition());

    dragStarted = false;
    repaint();
}

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
    addAndMakeVisible(dragHandle);
    addAndMakeVisible(replaceButton);
    addAndMakeVisible(deleteButton);

    replaceButton.setTooltip("Replace this macro target with the\ncurrent last touched parameter");
    deleteButton.setTooltip("Delete this macro mapping");

    dragHandle.onDragStarted = [this](juce::Point<int> screenPosition)
    {
        if (onDragStarted)
            onDragStarted(entry.macroIndex, screenPosition);
    };

    dragHandle.onDragMoved = [this](juce::Point<int> screenPosition)
    {
        if (onDragMoved)
            onDragMoved(entry.macroIndex, screenPosition);
    };

    dragHandle.onDragEnded = [this](juce::Point<int> screenPosition)
    {
        if (onDragEnded)
            onDragEnded(entry.macroIndex, screenPosition);
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

    repaint();
}

void MacroMappingsView::MappingRow::setReorderingEnabled(
    bool shouldBeEnabled)
{
    dragHandle.setReorderingEnabled(shouldBeEnabled);
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

    auto dragArea = area.removeFromLeft(20);
    dragHandle.setBounds(
        dragArea.withSizeKeepingCentre(
            dragArea.getWidth(),
            ButtonStyling::defaultButtonHeight()));
    area.removeFromLeft(8);

    auto deleteArea = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    deleteArea = deleteArea.withSizeKeepingCentre(deleteArea.getWidth(),
                                                  ButtonStyling::defaultButtonHeight());
    deleteButton.setBounds(deleteArea);

    area.removeFromRight(6);

    auto replaceArea = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    replaceArea = replaceArea.withSizeKeepingCentre(replaceArea.getWidth(),
                                                    ButtonStyling::defaultButtonHeight());
    replaceButton.setBounds(replaceArea);

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

    tabHeaderButton.onClick = [this]
    {
        cycleSort(SortColumn::tab);
    };

    pluginHeaderButton.onClick = [this]
    {
        cycleSort(SortColumn::plugin);
    };

    parameterHeaderButton.onClick = [this]
    {
        cycleSort(SortColumn::parameter);
    };

    addAndMakeVisible(tabHeaderButton);
    addAndMakeVisible(pluginHeaderButton);
    addAndMakeVisible(parameterHeaderButton);
    updateSortHeaderButtons();

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
    viewport.setSingleStepSizes(16, 4);
    addAndMakeVisible(viewport);
}

void MacroMappingsView::setMappings(const juce::Array<MappingEntry>& newMappings)
{
    if (draggedMacroIndex >= 0)
    {
        deferredMappings = newMappings;
        hasDeferredMappings = true;
        return;
    }

    allMappings = newMappings;

    if (allMappings.size() > 1)
    {
        std::sort(allMappings.begin(),
                  allMappings.end(),
                  [](const MappingEntry& first, const MappingEntry& second)
                  {
                      return first.macroIndex < second.macroIndex;
                  });
    }

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

void MacroMappingsView::cycleSort(SortColumn column)
{
    if (sortColumn != column)
    {
        sortColumn = column;
        sortAscending = true;
    }
    else if (sortAscending)
    {
        sortAscending = false;
    }
    else
    {
        sortColumn = SortColumn::macro;
        sortAscending = true;
    }

    updateSortHeaderButtons();
    rebuildRows();
    resized();
    viewport.setViewPosition(0, 0);
    repaint();
}

void MacroMappingsView::sortFilteredMappings()
{
    if (sortColumn == SortColumn::macro
        || filteredMappings.size() < 2)
    {
        return;
    }

    const auto selectedColumn = sortColumn;
    const bool ascending = sortAscending;

    std::stable_sort(
        filteredMappings.begin(),
        filteredMappings.end(),
        [selectedColumn, ascending](const MappingEntry& first,
                                    const MappingEntry& second)
        {
            int comparison = 0;

            if (selectedColumn == SortColumn::tab)
            {
                const bool firstIsValid = first.tabIndex >= 0;
                const bool secondIsValid = second.tabIndex >= 0;

                if (firstIsValid != secondIsValid)
                    return firstIsValid;

                if (first.tabIndex < second.tabIndex)
                    comparison = -1;
                else if (first.tabIndex > second.tabIndex)
                    comparison = 1;
            }
            else if (selectedColumn == SortColumn::plugin)
            {
                const bool firstIsEmpty = first.pluginName.isEmpty();
                const bool secondIsEmpty = second.pluginName.isEmpty();

                if (firstIsEmpty != secondIsEmpty)
                    return ! firstIsEmpty;

                comparison =
                    first.pluginName.compareIgnoreCase(
                        second.pluginName);
            }
            else if (selectedColumn == SortColumn::parameter)
            {
                const bool firstHasName = first.parameterName.isNotEmpty();
                const bool secondHasName = second.parameterName.isNotEmpty();

                if (firstHasName != secondHasName)
                    return firstHasName;

                if (firstHasName)
                {
                    comparison =
                        first.parameterName.compareIgnoreCase(
                            second.parameterName);
                }
                else if (first.parameterIndex < second.parameterIndex)
                {
                    comparison = -1;
                }
                else if (first.parameterIndex > second.parameterIndex)
                {
                    comparison = 1;
                }
            }

            if (comparison == 0)
                return false;

            return ascending
                       ? comparison < 0
                       : comparison > 0;
        });
}

void MacroMappingsView::updateSortHeaderButtons()
{
    using Direction = SortHeaderButton::Direction;

    const auto directionFor = [this](SortColumn column)
    {
        if (sortColumn != column)
            return Direction::none;

        return sortAscending
                   ? Direction::ascending
                   : Direction::descending;
    };

    tabHeaderButton.setDirection(
        directionFor(SortColumn::tab));
    pluginHeaderButton.setDirection(
        directionFor(SortColumn::plugin));
    parameterHeaderButton.setDirection(
        directionFor(SortColumn::parameter));

    const auto setTooltip =
        [this](SortHeaderButton& button,
               SortColumn column,
               const juce::String& title)
        {
            if (sortColumn != column)
            {
                button.setTooltip(
                    "Sort by " + title + " in ascending order");
            }
            else if (sortAscending)
            {
                button.setTooltip(
                    "Sorted by " + title
                    + " ascending. Click for descending order");
            }
            else
            {
                button.setTooltip(
                    "Sorted by " + title
                    + " descending. Click to restore Macro order");
            }
        };

    setTooltip(tabHeaderButton, SortColumn::tab, "Tab");
    setTooltip(pluginHeaderButton, SortColumn::plugin, "Plugin");
    setTooltip(parameterHeaderButton, SortColumn::parameter, "Parameter");
}

void MacroMappingsView::rebuildRows()
{
    filteredMappings.clear();

    for (auto& entry : allMappings)
    {
        if (matchesFilter(entry))
            filteredMappings.add(entry);
    }

    sortFilteredMappings();

    mappingRows.clear();
    contentComponent.removeAllChildren();

    for (auto& mapping : filteredMappings)
    {
        auto* row = mappingRows.add(new MappingRow());
        row->setMapping(mapping);
        row->setReorderingEnabled(
            sortColumn == SortColumn::macro);

        row->onDeleteMapping = [this](int macroIndex)
        {
            if (onDeleteMapping)
                onDeleteMapping(macroIndex);
        };

        row->onDragStarted = [this](int macroIndex, juce::Point<int> screenPosition)
        {
            beginMappingDrag(macroIndex, screenPosition);
        };

        row->onDragMoved = [this](int macroIndex, juce::Point<int> screenPosition)
        {
            updateMappingDrag(macroIndex, screenPosition);
        };

        row->onDragEnded = [this](int macroIndex, juce::Point<int> screenPosition)
        {
            endMappingDrag(macroIndex, screenPosition);
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
    tabHeaderButton.setVisible(hasMappings);
    pluginHeaderButton.setVisible(hasMappings);
    parameterHeaderButton.setVisible(hasMappings);
}

void MacroMappingsView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1B263B));
}

void MacroMappingsView::paintOverChildren(juce::Graphics& g)
{
    if (draggedMacroIndex < 0
        || dropInsertionIndex < 0
        || mappingRows.isEmpty()
        || ! viewport.isVisible())
    {
        return;
    }

    const int contentY = getDropIndicatorContentY();

    const int localY =
        getLocalPoint(
            &contentComponent,
            juce::Point<int>(0, contentY)).y;

    auto clip = viewport.getBounds().reduced(2);

    if (localY < clip.getY() - 2
        || localY > clip.getBottom() + 2)
    {
        return;
    }

    juce::Graphics::ScopedSaveState saveState(g);
    g.reduceClipRegion(clip);

    auto line = juce::Rectangle<float>(
        (float) clip.getX() + 8.0f,
        (float) localY - 1.5f,
        (float) juce::jmax(0, clip.getWidth() - 28),
        3.0f);

    const auto indicatorColour = juce::Colour(0xFF4DA3FF);

    g.setColour(indicatorColour);
    g.fillRoundedRectangle(line, 1.5f);
    g.fillEllipse(line.getX() - 2.0f,
                  line.getCentreY() - 3.5f,
                  7.0f,
                  7.0f);
    g.fillEllipse(line.getRight() - 5.0f,
                  line.getCentreY() - 3.5f,
                  7.0f,
                  7.0f);
}

void MacroMappingsView::beginMappingDrag(
    int macroIndex,
    juce::Point<int> screenPosition)
{
    if (sortColumn != SortColumn::macro
        || findFilteredMappingIndex(macroIndex) < 0)
        return;

    draggedMacroIndex = macroIndex;
    lastDragScreenPosition = screenPosition;
    dropInsertionIndex = getDropInsertionIndex(screenPosition);
    startTimerHz(30);
    repaint();
}

void MacroMappingsView::updateMappingDrag(
    int macroIndex,
    juce::Point<int> screenPosition)
{
    if (macroIndex != draggedMacroIndex)
        return;

    lastDragScreenPosition = screenPosition;
    autoScrollForDrag(screenPosition);

    const int newInsertionIndex =
        getDropInsertionIndex(screenPosition);

    if (newInsertionIndex != dropInsertionIndex)
    {
        dropInsertionIndex = newInsertionIndex;
        repaint();
    }
}

void MacroMappingsView::endMappingDrag(
    int macroIndex,
    juce::Point<int> screenPosition)
{
    if (macroIndex != draggedMacroIndex)
        return;

    stopTimer();
    lastDragScreenPosition = screenPosition;

    const int sourceRowIndex =
        findFilteredMappingIndex(macroIndex);

    const auto viewportPosition =
        viewport.getLocalPoint(
            nullptr,
            screenPosition);

    const bool droppedInsideViewport =
        viewport.getLocalBounds().contains(
            viewportPosition);

    if (droppedInsideViewport)
    {
        dropInsertionIndex =
            getDropInsertionIndex(screenPosition);
    }

    int destinationRowIndex =
        droppedInsideViewport
            ? dropInsertionIndex
            : sourceRowIndex;

    if (sourceRowIndex < destinationRowIndex)
        --destinationRowIndex;

    destinationRowIndex = juce::jlimit(
        0,
        juce::jmax(0, filteredMappings.size() - 1),
        destinationRowIndex);

    const int destinationMacroIndex =
        juce::isPositiveAndBelow(
            destinationRowIndex,
            filteredMappings.size())
            ? filteredMappings.getReference(
                  destinationRowIndex).macroIndex
            : macroIndex;

    draggedMacroIndex = -1;
    dropInsertionIndex = -1;
    repaint();

    auto deferredMappingsToApply = deferredMappings;
    const bool shouldApplyDeferredMappings = hasDeferredMappings;

    deferredMappings.clear();
    hasDeferredMappings = false;

    if (destinationMacroIndex == macroIndex
        || ! onMoveMapping)
    {
        if (shouldApplyDeferredMappings)
        {
            juce::Component::SafePointer<MacroMappingsView> safeThis(this);

            juce::MessageManager::callAsync(
                [safeThis, deferredMappingsToApply]
                {
                    if (safeThis != nullptr)
                        safeThis->setMappings(deferredMappingsToApply);
                });
        }

        return;
    }

    juce::Component::SafePointer<MacroMappingsView> safeThis(this);

    juce::MessageManager::callAsync(
        [safeThis, macroIndex, destinationMacroIndex]
        {
            if (safeThis != nullptr && safeThis->onMoveMapping)
            {
                safeThis->onMoveMapping(
                    macroIndex,
                    destinationMacroIndex);
            }
        });
}

void MacroMappingsView::autoScrollForDrag(
    juce::Point<int> screenPosition)
{
    if (! viewport.isVisible())
        return;

    const auto localPosition =
        viewport.getLocalPoint(
            nullptr,
            screenPosition);

    constexpr int edgeSize = 28;
    constexpr int scrollStep = 18;

    auto viewPosition = viewport.getViewPosition();
    int newY = viewPosition.y;

    if (localPosition.y < edgeSize)
        newY -= scrollStep;
    else if (localPosition.y > viewport.getHeight() - edgeSize)
        newY += scrollStep;

    const int maximumY =
        juce::jmax(
            0,
            contentComponent.getHeight()
                - viewport.getViewHeight());

    newY = juce::jlimit(0, maximumY, newY);

    if (newY != viewPosition.y)
    {
        viewport.setViewPosition(
            viewPosition.x,
            newY);
        repaint();
    }
}

int MacroMappingsView::getDropInsertionIndex(
    juce::Point<int> screenPosition) const
{
    if (mappingRows.isEmpty())
        return -1;

    const auto contentPosition =
        contentComponent.getLocalPoint(
            nullptr,
            screenPosition);

    for (int rowIndex = 0;
         rowIndex < mappingRows.size();
         ++rowIndex)
    {
        const auto* row = mappingRows[rowIndex];

        if (row != nullptr
            && contentPosition.y
                < row->getBounds().getCentreY())
        {
            return rowIndex;
        }
    }

    return mappingRows.size();
}

int MacroMappingsView::getDropIndicatorContentY() const
{
    if (mappingRows.isEmpty()
        || dropInsertionIndex < 0)
    {
        return 0;
    }

    if (dropInsertionIndex <= 0)
        return mappingRows[0]->getY();

    if (dropInsertionIndex >= mappingRows.size())
        return mappingRows[mappingRows.size() - 1]->getBottom() + 4;

    const auto* previousRow =
        mappingRows[dropInsertionIndex - 1];
    const auto* nextRow =
        mappingRows[dropInsertionIndex];

    if (previousRow == nullptr || nextRow == nullptr)
        return 0;

    return (previousRow->getBottom()
            + nextRow->getY())
           / 2;
}

int MacroMappingsView::findFilteredMappingIndex(
    int macroIndex) const
{
    for (int index = 0;
         index < filteredMappings.size();
         ++index)
    {
        if (filteredMappings.getReference(index).macroIndex
            == macroIndex)
        {
            return index;
        }
    }

    return -1;
}

void MacroMappingsView::timerCallback()
{
    if (draggedMacroIndex < 0)
    {
        stopTimer();
        return;
    }

    updateMappingDrag(
        draggedMacroIndex,
        lastDragScreenPosition);
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
    headerRow.removeFromLeft(20);
    headerRow.removeFromLeft(8);

    macroHeaderLabel.setBounds(headerRow.removeFromLeft(100));
    headerRow.removeFromLeft(10);

    tabHeaderButton.setBounds(headerRow.removeFromLeft(70));
    headerRow.removeFromLeft(10);

    pluginHeaderButton.setBounds(headerRow.removeFromLeft(220));
    headerRow.removeFromLeft(10);

    parameterHeaderButton.setBounds(headerRow);

    area.removeFromTop(6);

    emptyLabel.setBounds(area);
    viewport.setBounds(area);

    auto contentArea = viewport.getLocalBounds();
    constexpr int topPadding = 6;
    constexpr int bottomPadding = 2;
    int y = topPadding;
    constexpr int rowHeight = 52;
    constexpr int rowGap = 8;

    for (auto* row : mappingRows)
    {
        row->setBounds(0, y, juce::jmax(0, contentArea.getWidth() - 12), rowHeight);
        y += rowHeight + rowGap;
    }

    contentComponent.setSize(juce::jmax(0, contentArea.getWidth() - 12),
                             juce::jmax(y + bottomPadding, viewport.getHeight()));
}
