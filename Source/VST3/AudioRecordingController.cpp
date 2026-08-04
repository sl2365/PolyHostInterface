#include "AudioRecordingController.h"
#include "DebugLog.h"
#include <cmath>

AudioRecordingController::AudioRecordingController()
{
}

AudioRecordingController::~AudioRecordingController()
{
    stopRecording("Recording stopped: PHI is closing");
    writerThread.stopThread(5000);
}

void AudioRecordingController::prepareToPlay(double sampleRate,
                                             int maximumBlockSize)
{
    const double validSampleRate = sampleRate > 0.0 ? sampleRate : 0.0;
    const double previousSampleRate = currentSampleRate.exchange(validSampleRate);

    currentMaximumBlockSize.store(juce::jmax(0, maximumBlockSize));

    if (isRecording()
        && (validSampleRate <= 0.0
            || previousSampleRate <= 0.0
            || std::abs(validSampleRate - previousSampleRate) > 0.5))
    {
        stopRecording("Recording stopped: audio device sample rate changed");
    }
}

void AudioRecordingController::releaseResources()
{
    if (isRecording())
        stopRecording("Recording stopped: audio device was stopped");

    currentSampleRate.store(0.0);
    currentMaximumBlockSize.store(0);
}

void AudioRecordingController::processAudioBlock(
    const juce::AudioBuffer<float>& buffer) noexcept
{
    if (! captureEnabled.load(std::memory_order_acquire))
        return;

    auto* writer = activeWriter.load(std::memory_order_acquire);

    if (writer == nullptr || buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    activeAudioCallbacks.fetch_add(1, std::memory_order_acq_rel);
    writer = activeWriter.load(std::memory_order_acquire);

    if (writer != nullptr)
    {
        const float* channels[2]
        {
            buffer.getReadPointer(0),
            buffer.getReadPointer(buffer.getNumChannels() > 1 ? 1 : 0)
        };

        if (writer->write(channels, buffer.getNumSamples()))
        {
            recordedSamples.fetch_add(buffer.getNumSamples(),
                                      std::memory_order_relaxed);
        }
        else
        {
            droppedBlocks.fetch_add(1, std::memory_order_relaxed);
        }
    }

    activeAudioCallbacks.fetch_sub(1, std::memory_order_acq_rel);
}

juce::Result AudioRecordingController::startRecording()
{
    const auto result = armRecording();

    if (result.wasOk())
        beginCapturing();

    return result;
}

juce::Result AudioRecordingController::armRecording()
{
    const juce::ScopedLock lifecycleScopedLock(lifecycleLock);

    stopRecordingInternal({});

    const double sampleRate = currentSampleRate.load(std::memory_order_acquire);

    if (sampleRate <= 0.0)
    {
        const juce::String error = "No active audio device sample rate is available.";
        setStatusMessage(error);
        return juce::Result::fail(error);
    }

    auto recordingsDirectory = getRecordingsDirectory();
    const auto directoryResult = recordingsDirectory.createDirectory();

    if (directoryResult.failed())
    {
        const auto error = "The Recordings folder could not be created:\n\n"
                         + recordingsDirectory.getFullPathName()
                         + "\n\n"
                         + directoryResult.getErrorMessage();
        setStatusMessage(error);
        return juce::Result::fail(error);
    }

    auto recordingFile = createUniqueRecordingFile();
    auto fileOutputStream = recordingFile.createOutputStream();

    if (fileOutputStream == nullptr || ! fileOutputStream->openedOk())
    {
        const auto error = "The WAV file could not be created:\n\n"
                         + recordingFile.getFullPathName();
        setStatusMessage(error);
        fileOutputStream.reset();
        recordingFile.deleteFile();
        return juce::Result::fail(error);
    }

    std::unique_ptr<juce::OutputStream> outputStream(fileOutputStream.release());
    juce::WavAudioFormat wavFormat;
    using WriterOptions = juce::AudioFormatWriterOptions;
    auto wavWriter = wavFormat.createWriterFor(
        outputStream,
        WriterOptions()
            .withSampleRate(sampleRate)
            .withChannelLayout(juce::AudioChannelSet::stereo())
            .withBitsPerSample(24));

    if (wavWriter == nullptr)
    {
        const auto error = "The 24-bit stereo WAV writer could not be created.";
        setStatusMessage(error);
        outputStream.reset();
        recordingFile.deleteFile();
        return juce::Result::fail(error);
    }

    if (! writerThread.isThreadRunning())
        writerThread.startThread();

    const int requestedBufferSamples =
        juce::jmax(32768,
                   juce::jmax(juce::roundToInt(sampleRate * 2.0),
                              currentMaximumBlockSize.load(std::memory_order_relaxed) * 8));

    auto newThreadedWriter =
        std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
            wavWriter.release(),
            writerThread,
            requestedBufferSamples);

    newThreadedWriter->setFlushInterval(juce::roundToInt(sampleRate));

    auto* writerToActivate = newThreadedWriter.get();

    {
        const juce::ScopedLock stateScopedLock(stateLock);
        threadedWriter = std::move(newThreadedWriter);
        activeFile = recordingFile;
        statusMessage = "Armed";
    }

    recordedSamples.store(0, std::memory_order_release);
    droppedBlocks.store(0, std::memory_order_release);
    captureEnabled.store(false, std::memory_order_release);
    activeWriter.store(writerToActivate, std::memory_order_release);

    DebugLog::write("[Recording] audio recording armed | file="
                    + recordingFile.getFullPathName()
                    + " | sampleRate="
                    + juce::String(sampleRate, 1)
                    + " | channels=2 | bitDepth=24");

    return juce::Result::ok();
}

void AudioRecordingController::beginCapturing() noexcept
{
    if (activeWriter.load(std::memory_order_acquire) != nullptr)
        captureEnabled.store(true, std::memory_order_release);
}

void AudioRecordingController::stopRecording(const juce::String& reason)
{
    const juce::ScopedLock lifecycleScopedLock(lifecycleLock);
    stopRecordingInternal(reason);
}

bool AudioRecordingController::isRecording() const noexcept
{
    return activeWriter.load(std::memory_order_acquire) != nullptr;
}

bool AudioRecordingController::isCapturing() const noexcept
{
    return captureEnabled.load(std::memory_order_acquire)
           && isRecording();
}

AudioRecordingController::Status AudioRecordingController::getStatus() const
{
    Status status;
    status.armed = isRecording();
    status.recording = isCapturing();
    status.sampleRate = currentSampleRate.load(std::memory_order_acquire);
    status.recordedSamples = recordedSamples.load(std::memory_order_acquire);
    status.droppedBlocks = droppedBlocks.load(std::memory_order_acquire);

    const juce::ScopedLock stateScopedLock(stateLock);
    status.activeFile = activeFile;
    status.lastCompletedFile = lastCompletedFile;
    status.message = statusMessage;
    return status;
}

juce::File AudioRecordingController::getRecordingsDirectory()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getChildFile("Recordings");
}

juce::File AudioRecordingController::createUniqueRecordingFile() const
{
    auto directory = getRecordingsDirectory();
    const auto baseName = "Recording "
                        + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S");

    auto candidate = directory.getChildFile(baseName + ".wav");
    int suffix = 2;

    while (candidate.exists())
    {
        candidate = directory.getChildFile(baseName
                                            + " ("
                                            + juce::String(suffix)
                                            + ").wav");
        ++suffix;
    }

    return candidate;
}

void AudioRecordingController::stopRecordingInternal(const juce::String& reason)
{
    captureEnabled.store(false, std::memory_order_release);

    auto* writerThatWasActive =
        activeWriter.exchange(nullptr, std::memory_order_acq_rel);

    if (writerThatWasActive == nullptr && threadedWriter == nullptr)
        return;

    waitForAudioCallbacksToFinish();

    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writerToClose;
    juce::File completedFile;

    {
        const juce::ScopedLock stateScopedLock(stateLock);
        writerToClose = std::move(threadedWriter);
        completedFile = activeFile;
        activeFile = {};
        statusMessage = "Finalising recording...";
    }

    writerToClose.reset();

    const auto finalSampleCount =
        recordedSamples.load(std::memory_order_relaxed);

    const bool shouldKeepFile =
        finalSampleCount > 0
        && completedFile.existsAsFile();

    if (! shouldKeepFile && completedFile.existsAsFile())
        completedFile.deleteFile();

    {
        const juce::ScopedLock stateScopedLock(stateLock);

        if (shouldKeepFile && completedFile.existsAsFile())
            lastCompletedFile = completedFile;

        statusMessage = reason.isNotEmpty() ? reason : "Ready";
    }

    if (shouldKeepFile)
    {
        DebugLog::write("[Recording] audio recording stopped | file="
                        + completedFile.getFullPathName()
                        + " | samples="
                        + juce::String(finalSampleCount)
                        + " | droppedBlocks="
                        + juce::String(droppedBlocks.load(std::memory_order_relaxed))
                        + " | reason="
                        + (reason.isNotEmpty() ? reason : "Ready"));
    }
    else if (completedFile.getFullPathName().isNotEmpty())
    {
        DebugLog::write("[Recording] empty armed recording discarded | file="
                        + completedFile.getFullPathName());
    }
}

void AudioRecordingController::waitForAudioCallbacksToFinish() const noexcept
{
    while (activeAudioCallbacks.load(std::memory_order_acquire) > 0)
        juce::Thread::yield();
}

void AudioRecordingController::setStatusMessage(const juce::String& message)
{
    const juce::ScopedLock stateScopedLock(stateLock);
    statusMessage = message;
}
