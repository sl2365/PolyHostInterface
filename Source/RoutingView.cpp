#include "RoutingView.h"

RoutingView::ModuleRow::ModuleRow()
{
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nameLabel);

    typeLabel.setJustificationType(juce::Justification::centred);
    typeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    typeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF555555));
    addAndMakeVisible(typeLabel);

    bypassButton.setEnabled(false);

    addAndMakeVisible(bypassButton);
    addAndMakeVisible(upButton);
    addAndMakeVisible(downButton);

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
        typeLabel.setText("Synth", juce::dontSendNotification);
        typeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF3A7BD5));
    }
    else if (entry.type == PluginTabComponent::SlotType::FX)
    {
        typeLabel.setText("FX", juce::dontSendNotification);
        typeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFFE67E22));
    }
    else
    {
        typeLabel.setText("Empty", juce::dontSendNotification);
        typeLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF555555));
    }

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

    typeLabel.setBounds(area.removeFromLeft(80).reduced(0, 8));
    area.removeFromLeft(10);

    downButton.setBounds(area.removeFromRight(70).reduced(0, 8));
    area.removeFromRight(8);
    upButton.setBounds(area.removeFromRight(70).reduced(0, 8));
    area.removeFromRight(8);
    bypassButton.setBounds(area.removeFromRight(90).reduced(0, 8));
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

    titleLabel.setBounds(area.removeFromTop(36));
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
