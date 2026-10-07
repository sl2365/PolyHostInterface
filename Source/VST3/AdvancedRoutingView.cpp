#include "AdvancedRoutingView.h"
#include "ButtonStyling.h"

namespace
{
    constexpr auto backgroundColour = 0xFF111A2A;
    constexpr auto canvasColour = 0xFF172338;
    constexpr auto nodeColour = 0xFF26364D;
    constexpr auto nodeBorderColour = 0xFF6B819E;
    constexpr auto inputNodeColour = 0xFF284B3A;
    constexpr auto inputNodeBorderColour = 0xFF4F8B68;
    constexpr auto effectNodeColour = 0xFF4B3726;
    constexpr auto effectNodeBorderColour = 0xFF96704A;
    constexpr auto outputNodeColour = 0xFF4A3040;
    constexpr auto outputNodeBorderColour = 0xFF98556F;
    constexpr auto portColour = 0xFF4DA3FF;
    constexpr auto cableColour = 0xFF3A7BD5;
}

class AdvancedRoutingView::Canvas::NodeControls final
    : public juce::Component
{
public:
    explicit NodeControls(AdvancedRoutingView& ownerIn)
        : owner(ownerIn),
          closeButton(ButtonStyling::Glyphs::close(),
                      ButtonStyling::defaultBackground(),
                      3.0f,
                      12.5f,
                      0),
          midiButton(ButtonStyling::defaultBackground(), 3.0f),
          bypassButton(ButtonStyling::Glyphs::activeTick(),
                       ButtonStyling::Glyphs::bypassCross(),
                       ButtonStyling::bypassActiveBackground(),
                       ButtonStyling::bypassInactiveBackground(),
                       3.0f,
                       12.5f,
                       0),
          soloButton(ButtonStyling::Glyphs::solo(),
                     ButtonStyling::Glyphs::solo(),
                     juce::Colour(0xFFB8860B),
                     ButtonStyling::defaultBackground(),
                     3.0f,
                     11.5f,
                     0),
          infoButton(ButtonStyling::Glyphs::info(),
                     ButtonStyling::defaultBackground(),
                     3.0f,
                     12.5f,
                     0)
    {
        setInterceptsMouseClicks(false, true);

        typeButton.setColour(juce::TextButton::textColourOffId,
                             juce::Colours::white);
        typeButton.setTooltip(ButtonStyling::Tooltips::viewTab());
        typeButton.setWantsKeyboardFocus(false);
        typeButton.onClick = [this]
        {
            if (owner.onSelectTab)
                owner.onSelectTab(entry.tabIndex);
        };
        addAndMakeVisible(typeButton);

        configureLabel(volumeLabel, "VOL", 8.0f,
                       juce::Colours::lightgrey);
        configureLabel(volumeValueLabel, "0", 8.0f,
                       juce::Colours::white);
        configureLabel(adjustLabel, "ADJ", 8.0f,
                       juce::Colours::lightgrey);
        configureLabel(adjustMethodValueLabel, "Global", 8.0f,
                       juce::Colours::white);
        addAndMakeVisible(volumeLabel);
        addAndMakeVisible(volumeValueLabel);
        addAndMakeVisible(adjustLabel);
        addAndMakeVisible(adjustMethodValueLabel);

        configureKnob(volumeSlider);
        volumeSlider.setRange(-12.0, 12.0, 0.1);
        volumeSlider.setDoubleClickReturnValue(true, 0.0);
        volumeSlider.setTooltip(
            "Set this tab's output volume from -12 dB to +12 dB\n"
            "Double-click to reset to 0 dB");
        volumeSlider.onValueChange = [this]
        {
            updateVolumeValueLabel();

            if (owner.onSetOutputGainDb)
            {
                owner.onSetOutputGainDb(
                    entry.tabIndex,
                    static_cast<float>(volumeSlider.getValue()));
            }
        };

        configureKnob(adjustMethodSlider);
        adjustMethodSlider.setRange(0.0, 2.0, 1.0);
        adjustMethodSlider.setDoubleClickReturnValue(true, 1.0);
        adjustMethodSlider.setMouseDragSensitivity(60);
        adjustMethodSlider.setVelocityBasedMode(false);
        adjustMethodSlider.setTooltip(
            "Set the Pointer Control adjustment method\n"
            "Left: Scroll  Centre: Global  Right: Drag\n"
            "Double-click to reset to Global");
        adjustMethodSlider.onValueChange = [this]
        {
            updateAdjustMethodValueLabel();

            if (owner.onSetPointerAdjustMethodOverride)
            {
                owner.onSetPointerAdjustMethodOverride(
                    entry.tabIndex,
                    knobValueToAdjustMethod(
                        adjustMethodSlider.getValue()));
            }
        };

        addAndMakeVisible(volumeSlider);
        addAndMakeVisible(adjustMethodSlider);
        addAndMakeVisible(midiButton);
        addAndMakeVisible(bypassButton);
        addAndMakeVisible(soloButton);
        addAndMakeVisible(infoButton);
        addAndMakeVisible(closeButton);

        closeButton.setTooltip(ButtonStyling::Tooltips::closeTab());
        midiButton.setTooltip(ButtonStyling::Tooltips::midiAssignments());
        bypassButton.setTooltip(ButtonStyling::Tooltips::toggleBypass());
        soloButton.setTooltip(ButtonStyling::Tooltips::toggleSolo());
        infoButton.setTooltip(ButtonStyling::Tooltips::routingInfo());

        midiButton.onClick = [this]
        {
            if (owner.onShowMidiAssignments)
            {
                owner.onShowMidiAssignments(entry.tabIndex,
                                            &midiButton);
            }
        };

        bypassButton.onClick = [this]
        {
            if (owner.onToggleBypass)
                owner.onToggleBypass(entry.tabIndex);
        };

        soloButton.onClick = [this]
        {
            if (owner.onToggleSolo)
                owner.onToggleSolo(entry.tabIndex);
        };

        infoButton.onClick = [this]
        {
            if (owner.onShowPluginInfo)
                owner.onShowPluginInfo(entry.tabIndex, &infoButton);
        };

        closeButton.onClick = [this]
        {
            if (owner.onCloseTab)
                owner.onCloseTab(entry.tabIndex);
        };

        for (auto* button : { static_cast<juce::Button*>(&midiButton),
                              static_cast<juce::Button*>(&bypassButton),
                              static_cast<juce::Button*>(&soloButton),
                              static_cast<juce::Button*>(&infoButton),
                              static_cast<juce::Button*>(&closeButton) })
        {
            button->setWantsKeyboardFocus(false);
        }
    }

    ~NodeControls() override
    {
        volumeSlider.setLookAndFeel(nullptr);
        adjustMethodSlider.setLookAndFeel(nullptr);
    }

    void setNode(const NodeEntry& newEntry)
    {
        entry = newEntry;

        if (entry.kind == NodeKind::Synth)
        {
            typeButton.setButtonText(ButtonStyling::Labels::synth());
            typeButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xFF3A7BD5));
        }
        else
        {
            typeButton.setButtonText(ButtonStyling::Labels::fx());
            typeButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xFFE67E22));
        }

        midiButton.setTooltip(
            entry.midiAssignmentsTooltip.isNotEmpty()
                ? entry.midiAssignmentsTooltip
                : "MIDI Ch: None");

        juce::String infoTooltip =
            entry.routingTooltip.isNotEmpty()
                ? entry.routingTooltip
                : ButtonStyling::Tooltips::routingInfo();

        if (entry.isMutedBySolo)
            infoTooltip << "\n\nMuted by Solo";

        if (entry.needsAttention
            && entry.attentionMessage.isNotEmpty())
        {
            infoTooltip << "\n\nAttention required:\n"
                        << entry.attentionMessage;
        }

        infoButton.setTooltip(
            infoTooltip + "\n\nClick for plugin diagnostics.");
        bypassButton.setVisualState(! entry.isBypassed);
        soloButton.setVisualState(entry.isSoloed);

        volumeSlider.setValue(entry.outputGainDb,
                              juce::dontSendNotification);
        adjustMethodSlider.setValue(
            adjustMethodToKnobValue(
                entry.pointerAdjustMethodOverride),
            juce::dontSendNotification);
        updateVolumeValueLabel();
        updateAdjustMethodValueLabel();
        repaint();
    }

    const juce::String& getNodeId() const noexcept
    {
        return entry.id;
    }

    void resized() override
    {
        typeButton.setBounds(4, 3, 42, 17);

        volumeLabel.setBounds(4, 20, 31, 9);
        volumeSlider.setBounds(8, 29, 23, 23);
        volumeValueLabel.setBounds(4, 52, 31, 11);

        adjustLabel.setBounds(35, 20, 38, 9);
        adjustMethodSlider.setBounds(43, 29, 23, 23);
        adjustMethodValueLabel.setBounds(34, 52, 40, 11);

        midiButton.setBounds(76, 22, 26, 18);
        bypassButton.setBounds(106, 22, 26, 18);
        soloButton.setBounds(136, 22, 26, 18);
        infoButton.setBounds(91, 44, 26, 18);
        closeButton.setBounds(121, 44, 26, 18);
    }

private:
    class CompactKnobLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider(juce::Graphics& g,
                              int x,
                              int y,
                              int width,
                              int height,
                              float sliderPosition,
                              float rotaryStartAngle,
                              float rotaryEndAngle,
                              juce::Slider& slider) override
        {
            auto dialArea = juce::Rectangle<float>(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(width),
                static_cast<float>(height)).reduced(1.0f);
            const auto centre = dialArea.getCentre();
            const float radius = 0.5f
                * juce::jmin(dialArea.getWidth(),
                             dialArea.getHeight());
            const float angle = rotaryStartAngle
                + sliderPosition
                    * (rotaryEndAngle - rotaryStartAngle);
            const float opacity = slider.isEnabled() ? 1.0f
                                                      : 0.38f;

            const auto knobColour =
                ButtonStyling::resolveBackgroundColour(
                    ButtonStyling::defaultBackground(),
                    slider.isMouseOverOrDragging(),
                    slider.isMouseButtonDown(),
                    false);

            g.setColour(knobColour.withMultipliedAlpha(opacity));
            g.fillEllipse(dialArea);

            const auto borderColour =
                ButtonStyling::outlineColour()
                    .withAlpha(0.38f)
                    .withMultipliedAlpha(opacity);

            g.setColour(borderColour);
            g.drawEllipse(dialArea.reduced(0.5f), 1.0f);

            const auto pointerEnd = centre
                + juce::Point<float>(std::sin(angle),
                                     -std::cos(angle))
                    * (radius * 0.67f);

            g.drawLine(juce::Line<float>(centre, pointerEnd), 1.4f);
        }
    };

    static void configureLabel(juce::Label& label,
                               const juce::String& text,
                               float fontHeight,
                               juce::Colour colour)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, colour);
        label.setFont(ButtonStyling::textFont(fontHeight, true));
        label.setInterceptsMouseClicks(false, false);
    }

    void configureKnob(juce::Slider& slider)
    {
        slider.setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox,
                               false,
                               0,
                               0);
        slider.setRotaryParameters(
            juce::MathConstants<float>::pi * 1.25f,
            juce::MathConstants<float>::pi * 2.75f,
            true);
        slider.setLookAndFeel(&knobLookAndFeel);
        slider.setWantsKeyboardFocus(false);
    }

    static double adjustMethodToKnobValue(int methodOverride)
    {
        switch (methodOverride)
        {
            case 1:  return 0.0;
            case 2:  return 2.0;
            case 0:
            default: return 1.0;
        }
    }

    static int knobValueToAdjustMethod(double knobValue)
    {
        switch (juce::jlimit(0,
                             2,
                             juce::roundToInt(knobValue)))
        {
            case 0:  return 1;
            case 2:  return 2;
            case 1:
            default: return 0;
        }
    }

    void updateVolumeValueLabel()
    {
        const double value = volumeSlider.getValue();
        const int wholeValue = juce::roundToInt(value);
        const bool isWholeValue =
            std::abs(value - static_cast<double>(wholeValue))
                < 0.001;
        juce::String displayValue =
            isWholeValue ? juce::String(wholeValue)
                         : juce::String(value, 1);

        if (value > 0.0)
            displayValue = "+" + displayValue;

        volumeValueLabel.setText(displayValue,
                                 juce::dontSendNotification);
    }

    void updateAdjustMethodValueLabel()
    {
        juce::String displayValue;

        switch (knobValueToAdjustMethod(
            adjustMethodSlider.getValue()))
        {
            case 1:  displayValue = "Scroll"; break;
            case 2:  displayValue = "Drag";   break;
            case 0:
            default: displayValue = "Global"; break;
        }

        adjustMethodValueLabel.setText(
            displayValue,
            juce::dontSendNotification);
    }

    AdvancedRoutingView& owner;
    NodeEntry entry;
    CompactKnobLookAndFeel knobLookAndFeel;
    ButtonStyling::TypeBadgeButton typeButton;
    juce::Label volumeLabel;
    juce::Slider volumeSlider;
    juce::Label volumeValueLabel;
    juce::Label adjustLabel;
    juce::Slider adjustMethodSlider;
    juce::Label adjustMethodValueLabel;
    ButtonStyling::SmallIconButton closeButton;
    ButtonStyling::MidiConnectorIconButton midiButton;
    ButtonStyling::StatusIconButton bypassButton;
    ButtonStyling::StatusIconButton soloButton;
    ButtonStyling::SmallIconButton infoButton;
};

class AdvancedRoutingView::Canvas::DragOverlay final
    : public juce::Component
{
public:
    explicit DragOverlay(Canvas& ownerIn)
        : owner(ownerIn)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        owner.paintDraggedNodeOverlay(g);
    }

private:
    Canvas& owner;
};

AdvancedRoutingView::Canvas::Canvas(AdvancedRoutingView& ownerIn)
    : owner(ownerIn),
      dragOverlay(std::make_unique<DragOverlay>(*this))
{
    setWantsKeyboardFocus(true);
    addChildComponent(dragOverlay.get());
    setSize(1800, 1200);
}

AdvancedRoutingView::Canvas::~Canvas() = default;

void AdvancedRoutingView::Canvas::resized()
{
    dragOverlay->setBounds(getLocalBounds());
}

void AdvancedRoutingView::Canvas::setModel(
    const juce::Array<NodeEntry>& nodes,
    const juce::Array<AdvancedRoutingConnection>& connections)
{
    nodeEntries = nodes;
    connectionEntries = connections;
    selectedConnectionIndex = -1;
    selectedNodeIndex = -1;
    hoveredConnectionIndex = -1;
    reconnectConnectionIndex = -1;
    reconnectEnd = ReconnectEnd::None;
    draggedNodeIndex = -1;
    hasValidDragPosition = false;
    dragOverlay->setVisible(false);

    rebuildNodeControls();
    updateCanvasSizeFromNodes();
    repaint();
}

void AdvancedRoutingView::Canvas::rebuildNodeControls()
{
    nodeControls.clear();

    for (const auto& node : nodeEntries)
    {
        if (node.kind != NodeKind::Synth
            && node.kind != NodeKind::Effect)
        {
            continue;
        }

        auto* controls = nodeControls.add(
            new NodeControls(owner));
        controls->setNode(node);
        addAndMakeVisible(controls);
    }

    updateNodeControlBounds();
}

void AdvancedRoutingView::Canvas::updateNodeControlBounds()
{
    for (auto* controls : nodeControls)
    {
        const int nodeIndex = findNodeIndex(
            controls->getNodeId());

        if (juce::isPositiveAndBelow(nodeIndex,
                                     nodeEntries.size()))
        {
            controls->setBounds(
                getNodeBounds(
                    nodeEntries.getReference(nodeIndex)));
        }
    }
}

void AdvancedRoutingView::Canvas::updateCanvasSizeFromNodes(
    bool allowShrink)
{
    int requiredWidth = minimumCanvasWidth;
    int requiredHeight = minimumCanvasHeight;

    for (const auto& node : nodeEntries)
    {
        const auto bounds = getNodeBounds(node);
        requiredWidth = juce::jmax(requiredWidth,
                                   bounds.getRight()
                                       + canvasContentPadding);
        requiredHeight = juce::jmax(requiredHeight,
                                    bounds.getBottom()
                                        + canvasContentPadding);
    }

    if (! allowShrink)
    {
        requiredWidth = juce::jmax(requiredWidth, getWidth());
        requiredHeight = juce::jmax(requiredHeight, getHeight());
    }

    if (getWidth() != requiredWidth
        || getHeight() != requiredHeight)
    {
        setSize(requiredWidth, requiredHeight);
    }
}

juce::Point<int> AdvancedRoutingView::Canvas::snapPositionToGrid(
    juce::Point<int> position)
{
    auto snapValue = [](int value)
    {
        return ((value + gridSnapSize / 2) / gridSnapSize)
               * gridSnapSize;
    };

    return {
        juce::jmax(minimumNodePosition, snapValue(position.x)),
        juce::jmax(minimumNodePosition, snapValue(position.y))
    };
}

juce::Point<int> AdvancedRoutingView::Canvas::constrainNodePosition(
    int movingNodeIndex,
    juce::Point<int> desiredPosition) const
{
    if (! juce::isPositiveAndBelow(movingNodeIndex,
                                   nodeEntries.size()))
    {
        return snapPositionToGrid(desiredPosition);
    }

    const auto& movingNode = nodeEntries.getReference(movingNodeIndex);
    const auto currentBounds = getNodeBounds(movingNode);
    const juce::Point<int> movingSize(currentBounds.getWidth(),
                                      currentBounds.getHeight());
    juce::Array<juce::Rectangle<int>> obstacles;
    bool outputModuleAdded = false;

    for (int i = 0; i < nodeEntries.size(); ++i)
    {
        const auto& node = nodeEntries.getReference(i);

        if (node.kind == NodeKind::Output)
        {
            if (movingNode.kind == NodeKind::Output
                || outputModuleAdded)
                continue;

            obstacles.add(getOutputModuleBounds());
            outputModuleAdded = true;
            continue;
        }

        if (i != movingNodeIndex)
            obstacles.add(getNodeBounds(node));
    }

    const auto position = snapPositionToGrid(desiredPosition);
    const juce::Rectangle<int> movingBounds(position, movingSize);
    auto bestPosition = position;
    int bestSnapDistance = moduleSnapDistance + 1;

    for (const auto& obstacle : obstacles)
    {
        const bool verticallyAligned =
            movingBounds.getBottom() > obstacle.getY()
            && movingBounds.getY() < obstacle.getBottom();

        if (verticallyAligned)
        {
            const int leftOfObstacle =
                snapPositionToGrid({
                    obstacle.getX() - moduleGap - movingSize.x,
                    position.y }).x;
            const int rightOfObstacle =
                snapPositionToGrid({
                    obstacle.getRight() + moduleGap,
                    position.y }).x;
            const int leftDistance =
                std::abs(position.x - leftOfObstacle);
            const int rightDistance =
                std::abs(position.x - rightOfObstacle);

            if (leftOfObstacle >= minimumNodePosition
                && leftDistance < bestSnapDistance)
            {
                const juce::Point<int> candidate(leftOfObstacle,
                                                 position.y);
                if (! isNodePositionOverlapping(movingNodeIndex,
                                                candidate))
                {
                    bestSnapDistance = leftDistance;
                    bestPosition = candidate;
                }
            }

            if (rightDistance < bestSnapDistance)
            {
                const juce::Point<int> candidate(rightOfObstacle,
                                                 position.y);
                if (! isNodePositionOverlapping(movingNodeIndex,
                                                candidate))
                {
                    bestSnapDistance = rightDistance;
                    bestPosition = candidate;
                }
            }
        }

        const bool horizontallyAligned =
            movingBounds.getRight() > obstacle.getX()
            && movingBounds.getX() < obstacle.getRight();

        if (horizontallyAligned)
        {
            const int aboveObstacle =
                snapPositionToGrid({
                    position.x,
                    obstacle.getY() - moduleGap - movingSize.y }).y;
            const int belowObstacle =
                snapPositionToGrid({
                    position.x,
                    obstacle.getBottom() + moduleGap }).y;
            const int aboveDistance =
                std::abs(position.y - aboveObstacle);
            const int belowDistance =
                std::abs(position.y - belowObstacle);

            if (aboveObstacle >= minimumNodePosition
                && aboveDistance < bestSnapDistance)
            {
                const juce::Point<int> candidate(position.x,
                                                 aboveObstacle);
                if (! isNodePositionOverlapping(movingNodeIndex,
                                                candidate))
                {
                    bestSnapDistance = aboveDistance;
                    bestPosition = candidate;
                }
            }

            if (belowDistance < bestSnapDistance)
            {
                const juce::Point<int> candidate(position.x,
                                                 belowObstacle);
                if (! isNodePositionOverlapping(movingNodeIndex,
                                                candidate))
                {
                    bestSnapDistance = belowDistance;
                    bestPosition = candidate;
                }
            }
        }
    }

    return bestPosition;
}

bool AdvancedRoutingView::Canvas::isNodePositionOverlapping(
    int movingNodeIndex,
    juce::Point<int> position) const
{
    if (! juce::isPositiveAndBelow(movingNodeIndex,
                                   nodeEntries.size()))
    {
        return false;
    }

    const auto& movingNode = nodeEntries.getReference(movingNodeIndex);
    const auto currentBounds = getNodeBounds(movingNode);
    const juce::Rectangle<int> movingBounds(
        position.x,
        position.y,
        currentBounds.getWidth(),
        currentBounds.getHeight());
    bool outputModuleChecked = false;

    for (int i = 0; i < nodeEntries.size(); ++i)
    {
        const auto& node = nodeEntries.getReference(i);

        if (node.kind == NodeKind::Output)
        {
            if (movingNode.kind == NodeKind::Output
                || outputModuleChecked)
            {
                continue;
            }

            outputModuleChecked = true;
            if (movingBounds.intersects(getOutputModuleBounds()))
                return true;

            continue;
        }

        if (i != movingNodeIndex
            && movingBounds.intersects(getNodeBounds(node)))
        {
            return true;
        }
    }

    return false;
}

juce::Rectangle<int>
AdvancedRoutingView::Canvas::getNodeBounds(const NodeEntry& node) const
{
    if (node.kind == NodeKind::Output)
        return getOutputModuleBounds();

    return { node.position.x, node.position.y, nodeWidth, nodeHeight };
}

juce::Rectangle<int>
AdvancedRoutingView::Canvas::getOutputModuleBounds() const
{
    const int anchorIndex = findOutputAnchorIndex();

    if (juce::isPositiveAndBelow(anchorIndex, nodeEntries.size()))
    {
        const auto position =
            nodeEntries.getReference(anchorIndex).position;
        return { position.x,
                 position.y,
                 outputModuleWidth,
                 outputModuleHeight };
    }

    return {};
}

juce::Point<float>
AdvancedRoutingView::Canvas::getInputPort(const NodeEntry& node) const
{
    const auto bounds = getNodeBounds(node).toFloat();

    if (node.kind == NodeKind::Output
        && juce::isPositiveAndBelow(node.outputBusIndex,
                                    outputBusCount))
    {
        return {
            bounds.getX(),
            bounds.getY()
                + static_cast<float>(outputHeaderHeight)
                + (static_cast<float>(node.outputBusIndex) + 0.5f)
                    * static_cast<float>(outputRowHeight)
        };
    }

    return { bounds.getX(), bounds.getCentreY() };
}

juce::Point<float>
AdvancedRoutingView::Canvas::getOutputPort(const NodeEntry& node) const
{
    const auto bounds = getNodeBounds(node).toFloat();
    return { bounds.getRight(), bounds.getCentreY() };
}

int AdvancedRoutingView::Canvas::findNodeIndex(
    const juce::String& id) const
{
    for (int i = 0; i < nodeEntries.size(); ++i)
        if (nodeEntries.getReference(i).id == id)
            return i;

    return -1;
}

int AdvancedRoutingView::Canvas::findOutputAnchorIndex() const
{
    for (int i = 0; i < nodeEntries.size(); ++i)
    {
        const auto& node = nodeEntries.getReference(i);
        if (node.kind == NodeKind::Output
            && node.outputBusIndex == 0)
        {
            return i;
        }
    }

    for (int i = 0; i < nodeEntries.size(); ++i)
        if (nodeEntries.getReference(i).kind == NodeKind::Output)
            return i;

    return -1;
}

juce::Path AdvancedRoutingView::Canvas::makeCablePath(
    juce::Point<float> start,
    juce::Point<float> end)
{
    juce::Path path;
    const float controlDistance =
        juce::jmax(48.0f, std::abs(end.x - start.x) * 0.45f);

    path.startNewSubPath(start);
    path.cubicTo(start.x + controlDistance,
                 start.y,
                 end.x - controlDistance,
                 end.y,
                 end.x,
                 end.y);
    return path;
}

juce::Path AdvancedRoutingView::Canvas::makeConnectionPath(
    const AdvancedRoutingConnection& connection) const
{
    const int sourceIndex = findNodeIndex(connection.sourceNodeId);
    const int destinationIndex =
        findNodeIndex(connection.destinationNodeId);

    if (! juce::isPositiveAndBelow(sourceIndex, nodeEntries.size())
        || ! juce::isPositiveAndBelow(destinationIndex,
                                      nodeEntries.size()))
    {
        return {};
    }

    return makeCablePath(
        getOutputPort(nodeEntries.getReference(sourceIndex)),
        getInputPort(nodeEntries.getReference(destinationIndex)));
}

int AdvancedRoutingView::Canvas::findNodeAt(
    juce::Point<float> position) const
{
    const int outputAnchorIndex = findOutputAnchorIndex();
    if (juce::isPositiveAndBelow(outputAnchorIndex,
                                 nodeEntries.size())
        && getOutputModuleBounds().toFloat().contains(position))
    {
        return outputAnchorIndex;
    }

    for (int i = nodeEntries.size(); --i >= 0;)
    {
        const auto& node = nodeEntries.getReference(i);
        if (node.kind != NodeKind::Output
            && getNodeBounds(node)
                .toFloat().contains(position))
        {
            return i;
        }
    }

    return -1;
}

int AdvancedRoutingView::Canvas::findInputPortAt(
    juce::Point<float> position) const
{
    for (int i = 0; i < nodeEntries.size(); ++i)
    {
        const auto& node = nodeEntries.getReference(i);
        const float hitRadius =
            node.kind == NodeKind::Output ? 9.0f : 13.0f;

        if (node.acceptsInput
            && getInputPort(node).getDistanceFrom(position) <= hitRadius)
            return i;
    }

    return -1;
}

int AdvancedRoutingView::Canvas::findOutputPortAt(
    juce::Point<float> position) const
{
    for (int i = 0; i < nodeEntries.size(); ++i)
    {
        const auto& node = nodeEntries.getReference(i);
        if (node.producesOutput
            && getOutputPort(node).getDistanceFrom(position) <= 13.0f)
            return i;
    }

    return -1;
}

int AdvancedRoutingView::Canvas::findConnectionAt(
    juce::Point<float> position) const
{
    for (int i = connectionEntries.size(); --i >= 0;)
    {
        juce::Path hitPath;
        juce::PathStrokeType(10.0f).createStrokedPath(
            hitPath,
            makeConnectionPath(connectionEntries.getReference(i)));

        if (hitPath.contains(position))
            return i;
    }

    return -1;
}

juce::Colour AdvancedRoutingView::Canvas::getCableBaseColour()
{
    return juce::Colour(cableColour);
}

bool AdvancedRoutingView::Canvas::isConnectionAttachedToSelectedNode(
    const AdvancedRoutingConnection& connection) const
{
    if (! juce::isPositiveAndBelow(selectedNodeIndex,
                                   nodeEntries.size()))
    {
        return false;
    }

    const auto& selectedNode =
        nodeEntries.getReference(selectedNodeIndex);

    if (selectedNode.kind != NodeKind::Output)
    {
        return connection.sourceNodeId == selectedNode.id
               || connection.destinationNodeId == selectedNode.id;
    }

    const int destinationIndex =
        findNodeIndex(connection.destinationNodeId);
    return juce::isPositiveAndBelow(destinationIndex,
                                    nodeEntries.size())
           && nodeEntries.getReference(destinationIndex).kind
                  == NodeKind::Output;
}

void AdvancedRoutingView::Canvas::paintNode(
    juce::Graphics& g,
    const NodeEntry& node) const
{
    const auto bounds = getNodeBounds(node).toFloat();
    juce::Colour fill(nodeColour);
    juce::Colour border(nodeBorderColour);

    if (node.kind == NodeKind::AudioInput)
    {
        fill = juce::Colour(inputNodeColour);
        border = juce::Colour(inputNodeBorderColour);
    }
    else if (node.kind == NodeKind::Effect)
    {
        fill = juce::Colour(effectNodeColour);
        border = juce::Colour(effectNodeBorderColour);
    }

    const bool isPluginNode =
        node.kind == NodeKind::Synth
        || node.kind == NodeKind::Effect;
    const bool isInactive =
        node.isMutedBySolo
        || (node.isBypassed && ! node.isSoloed);

    if (isPluginNode && node.needsAttention)
    {
        fill = juce::Colour(0xFF4A1F1F);
        border = juce::Colour(0xFFFF6B6B).withAlpha(0.80f);
    }
    else if (isPluginNode && isInactive)
    {
        fill = fill.darker(0.35f);
        border = border.withAlpha(0.55f);
    }

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 8.0f, 1.5f);

    if (isPluginNode)
    {
        g.setColour(
            node.needsAttention
                ? juce::Colour(0xFFFF6B6B)
                : (isInactive
                       ? juce::Colours::lightgrey.withAlpha(0.65f)
                       : juce::Colours::white));
        g.setFont(juce::Font(
            juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawFittedText(
            node.label,
            juce::Rectangle<float>(bounds.getX() + 50.0f,
                                   bounds.getY() + 3.0f,
                                   114.0f,
                                   17.0f).toNearestInt(),
            juce::Justification::centredLeft,
            1);
    }
    else
    {
        auto textBounds = bounds.reduced(14.0f, 9.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(
            juce::FontOptions(15.0f, juce::Font::bold)));
        g.drawFittedText(
            node.label,
            textBounds.removeFromTop(24.0f).toNearestInt(),
            juce::Justification::centredLeft,
            1);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(juce::FontOptions(11.5f)));
        g.drawFittedText(node.subtitle,
                         textBounds.toNearestInt(),
                         juce::Justification::centredLeft,
                         1);
    }

    if (node.acceptsInput)
    {
        g.setColour(juce::Colour(portColour));
        const auto port = getInputPort(node);
        g.fillEllipse(port.x - portRadius,
                      port.y - portRadius,
                      portRadius * 2.0f,
                      portRadius * 2.0f);
    }

    if (node.producesOutput)
    {
        g.setColour(juce::Colour(portColour));
        const auto port = getOutputPort(node);
        g.fillEllipse(port.x - portRadius,
                      port.y - portRadius,
                      portRadius * 2.0f,
                      portRadius * 2.0f);
    }
}

void AdvancedRoutingView::Canvas::paintOutputModule(
    juce::Graphics& g) const
{
    const auto outputBounds = getOutputModuleBounds().toFloat();

    if (outputBounds.isEmpty())
        return;

    g.setColour(juce::Colour(outputNodeColour));
    g.fillRoundedRectangle(outputBounds, 8.0f);
    g.setColour(juce::Colour(outputNodeBorderColour));
    g.drawRoundedRectangle(outputBounds, 8.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(13.5f,
                                           juce::Font::bold)));
    g.drawFittedText(
        "OUTPUTS",
        juce::Rectangle<float>(outputBounds.getX() + 14.0f,
                               outputBounds.getY() + 4.0f,
                               outputBounds.getWidth() - 28.0f,
                               24.0f).toNearestInt(),
        juce::Justification::centredLeft,
        1);

    g.setColour(
        juce::Colour(outputNodeBorderColour).withAlpha(0.45f));
    g.drawHorizontalLine(
        juce::roundToInt(outputBounds.getY()
                         + outputHeaderHeight),
        outputBounds.getX() + 10.0f,
        outputBounds.getRight() - 10.0f);

    for (int busIndex = 0;
         busIndex < outputBusCount;
         ++busIndex)
    {
        const NodeEntry* outputNode = nullptr;

        for (const auto& node : nodeEntries)
        {
            if (node.kind == NodeKind::Output
                && node.outputBusIndex == busIndex)
            {
                outputNode = &node;
                break;
            }
        }

        if (outputNode == nullptr)
            continue;

        const auto port = getInputPort(*outputNode);
        g.setColour(
            outputNode->available
                ? juce::Colour(portColour)
                : juce::Colour(outputNodeBorderColour)
                      .withAlpha(0.45f));
        g.fillEllipse(port.x - portRadius,
                      port.y - portRadius,
                      portRadius * 2.0f,
                      portRadius * 2.0f);

        g.setColour(
            outputNode->available
                ? (busIndex == 0
                       ? juce::Colours::white
                       : juce::Colours::lightgrey)
                : juce::Colour(outputNodeBorderColour)
                      .withAlpha(0.55f));
        g.setFont(juce::Font(
            juce::FontOptions(busIndex == 0 ? 11.5f : 10.5f,
                              busIndex == 0
                                  ? juce::Font::bold
                                  : juce::Font::plain)));
        g.drawFittedText(
            outputNode->label,
            juce::Rectangle<float>(
                outputBounds.getX() + 16.0f,
                port.y
                    - static_cast<float>(outputRowHeight) * 0.5f,
                outputBounds.getWidth() - 28.0f,
                static_cast<float>(outputRowHeight)).toNearestInt(),
            juce::Justification::centredLeft,
            1);

        if (busIndex < outputBusCount - 1)
        {
            g.setColour(
                juce::Colour(outputNodeBorderColour)
                    .withAlpha(0.16f));
            g.drawHorizontalLine(
                juce::roundToInt(port.y
                                 + outputRowHeight * 0.5f),
                outputBounds.getX() + 12.0f,
                outputBounds.getRight() - 10.0f);
        }
    }
}

void AdvancedRoutingView::Canvas::paintDraggedNodeOverlay(
    juce::Graphics& g)
{
    if (! juce::isPositiveAndBelow(draggedNodeIndex,
                                   nodeEntries.size()))
    {
        return;
    }

    const auto& draggedNode =
        nodeEntries.getReference(draggedNodeIndex);

    if (draggedNode.kind == NodeKind::Output)
        paintOutputModule(g);
    else
        paintNode(g, draggedNode);

    if (draggedNode.kind != NodeKind::Synth
        && draggedNode.kind != NodeKind::Effect)
    {
        return;
    }

    for (auto* controls : nodeControls)
    {
        if (controls->getNodeId() != draggedNode.id)
            continue;

        juce::Graphics::ScopedSaveState savedState(g);
        g.addTransform(juce::AffineTransform::translation(
            static_cast<float>(controls->getX()),
            static_cast<float>(controls->getY())));
        controls->paintEntireComponent(g, true);
        break;
    }
}

void AdvancedRoutingView::Canvas::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(canvasColour));

    const auto clip = g.getClipBounds();
    const int firstMinorX =
        (clip.getX() / gridSnapSize) * gridSnapSize;
    const int firstMinorY =
        (clip.getY() / gridSnapSize) * gridSnapSize;
    const int firstMajorX =
        (clip.getX() / majorGridSize) * majorGridSize;
    const int firstMajorY =
        (clip.getY() / majorGridSize) * majorGridSize;

    g.setColour(juce::Colour(0x0C6B819E));
    for (int x = firstMinorX;
         x <= clip.getRight();
         x += gridSnapSize)
    {
        g.drawVerticalLine(x,
                           static_cast<float>(clip.getY()),
                           static_cast<float>(clip.getBottom()));
    }
    for (int y = firstMinorY;
         y <= clip.getBottom();
         y += gridSnapSize)
    {
        g.drawHorizontalLine(y,
                             static_cast<float>(clip.getX()),
                             static_cast<float>(clip.getRight()));
    }

    g.setColour(juce::Colour(0x246B819E));
    for (int x = firstMajorX;
         x <= clip.getRight();
         x += majorGridSize)
    {
        g.drawVerticalLine(x,
                           static_cast<float>(clip.getY()),
                           static_cast<float>(clip.getBottom()));
    }
    for (int y = firstMajorY;
         y <= clip.getBottom();
         y += majorGridSize)
    {
        g.drawHorizontalLine(y,
                             static_cast<float>(clip.getX()),
                             static_cast<float>(clip.getRight()));
    }

    for (int i = 0; i < connectionEntries.size(); ++i)
    {
        if (i == reconnectConnectionIndex)
            continue;

        const auto& connection = connectionEntries.getReference(i);
        const bool isSelected =
            i == selectedConnectionIndex
            || isConnectionAttachedToSelectedNode(connection);
        const bool isHovered = i == hoveredConnectionIndex;
        auto colour = getCableBaseColour();
        float strokeWidth = 2.5f;

        if (isHovered)
        {
            colour = colour.brighter(0.22f);
            strokeWidth = 4.2f;
        }
        else if (isSelected)
        {
            strokeWidth = 3.8f;
        }
        else
        {
            colour = colour.darker(0.60f);
        }

        g.setColour(colour);
        g.strokePath(makeConnectionPath(connection),
                     juce::PathStrokeType(
                         strokeWidth,
                         juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded));
    }

    if (juce::isPositiveAndBelow(cableSourceNodeIndex,
                                 nodeEntries.size()))
    {
        const auto start =
            getOutputPort(nodeEntries.getReference(cableSourceNodeIndex));
        g.setColour(getCableBaseColour());
        g.strokePath(makeCablePath(start, cableEnd),
                     juce::PathStrokeType(3.0f,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }

    if (juce::isPositiveAndBelow(reconnectConnectionIndex,
                                 connectionEntries.size()))
    {
        const auto& connection =
            connectionEntries.getReference(reconnectConnectionIndex);
        const int sourceIndex = findNodeIndex(connection.sourceNodeId);
        const int destinationIndex =
            findNodeIndex(connection.destinationNodeId);

        if (juce::isPositiveAndBelow(sourceIndex, nodeEntries.size())
            && juce::isPositiveAndBelow(destinationIndex,
                                        nodeEntries.size()))
        {
            auto start = getOutputPort(
                nodeEntries.getReference(sourceIndex));
            auto end = getInputPort(
                nodeEntries.getReference(destinationIndex));

            if (reconnectEnd == ReconnectEnd::Source)
                start = cableEnd;
            else if (reconnectEnd == ReconnectEnd::Destination)
                end = cableEnd;

            g.setColour(getCableBaseColour());
            g.strokePath(makeCablePath(start, end),
                         juce::PathStrokeType(
                             3.8f,
                             juce::PathStrokeType::curved,
                             juce::PathStrokeType::rounded));
        }
    }

    for (int nodeIndex = 0;
         nodeIndex < nodeEntries.size();
         ++nodeIndex)
    {
        const auto& node = nodeEntries.getReference(nodeIndex);

        if (node.kind == NodeKind::Output)
            continue;

        if (nodeIndex != draggedNodeIndex)
            paintNode(g, node);
    }

    paintOutputModule(g);
}

void AdvancedRoutingView::Canvas::mouseMove(
    const juce::MouseEvent& event)
{
    const int newHoveredConnectionIndex =
        findNodeAt(event.position) >= 0
            ? -1
            : findConnectionAt(event.position);

    if (hoveredConnectionIndex != newHoveredConnectionIndex)
    {
        hoveredConnectionIndex = newHoveredConnectionIndex;
        repaint();
    }
}

void AdvancedRoutingView::Canvas::mouseExit(
    const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (hoveredConnectionIndex >= 0)
    {
        hoveredConnectionIndex = -1;
        repaint();
    }
}

void AdvancedRoutingView::Canvas::mouseDown(
    const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto position = event.position;
    const int nodeIndexAtPosition = findNodeAt(position);
    const int connectionIndex = findConnectionAt(position);

    if (event.mods.isPopupMenu())
    {
        if (nodeIndexAtPosition < 0
            && juce::isPositiveAndBelow(connectionIndex,
                                     connectionEntries.size()))
        {
            const auto connection =
                connectionEntries.getReference(connectionIndex);
            if (owner.onRemoveConnection)
                owner.onRemoveConnection(connection.sourceNodeId,
                                         connection.destinationNodeId);
        }
        return;
    }

    const int outputPortIndex = findOutputPortAt(position);
    cableSourceNodeIndex =
        outputPortIndex >= 0
        && (nodeIndexAtPosition < 0
            || outputPortIndex == nodeIndexAtPosition)
            ? outputPortIndex
            : -1;

    if (cableSourceNodeIndex >= 0)
    {
        selectedConnectionIndex = -1;
        selectedNodeIndex = -1;
        hoveredConnectionIndex = -1;
        cableEnd = position;
        repaint();
        return;
    }

    if (nodeIndexAtPosition >= 0)
    {
        draggedNodeIndex = nodeIndexAtPosition;
        selectedConnectionIndex = -1;
        selectedNodeIndex = draggedNodeIndex;
        hoveredConnectionIndex = -1;
        const auto& node = nodeEntries.getReference(draggedNodeIndex);
        dragOffset = position.toInt()
                     - getNodeBounds(node).getPosition();
        dragStartPosition = node.position;
        lastValidDragPosition = node.position;
        hasValidDragPosition = ! isNodePositionOverlapping(
            draggedNodeIndex,
            node.position);
        dragOverlay->setVisible(true);
        dragOverlay->toFront(false);
        dragOverlay->repaint();
        repaint();
        return;
    }

    if (juce::isPositiveAndBelow(connectionIndex,
                                 connectionEntries.size()))
    {
        const auto& connection =
            connectionEntries.getReference(connectionIndex);
        const int sourceIndex = findNodeIndex(connection.sourceNodeId);
        const int destinationIndex =
            findNodeIndex(connection.destinationNodeId);

        if (juce::isPositiveAndBelow(sourceIndex, nodeEntries.size())
            && juce::isPositiveAndBelow(destinationIndex,
                                        nodeEntries.size()))
        {
            const auto sourcePosition = getOutputPort(
                nodeEntries.getReference(sourceIndex));
            const auto destinationPosition = getInputPort(
                nodeEntries.getReference(destinationIndex));

            reconnectConnectionIndex = connectionIndex;
            reconnectEnd =
                position.getDistanceFrom(sourcePosition)
                        <= position.getDistanceFrom(destinationPosition)
                    ? ReconnectEnd::Source
                    : ReconnectEnd::Destination;
            cableEnd = reconnectEnd == ReconnectEnd::Source
                           ? sourcePosition
                           : destinationPosition;
            selectedConnectionIndex = connectionIndex;
            selectedNodeIndex = -1;
            hoveredConnectionIndex = -1;
            repaint();
            return;
        }
    }

    selectedConnectionIndex = -1;
    selectedNodeIndex = -1;
    hoveredConnectionIndex = -1;

    repaint();
}

void AdvancedRoutingView::Canvas::mouseDrag(
    const juce::MouseEvent& event)
{
    if (reconnectEnd != ReconnectEnd::None)
    {
        cableEnd = event.position;
        hoveredConnectionIndex = -1;
        repaint();
        return;
    }

    if (cableSourceNodeIndex >= 0)
    {
        hoveredConnectionIndex = -1;
        cableEnd = event.position;
        repaint();
        return;
    }

    if (! juce::isPositiveAndBelow(draggedNodeIndex,
                                   nodeEntries.size()))
    {
        return;
    }

    auto& node = nodeEntries.getReference(draggedNodeIndex);
    hoveredConnectionIndex = -1;
    const auto requestedPosition =
        event.position.toInt() - dragOffset;
    const auto newPosition = snapPositionToGrid(
        requestedPosition);
    const auto snappedPosition = constrainNodePosition(
        draggedNodeIndex,
        newPosition);
    const bool snappedPositionIsValid =
        ! isNodePositionOverlapping(draggedNodeIndex,
                                    snappedPosition);

    if (node.kind == NodeKind::Output)
    {
        for (auto& outputNode : nodeEntries)
            if (outputNode.kind == NodeKind::Output)
                outputNode.position = newPosition;
    }
    else
    {
        node.position = newPosition;
    }

    if (snappedPositionIsValid)
    {
        lastValidDragPosition = snappedPosition;
        hasValidDragPosition = true;
    }

    updateNodeControlBounds();
    updateCanvasSizeFromNodes(false);
    dragOverlay->repaint();
    repaint();
}

void AdvancedRoutingView::Canvas::mouseUp(
    const juce::MouseEvent& event)
{
    if (reconnectEnd != ReconnectEnd::None
        && juce::isPositiveAndBelow(reconnectConnectionIndex,
                                    connectionEntries.size()))
    {
        const auto oldConnection =
            connectionEntries.getReference(reconnectConnectionIndex);
        const auto endBeingMoved = reconnectEnd;
        int targetIndex = -1;

        if (endBeingMoved == ReconnectEnd::Source)
            targetIndex = findOutputPortAt(event.position);
        else
            targetIndex = findInputPortAt(event.position);

        reconnectConnectionIndex = -1;
        reconnectEnd = ReconnectEnd::None;

        if (juce::isPositiveAndBelow(targetIndex, nodeEntries.size())
            && owner.onReconnectConnection)
        {
            juce::String errorMessage;
            const auto& target = nodeEntries.getReference(targetIndex);
            const auto newSourceNodeId =
                endBeingMoved == ReconnectEnd::Source
                    ? target.id
                    : oldConnection.sourceNodeId;
            const auto newDestinationNodeId =
                endBeingMoved == ReconnectEnd::Destination
                    ? target.id
                    : oldConnection.destinationNodeId;

            if (! owner.onReconnectConnection(
                    oldConnection.sourceNodeId,
                    oldConnection.destinationNodeId,
                    newSourceNodeId,
                    newDestinationNodeId,
                    errorMessage)
                && errorMessage.isNotEmpty()
                && owner.onStatusMessage)
            {
                owner.onStatusMessage(errorMessage);
            }
        }

        repaint();
        return;
    }

    if (cableSourceNodeIndex >= 0)
    {
        const int destinationIndex =
            findInputPortAt(event.position);

        if (juce::isPositiveAndBelow(destinationIndex,
                                     nodeEntries.size())
            && destinationIndex != cableSourceNodeIndex
            && owner.onAddConnection)
        {
            juce::String errorMessage;
            const auto& source =
                nodeEntries.getReference(cableSourceNodeIndex);
            const auto& destination =
                nodeEntries.getReference(destinationIndex);

            if (! owner.onAddConnection(source.id,
                                        destination.id,
                                        errorMessage)
                && errorMessage.isNotEmpty()
                && owner.onStatusMessage)
            {
                owner.onStatusMessage(errorMessage);
            }
        }

        cableSourceNodeIndex = -1;
        repaint();
    }

    if (juce::isPositiveAndBelow(draggedNodeIndex,
                                 nodeEntries.size()))
    {
        auto& node = nodeEntries.getReference(draggedNodeIndex);
        const auto snappedDropPosition = constrainNodePosition(
            draggedNodeIndex,
            node.position);
        const bool snappedDropPositionIsValid =
            ! isNodePositionOverlapping(draggedNodeIndex,
                                        snappedDropPosition);
        const auto finalPosition =
            snappedDropPositionIsValid
                ? snappedDropPosition
                : (hasValidDragPosition
                       ? lastValidDragPosition
                       : dragStartPosition);

        if (node.kind == NodeKind::Output)
        {
            for (auto& outputNode : nodeEntries)
                if (outputNode.kind == NodeKind::Output)
                    outputNode.position = finalPosition;
        }
        else
        {
            node.position = finalPosition;
        }

        const auto movedNodeId = node.id;
        const auto movedNodePosition = node.position;

        updateNodeControlBounds();
        updateCanvasSizeFromNodes();
        draggedNodeIndex = -1;
        hasValidDragPosition = false;
        dragOverlay->setVisible(false);
        repaint();

        if (owner.onNodeMoved)
            owner.onNodeMoved(movedNodeId, movedNodePosition);

        return;
    }

    draggedNodeIndex = -1;
    hasValidDragPosition = false;
    dragOverlay->setVisible(false);
}

void AdvancedRoutingView::Canvas::removeSelectedConnection()
{
    if (! juce::isPositiveAndBelow(selectedConnectionIndex,
                                   connectionEntries.size()))
    {
        return;
    }

    const auto connection =
        connectionEntries.getReference(selectedConnectionIndex);
    selectedConnectionIndex = -1;

    if (owner.onRemoveConnection)
        owner.onRemoveConnection(connection.sourceNodeId,
                                 connection.destinationNodeId);
}

bool AdvancedRoutingView::Canvas::keyPressed(
    const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey
        || key == juce::KeyPress::backspaceKey)
    {
        removeSelectedConnection();
        return true;
    }

    return false;
}

AdvancedRoutingView::AdvancedRoutingView()
    : canvas(*this)
{
    titleLabel.setText("Advanced Routing",
                       juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(22.0f,
                                                   juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white);
    addAndMakeVisible(titleLabel);

    helpLabel.setText(
        "Drag from an output dot to connect. Drag either half of a cable to move its nearest end. Right-click, or select and press Delete, to remove.",
        juce::dontSendNotification);
    helpLabel.setJustificationType(juce::Justification::centredLeft);
    helpLabel.setColour(juce::Label::textColourId,
                        juce::Colours::lightgrey);
    helpLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    addAndMakeVisible(helpLabel);

    simpleButton.onClick = [this]
    {
        if (onShowSimple)
            onShowSimple();
    };
    addAndMakeVisible(simpleButton);

    undoDeleteButton.setEnabled(false);
    undoDeleteButton.setTooltip(
        "Restore the most recently deleted tab");
    undoDeleteButton.onClick = [this]
    {
        if (onUndoDelete)
            onUndoDelete();
    };
    addAndMakeVisible(undoDeleteButton);

    autoLayoutButton.onClick = [this]
    {
        if (onAutoLayout)
            onAutoLayout();
    };
    addAndMakeVisible(autoLayoutButton);

    clearButton.onClick = [this]
    {
        juce::Component::SafePointer<AdvancedRoutingView> safeThis(
            this);

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Clear Advanced Routing",
            "Remove every cable from the Advanced routing graph?",
            "Clear",
            "Cancel",
            this,
            juce::ModalCallbackFunction::create(
                [safeThis](int result)
                {
                    if (result != 0
                        && safeThis != nullptr
                        && safeThis->onClearConnections)
                    {
                        safeThis->onClearConnections();
                    }
                }));
    };
    addAndMakeVisible(clearButton);

    viewport.setViewedComponent(&canvas, false);
    viewport.setScrollBarsShown(true, true);
    viewport.setSingleStepSizes(24, 24);
    addAndMakeVisible(viewport);
}

void AdvancedRoutingView::setModel(
    const juce::Array<NodeEntry>& nodes,
    const juce::Array<AdvancedRoutingConnection>& connections)
{
    canvas.setModel(nodes, connections);
}

void AdvancedRoutingView::setDeleteUndoAvailable(
    bool shouldBeAvailable)
{
    undoDeleteButton.setEnabled(shouldBeAvailable);
}

void AdvancedRoutingView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(backgroundColour));
}

void AdvancedRoutingView::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto header = area.removeFromTop(36);
    titleLabel.setBounds(header.removeFromLeft(250));

    clearButton.setBounds(header.removeFromRight(104).reduced(2));
    header.removeFromRight(6);
    autoLayoutButton.setBounds(header.removeFromRight(94).reduced(2));
    header.removeFromRight(6);
    undoDeleteButton.setBounds(header.removeFromRight(70).reduced(2));
    header.removeFromRight(6);
    simpleButton.setBounds(header.removeFromRight(80).reduced(2));

    helpLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);
    viewport.setBounds(area);
}
