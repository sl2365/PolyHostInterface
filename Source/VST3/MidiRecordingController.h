#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class MidiRecordingController final : private juce::Thread
{
public:
    struct Options
    {
        bool externalNotes = true;
        bool externalControllers = true;
        bool hostedOutput = false;
    };

    struct Status
    {
        bool armed = false;
        bool recording = false;
        double sampleRate = 0.0;
        juce::int64 recordedSamples = 0;
        juce::uint32 recordedEvents = 0;
        juce::uint32 droppedEvents = 0;
        Options options;
        juce::File activeFile;
        juce::File lastCompletedFile;
        juce::String message;
    };

    MidiRecordingController();
    ~MidiRecordingController() override;

    void prepareToPlay(double sampleRate, int maximumBlockSize);
    void releaseResources();

    juce::Result armRecording();
    void beginCapturing(double tempoBpm) noexcept;
    void stopRecording(const juce::String& reason = "Recording stopped");

    void processExternalMidi(const juce::MidiBuffer& midiMessages,
                             juce::int64 blockStartSample,
                             int captureStartOffset,
                             double tempoBpm) noexcept;

    void processHostedMidiOutput(const juce::MidiBuffer& midiMessages,
                                 juce::int64 blockStartSample,
                                 int captureStartOffset) noexcept;

    void advanceCapturedSamples(int numSamples) noexcept;

    bool isRecording() const noexcept;
    bool isCapturing() const noexcept;
    Status getStatus() const;

    void setMidiModeSelected(bool shouldSelectMidi) noexcept;
    bool isMidiModeSelected() const noexcept;

    void setOptions(const Options& newOptions) noexcept;
    Options getOptions() const noexcept;

    static juce::File getRecordingsDirectory();

private:
    enum class PendingKind : juce::uint8
    {
        ExternalMidi,
        HostedMidi,
        Tempo
    };

    struct PendingEvent
    {
        PendingKind kind = PendingKind::ExternalMidi;
        juce::int64 samplePosition = 0;
        double tempoBpm = 0.0;
        std::array<juce::uint8, 3> data {};
        int dataSize = 0;
    };

    struct TempoPoint
    {
        juce::int64 samplePosition = 0;
        double tempoBpm = 120.0;
    };

    static constexpr int pendingQueueCapacity = 65536;
    static constexpr juce::uint32 maximumCapturedMidiEvents = 1000000;
    static constexpr int ticksPerQuarterNote = 960;

    void run() override;
    void drainPendingEvents();
    bool enqueueEvent(const PendingEvent& event) noexcept;
    bool enqueueMidiMessage(PendingKind kind,
                            const juce::MidiMessage& message,
                            juce::int64 samplePosition) noexcept;
    void enqueueTempoIfChanged(juce::int64 samplePosition,
                               double tempoBpm) noexcept;

    static bool isNoteMessage(const juce::MidiMessage& message) noexcept;
    static bool isControllerMessage(const juce::MidiMessage& message) noexcept;
    static bool isRecordableChannelMessage(const juce::MidiMessage& message) noexcept;

    juce::File createUniqueRecordingFile() const;
    juce::Result writeCompletedMidiFile(const juce::File& file,
                                        juce::int64 totalSamples);
    void stopRecordingInternal(const juce::String& reason);
    void waitForMidiCallbacksToFinish() const noexcept;
    void waitForPendingEventsToDrain() noexcept;
    void setStatusMessage(const juce::String& message);

    std::unique_ptr<PendingEvent[]> pendingEvents;
    juce::AbstractFifo pendingFifo { pendingQueueCapacity };
    juce::WaitableEvent pendingEventSignal;

    std::vector<PendingEvent> externalEvents;
    std::vector<PendingEvent> hostedEvents;
    std::vector<TempoPoint> tempoPoints;

    std::atomic<bool> armed { false };
    std::atomic<bool> captureEnabled { false };
    std::atomic<bool> midiModeSelected { false };
    std::atomic<bool> recordExternalNotes { true };
    std::atomic<bool> recordExternalControllers { true };
    std::atomic<bool> recordHostedOutput { false };
    std::atomic<bool> armedExternalNotes { true };
    std::atomic<bool> armedExternalControllers { true };
    std::atomic<bool> armedHostedOutput { false };
    mutable std::atomic<int> activeMidiCallbacks { 0 };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<juce::int64> recordedSamples { 0 };
    std::atomic<juce::uint32> recordedEvents { 0 };
    std::atomic<juce::uint32> droppedEvents { 0 };

    double lastQueuedTempoBpm = 0.0;

    juce::CriticalSection lifecycleLock;
    mutable juce::CriticalSection stateLock;
    mutable juce::CriticalSection eventDataLock;
    juce::File activeFile;
    juce::File lastCompletedFile;
    juce::String statusMessage { "Ready" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiRecordingController)
};
