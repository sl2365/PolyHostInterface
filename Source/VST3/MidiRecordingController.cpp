#include "MidiRecordingController.h"
#include "DebugLog.h"
#include <cmath>
#include <new>

MidiRecordingController::MidiRecordingController()
    : juce::Thread("PHI MIDI Recording Writer")
{
}

MidiRecordingController::~MidiRecordingController()
{
    stopRecording("Recording stopped: PHI is closing");
    signalThreadShouldExit();
    pendingEventSignal.signal();
    stopThread(5000);
}

void MidiRecordingController::prepareToPlay(double sampleRate,
                                            int maximumBlockSize)
{
    juce::ignoreUnused(maximumBlockSize);

    const double validSampleRate = sampleRate > 0.0 ? sampleRate : 0.0;
    const double previousSampleRate = currentSampleRate.exchange(validSampleRate);

    if (isRecording()
        && (validSampleRate <= 0.0
            || previousSampleRate <= 0.0
            || std::abs(validSampleRate - previousSampleRate) > 0.5))
    {
        stopRecording("Recording stopped: audio device sample rate changed");
    }
}

void MidiRecordingController::releaseResources()
{
    if (isRecording())
        stopRecording("Recording stopped: audio device was stopped");

    currentSampleRate.store(0.0, std::memory_order_release);
}

juce::Result MidiRecordingController::armRecording()
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

    const auto options = getOptions();

    if (! options.externalNotes
        && ! options.externalControllers
        && ! options.hostedOutput)
    {
        const juce::String error = "Select at least one MIDI source to record.";
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
    auto testOutputStream = recordingFile.createOutputStream();

    if (testOutputStream == nullptr || ! testOutputStream->openedOk())
    {
        const auto error = "The MIDI file could not be created:\n\n"
                         + recordingFile.getFullPathName();
        setStatusMessage(error);
        testOutputStream.reset();
        recordingFile.deleteFile();
        return juce::Result::fail(error);
    }

    testOutputStream.reset();

    if (pendingEvents == nullptr)
    {
        try
        {
            pendingEvents =
                std::make_unique<PendingEvent[]>(pendingQueueCapacity);
        }
        catch (const std::bad_alloc&)
        {
            const juce::String error =
                "The MIDI recording buffer could not be allocated.";
            setStatusMessage(error);
            recordingFile.deleteFile();
            return juce::Result::fail(error);
        }
    }

    if (! isThreadRunning() && ! startThread())
    {
        const juce::String error =
            "The MIDI recording writer thread could not be started.";
        setStatusMessage(error);
        recordingFile.deleteFile();
        return juce::Result::fail(error);
    }

    waitForPendingEventsToDrain();

    {
        const juce::ScopedLock eventScopedLock(eventDataLock);
        externalEvents.clear();
        hostedEvents.clear();
        tempoPoints.clear();
    }

    recordedSamples.store(0, std::memory_order_release);
    recordedEvents.store(0, std::memory_order_release);
    droppedEvents.store(0, std::memory_order_release);
    captureEnabled.store(false, std::memory_order_release);
    armedExternalNotes.store(options.externalNotes, std::memory_order_release);
    armedExternalControllers.store(options.externalControllers,
                                   std::memory_order_release);
    armedHostedOutput.store(options.hostedOutput, std::memory_order_release);
    lastQueuedTempoBpm = 0.0;

    {
        const juce::ScopedLock stateScopedLock(stateLock);
        activeFile = recordingFile;
        statusMessage = "Armed";
    }

    armed.store(true, std::memory_order_release);

    DebugLog::write("[Recording] MIDI recording armed | file="
                    + recordingFile.getFullPathName()
                    + " | sampleRate="
                    + juce::String(sampleRate, 1)
                    + " | externalNotes="
                    + juce::String(options.externalNotes ? 1 : 0)
                    + " | externalControllers="
                    + juce::String(options.externalControllers ? 1 : 0)
                    + " | hostedOutput="
                    + juce::String(options.hostedOutput ? 1 : 0));

    return juce::Result::ok();
}

void MidiRecordingController::beginCapturing(double tempoBpm) noexcept
{
    activeMidiCallbacks.fetch_add(1, std::memory_order_acq_rel);

    if (armed.load(std::memory_order_acquire))
    {
        lastQueuedTempoBpm = 0.0;
        captureEnabled.store(true, std::memory_order_release);
        enqueueTempoIfChanged(0, tempoBpm);
    }

    activeMidiCallbacks.fetch_sub(1, std::memory_order_acq_rel);
}

void MidiRecordingController::stopRecording(const juce::String& reason)
{
    const juce::ScopedLock lifecycleScopedLock(lifecycleLock);
    stopRecordingInternal(reason);
}

void MidiRecordingController::processExternalMidi(
    const juce::MidiBuffer& midiMessages,
    juce::int64 blockStartSample,
    int captureStartOffset,
    double tempoBpm) noexcept
{
    if (! captureEnabled.load(std::memory_order_acquire)
        || ! armed.load(std::memory_order_acquire))
        return;

    activeMidiCallbacks.fetch_add(1, std::memory_order_acq_rel);

    if (! captureEnabled.load(std::memory_order_acquire)
        || ! armed.load(std::memory_order_acquire))
    {
        activeMidiCallbacks.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    enqueueTempoIfChanged(blockStartSample, tempoBpm);

    const bool shouldRecordNotes =
        armedExternalNotes.load(std::memory_order_relaxed);
    const bool shouldRecordControllers =
        armedExternalControllers.load(std::memory_order_relaxed);
    const int validStartOffset = juce::jmax(0, captureStartOffset);
    bool queuedAny = false;

    for (const auto metadata : midiMessages)
    {
        if (metadata.numBytes <= 0 || metadata.numBytes > 3)
            continue;

        const auto& message = metadata.getMessage();
        const int samplePosition = metadata.samplePosition;

        if (samplePosition < validStartOffset)
            continue;

        const bool shouldRecord =
            (shouldRecordNotes && isNoteMessage(message))
            || (shouldRecordControllers && isControllerMessage(message));

        if (! shouldRecord)
            continue;

        queuedAny = enqueueMidiMessage(
                        PendingKind::ExternalMidi,
                        message,
                        blockStartSample
                            + samplePosition
                            - validStartOffset)
                    || queuedAny;
    }

    if (queuedAny)
        pendingEventSignal.signal();

    activeMidiCallbacks.fetch_sub(1, std::memory_order_acq_rel);
}

void MidiRecordingController::processHostedMidiOutput(
    const juce::MidiBuffer& midiMessages,
    juce::int64 blockStartSample,
    int captureStartOffset) noexcept
{
    if (! captureEnabled.load(std::memory_order_acquire)
        || ! armed.load(std::memory_order_acquire)
        || ! armedHostedOutput.load(std::memory_order_relaxed))
    {
        return;
    }

    activeMidiCallbacks.fetch_add(1, std::memory_order_acq_rel);

    if (! captureEnabled.load(std::memory_order_acquire)
        || ! armed.load(std::memory_order_acquire))
    {
        activeMidiCallbacks.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    const int validStartOffset = juce::jmax(0, captureStartOffset);
    bool queuedAny = false;

    for (const auto metadata : midiMessages)
    {
        if (metadata.numBytes <= 0 || metadata.numBytes > 3)
            continue;

        const auto& message = metadata.getMessage();
        const int samplePosition = metadata.samplePosition;

        if (samplePosition < validStartOffset
            || ! isRecordableChannelMessage(message))
        {
            continue;
        }

        queuedAny = enqueueMidiMessage(
                        PendingKind::HostedMidi,
                        message,
                        blockStartSample
                            + samplePosition
                            - validStartOffset)
                    || queuedAny;
    }

    if (queuedAny)
        pendingEventSignal.signal();

    activeMidiCallbacks.fetch_sub(1, std::memory_order_acq_rel);
}

void MidiRecordingController::advanceCapturedSamples(int numSamples) noexcept
{
    if (captureEnabled.load(std::memory_order_acquire)
        && armed.load(std::memory_order_acquire)
        && numSamples > 0)
    {
        recordedSamples.fetch_add(numSamples, std::memory_order_relaxed);
    }
}

bool MidiRecordingController::isRecording() const noexcept
{
    return armed.load(std::memory_order_acquire);
}

bool MidiRecordingController::isCapturing() const noexcept
{
    return captureEnabled.load(std::memory_order_acquire)
           && isRecording();
}

MidiRecordingController::Status MidiRecordingController::getStatus() const
{
    Status status;
    status.armed = isRecording();
    status.recording = isCapturing();
    status.sampleRate = currentSampleRate.load(std::memory_order_acquire);
    status.recordedSamples = recordedSamples.load(std::memory_order_acquire);
    status.recordedEvents = recordedEvents.load(std::memory_order_acquire);
    status.droppedEvents = droppedEvents.load(std::memory_order_acquire);
    status.options.externalNotes =
        armedExternalNotes.load(std::memory_order_acquire);
    status.options.externalControllers =
        armedExternalControllers.load(std::memory_order_acquire);
    status.options.hostedOutput =
        armedHostedOutput.load(std::memory_order_acquire);

    const juce::ScopedLock stateScopedLock(stateLock);
    status.activeFile = activeFile;
    status.lastCompletedFile = lastCompletedFile;
    status.message = statusMessage;
    return status;
}

void MidiRecordingController::setMidiModeSelected(bool shouldSelectMidi) noexcept
{
    if (! isRecording())
        midiModeSelected.store(shouldSelectMidi, std::memory_order_release);
}

bool MidiRecordingController::isMidiModeSelected() const noexcept
{
    return midiModeSelected.load(std::memory_order_acquire);
}

void MidiRecordingController::setOptions(const Options& newOptions) noexcept
{
    if (isRecording())
        return;

    recordExternalNotes.store(newOptions.externalNotes,
                              std::memory_order_release);
    recordExternalControllers.store(newOptions.externalControllers,
                                    std::memory_order_release);
    recordHostedOutput.store(newOptions.hostedOutput,
                             std::memory_order_release);
    armedExternalNotes.store(newOptions.externalNotes,
                             std::memory_order_release);
    armedExternalControllers.store(newOptions.externalControllers,
                                   std::memory_order_release);
    armedHostedOutput.store(newOptions.hostedOutput,
                            std::memory_order_release);
}

MidiRecordingController::Options MidiRecordingController::getOptions() const noexcept
{
    Options options;
    options.externalNotes =
        recordExternalNotes.load(std::memory_order_acquire);
    options.externalControllers =
        recordExternalControllers.load(std::memory_order_acquire);
    options.hostedOutput =
        recordHostedOutput.load(std::memory_order_acquire);
    return options;
}

juce::File MidiRecordingController::getRecordingsDirectory()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getChildFile("Recordings");
}

void MidiRecordingController::run()
{
    while (! threadShouldExit())
    {
        drainPendingEvents();
        pendingEventSignal.wait(20);
    }

    drainPendingEvents();
}

void MidiRecordingController::drainPendingEvents()
{
    if (pendingEvents == nullptr)
        return;

    for (;;)
    {
        int startIndex1 = 0;
        int blockSize1 = 0;
        int startIndex2 = 0;
        int blockSize2 = 0;

        pendingFifo.prepareToRead(1024,
                                  startIndex1,
                                  blockSize1,
                                  startIndex2,
                                  blockSize2);

        const int totalToRead = blockSize1 + blockSize2;

        if (totalToRead <= 0)
            return;

        {
            const juce::ScopedLock eventScopedLock(eventDataLock);

            auto appendRange = [this](int startIndex, int count)
            {
                for (int offset = 0; offset < count; ++offset)
                {
                    const auto& event =
                        pendingEvents[static_cast<size_t>(startIndex + offset)];

                    try
                    {
                        if (event.kind == PendingKind::ExternalMidi)
                        {
                            externalEvents.push_back(event);
                        }
                        else if (event.kind == PendingKind::HostedMidi)
                        {
                            hostedEvents.push_back(event);
                        }
                        else
                        {
                            tempoPoints.push_back(
                                { event.samplePosition, event.tempoBpm });
                        }
                    }
                    catch (const std::bad_alloc&)
                    {
                        droppedEvents.fetch_add(1,
                                                std::memory_order_relaxed);

                        if (event.kind != PendingKind::Tempo)
                        {
                            recordedEvents.fetch_sub(
                                1,
                                std::memory_order_relaxed);
                        }
                    }
                }
            };

            appendRange(startIndex1, blockSize1);
            appendRange(startIndex2, blockSize2);
        }

        pendingFifo.finishedRead(totalToRead);
    }
}

bool MidiRecordingController::enqueueEvent(const PendingEvent& event) noexcept
{
    if (pendingEvents == nullptr)
    {
        droppedEvents.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    int startIndex1 = 0;
    int blockSize1 = 0;
    int startIndex2 = 0;
    int blockSize2 = 0;

    pendingFifo.prepareToWrite(1,
                               startIndex1,
                               blockSize1,
                               startIndex2,
                               blockSize2);

    if (blockSize1 + blockSize2 <= 0)
    {
        droppedEvents.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const int writeIndex = blockSize1 > 0 ? startIndex1 : startIndex2;
    pendingEvents[static_cast<size_t>(writeIndex)] = event;
    pendingFifo.finishedWrite(1);
    return true;
}

bool MidiRecordingController::enqueueMidiMessage(
    PendingKind kind,
    const juce::MidiMessage& message,
    juce::int64 samplePosition) noexcept
{
    const int dataSize = message.getRawDataSize();

    if (dataSize <= 0
        || dataSize > 3
        || ! isRecordableChannelMessage(message))
    {
        return false;
    }

    if (recordedEvents.load(std::memory_order_relaxed)
        >= maximumCapturedMidiEvents)
    {
        droppedEvents.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    PendingEvent event;
    event.kind = kind;
    event.samplePosition = juce::jmax<juce::int64>(0, samplePosition);
    event.dataSize = dataSize;

    const auto* rawData = message.getRawData();

    for (int byteIndex = 0; byteIndex < dataSize; ++byteIndex)
        event.data[static_cast<size_t>(byteIndex)] = rawData[byteIndex];

    if (! enqueueEvent(event))
        return false;

    recordedEvents.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void MidiRecordingController::enqueueTempoIfChanged(
    juce::int64 samplePosition,
    double tempoBpm) noexcept
{
    const double validTempo = juce::jlimit(20.0, 300.0, tempoBpm);

    if (std::abs(validTempo - lastQueuedTempoBpm) < 0.0001)
        return;

    PendingEvent event;
    event.kind = PendingKind::Tempo;
    event.samplePosition = juce::jmax<juce::int64>(0, samplePosition);
    event.tempoBpm = validTempo;

    if (enqueueEvent(event))
    {
        lastQueuedTempoBpm = validTempo;
        pendingEventSignal.signal();
    }
}

bool MidiRecordingController::isNoteMessage(
    const juce::MidiMessage& message) noexcept
{
    return message.isNoteOn() || message.isNoteOff();
}

bool MidiRecordingController::isControllerMessage(
    const juce::MidiMessage& message) noexcept
{
    return message.isController()
           || message.isPitchWheel()
           || message.isAftertouch()
           || message.isChannelPressure()
           || message.isProgramChange();
}

bool MidiRecordingController::isRecordableChannelMessage(
    const juce::MidiMessage& message) noexcept
{
    return message.getChannel() > 0
           && (isNoteMessage(message)
               || isControllerMessage(message));
}

juce::File MidiRecordingController::createUniqueRecordingFile() const
{
    auto directory = getRecordingsDirectory();
    const auto baseName = "Recording "
                        + juce::Time::getCurrentTime().formatted(
                            "%Y-%m-%d %H-%M-%S");

    auto candidate = directory.getChildFile(baseName + ".mid");
    int suffix = 2;

    while (candidate.exists())
    {
        candidate = directory.getChildFile(baseName
                                            + " ("
                                            + juce::String(suffix)
                                            + ").mid");
        ++suffix;
    }

    return candidate;
}

juce::Result MidiRecordingController::writeCompletedMidiFile(
    const juce::File& file,
    juce::int64 totalSamples)
{
    const double sampleRate = currentSampleRate.load(std::memory_order_acquire);

    if (sampleRate <= 0.0)
        return juce::Result::fail("The MIDI recording sample rate is unavailable.");

    if (tempoPoints.empty())
        tempoPoints.push_back({ 0, 120.0 });

    if (tempoPoints.front().samplePosition > 0)
    {
        tempoPoints.insert(tempoPoints.begin(),
                           { 0, tempoPoints.front().tempoBpm });
    }

    auto samplesToTicks =
        [this, sampleRate](juce::int64 targetSample)
        {
            double ticks = 0.0;
            juce::int64 previousSample = 0;
            double tempoBpm = tempoPoints.front().tempoBpm;

            for (size_t index = 1; index < tempoPoints.size(); ++index)
            {
                const auto& point = tempoPoints[index];

                if (point.samplePosition > targetSample)
                    break;

                ticks += static_cast<double>(point.samplePosition - previousSample)
                         / sampleRate
                         * (tempoBpm / 60.0)
                         * ticksPerQuarterNote;
                previousSample = point.samplePosition;
                tempoBpm = point.tempoBpm;
            }

            ticks += static_cast<double>(targetSample - previousSample)
                     / sampleRate
                     * (tempoBpm / 60.0)
                     * ticksPerQuarterNote;
            return ticks;
        };

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(ticksPerQuarterNote);

    juce::MidiMessageSequence tempoTrack;
    auto tempoTrackName = juce::MidiMessage::textMetaEvent(3, "Tempo");
    tempoTrackName.setTimeStamp(0.0);
    tempoTrack.addEvent(tempoTrackName);

    auto timeSignature = juce::MidiMessage::timeSignatureMetaEvent(4, 4);
    timeSignature.setTimeStamp(0.0);
    tempoTrack.addEvent(timeSignature);

    for (const auto& point : tempoPoints)
    {
        const int microsecondsPerQuarterNote =
            juce::roundToInt(60000000.0
                             / juce::jlimit(20.0, 300.0, point.tempoBpm));
        auto tempoMessage =
            juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote);
        tempoMessage.setTimeStamp(samplesToTicks(point.samplePosition));
        tempoTrack.addEvent(tempoMessage);
    }

    const double finalTick = samplesToTicks(juce::jmax<juce::int64>(0,
                                                                    totalSamples));
    auto tempoEnd = juce::MidiMessage::endOfTrack();
    tempoEnd.setTimeStamp(finalTick + 1.0);
    tempoTrack.addEvent(tempoEnd);
    midiFile.addTrack(tempoTrack);

    auto addSourceTrack =
        [&](const std::vector<PendingEvent>& events,
            const juce::String& trackName)
        {
            juce::MidiMessageSequence track;
            auto nameMessage = juce::MidiMessage::textMetaEvent(3, trackName);
            nameMessage.setTimeStamp(0.0);
            track.addEvent(nameMessage);

            std::array<std::array<int, 128>, 16> heldNotes {};

            for (const auto& event : events)
            {
                auto message = juce::MidiMessage(event.data.data(),
                                                 event.dataSize,
                                                 samplesToTicks(event.samplePosition));
                track.addEvent(message);

                if (message.isNoteOn())
                {
                    ++heldNotes[static_cast<size_t>(message.getChannel() - 1)]
                               [static_cast<size_t>(message.getNoteNumber())];
                }
                else if (message.isNoteOff())
                {
                    auto& heldCount =
                        heldNotes[static_cast<size_t>(message.getChannel() - 1)]
                                 [static_cast<size_t>(message.getNoteNumber())];
                    heldCount = juce::jmax(0, heldCount - 1);
                }
            }

            for (int channel = 1; channel <= 16; ++channel)
            {
                for (int note = 0; note < 128; ++note)
                {
                    if (heldNotes[static_cast<size_t>(channel - 1)]
                                 [static_cast<size_t>(note)] <= 0)
                    {
                        continue;
                    }

                    auto noteOff = juce::MidiMessage::noteOff(channel, note);
                    noteOff.setTimeStamp(finalTick);
                    track.addEvent(noteOff);
                }
            }

            auto endMessage = juce::MidiMessage::endOfTrack();
            endMessage.setTimeStamp(finalTick + 1.0);
            track.addEvent(endMessage);
            track.updateMatchedPairs();
            midiFile.addTrack(track);
        };

    if (armedExternalNotes.load(std::memory_order_acquire)
        || armedExternalControllers.load(std::memory_order_acquire))
    {
        addSourceTrack(externalEvents, "External MIDI");
    }

    if (armedHostedOutput.load(std::memory_order_acquire))
        addSourceTrack(hostedEvents, "Hosted MIDI Output");

    file.deleteFile();
    auto outputStream = file.createOutputStream();

    if (outputStream == nullptr || ! outputStream->openedOk())
        return juce::Result::fail("The MIDI file could not be opened for writing.");

    if (! midiFile.writeTo(*outputStream, 1))
        return juce::Result::fail("The MIDI file could not be written.");

    outputStream->flush();
    return juce::Result::ok();
}

void MidiRecordingController::stopRecordingInternal(const juce::String& reason)
{
    captureEnabled.store(false, std::memory_order_release);

    const bool wasArmed = armed.exchange(false, std::memory_order_acq_rel);

    if (! wasArmed)
        return;

    waitForMidiCallbacksToFinish();
    captureEnabled.store(false, std::memory_order_release);
    waitForPendingEventsToDrain();

    juce::File completedFile;

    {
        const juce::ScopedLock stateScopedLock(stateLock);
        completedFile = activeFile;
        activeFile = {};
        statusMessage = "Finalising recording...";
    }

    const auto finalEventCount = recordedEvents.load(std::memory_order_relaxed);
    const auto finalSampleCount = recordedSamples.load(std::memory_order_relaxed);
    juce::Result writeResult = juce::Result::ok();

    if (finalEventCount > 0)
    {
        const juce::ScopedLock eventScopedLock(eventDataLock);

        try
        {
            writeResult =
                writeCompletedMidiFile(completedFile, finalSampleCount);
        }
        catch (const std::bad_alloc&)
        {
            writeResult = juce::Result::fail(
                "There was not enough memory to finalise the MIDI file.");
        }
    }
    else if (completedFile.existsAsFile())
    {
        completedFile.deleteFile();
    }

    const bool shouldKeepFile =
        finalEventCount > 0
        && writeResult.wasOk()
        && completedFile.existsAsFile();

    if (! shouldKeepFile && completedFile.existsAsFile())
        completedFile.deleteFile();

    {
        const juce::ScopedLock stateScopedLock(stateLock);

        if (shouldKeepFile)
            lastCompletedFile = completedFile;

        if (writeResult.failed())
        {
            statusMessage = "MIDI recording failed: "
                          + writeResult.getErrorMessage();
        }
        else if (finalEventCount == 0)
        {
            statusMessage = "No MIDI events were recorded";
        }
        else
        {
            statusMessage = reason.isNotEmpty() ? reason : "Ready";
        }
    }

    if (shouldKeepFile)
    {
        DebugLog::write("[Recording] MIDI recording stopped | file="
                        + completedFile.getFullPathName()
                        + " | events="
                        + juce::String(finalEventCount)
                        + " | droppedEvents="
                        + juce::String(droppedEvents.load(std::memory_order_relaxed))
                        + " | reason="
                        + (reason.isNotEmpty() ? reason : "Ready"));
    }
    else if (writeResult.failed())
    {
        DebugLog::write("[Recording] MIDI recording write failed | error="
                        + writeResult.getErrorMessage());
    }
    else
    {
        DebugLog::write("[Recording] empty MIDI recording discarded");
    }
}

void MidiRecordingController::waitForMidiCallbacksToFinish() const noexcept
{
    while (activeMidiCallbacks.load(std::memory_order_acquire) > 0)
        juce::Thread::yield();
}

void MidiRecordingController::waitForPendingEventsToDrain() noexcept
{
    if (! isThreadRunning())
    {
        drainPendingEvents();
        return;
    }

    pendingEventSignal.signal();

    while (pendingFifo.getNumReady() > 0)
        juce::Thread::sleep(1);
}

void MidiRecordingController::setStatusMessage(const juce::String& message)
{
    const juce::ScopedLock stateScopedLock(stateLock);
    statusMessage = message;
}
