#pragma once
#include <JuceHeader.h>

class AudioEngine : private juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise(const juce::String& savedAudioDeviceState = {});
    juce::String createAudioDeviceStateXml() const;
    void shutdown();

    juce::AudioProcessorGraph::NodeID addPlugin(std::unique_ptr<juce::AudioPluginInstance> instance, bool isSynth);
    void removePlugin(juce::AudioProcessorGraph::NodeID nodeId);

    void setRoutingState(const juce::Array<juce::AudioProcessorGraph::NodeID>& synthIds,
                         const juce::Array<juce::AudioProcessorGraph::NodeID>& fxIds);

    void queueMidiToNodes(const juce::MidiMessage& message,
                          const juce::Array<juce::AudioProcessorGraph::NodeID>& targetNodeIds);

    void setTempoBpm(double newTempoBpm);
    void setHostSyncEnabled(bool shouldUseHostSync);
    bool isHostSyncEnabled() const;
    bool isExternalHostTempoAvailable() const;
    double getCurrentTempoBpm() const;
    void setExternalHostTempoBpm(double bpm);
    void clearExternalHostTempo();

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::AudioProcessorGraph& getGraph() { return graph; }

private:
    class HostPlayHead final : public juce::AudioPlayHead
    {
    public:
        void prepareToPlay(double newSampleRate)
        {
            const juce::ScopedLock sl(lock);
            sampleRate = (newSampleRate > 0.0 ? newSampleRate : 44100.0);
            samplePosition = 0.0;
        }

        void setTempoBpm(double newTempoBpm)
        {
            const juce::ScopedLock sl(lock);
            tempoBpm = juce::jlimit(20.0, 300.0, newTempoBpm);
        }

        void advance(int numSamples)
        {
            const juce::ScopedLock sl(lock);
            samplePosition += juce::jmax(0, numSamples);
        }

        Optional<PositionInfo> getPosition() const override
        {
            const juce::ScopedLock sl(lock);

            PositionInfo info;
            info.setIsPlaying(true);
            info.setIsRecording(false);
            info.setBpm(tempoBpm);
            info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 4, 4 });
            info.setTimeInSamples((juce::int64) samplePosition);
            info.setTimeInSeconds(samplePosition / sampleRate);

            const auto ppq = (samplePosition / sampleRate) * (tempoBpm / 60.0);
            info.setPpqPosition(ppq);
            info.setPpqPositionOfLastBarStart(std::floor(ppq / 4.0) * 4.0);

            return info;
        }

    private:
        mutable juce::CriticalSection lock;
        double sampleRate = 44100.0;
        double samplePosition = 0.0;
        double tempoBpm = 120.0;
    };

    class TempoAwareGraphPlayer final : public juce::AudioIODeviceCallback
    {
    public:
        TempoAwareGraphPlayer(juce::AudioProcessorPlayer& playerIn, HostPlayHead& playHeadIn)
            : player(playerIn), playHead(playHeadIn) {}

        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override
        {
            player.audioDeviceIOCallbackWithContext(inputChannelData,
                                                    numInputChannels,
                                                    outputChannelData,
                                                    numOutputChannels,
                                                    numSamples,
                                                    context);

            playHead.advance(numSamples);
        }

        void audioDeviceAboutToStart(juce::AudioIODevice* device) override
        {
            player.audioDeviceAboutToStart(device);

            if (device != nullptr)
                playHead.prepareToPlay(device->getCurrentSampleRate());
        }

        void audioDeviceStopped() override
        {
            player.audioDeviceStopped();
        }

    private:
        juce::AudioProcessorPlayer& player;
        HostPlayHead& playHead;
    };

    class QueuedMidiInputProcessor final : public juce::AudioProcessor
    {
    public:
        QueuedMidiInputProcessor();

        const juce::String getName() const override { return "Queued MIDI Input"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}

        bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }

        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
        void processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) override;

        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }

        const juce::String getInputChannelName(int channelIndex) const override { return juce::String(channelIndex + 1); }
        const juce::String getOutputChannelName(int channelIndex) const override { return juce::String(channelIndex + 1); }
        bool isInputChannelStereoPair(int) const override { return true; }
        bool isOutputChannelStereoPair(int) const override { return true; }

        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
        bool isMidiEffect() const override { return true; }

        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}

        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}

        void enqueueMidi(const juce::MidiMessage& message);

    private:
        juce::MidiBuffer pendingMidi;
        juce::CriticalSection midiLock;

        template <typename FloatType>
        void process(juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages);
    };

    struct MidiRoutingNode
    {
        juce::AudioProcessorGraph::NodeID pluginNodeId;
        juce::AudioProcessorGraph::Node::Ptr midiNode;
        QueuedMidiInputProcessor* processor = nullptr;
    };

    QueuedMidiInputProcessor* getMidiRouterForPluginNode(juce::AudioProcessorGraph::NodeID pluginNodeId) const;
    void rebuildConnections();
    void rebuildMidiRoutingNodes();
    void updateNodePlayHeads();
    void changeListenerCallback(juce::ChangeBroadcaster*) override {}

    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer graphPlayer;
    HostPlayHead hostPlayHead;
    TempoAwareGraphPlayer tempoAwareGraphPlayer { graphPlayer, hostPlayHead };
    juce::AudioProcessorGraph::Node::Ptr audioOutNode;
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> synthNodes;
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> fxNodes;
    juce::Array<MidiRoutingNode> midiRoutingNodes;

    bool hostSyncEnabled = false;
    double internalTempoBpm = 120.0;
    double externalHostTempoBpm = 120.0;
    bool externalHostTempoAvailable = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};