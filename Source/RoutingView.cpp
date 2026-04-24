#include "RoutingView.h"

RoutingView::ModuleRow::ModuleRow()
{
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nameLabel);

    typeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF555555));
    typeButton.onClick = [this]
    {
        if (onSelectTab)
            onSelectTab(entry.tabIndex);
    };
    addAndMakeVisible(typeButton);

    addAndMakeVisible(midiButton);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(upButton);
    addAndMakeVisible(downButton);

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

    upButton.onClick = [this]
    {
        if (onMoveUp)
            onMoveUp(entry.tabIndex);
    };

    downButton.onClick = [this]
    {
        if (onMoveDown)
            onMoveDown(entry.tabIndex);
    };
}

void RoutingView::ModuleRow::setModule(const ModuleEntry& newEntry)
{
    entry = newEntry;

    nameLabel.setText(entry.name, juce::dontSendNotification);

    if (entry.type == PluginTabComponent::SlotType::Synth)
    {
        typeButton.setButtonText("Synth");
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A7BD5));
    }
    else if (entry.type == PluginTabComponent::SlotType::FX)
    {
        typeButton.setButtonText("FX");
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFE67E22));
    }
    else
    {
        typeButton.setButtonText("Empty");
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF555555));
    }

    if (entry.midiAssignmentCount > 0)
        midiButton.setButtonText("MIDI (" + juce::String(entry.midiAssignmentCount) + ")");
    else
        midiButton.setButtonText("MIDI");

    bypassButton.setButtonText(entry.isBypassed ? "Bypassed" : "Active");
    bypassButton.setColour(juce::TextButton::buttonColourId,
                           entry.isBypassed ? juce::Colour(0xFF7F8C8D)
                                            : juce::Colour(0xFF27AE60));

    upButton.setEnabled(entry.canMoveUp);
    downButton.setEnabled(entry.canMoveDown);

    repaint();
}

void RoutingView::ModuleRow::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xFF243B55));
    g.fillRoundedRectangle(area.reduced(1.0f), 8.0f);

    g.setColour(juce::Colour(0xFF3A506B));
    g.drawRoundedRectangle(area.reduced(1.0f), 8.0f, 1.2f);
}

void RoutingView::ModuleRow::resized()
{
    auto area = getLocalBounds().reduced(10);

    typeButton.setBounds(area.removeFromLeft(80).reduced(0, 8));
    area.removeFromLeft(10);

    downButton.setBounds(area.removeFromRight(70).reduced(0, 8));
    area.removeFromRight(8);

    upButton.setBounds(area.removeFromRight(70).reduced(0, 8));
    area.removeFromRight(8);

    bypassButton.setBounds(area.removeFromRight(90).reduced(0, 8));
    area.removeFromRight(8);

    midiButton.setBounds(area.removeFromRight(90).reduced(0, 8));
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

    refreshMidiButton.onClick = [this]
    {
        if (onRefreshMidiDevices)
            onRefreshMidiDevices();
    };
    addAndMakeVisible(refreshMidiButton);

    emptyLabel.setText("No loaded plugins.\nLoad a synth or FX in the tab view to see it here.",
                       juce::dontSendNotification);
    emptyLabel.setJustificationType(juce::Justification::centred);
    emptyLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(emptyLabel);

    viewport.setViewedComponent(&contentComponent, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void RoutingView::setModules(const juce::Array<ModuleEntry>& newModules)
{
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

        row->onShowMidiAssignments = [this](int tabIndex, juce::Component* anchorComponent)
        {
            if (onShowMidiAssignments)
                onShowMidiAssignments(tabIndex, anchorComponent);
        };

        row->onToggleBypass = [this](int tabIndex)
        {
            if (onToggleBypass)
                onToggleBypass(tabIndex);
        };

        row->onSelectTab = [this](int tabIndex)
        {
            if (onSelectTab)
                onSelectTab(tabIndex);
        };

        row->onMoveUp = [this](int tabIndex)
        {
            if (onMoveUp)
                onMoveUp(tabIndex);
        };

        row->onMoveDown = [this](int tabIndex)
        {
            if (onMoveDown)
                onMoveDown(tabIndex);
        };

        contentComponent.addAndMakeVisible(row);
    }
}

void RoutingView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1B263B));
}

void RoutingView::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto headerArea = area.removeFromTop(36);
    refreshMidiButton.setBounds(headerArea.removeFromRight(130));
    headerArea.removeFromRight(8);
    titleLabel.setBounds(headerArea);

    area.removeFromTop(10);

    emptyLabel.setBounds(area);
    viewport.setBounds(area);

    auto contentArea = viewport.getLocalBounds();
    int y = 0;
    constexpr int rowHeight = 56;
    constexpr int rowGap = 8;

    for (auto* row : moduleRows)
    {
        row->setBounds(0, y, juce::jmax(0, contentArea.getWidth() - 12), rowHeight);
        y += rowHeight + rowGap;
    }

    contentComponent.setSize(juce::jmax(0, contentArea.getWidth() - 12),
                             juce::jmax(y, viewport.getHeight()));
}