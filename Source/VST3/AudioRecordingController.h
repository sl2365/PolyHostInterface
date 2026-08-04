#pragma once

#include <JuceHeader.h>
#include <atomic>

class AudioRecordingController final
{
public:
    struct Status
    {
        bool armed = false;
        bool recording = false;
        double sampleRate = 0.0;
        int bitDepth = 24;
        juce::int64 recordedSamples = 0;
        juce::uint32 droppedBlocks = 0;
        juce::File activeFile;
        juce::File lastCompletedFile;
        juce::String message;
    };

    AudioRecordingController();
    ~AudioRecordingController();

    void prepareToPlay(double sampleRate, int maximumBlockSize);
    void releaseResources();
    void processAudioBlock(const juce::AudioBuffer<float>& buffer) noexcept;

    juce::Result startRecording();
    juce::Result armRecording();
    void beginCapturing() noexcept;
    void stopRecording(const juce::String& reason = "Recording stopped");

    bool isRecording() const noexcept;
    bool isCapturing() const noexcept;
    Status getStatus() const;

    static juce::File getRecordingsDirectory();

private:
    juce::File createUniqueRecordingFile() const;
    void stopRecordingInternal(const juce::String& reason);
    void waitForAudioCallbacksToFinish() const noexcept;
    void setStatusMessage(const juce::String& message);

    juce::TimeSliceThread writerThread { "PHI Audio Recording Writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;

    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<bool> captureEnabled { false };
    mutable std::atomic<int> activeAudioCallbacks { 0 };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentMaximumBlockSize { 0 };
    std::atomic<juce::int64> recordedSamples { 0 };
    std::atomic<juce::uint32> droppedBlocks { 0 };

    juce::CriticalSection lifecycleLock;
    mutable juce::CriticalSection stateLock;
    juce::File activeFile;
    juce::File lastCompletedFile;
    juce::String statusMessage { "Ready" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRecordingController)
};
