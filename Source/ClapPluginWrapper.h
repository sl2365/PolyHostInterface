#pragma once
#include <JuceHeader.h>
#include <clap/clap.h>
#include <clap/process.h>
#include <clap/audio-buffer.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/gui.h>
#include <clap/ext/state.h>
#include <cstring>
#include <vector>
#include <variant>

class ClapPluginWrapper final : public juce::AudioPluginInstance
{
public:
    explicit ClapPluginWrapper(const juce::File& pluginFile);
    ~ClapPluginWrapper() override;

    bool didLastStateLoadSucceed() const noexcept;
    void clearLastStateLoadResult() noexcept;

    void fillInPluginDescription(juce::PluginDescription& description) const override;

    bool isLoaded() const noexcept;
    juce::String getLastError() const noexcept;
    juce::String getPluginName() const noexcept;

    bool hasAudioInputPort() const noexcept;
    bool hasAudioOutputPort() const noexcept;
    bool isSynthLikePlugin() const noexcept;
    bool supportsGui() const noexcept;
    bool supportsEmbeddedGui() const noexcept;

    bool createEmbeddedEditorIfNeeded();
    void destroyEmbeddedEditor();
    bool attachGuiToHostWindow(void* nativeWindowHandle);
    void detachGuiFromHostWindow();
    void setEmbeddedGuiVisible(bool shouldBeVisible);
    juce::Rectangle<int> getEmbeddedGuiSize() const noexcept;
    bool refreshEmbeddedGuiSize();

    const juce::String getName() const override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getInputChannelName(int channelIndex) const override;
    const juce::String getOutputChannelName(int channelIndex) const override;
    bool isInputChannelStereoPair(int index) const override;
    bool isOutputChannelStereoPair(int index) const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;

    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    class PlaceholderEditor;

    bool lastStateLoadSucceeded = true;

    bool loadModule();
    void unloadModule();
    bool createPluginInstance();
    void inspectAudioPorts();

    static uint32_t CLAP_ABI inputEventsSize(const clap_input_events* list);
    static const clap_event_header_t* CLAP_ABI inputEventsGet(const clap_input_events* list, uint32_t index);
    static bool CLAP_ABI outputEventsTryPush(const clap_output_events* list, const clap_event_header_t* event);

    struct InputEventList
    {
        std::vector<clap_event_midi_t> midiEvents;
        std::vector<const clap_event_header_t*> eventPtrs;

        void clear()
        {
            midiEvents.clear();
            eventPtrs.clear();
        }
    };

    static const clap_input_events_t* getInputEventsAdaptor() noexcept;
    void buildInputEventsFromMidi(const juce::MidiBuffer& midiMessages);

    InputEventList currentInputEvents;

    juce::File moduleFile;
    juce::String pluginName { "CLAP Plugin" };
    juce::String lastError;

    void* libraryHandle = nullptr;
    const clap_plugin_entry* entry = nullptr;
    const clap_plugin_factory* factory = nullptr;
    const clap_plugin* plugin = nullptr;
    const clap_plugin_descriptor* descriptor = nullptr;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    bool activated = false;
    int64_t steadyTime = 0;

    bool guiCreated = false;
    bool guiAttached = false;
    bool guiVisible = false;
    uint32_t guiWidth = 520;
    uint32_t guiHeight = 180;

    std::vector<float*> inputChannelPointers;
    std::vector<float*> outputChannelPointers;
    std::vector<juce::AudioBuffer<float>> scratchBuffers;

    clap_audio_buffer_t clapInputBuffer {};
    clap_audio_buffer_t clapOutputBuffer {};

    clap_input_events_t emptyInputEvents {};
    clap_output_events_t emptyOutputEvents {};

    bool allowProcessing = true;

    const clap_plugin_audio_ports_t* audioPorts = nullptr;
    const clap_plugin_gui_t* guiExtension = nullptr;
    const clap_plugin_state_t* stateExtension = nullptr;
    uint32_t mainInputChannelCount = 0;
    uint32_t mainOutputChannelCount = 0;
    bool hasAudioInput = false;
    bool hasAudioOutput = false;
    bool isSimpleStereoProcessor = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClapPluginWrapper)
};