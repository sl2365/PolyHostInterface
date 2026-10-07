#pragma once

#include <JuceHeader.h>
#include "SessionManager.h"

class AdvancedRoutingView final : public juce::Component
{
public:
    enum class NodeKind
    {
        AudioInput,
        Synth,
        Effect,
        Output
    };

    struct NodeEntry
    {
        juce::String id;
        juce::String label;
        juce::String subtitle;
        int tabIndex = -1;
        NodeKind kind = NodeKind::Synth;
        bool acceptsInput = false;
        bool producesOutput = false;
        bool available = true;
        bool isBypassed = false;
        bool isSoloed = false;
        bool isMutedBySolo = false;
        float outputGainDb = 0.0f;
        juce::String midiAssignmentsTooltip;
        juce::String routingTooltip;
        int pointerAdjustMethodOverride = 0;
        bool needsAttention = false;
        juce::String attentionMessage;
        int outputBusIndex = -1;
        juce::Point<int> position;
    };

    AdvancedRoutingView();

    void setModel(
        const juce::Array<NodeEntry>& nodes,
        const juce::Array<AdvancedRoutingConnection>& connections);
    void setDeleteUndoAvailable(bool shouldBeAvailable);

    std::function<void()> onShowSimple;
    std::function<void()> onUndoDelete;
    std::function<void()> onClearConnections;
    std::function<void()> onAutoLayout;
    std::function<bool(const juce::String& sourceNodeId,
                       const juce::String& destinationNodeId,
                       juce::String& errorMessage)> onAddConnection;
    std::function<void(const juce::String& sourceNodeId,
                       const juce::String& destinationNodeId)>
        onRemoveConnection;
    std::function<bool(const juce::String& oldSourceNodeId,
                       const juce::String& oldDestinationNodeId,
                       const juce::String& newSourceNodeId,
                       const juce::String& newDestinationNodeId,
                       juce::String& errorMessage)>
        onReconnectConnection;
    std::function<void(const juce::String& nodeId,
                       juce::Point<int> position)> onNodeMoved;
    std::function<void(const juce::String& message)> onStatusMessage;
    std::function<void(int tabIndex)> onToggleBypass;
    std::function<void(int tabIndex)> onToggleSolo;
    std::function<void(int tabIndex, float gainDb)> onSetOutputGainDb;
    std::function<void(int tabIndex)> onSelectTab;
    std::function<void(int tabIndex)> onCloseTab;
    std::function<void(int tabIndex,
                       juce::Component* anchorComponent)>
        onShowMidiAssignments;
    std::function<void(int tabIndex,
                       juce::Component* anchorComponent)>
        onShowPluginInfo;
    std::function<void(int tabIndex, int methodOverride)>
        onSetPointerAdjustMethodOverride;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class Canvas final : public juce::Component
    {
    public:
        explicit Canvas(AdvancedRoutingView& ownerIn);
        ~Canvas() override;

        void setModel(
            const juce::Array<NodeEntry>& nodes,
            const juce::Array<AdvancedRoutingConnection>& connections);

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        bool keyPressed(const juce::KeyPress& key) override;
        void resized() override;

    private:
        class NodeControls;
        class DragOverlay;

        enum class ReconnectEnd
        {
            None,
            Source,
            Destination
        };

        static constexpr int minimumCanvasWidth = 1200;
        static constexpr int minimumCanvasHeight = 800;
        static constexpr int canvasContentPadding = 240;
        static constexpr int gridSnapSize = 5;
        static constexpr int majorGridSize = 20;
        static constexpr int minimumNodePosition = 0;
        static constexpr int moduleGap = 10;
        static constexpr int moduleSnapDistance = 10;
        static constexpr int nodeWidth = 168;
        static constexpr int nodeHeight = 66;
        static constexpr int outputModuleWidth = 96;
        static constexpr int outputHeaderHeight = 32;
        static constexpr int outputRowHeight = 20;
        static constexpr int outputBottomPadding = 8;
        static constexpr int outputBusCount = 17;
        static constexpr int outputModuleHeight =
            outputHeaderHeight
            + outputRowHeight * outputBusCount
            + outputBottomPadding;
        static constexpr float portRadius = 6.0f;

        juce::Rectangle<int> getNodeBounds(const NodeEntry& node) const;
        juce::Rectangle<int> getOutputModuleBounds() const;
        juce::Point<float> getInputPort(const NodeEntry& node) const;
        juce::Point<float> getOutputPort(const NodeEntry& node) const;
        static juce::Path makeCablePath(juce::Point<float> start,
                                        juce::Point<float> end);
        juce::Path makeConnectionPath(
            const AdvancedRoutingConnection& connection) const;
        int findNodeAt(juce::Point<float> position) const;
        int findInputPortAt(juce::Point<float> position) const;
        int findOutputPortAt(juce::Point<float> position) const;
        int findConnectionAt(juce::Point<float> position) const;
        int findNodeIndex(const juce::String& id) const;
        int findOutputAnchorIndex() const;
        void updateCanvasSizeFromNodes(bool allowShrink = true);
        static juce::Point<int> snapPositionToGrid(
            juce::Point<int> position);
        juce::Point<int> constrainNodePosition(
            int movingNodeIndex,
            juce::Point<int> desiredPosition) const;
        bool isNodePositionOverlapping(
            int movingNodeIndex,
            juce::Point<int> position) const;
        static juce::Colour getCableBaseColour();
        bool isConnectionAttachedToSelectedNode(
            const AdvancedRoutingConnection& connection) const;
        void paintNode(juce::Graphics& g,
                       const NodeEntry& node) const;
        void paintOutputModule(juce::Graphics& g) const;
        void paintDraggedNodeOverlay(juce::Graphics& g);
        void removeSelectedConnection();
        void rebuildNodeControls();
        void updateNodeControlBounds();

        AdvancedRoutingView& owner;
        juce::Array<NodeEntry> nodeEntries;
        juce::Array<AdvancedRoutingConnection> connectionEntries;
        juce::OwnedArray<NodeControls> nodeControls;
        std::unique_ptr<DragOverlay> dragOverlay;
        int draggedNodeIndex = -1;
        juce::Point<int> dragOffset;
        juce::Point<int> dragStartPosition;
        juce::Point<int> lastValidDragPosition;
        bool hasValidDragPosition = false;
        int cableSourceNodeIndex = -1;
        juce::Point<float> cableEnd;
        int reconnectConnectionIndex = -1;
        ReconnectEnd reconnectEnd = ReconnectEnd::None;
        int selectedConnectionIndex = -1;
        int selectedNodeIndex = -1;
        int hoveredConnectionIndex = -1;
    };

    juce::Label titleLabel;
    juce::Label helpLabel;
    juce::TextButton simpleButton { "Simple" };
    juce::TextButton undoDeleteButton { "Undo" };
    juce::TextButton autoLayoutButton { "Auto Layout" };
    juce::TextButton clearButton { "Clear Cables" };
    juce::Viewport viewport;
    Canvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedRoutingView)
};
