#include "RoutingView.h"
#include "ButtonStyling.h"

RoutingView::ModuleRow::ModuleRow()
{
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

    addAndMakeVisible(midiButton);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(upButton);
    addAndMakeVisible(downButton);
    addAndMakeVisible(infoButton);
    addAndMakeVisible(closeButton);

    midiButton.setLookAndFeel(&roundedButtonLookAndFeel);

    midiButton.setColour(juce::TextButton::buttonColourId, ButtonStyling::defaultBackground());
    midiButton.setColour(juce::TextButton::buttonOnColourId, ButtonStyling::defaultBackground());

    closeButton.setTooltip(ButtonStyling::Tooltips::closeTab());
    midiButton.setTooltip(ButtonStyling::Tooltips::midiAssignments());
    bypassButton.setTooltip(ButtonStyling::Tooltips::toggleBypass());
    upButton.setTooltip(ButtonStyling::Tooltips::moveUp());
    downButton.setTooltip(ButtonStyling::Tooltips::moveDown());
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
    infoButton.setTooltip(entry.routingTooltip.isNotEmpty() ? entry.routingTooltip
                                                            : ButtonStyling::Tooltips::routingInfo());
    infoButton.setVisible(entry.routingTooltip.isNotEmpty());
    infoButton.setEnabled(entry.routingTooltip.isNotEmpty());

    if (entry.type == PluginTabComponent::SlotType::Synth)
    {
        typeButton.setButtonText(ButtonStyling::Labels::synth());
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A7BD5));
    }
    else if (entry.type == PluginTabComponent::SlotType::FX)
    {
        typeButton.setButtonText(ButtonStyling::Labels::fx());
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFE67E22));
    }
    else
    {
        typeButton.setButtonText(ButtonStyling::Labels::empty());
        typeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF555555));
    }

    if (entry.midiAssignmentCount > 0)
        midiButton.setButtonText(ButtonStyling::Labels::midi() + " (" + juce::String(entry.midiAssignmentCount) + ")");
    else
        midiButton.setButtonText(ButtonStyling::Labels::midi());

    bypassButton.setVisualState(! entry.isBypassed);
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
    const int buttonHeight = ButtonStyling::defaultButtonHeight();

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

    auto downBounds = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    downBounds = downBounds.withSizeKeepingCentre(downBounds.getWidth(), buttonHeight);
    downButton.setBounds(downBounds);
    area.removeFromRight(8);

    auto upBounds = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    upBounds = upBounds.withSizeKeepingCentre(upBounds.getWidth(), buttonHeight);
    upButton.setBounds(upBounds);
    area.removeFromRight(8);

    auto bypassBounds = area.removeFromRight(ButtonStyling::defaultButtonWidth());
    bypassBounds = bypassBounds.withSizeKeepingCentre(bypassBounds.getWidth(), ButtonStyling::defaultButtonHeight());
    bypassButton.setBounds(bypassBounds);
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

    midiHelpLabel.setText("NOTE: Enabled MIDI devices for new tabs follows current MIDI menu auto-assign setting. Use the MIDI button to adjust per-tab settings.",
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
