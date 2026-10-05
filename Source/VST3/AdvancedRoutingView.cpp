#include "AdvancedRoutingView.h"

namespace
{
    constexpr auto backgroundColour = 0xFF111A2A;
    constexpr auto canvasColour = 0xFF172338;
    constexpr auto nodeColour = 0xFF26364D;
    constexpr auto nodeBorderColour = 0xFF6B819E;
    constexpr auto effectNodeColour = 0xFF4B3726;
    constexpr auto effectNodeBorderColour = 0xFF96704A;
    constexpr auto outputNodeColour = 0xFF4A3040;
    constexpr auto portColour = 0xFF4DA3FF;
    constexpr auto cableColour = 0xFF3A7BD5;
}

AdvancedRoutingView::Canvas::Canvas(AdvancedRoutingView& ownerIn)
    : owner(ownerIn)
{
    setWantsKeyboardFocus(true);
    setSize(1800, 1200);
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

    updateCanvasSizeFromNodes();
    repaint();
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

    for (const auto& node : nodeEntries)
    {
        if (node.kind == NodeKind::Output)
            continue;

        const auto bounds = getNodeBounds(node).toFloat();
        juce::Colour fill(nodeColour);
        juce::Colour border(nodeBorderColour);

        if (node.kind == NodeKind::AudioInput)
            fill = juce::Colour(0xFF284B3A);
        else if (node.kind == NodeKind::Effect)
        {
            fill = juce::Colour(effectNodeColour);
            border = juce::Colour(effectNodeBorderColour);
        }

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(border);
        g.drawRoundedRectangle(bounds, 8.0f, 1.5f);

        auto textBounds = bounds.reduced(14.0f, 9.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(15.0f,
                                               juce::Font::bold)));
        g.drawFittedText(node.label,
                         textBounds.removeFromTop(24.0f).toNearestInt(),
                         juce::Justification::centredLeft,
                         1);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(juce::FontOptions(11.5f)));
        g.drawFittedText(node.subtitle,
                         textBounds.toNearestInt(),
                         juce::Justification::centredLeft,
                         1);

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

    const auto outputBounds = getOutputModuleBounds().toFloat();
    if (! outputBounds.isEmpty())
    {
        g.setColour(juce::Colour(outputNodeColour));
        g.fillRoundedRectangle(outputBounds, 8.0f);
        g.setColour(juce::Colour(nodeBorderColour));
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

        g.setColour(juce::Colour(nodeBorderColour).withAlpha(0.45f));
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
            g.setColour(juce::Colour(portColour));
            g.fillEllipse(port.x - portRadius,
                          port.y - portRadius,
                          portRadius * 2.0f,
                          portRadius * 2.0f);

            g.setColour(busIndex == 0
                            ? juce::Colours::white
                            : juce::Colours::lightgrey);
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
                    juce::Colour(nodeBorderColour).withAlpha(0.16f));
                g.drawHorizontalLine(
                    juce::roundToInt(port.y
                                     + outputRowHeight * 0.5f),
                    outputBounds.getX() + 12.0f,
                    outputBounds.getRight() - 10.0f);
            }
        }
    }
}

void AdvancedRoutingView::Canvas::mouseMove(
    const juce::MouseEvent& event)
{
    const int newHoveredConnectionIndex =
        findConnectionAt(event.position);

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
    const int connectionIndex = findConnectionAt(position);

    if (event.mods.isPopupMenu())
    {
        if (juce::isPositiveAndBelow(connectionIndex,
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

    cableSourceNodeIndex = findOutputPortAt(position);

    if (cableSourceNodeIndex >= 0)
    {
        selectedConnectionIndex = -1;
        selectedNodeIndex = -1;
        hoveredConnectionIndex = -1;
        cableEnd = position;
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

    draggedNodeIndex = findNodeAt(position);
    if (draggedNodeIndex >= 0)
    {
        selectedConnectionIndex = -1;
        selectedNodeIndex = draggedNodeIndex;
        hoveredConnectionIndex = -1;
        const auto& node = nodeEntries.getReference(draggedNodeIndex);
        dragOffset = position.toInt()
                     - getNodeBounds(node).getPosition();
    }
    else
    {
        selectedConnectionIndex = -1;
        selectedNodeIndex = -1;
        hoveredConnectionIndex = -1;
    }

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
    const auto newPosition = snapPositionToGrid(
        event.position.toInt() - dragOffset);

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

    updateCanvasSizeFromNodes(false);
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
        const auto& node = nodeEntries.getReference(draggedNodeIndex);
        if (owner.onNodeMoved)
            owner.onNodeMoved(node.id, node.position);

        updateCanvasSizeFromNodes();
    }

    draggedNodeIndex = -1;
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
    simpleButton.setBounds(header.removeFromRight(80).reduced(2));

    helpLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);
    viewport.setBounds(area);
}
