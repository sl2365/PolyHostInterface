#pragma once
#include <JuceHeader.h>
#include <memory>

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
    void sendMidiPanicToNodes(const juce::Array<juce::AudioProcessorGraph::NodeID>& targetNodeIds);

    void setTempoBpm(double newTempoBpm);
    void setPlayHead(juce::AudioPlayHead* playHead);

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::AudioProcessorGraph& getGraph() { return graph; }
    juce::AudioProcessorPlayer& getGraphPlayer() { return graphPlayer; }

    float getInputMeterLevelL() const;
    float getInputMeterLevelR() const;
    float getOutputMeterLevelL() const;
    float getOutputMeterLevelR() const;

private:
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

    class MeterTapProcessor final : public juce::AudioProcessor
    {
    public:
        MeterTapProcessor(std::atomic<float>& leftLevelIn,
                          std::atomic<float>& rightLevelIn)
            : leftLevel(leftLevelIn), rightLevel(rightLevelIn),
              juce::AudioProcessor(BusesProperties()
                                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                   .withOutput("Output", juce::AudioChannelSet::stereo(), true))
        {
        }

        const juce::String getName() const override { return "Meter Tap"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}

        bool isBusesLayoutSupported(const BusesLayout& layouts) const override
        {
            return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
                && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
        }

        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            process(buffer);
        }

        void processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) override
        {
            process(buffer);
        }

        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }

        const juce::String getInputChannelName(int channelIndex) const override { return juce::String(channelIndex + 1); }
        const juce::String getOutputChannelName(int channelIndex) const override { return juce::String(channelIndex + 1); }
        bool isInputChannelStereoPair(int) const override { return true; }
        bool isOutputChannelStereoPair(int) const override { return true; }

        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }

        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}

        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}

    private:
        template <typename FloatType>
        void process(juce::AudioBuffer<FloatType>& buffer)
        {
            const int numSamples = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();

            float peakL = 0.0f;
            float peakR = 0.0f;

            if (numChannels > 0)
            {
                auto* left = buffer.getReadPointer(0);

                for (int i = 0; i < numSamples; ++i)
                    peakL = juce::jmax(peakL, std::abs((float) left[i]));
            }

            if (numChannels > 1)
            {
                auto* right = buffer.getReadPointer(1);

                for (int i = 0; i < numSamples; ++i)
                    peakR = juce::jmax(peakR, std::abs((float) right[i]));
            }
            else
            {
                peakR = peakL;
            }

            constexpr float attackCoeff = 0.6f;
            constexpr float releaseCoeff = 0.08f;

            auto smooth = [attackCoeff, releaseCoeff](float current, float target) -> float
            {
                const float coeff = target > current ? attackCoeff : releaseCoeff;
                return current + (target - current) * coeff;
            };

            leftLevel.store(smooth(leftLevel.load(std::memory_order_relaxed), peakL), std::memory_order_relaxed);
            rightLevel.store(smooth(rightLevel.load(std::memory_order_relaxed), peakR), std::memory_order_relaxed);
        }

        std::atomic<float>& leftLevel;
        std::atomic<float>& rightLevel;
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
    juce::AudioPlayHead* playHead = nullptr;
    juce::AudioProcessorGraph::Node::Ptr audioOutNode;
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> synthNodes;
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> fxNodes;
    juce::Array<MidiRoutingNode> midiRoutingNodes;

    std::atomic<float> inputMeterLevelL { 0.0f };
    std::atomic<float> inputMeterLevelR { 0.0f };
    std::atomic<float> outputMeterLevelL { 0.0f };
    std::atomic<float> outputMeterLevelR { 0.0f };

    juce::AudioProcessorGraph::Node::Ptr inputMeterNode;
    juce::AudioProcessorGraph::Node::Ptr outputMeterNode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};