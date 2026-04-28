#include "AudioEngine.h"
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
    if (instance == nullptr)
        return {};

    instance->setPlayHead(playHead);
    instance->enableAllBuses();

    auto node = graph.addNode(std::move(instance));

    if (node == nullptr)
        return {};

    if (isSynth)
        synthNodes.add(node);
    else
        fxNodes.add(node);

    updateNodePlayHeads();
    rebuildMidiRoutingNodes();
    rebuildConnections();
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

void AudioEngine::setRoutingState(const juce::Array<juce::AudioProcessorGraph::NodeID>& synthIds,
                                  const juce::Array<juce::AudioProcessorGraph::NodeID>& fxIds)
{
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> reorderedSynths;
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> reorderedFx;

    for (auto id : synthIds)
        if (auto* n = graph.getNodeForId(id))
            reorderedSynths.add(n);

    for (auto id : fxIds)
        if (auto* n = graph.getNodeForId(id))
            reorderedFx.add(n);

    synthNodes = reorderedSynths;
    fxNodes = reorderedFx;
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

void AudioEngine::setTempoBpm(double newTempoBpm)
{
    juce::ignoreUnused(newTempoBpm);
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

void AudioEngine::rebuildConnections()
{
    graph.disconnectNode(audioOutNode->nodeID);

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

    if (!fxNodes.isEmpty())
    {
        auto firstFxNode = fxNodes.getFirst();

        for (auto& synthNode : synthNodes)
        {
            graph.addConnection({ { synthNode->nodeID, 0 }, { firstFxNode->nodeID, 0 } });
            graph.addConnection({ { synthNode->nodeID, 1 }, { firstFxNode->nodeID, 1 } });
        }

        for (int i = 0; i < fxNodes.size() - 1; ++i)
        {
            auto src = fxNodes[i];
            auto dst = fxNodes[i + 1];

            graph.addConnection({ { src->nodeID, 0 }, { dst->nodeID, 0 } });
            graph.addConnection({ { src->nodeID, 1 }, { dst->nodeID, 1 } });
        }

        auto lastFxNode = fxNodes.getLast();
        graph.addConnection({ { lastFxNode->nodeID, 0 }, { audioOutNode->nodeID, 0 } });
        graph.addConnection({ { lastFxNode->nodeID, 1 }, { audioOutNode->nodeID, 1 } });

        return;
    }

    for (auto& synthNode : synthNodes)
    {
        graph.addConnection({ { synthNode->nodeID, 0 }, { audioOutNode->nodeID, 0 } });
        graph.addConnection({ { synthNode->nodeID, 1 }, { audioOutNode->nodeID, 1 } });
    }
}


