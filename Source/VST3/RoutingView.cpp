#include "RoutingView.h"
#include "ButtonStyling.h"

RoutingView::ModuleRow::DragHandle::DragHandle()
{
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setTooltip("Drag to reorder this tab");
}

void RoutingView::ModuleRow::DragHandle::paint(juce::Graphics& g)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const auto colour = dragStarted || isMouseOverOrDragging()
                            ? juce::Colour(0xFF79B8FF)
                            : juce::Colours::lightgrey.withAlpha(0.70f);

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

void RoutingView::ModuleRow::DragHandle::mouseDown(
    const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    dragStarted = false;
    repaint();
}

void RoutingView::ModuleRow::DragHandle::mouseDrag(
    const juce::MouseEvent& event)
{
    if (! event.mods.isLeftButtonDown())
        return;

    const auto screenPosition =
        event.getScreenPosition();

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

void RoutingView::ModuleRow::DragHandle::mouseUp(
    const juce::MouseEvent& event)
{
    if (dragStarted && onDragEnded)
        onDragEnded(event.getScreenPosition());

    dragStarted = false;
    repaint();
}

RoutingView::ModuleRow::ModuleRow()
{
    dragHandle.onDragStarted = [this](juce::Point<int> screenPosition)
    {
        if (onDragStarted)
            onDragStarted(entry.tabIndex, screenPosition);
    };

    dragHandle.onDragMoved = [this](juce::Point<int> screenPosition)
    {
        if (onDragMoved)
            onDragMoved(entry.tabIndex, screenPosition);
    };

    dragHandle.onDragEnded = [this](juce::Point<int> screenPosition)
    {
        if (onDragEnded)
            onDragEnded(entry.tabIndex, screenPosition);
    };

    addAndMakeVisible(dragHandle);

    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nameLabel);

    typeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF555555));
    typeButton.setTooltip(ButtonStyling::Tooltips::viewTab());
    typeButton.onClick = [this]
    {
        if (onSelectTab)
            onSelectTab(entry.tabIndex);
    };
    addAndMakeVisible(typeButton);

    adjustLabel.setText("Adjust\nMethod", juce::dontSendNotification);
    adjustLabel.setJustificationType(juce::Justification::centred);
    adjustLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    adjustLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(adjustLabel);

    adjustMethodEditor.onValueChanged = [this]
    {
        if (onSetPointerAdjustMethodOverride)
            onSetPointerAdjustMethodOverride(entry.tabIndex, adjustMethodEditor.getMethodOverride());
    };
    addAndMakeVisible(adjustMethodEditor);
    adjustMethodEditor.setTooltip("Scroll to change Adjust Method");

    addAndMakeVisible(midiButton);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(soloButton);
    addAndMakeVisible(infoButton);
    addAndMakeVisible(closeButton);

    midiButton.setLookAndFeel(&roundedButtonLookAndFeel);

    midiButton.setColour(juce::TextButton::buttonColourId, ButtonStyling::defaultBackground());
    midiButton.setColour(juce::TextButton::buttonOnColourId, ButtonStyling::defaultBackground());

    closeButton.setTooltip(ButtonStyling::Tooltips::closeTab());
    midiButton.setTooltip(ButtonStyling::Tooltips::midiAssignments());
    bypassButton.setTooltip(ButtonStyling::Tooltips::toggleBypass());
    soloButton.setTooltip(ButtonStyling::Tooltips::toggleSolo());
    infoButton.setTooltip(ButtonStyling::Tooltips::routingInfo());

    midiButton.onClick = [this]
    {
        if (onShowMidiAssignments)
            onShowMidiAssignments(entry.tabIndex, &midiButton);
    };

    bypassButton.onClick = [this]
    {
        if (onToggleBypass)
            onToggleBypass(entry.tabIndex);
    };

    soloButton.onClick = [this]
    {
        if (onToggleSolo)
            onToggleSolo(entry.tabIndex);
    };

    infoButton.onClick = [this]
    {
        if (onShowPluginInfo)
            onShowPluginInfo(entry.tabIndex, &infoButton);
    };

    closeButton.onClick = [this]
    {
        if (onCloseTab)
            onCloseTab(entry.tabIndex);
    };
}

RoutingView::ModuleRow::~ModuleRow()
{
    midiButton.setLookAndFeel(nullptr);
}

void RoutingView::ModuleRow::setModule(const ModuleEntry& newEntry)
{
    entry = newEntry;

    nameLabel.setText(entry.name, juce::dontSendNotification);

    const bool isInactive = entry.isMutedBySolo || (entry.isBypassed && ! entry.isSoloed);

    if (entry.needsAttention)
    {
        nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFFF6B6B));
    }
    else
    {
        nameLabel.setColour(juce::Label::textColourId,
                            isInactive ? juce::Colours::lightgrey.withAlpha(0.65f)
                                       : juce::Colours::white);
    }
    adjustMethodEditor.setMethodOverride(entry.pointerAdjustMethodOverride);
    juce::String infoTooltip = entry.routingTooltip.isNotEmpty()
                                   ? entry.routingTooltip
                                   : ButtonStyling::Tooltips::routingInfo();

    if (entry.isMutedBySolo)
        infoTooltip << "\n\nMuted by Solo";

    if (entry.needsAttention && entry.attentionMessage.isNotEmpty())
        infoTooltip << "\n\nAttention required:\n" << entry.attentionMessage;

    infoButton.setTooltip(infoTooltip + "\n\nClick for plugin diagnostics.");
    infoButton.setVisible(true);
    infoButton.setEnabled(true);

    if (entry.type == PluginSlotType::Synth)
    {
        typeButton.setButtonText(ButtonStyling::Labels::synth());
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A7BD5));
    }
    else if (entry.type == PluginSlotType::FX)
    {
        typeButton.setButtonText(ButtonStyling::Labels::fx());
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFE67E22));
    }
    else
    {
        typeButton.setButtonText(ButtonStyling::Labels::empty());
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF555555));
    }

    midiButton.setButtonText("MIDI Ch");

    midiButton.setTooltip(
        entry.midiAssignmentsTooltip.isNotEmpty()
            ? entry.midiAssignmentsTooltip
            : "MIDI Ch: None");

    bypassButton.setVisualState(! entry.isBypassed);
    soloButton.setVisualState(entry.isSoloed);

    repaint();
}

void RoutingView::ModuleRow::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    auto fill = juce::Colour(0xFF243B55);
    auto outline = juce::Colour(0xFF3A506B);

    const bool isInactive = entry.isMutedBySolo || (entry.isBypassed && ! entry.isSoloed);

    if (entry.needsAttention)
    {
        fill = juce::Colour(0xFF4A1F1F);
        outline = juce::Colour(0xFFFF6B6B).withAlpha(0.80f);
    }
    else if (isInactive)
    {
        fill = fill.darker(0.35f);
        outline = outline.withAlpha(0.55f);
    }

    g.setColour(fill);
    g.fillRoundedRectangle(area.reduced(1.0f), 8.0f);

    g.setColour(outline);
    g.drawRoundedRectangle(area.reduced(1.0f), 8.0f, 1.2f);
}

void RoutingView::ModuleRow::resized()
{
    auto area = getLocalBounds().reduced(10);
    const int buttonHeight = ButtonStyling::defaultButtonHeight();

    auto dragArea = area.removeFromLeft(20);
    dragHandle.setBounds(
        dragArea.withSizeKeepingCentre(
            dragArea.getWidth(),
            buttonHeight));
    area.removeFromLeft(8);

    typeButton.setBounds(area.removeFromLeft(80).reduced(0, 8));
    area.removeFromLeft(10);

    auto closeArea = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    closeArea = closeArea.withSizeKeepingCentre(closeArea.getWidth(), buttonHeight);
    closeButton.setBounds(closeArea);
    area.removeFromRight(8);

    auto infoArea = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    infoArea = infoArea.withSizeKeepingCentre(infoArea.getWidth(), buttonHeight);
    infoButton.setBounds(infoArea);
    area.removeFromRight(8);

    auto soloBounds = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    soloBounds = soloBounds.withSizeKeepingCentre(soloBounds.getWidth(), ButtonStyling::defaultButtonHeight());
    soloButton.setBounds(soloBounds);
    area.removeFromRight(8);

    auto bypassBounds = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    bypassBounds = bypassBounds.withSizeKeepingCentre(bypassBounds.getWidth(), ButtonStyling::defaultButtonHeight());
    bypassButton.setBounds(bypassBounds);
    area.removeFromRight(8);

    auto midiArea = area.removeFromRight(90);
    midiButton.setBounds(midiArea.reduced(0, 8));
    area.removeFromRight(10);

    auto adjustArea = area.removeFromRight(50).translated(0, -4);
    auto adjustLabelArea = adjustArea.removeFromTop(24);
    adjustArea.removeFromTop(6);
    auto adjustEditorArea = adjustArea.removeFromTop(22);

    adjustLabel.setBounds(adjustLabelArea.withSizeKeepingCentre(adjustLabelArea.getWidth(), 18));
    adjustMethodEditor.setBounds(adjustEditorArea.withSizeKeepingCentre(adjustEditorArea.getWidth(), 16));

    area.removeFromRight(12);

    nameLabel.setBounds(area);
}

RoutingView::RoutingView()
{
    titleLabel.setText("Routing", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    midiHelpLabel.setText("Choose where each tab receives audio and MIDI, and where its output is sent. Changes apply immediately.",
                          juce::dontSendNotification);
    midiHelpLabel.setJustificationType(juce::Justification::centredLeft);
    midiHelpLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    midiHelpLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    addAndMakeVisible(midiHelpLabel);

    refreshMidiButton.onClick = [this]
    {
        if (onRefreshMidiDevices)
            onRefreshMidiDevices();
    };
    addAndMakeVisible(refreshMidiButton);
    refreshMidiButton.setTooltip(ButtonStyling::Tooltips::refreshMidi());

    emptyLabel.setText("No loaded plugins.\nLoad a synth or FX in the tab view to see it here.",
                       juce::dontSendNotification);
    emptyLabel.setJustificationType(juce::Justification::centred);
    emptyLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(emptyLabel);

    viewport.setViewedComponent(&contentComponent, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setSingleStepSizes(16, 4);
    addAndMakeVisible(viewport);
}

void RoutingView::setModules(const juce::Array<ModuleEntry>& newModules)
{
    if (draggedTabIndex >= 0)
    {
        deferredModules = newModules;
        hasDeferredModules = true;
        return;
    }

    modules = newModules;
    rebuildModuleRows();

    const bool hasModules = !modules.isEmpty();
    viewport.setVisible(hasModules);
    emptyLabel.setVisible(!hasModules);

    resized();
}

void RoutingView::rebuildModuleRows()
{
    moduleRows.clear();
    contentComponent.removeAllChildren();

    for (auto& module : modules)
    {
        auto* row = moduleRows.add(new ModuleRow());
        row->setModule(module);

        row->onSetPointerAdjustMethodOverride = [this](int tabIndex, int methodOverride)
        {
            if (onSetPointerAdjustMethodOverride)
                onSetPointerAdjustMethodOverride(tabIndex, methodOverride);
        };

        row->onShowMidiAssignments = [this](int tabIndex, juce::Component* anchorComponent)
        {
            if (onShowMidiAssignments)
                onShowMidiAssignments(tabIndex, anchorComponent);
        };

        row->onShowPluginInfo = [this](int tabIndex, juce::Component* anchorComponent)
        {
            if (onShowPluginInfo)
                onShowPluginInfo(tabIndex, anchorComponent);
        };

        row->onToggleBypass = [this](int tabIndex)
        {
            if (onToggleBypass)
                onToggleBypass(tabIndex);
        };

        row->onToggleSolo = [this](int tabIndex)
        {
            if (onToggleSolo)
                onToggleSolo(tabIndex);
        };

        row->onSelectTab = [this](int tabIndex)
        {
            if (onSelectTab)
                onSelectTab(tabIndex);
        };

        row->onDragStarted = [this](int tabIndex, juce::Point<int> screenPosition)
        {
            beginModuleDrag(tabIndex, screenPosition);
        };

        row->onDragMoved = [this](int tabIndex, juce::Point<int> screenPosition)
        {
            updateModuleDrag(tabIndex, screenPosition);
        };

        row->onDragEnded = [this](int tabIndex, juce::Point<int> screenPosition)
        {
            endModuleDrag(tabIndex, screenPosition);
        };

        row->onCloseTab = [this](int tabIndex)
        {
            if (onCloseTab)
                onCloseTab(tabIndex);
        };

        contentComponent.addAndMakeVisible(row);
    }
}

void RoutingView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1B263B));
}

void RoutingView::paintOverChildren(juce::Graphics& g)
{
    if (draggedTabIndex < 0
        || dropInsertionIndex < 0
        || moduleRows.isEmpty()
        || ! viewport.isVisible())
    {
        return;
    }

    const int contentY =
        getDropIndicatorContentY();

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

    const auto indicatorColour =
        juce::Colour(0xFF4DA3FF);

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

void RoutingView::beginModuleDrag(
    int tabIndex,
    juce::Point<int> screenPosition)
{
    if (! juce::isPositiveAndBelow(
            tabIndex,
            modules.size()))
    {
        return;
    }

    draggedTabIndex = tabIndex;
    lastDragScreenPosition = screenPosition;
    dropInsertionIndex =
        getDropInsertionIndex(screenPosition);
    startTimerHz(30);
    repaint();
}

void RoutingView::updateModuleDrag(
    int tabIndex,
    juce::Point<int> screenPosition)
{
    if (tabIndex != draggedTabIndex)
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

void RoutingView::endModuleDrag(
    int tabIndex,
    juce::Point<int> screenPosition)
{
    if (tabIndex != draggedTabIndex)
        return;

    stopTimer();
    lastDragScreenPosition = screenPosition;

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

    int destinationIndex =
        droppedInsideViewport
            ? dropInsertionIndex
            : tabIndex;

    if (tabIndex < destinationIndex)
        --destinationIndex;

    destinationIndex = juce::jlimit(
        0,
        juce::jmax(0, modules.size() - 1),
        destinationIndex);

    draggedTabIndex = -1;
    dropInsertionIndex = -1;
    repaint();

    auto deferredModulesToApply =
        deferredModules;
    const bool shouldApplyDeferredModules =
        hasDeferredModules;

    deferredModules.clear();
    hasDeferredModules = false;

    if (destinationIndex == tabIndex
        || ! onMove)
    {
        if (shouldApplyDeferredModules)
        {
            juce::Component::SafePointer<RoutingView> safeThis(this);

            juce::MessageManager::callAsync(
                [safeThis, deferredModulesToApply]
                {
                    if (safeThis != nullptr)
                        safeThis->setModules(deferredModulesToApply);
                });
        }

        return;
    }

    juce::Component::SafePointer<RoutingView> safeThis(this);

    juce::MessageManager::callAsync(
        [safeThis, tabIndex, destinationIndex]
        {
            if (safeThis != nullptr && safeThis->onMove)
                safeThis->onMove(tabIndex, destinationIndex);
        });
}

void RoutingView::autoScrollForDrag(
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

    auto viewPosition =
        viewport.getViewPosition();

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

int RoutingView::getDropInsertionIndex(
    juce::Point<int> screenPosition) const
{
    if (moduleRows.isEmpty())
        return -1;

    const auto contentPosition =
        contentComponent.getLocalPoint(
            nullptr,
            screenPosition);

    for (int rowIndex = 0;
         rowIndex < moduleRows.size();
         ++rowIndex)
    {
        const auto* row = moduleRows[rowIndex];

        if (row != nullptr
            && contentPosition.y
                < row->getBounds().getCentreY())
        {
            return rowIndex;
        }
    }

    return moduleRows.size();
}

int RoutingView::getDropIndicatorContentY() const
{
    if (moduleRows.isEmpty()
        || dropInsertionIndex < 0)
    {
        return 0;
    }

    if (dropInsertionIndex <= 0)
        return moduleRows[0]->getY();

    if (dropInsertionIndex >= moduleRows.size())
        return moduleRows[moduleRows.size() - 1]->getBottom() + 4;

    const auto* previousRow =
        moduleRows[dropInsertionIndex - 1];
    const auto* nextRow =
        moduleRows[dropInsertionIndex];

    if (previousRow == nullptr || nextRow == nullptr)
        return 0;

    return (previousRow->getBottom()
            + nextRow->getY())
           / 2;
}

void RoutingView::timerCallback()
{
    if (draggedTabIndex < 0)
    {
        stopTimer();
        return;
    }

    updateModuleDrag(
        draggedTabIndex,
        lastDragScreenPosition);
}

void RoutingView::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto headerArea = area.removeFromTop(36);
    refreshMidiButton.setBounds(headerArea.removeFromRight(130));
    headerArea.removeFromRight(8);
    titleLabel.setBounds(headerArea);

    area.removeFromTop(6);
    midiHelpLabel.setBounds(area.removeFromTop(24));

    area.removeFromTop(10);

    emptyLabel.setBounds(area);
    viewport.setBounds(area);

    auto contentArea = viewport.getLocalBounds();
    constexpr int topPadding = 6;
    constexpr int bottomPadding = 2;
    int y = topPadding;
    constexpr int rowHeight = 56;
    constexpr int rowGap = 8;

    for (auto* row : moduleRows)
    {
        row->setBounds(0, y, juce::jmax(0, contentArea.getWidth() - 12), rowHeight);
        y += rowHeight + rowGap;
    }

    contentComponent.setSize(juce::jmax(0, contentArea.getWidth() - 12),
                             juce::jmax(y + bottomPadding, viewport.getHeight()));
}
