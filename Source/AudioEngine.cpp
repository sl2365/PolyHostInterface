#include "AudioEngine.h"
#include "DebugLog.h"
#include <cmath>

AudioEngine::QueuedMidiInputProcessor::QueuedMidiInputProcessor()
    : juce::AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                           )
{
}

template <typename FloatType>
void AudioEngine::QueuedMidiInputProcessor::process(juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();
    midiMessages.clear();

    juce::ScopedLock sl(midiLock);
    midiMessages.swapWith(pendingMidi);
}

void AudioEngine::QueuedMidiInputProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    process(buffer, midiMessages);
}

void AudioEngine::QueuedMidiInputProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    process(buffer, midiMessages);
}

void AudioEngine::QueuedMidiInputProcessor::enqueueMidi(const juce::MidiMessage& message)
{
    juce::ScopedLock sl(midiLock);
    pendingMidi.addEvent(message, 0);
}

AudioEngine::AudioEngine() : graph() {}
AudioEngine::~AudioEngine() { shutdown(); }

void AudioEngine::initialise(const juce::String& savedAudioDeviceState)
{
    juce::String err;

    if (savedAudioDeviceState.isNotEmpty())
    {
        std::unique_ptr<juce::XmlElement> xml(juce::parseXML(savedAudioDeviceState));
        if (xml)
            err = deviceManager.initialise(0, 2, xml.get(), true);
        else
            err = deviceManager.initialiseWithDefaultDevices(0, 2);
    }
    else
    {
        err = deviceManager.initialiseWithDefaultDevices(0, 2);
    }

    if (err.isNotEmpty())
    {
       #if JUCE_DEBUG
        DBG("AudioDeviceManager init error: " << err);
       #endif
    }

    graph.enableAllBuses();
    graph.setPlayHead(playHead);

    audioOutNode = graph.addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    inputMeterNode = graph.addNode(std::make_unique<MeterTapProcessor>(inputMeterLevelL, inputMeterLevelR));
    outputMeterNode = graph.addNode(std::make_unique<MeterTapProcessor>(outputMeterLevelL, outputMeterLevelR));

    graphPlayer.setProcessor(&graph);
}

void AudioEngine::setPlayHead(juce::AudioPlayHead* newPlayHead)
{
    playHead = newPlayHead;
    graph.setPlayHead(playHead);

    for (auto& node : synthNodes)
        if (node != nullptr && node->getProcessor() != nullptr)
            node->getProcessor()->setPlayHead(playHead);

    for (auto& node : fxNodes)
        if (node != nullptr && node->getProcessor() != nullptr)
            node->getProcessor()->setPlayHead(playHead);
}

juce::String AudioEngine::createAudioDeviceStateXml() const
{
    if (auto xml = deviceManager.createStateXml())
        return xml->toString();

    return {};
}

void AudioEngine::shutdown()
{
    graphPlayer.setProcessor(nullptr);
    midiRoutingNodes.clear();
    graph.clear();
}

juce::AudioProcessorGraph::NodeID AudioEngine::addPlugin(std::unique_ptr<juce::AudioPluginInstance> instance, bool isSynth)
{
    DebugLog::write("AudioEngine::addPlugin - start");

    if (instance == nullptr)
    {
        DebugLog::write("AudioEngine::addPlugin - instance was null");
        return {};
    }

    DebugLog::write("AudioEngine::addPlugin - setting playhead");
    instance->setPlayHead(playHead);

    DebugLog::write("AudioEngine::addPlugin - enabling buses");
    instance->enableAllBuses();

    DebugLog::write("AudioEngine::addPlugin - calling graph.addNode");
    auto node = graph.addNode(std::move(instance));

    if (node == nullptr)
    {
        DebugLog::write("AudioEngine::addPlugin - graph.addNode returned null");
        return {};
    }

    DebugLog::write("AudioEngine::addPlugin - graph.addNode OK");

    if (isSynth)
    {
        DebugLog::write("AudioEngine::addPlugin - adding node to synthNodes");
        synthNodes.add(node);
    }
    else
    {
        DebugLog::write("AudioEngine::addPlugin - adding node to fxNodes");
        fxNodes.add(node);
    }

    DebugLog::write("AudioEngine::addPlugin - updating playheads");
    updateNodePlayHeads();

    DebugLog::write("AudioEngine::addPlugin - rebuilding MIDI routing nodes");
    rebuildMidiRoutingNodes();

    DebugLog::write("AudioEngine::addPlugin - rebuilding audio connections");
    rebuildConnections();

    DebugLog::write("AudioEngine::addPlugin - done");
    return node->nodeID;
}

void AudioEngine::removePlugin(juce::AudioProcessorGraph::NodeID nodeId)
{
    for (int i = synthNodes.size(); --i >= 0;)
        if (synthNodes[i]->nodeID == nodeId)
            synthNodes.remove(i);

    for (int i = fxNodes.size(); --i >= 0;)
        if (fxNodes[i]->nodeID == nodeId)
            fxNodes.remove(i);

    for (int i = midiRoutingNodes.size(); --i >= 0;)
    {
        if (midiRoutingNodes.getReference(i).pluginNodeId == nodeId)
        {
            if (midiRoutingNodes.getReference(i).midiNode != nullptr)
                graph.removeNode(midiRoutingNodes.getReference(i).midiNode->nodeID);

            midiRoutingNodes.remove(i);
        }
    }

    graph.removeNode(nodeId);
    rebuildConnections();
}

void AudioEngine::setRoutingState(const juce::Array<RoutingEntry>& orderedEntries)
{
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> reorderedSynths;
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> reorderedFx;

    for (auto& entry : orderedEntries)
    {
        if (auto* n = graph.getNodeForId(entry.nodeId))
        {
            if (entry.isSynth)
                reorderedSynths.add(n);
            else
                reorderedFx.add(n);
        }
    }

    synthNodes = reorderedSynths;
    fxNodes = reorderedFx;

    orderedRoutingEntries = orderedEntries;

    updateNodePlayHeads();
    rebuildConnections();
}

void AudioEngine::queueMidiToNodes(const juce::MidiMessage& message,
                                   const juce::Array<juce::AudioProcessorGraph::NodeID>& targetNodeIds)
{
    for (auto nodeId : targetNodeIds)
        if (auto* midiRouter = getMidiRouterForPluginNode(nodeId))
            midiRouter->enqueueMidi(message);
}

void AudioEngine::sendMidiPanicToNodes(const juce::Array<juce::AudioProcessorGraph::NodeID>& targetNodeIds)
{
    for (int channel = 1; channel <= 16; ++channel)
    {
        const auto sustainOff = juce::MidiMessage::controllerEvent(channel, 64, 0);
        const auto allSoundOff = juce::MidiMessage::controllerEvent(channel, 120, 0);
        const auto resetControllers = juce::MidiMessage::controllerEvent(channel, 121, 0);
        const auto allNotesOff = juce::MidiMessage::controllerEvent(channel, 123, 0);

        queueMidiToNodes(sustainOff, targetNodeIds);
        queueMidiToNodes(allSoundOff, targetNodeIds);
        queueMidiToNodes(resetControllers, targetNodeIds);
        queueMidiToNodes(allNotesOff, targetNodeIds);
    }
}

AudioEngine::QueuedMidiInputProcessor* AudioEngine::getMidiRouterForPluginNode(juce::AudioProcessorGraph::NodeID pluginNodeId) const
{
    for (auto& entry : midiRoutingNodes)
        if (entry.pluginNodeId == pluginNodeId)
            return entry.processor;

    return nullptr;
}

void AudioEngine::updateNodePlayHeads()
{
    for (auto& node : synthNodes)
        if (node != nullptr && node->getProcessor() != nullptr)
            node->getProcessor()->setPlayHead(playHead);

    for (auto& node : fxNodes)
        if (node != nullptr && node->getProcessor() != nullptr)
            node->getProcessor()->setPlayHead(playHead);
}

void AudioEngine::rebuildMidiRoutingNodes()
{
    juce::Array<juce::AudioProcessorGraph::NodeID> pluginNodeIds;

    for (auto& node : synthNodes)
        pluginNodeIds.add(node->nodeID);

    for (auto& node : fxNodes)
        pluginNodeIds.add(node->nodeID);

    for (int i = midiRoutingNodes.size(); --i >= 0;)
    {
        auto& entry = midiRoutingNodes.getReference(i);

        if (!pluginNodeIds.contains(entry.pluginNodeId))
        {
            if (entry.midiNode != nullptr)
                graph.removeNode(entry.midiNode->nodeID);

            midiRoutingNodes.remove(i);
        }
    }

    for (auto pluginNodeId : pluginNodeIds)
    {
        bool exists = false;

        for (auto& entry : midiRoutingNodes)
        {
            if (entry.pluginNodeId == pluginNodeId)
            {
                exists = true;
                break;
            }
        }

        if (!exists)
        {
            auto processor = std::make_unique<QueuedMidiInputProcessor>();
            auto* processorPtr = processor.get();
            auto midiNode = graph.addNode(std::move(processor));

            if (midiNode != nullptr)
            {
                MidiRoutingNode entry;
                entry.pluginNodeId = pluginNodeId;
                entry.midiNode = midiNode;
                entry.processor = processorPtr;
                midiRoutingNodes.add(entry);
            }
        }
    }
}

int AudioEngine::getMainInputChannels(juce::AudioProcessor* processor)
{
    if (processor == nullptr)
        return 0;

    return processor->getMainBusNumInputChannels();
}

int AudioEngine::getMainOutputChannels(juce::AudioProcessor* processor)
{
    if (processor == nullptr)
        return 0;

    return processor->getMainBusNumOutputChannels();
}

bool AudioEngine::connectAudioChannels(juce::AudioProcessorGraph::NodeID srcNode,
                                       juce::AudioProcessor* srcProcessor,
                                       juce::AudioProcessorGraph::NodeID dstNode,
                                       juce::AudioProcessor* dstProcessor)
{
    if (srcProcessor == nullptr || dstProcessor == nullptr)
        return false;

    const int srcChannels = getMainOutputChannels(srcProcessor);
    const int dstChannels = getMainInputChannels(dstProcessor);
    const int channelsToConnect = juce::jmin(srcChannels, dstChannels);

    bool madeAnyConnection = false;

    for (int ch = 0; ch < channelsToConnect; ++ch)
    {
        madeAnyConnection |= graph.addConnection({ { srcNode, ch }, { dstNode, ch } });
    }

    // mono -> stereo convenience copy
    if (srcChannels == 1 && dstChannels >= 2)
    {
        madeAnyConnection |= graph.addConnection({ { srcNode, 0 }, { dstNode, 1 } });
    }

    return madeAnyConnection;
}

void AudioEngine::rebuildConnections()
{
    graph.disconnectNode(audioOutNode->nodeID);

    if (inputMeterNode != nullptr)
        graph.disconnectNode(inputMeterNode->nodeID);

    if (outputMeterNode != nullptr)
        graph.disconnectNode(outputMeterNode->nodeID);

    for (auto& n : synthNodes)
        graph.disconnectNode(n->nodeID);

    for (auto& n : fxNodes)
        graph.disconnectNode(n->nodeID);

    for (auto& midiRoute : midiRoutingNodes)
        if (midiRoute.midiNode != nullptr)
            graph.disconnectNode(midiRoute.midiNode->nodeID);

    const int midiCh = juce::AudioProcessorGraph::midiChannelIndex;

    for (auto& midiRoute : midiRoutingNodes)
    {
        if (midiRoute.midiNode != nullptr)
            graph.addConnection({ { midiRoute.midiNode->nodeID, midiCh }, { midiRoute.pluginNodeId, midiCh } });
    }

    auto* inputMeterProc = inputMeterNode != nullptr ? inputMeterNode->getProcessor() : nullptr;
    auto* outputMeterProc = outputMeterNode != nullptr ? outputMeterNode->getProcessor() : nullptr;
    auto* audioOutProc = audioOutNode != nullptr ? audioOutNode->getProcessor() : nullptr;

    if (orderedRoutingEntries.isEmpty())
    {
        if (inputMeterNode != nullptr && outputMeterNode != nullptr)
        {
            connectAudioChannels(inputMeterNode->nodeID, inputMeterProc,
                                 outputMeterNode->nodeID, outputMeterProc);

            connectAudioChannels(outputMeterNode->nodeID, outputMeterProc,
                                 audioOutNode->nodeID, audioOutProc);
        }

        return;
    }

    bool madeAnySynthConnection = false;

    for (int i = 0; i < orderedRoutingEntries.size(); ++i)
    {
        const auto& entry = orderedRoutingEntries.getReference(i);

        if (!entry.isSynth)
            continue;

        auto* synthNode = graph.getNodeForId(entry.nodeId);
        if (synthNode == nullptr || synthNode->getProcessor() == nullptr)
            continue;

        juce::AudioProcessorGraph::Node::Ptr downstreamTarget = nullptr;

        for (int j = i + 1; j < orderedRoutingEntries.size(); ++j)
        {
            const auto& downstreamEntry = orderedRoutingEntries.getReference(j);

            if (downstreamEntry.isSynth)
                continue;

            downstreamTarget = graph.getNodeForId(downstreamEntry.nodeId);
            if (downstreamTarget != nullptr && downstreamTarget->getProcessor() != nullptr)
                break;

            downstreamTarget = nullptr;
        }

        if (downstreamTarget != nullptr)
        {
            connectAudioChannels(synthNode->nodeID, synthNode->getProcessor(),
                                 downstreamTarget->nodeID, downstreamTarget->getProcessor());
        }
        else
        {
            connectAudioChannels(synthNode->nodeID, synthNode->getProcessor(),
                                 outputMeterNode->nodeID, outputMeterProc);
        }

        madeAnySynthConnection = true;
    }

    for (int i = 0; i < orderedRoutingEntries.size(); ++i)
    {
        const auto& entry = orderedRoutingEntries.getReference(i);

        if (entry.isSynth)
            continue;

        auto* fxNode = graph.getNodeForId(entry.nodeId);
        if (fxNode == nullptr || fxNode->getProcessor() == nullptr)
            continue;

        juce::AudioProcessorGraph::Node::Ptr nextFxNode = nullptr;

        for (int j = i + 1; j < orderedRoutingEntries.size(); ++j)
        {
            const auto& downstreamEntry = orderedRoutingEntries.getReference(j);

            if (downstreamEntry.isSynth)
                continue;

            nextFxNode = graph.getNodeForId(downstreamEntry.nodeId);
            if (nextFxNode != nullptr && nextFxNode->getProcessor() != nullptr)
                break;

            nextFxNode = nullptr;
        }

        if (nextFxNode != nullptr)
        {
            connectAudioChannels(fxNode->nodeID, fxNode->getProcessor(),
                                 nextFxNode->nodeID, nextFxNode->getProcessor());
        }
        else
        {
            connectAudioChannels(fxNode->nodeID, fxNode->getProcessor(),
                                 outputMeterNode->nodeID, outputMeterProc);
        }
    }

    if (!madeAnySynthConnection)
    {
        connectAudioChannels(inputMeterNode->nodeID, inputMeterProc,
                             outputMeterNode->nodeID, outputMeterProc);
    }

    connectAudioChannels(outputMeterNode->nodeID, outputMeterProc,
                         audioOutNode->nodeID, audioOutProc);
}

float AudioEngine::getInputMeterLevelL() const
{
    return inputMeterLevelL.load(std::memory_order_relaxed);
}

float AudioEngine::getInputMeterLevelR() const
{
    return inputMeterLevelR.load(std::memory_order_relaxed);
}

float AudioEngine::getOutputMeterLevelL() const
{
    return outputMeterLevelL.load(std::memory_order_relaxed);
}

float AudioEngine::getOutputMeterLevelR() const
{
    return outputMeterLevelR.load(std::memory_order_relaxed);
}

