#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include <atomic>
#include <cmath>
#include <functional>

#include "AppSettings.h"
#include "ButtonStyling.h"
#include "DebugLog.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
class StandalonePlayHead final : public juce::AudioPlayHead
{
public:
    void prepareToPlay(double newSampleRate)
    {
        const juce::ScopedLock scopedLock(lock);

        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        samplePosition = 0;
    }

    void resetTimeline()
    {
        const juce::ScopedLock scopedLock(lock);
        samplePosition = 0;
    }

    void setTempoBpm(double newTempoBpm)
    {
        const juce::ScopedLock scopedLock(lock);
        tempoBpm = juce::jlimit(20.0, 300.0, newTempoBpm);
    }

    double getTempoBpm() const
    {
        const juce::ScopedLock scopedLock(lock);
        return tempoBpm;
    }

    void advance(int numSamples)
    {
        const juce::ScopedLock scopedLock(lock);
        samplePosition += juce::jmax(0, numSamples);
    }

    int getCurrentBeatInBar() const
    {
        const juce::ScopedLock scopedLock(lock);

        const double ppqPosition =
            (static_cast<double>(samplePosition) / sampleRate)
            * (tempoBpm / 60.0);

        const int beatIndex =
            static_cast<int>(std::floor(ppqPosition)) % 4;

        return (beatIndex + 4) % 4;
    }

    double getCurrentBeatProgress() const
    {
        const juce::ScopedLock scopedLock(lock);

        const double ppqPosition =
            (static_cast<double>(samplePosition) / sampleRate)
            * (tempoBpm / 60.0);

        return juce::jlimit(
            0.0,
            1.0,
            ppqPosition - std::floor(ppqPosition));
    }

    Optional<PositionInfo> getPosition() const override
    {
        const juce::ScopedLock scopedLock(lock);

        PositionInfo info;
        info.setIsPlaying(true);
        info.setIsRecording(false);
        info.setIsLooping(false);
        info.setBpm(tempoBpm);
        info.setTimeSignature(
            juce::AudioPlayHead::TimeSignature { 4, 4 });
        info.setTimeInSamples(samplePosition);
        info.setTimeInSeconds(
            static_cast<double>(samplePosition) / sampleRate);

        const double ppqPosition =
            (static_cast<double>(samplePosition) / sampleRate)
            * (tempoBpm / 60.0);

        info.setPpqPosition(ppqPosition);
        info.setPpqPositionOfLastBarStart(
            std::floor(ppqPosition / 4.0) * 4.0);

        return info;
    }

private:
    mutable juce::CriticalSection lock;
    double sampleRate = 44100.0;
    double tempoBpm = 120.0;
    juce::int64 samplePosition = 0;
};

class StandaloneMidiOutputController final
{
public:
    StandaloneMidiOutputController()
    {
        AppSettings settings;

        sendGeneratedMidiToOutput.store(
            settings.getStandaloneSendGeneratedMidiToOutput(),
            std::memory_order_release);

        midiThruEnabled.store(
            settings.getStandaloneMidiThruEnabled(),
            std::memory_order_release);

        selectedDeviceIdentifier =
            settings.getStandaloneMidiOutputDeviceIdentifier();

        if (selectedDeviceIdentifier.isNotEmpty())
            refreshSelectedOutputDevice();
    }

    ~StandaloneMidiOutputController()
    {
        const juce::ScopedLock scopedLock(outputLock);
        closeCurrentOutputLocked();
    }

    juce::String getSelectedDeviceIdentifier() const
    {
        const juce::ScopedLock scopedLock(outputLock);
        return selectedDeviceIdentifier;
    }

    bool isSelectedDeviceOpen() const
    {
        const juce::ScopedLock scopedLock(outputLock);
        return midiOutput != nullptr;
    }

    bool selectOutputDevice(const juce::String& identifier)
    {
        return selectOutputDeviceInternal(identifier, true);
    }

    bool refreshSelectedOutputDevice()
    {
        const auto identifier = getSelectedDeviceIdentifier();

        if (identifier.isEmpty())
            return true;

        bool deviceIsAvailable = false;

        for (const auto& device :
             juce::MidiOutput::getAvailableDevices())
        {
            if (device.identifier == identifier)
            {
                deviceIsAvailable = true;
                break;
            }
        }

        if (! deviceIsAvailable)
        {
            const juce::ScopedLock scopedLock(outputLock);
            closeCurrentOutputLocked();
            return false;
        }

        {
            const juce::ScopedLock scopedLock(outputLock);

            if (midiOutput != nullptr
                && selectedDeviceIdentifier == identifier)
            {
                return true;
            }
        }

        return selectOutputDeviceInternal(identifier, false);
    }

    bool getSendGeneratedMidiToOutput() const noexcept
    {
        return sendGeneratedMidiToOutput.load(
            std::memory_order_acquire);
    }

    void setSendGeneratedMidiToOutput(bool shouldSend)
    {
        const bool wasSending =
            sendGeneratedMidiToOutput.exchange(
                shouldSend,
                std::memory_order_acq_rel);

        if (wasSending && ! shouldSend)
            stopPendingOutputAndSendPanic();

        AppSettings settings;
        settings.setStandaloneSendGeneratedMidiToOutput(
            shouldSend);
    }

    bool getMidiThruEnabled() const noexcept
    {
        return midiThruEnabled.load(
            std::memory_order_acquire);
    }

    void setMidiThruEnabled(bool shouldEnable)
    {
        const bool wasEnabled =
            midiThruEnabled.exchange(
                shouldEnable,
                std::memory_order_acq_rel);

        if (wasEnabled && ! shouldEnable)
            stopPendingOutputAndSendPanic();

        AppSettings settings;
        settings.setStandaloneMidiThruEnabled(
            shouldEnable);
    }

    void sendInputMidi(const juce::MidiBuffer& midiMessages,
                       double sampleRate) noexcept
    {
        if (! midiThruEnabled.load(std::memory_order_acquire))
            return;

        sendMidiBuffer(midiMessages, sampleRate);
    }

    void sendGeneratedMidi(
        const juce::MidiBuffer& midiMessages,
        double sampleRate) noexcept
    {
        if (! sendGeneratedMidiToOutput.load(
                std::memory_order_acquire))
        {
            return;
        }

        sendMidiBuffer(midiMessages, sampleRate);
    }

private:
    bool selectOutputDeviceInternal(
        const juce::String& identifier,
        bool shouldSave)
    {
        const auto trimmedIdentifier = identifier.trim();

        if (trimmedIdentifier.isEmpty())
        {
            {
                const juce::ScopedLock scopedLock(outputLock);
                closeCurrentOutputLocked();
                selectedDeviceIdentifier.clear();
            }

            if (shouldSave)
            {
                AppSettings settings;
                settings.setStandaloneMidiOutputDeviceIdentifier(
                    juce::String());
            }

            return true;
        }

        {
            const juce::ScopedLock scopedLock(outputLock);

            if (midiOutput != nullptr
                && selectedDeviceIdentifier == trimmedIdentifier)
            {
                return true;
            }
        }

        auto newOutput =
            juce::MidiOutput::openDevice(trimmedIdentifier);

        if (newOutput == nullptr)
        {
            DebugLog::write(
                "[StandaloneMidiOutput] Could not open MIDI output | identifier="
                + trimmedIdentifier);
            return false;
        }

        newOutput->startBackgroundThread();

        {
            const juce::ScopedLock scopedLock(outputLock);
            closeCurrentOutputLocked();
            midiOutput = std::move(newOutput);
            selectedDeviceIdentifier = trimmedIdentifier;
        }

        if (shouldSave)
        {
            AppSettings settings;
            settings.setStandaloneMidiOutputDeviceIdentifier(
                trimmedIdentifier);
        }

        DebugLog::write(
            "[StandaloneMidiOutput] MIDI output opened | identifier="
            + trimmedIdentifier);

        return true;
    }

    void stopPendingOutputAndSendPanic()
    {
        const juce::ScopedLock scopedLock(outputLock);

        if (midiOutput == nullptr)
            return;

        midiOutput->clearAllPendingMessages();
        sendPanicLocked();
    }

    void closeCurrentOutputLocked()
    {
        if (midiOutput == nullptr)
            return;

        midiOutput->clearAllPendingMessages();
        sendPanicLocked();
        midiOutput->stopBackgroundThread();
        midiOutput.reset();
    }

    void sendPanicLocked()
    {
        if (midiOutput == nullptr)
            return;

        for (int channel = 1; channel <= 16; ++channel)
        {
            midiOutput->sendMessageNow(
                juce::MidiMessage::controllerEvent(channel, 64, 0));
            midiOutput->sendMessageNow(
                juce::MidiMessage::controllerEvent(channel, 66, 0));
            midiOutput->sendMessageNow(
                juce::MidiMessage::controllerEvent(channel, 67, 0));
            midiOutput->sendMessageNow(
                juce::MidiMessage::controllerEvent(channel, 121, 0));
            midiOutput->sendMessageNow(
                juce::MidiMessage::controllerEvent(channel, 123, 0));
            midiOutput->sendMessageNow(
                juce::MidiMessage::controllerEvent(channel, 120, 0));
        }
    }

    void sendMidiBuffer(const juce::MidiBuffer& midiMessages,
                        double sampleRate) noexcept
    {
        if (midiMessages.isEmpty() || sampleRate <= 0.0)
            return;

        const juce::ScopedTryLock scopedLock(outputLock);

        if (! scopedLock.isLocked() || midiOutput == nullptr)
            return;

        midiOutput->sendBlockOfMessages(
            midiMessages,
            juce::Time::getMillisecondCounterHiRes() + 1.0,
            sampleRate);
    }

    mutable juce::CriticalSection outputLock;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    juce::String selectedDeviceIdentifier;
    std::atomic<bool> sendGeneratedMidiToOutput { false };
    std::atomic<bool> midiThruEnabled { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        StandaloneMidiOutputController)
};

class StandalonePlayHeadTracker final
    : public PolyHostPluginProcessor::StandaloneAudioExtension
{
public:
    enum class RecordingTransportState
    {
        Idle,
        CountingIn,
        StartingRecording,
        WaitingForNote,
        Recording
    };

    explicit StandalonePlayHeadTracker(
        StandalonePlayHead& playHeadIn,
        StandaloneMidiOutputController& midiOutputControllerIn)
        : playHead(playHeadIn),
          midiOutputController(midiOutputControllerIn)
    {
    }

    void setMetronomeMode(AppSettings::MetronomeMode mode)
    {
        metronomeMode.store(mode, std::memory_order_release);
    }

    AppSettings::MetronomeMode getMetronomeMode() const
    {
        return metronomeMode.load(std::memory_order_acquire);
    }

    void setAudioRecordingController(
        AudioRecordingController* controller)
    {
        audioRecordingController.store(
            controller,
            std::memory_order_release);

        if (controller == nullptr
            && midiRecordingController.load(
                   std::memory_order_acquire) == nullptr)
            recordingTransportState.store(
                RecordingTransportState::Idle,
                std::memory_order_release);
    }

    void setMidiRecordingController(
        MidiRecordingController* controller)
    {
        midiRecordingController.store(
            controller,
            std::memory_order_release);

        if (controller == nullptr
            && audioRecordingController.load(
                   std::memory_order_acquire) == nullptr)
        {
            recordingTransportState.store(
                RecordingTransportState::Idle,
                std::memory_order_release);
        }
    }

    void armRecording(
        AppSettings::RecordingCountInMode countInMode,
        bool recordMidi)
    {
        activeRecordingIsMidi.store(
            recordMidi,
            std::memory_order_release);

        recordingCountInMode.store(
            countInMode,
            std::memory_order_release);

        if (countInMode
            == AppSettings::RecordingCountInMode::WaitNote)
        {
            recordingTimelineResetRequested.store(
                false,
                std::memory_order_release);
            playHeadTimelineResetRequested.store(
                false,
                std::memory_order_release);
            recordingTransportState.store(
                RecordingTransportState::WaitingForNote,
                std::memory_order_release);
            return;
        }

        recordingTimelineResetRequested.store(
            true,
            std::memory_order_release);
        playHeadTimelineResetRequested.store(
            true,
            std::memory_order_release);

        if (countInMode
            == AppSettings::RecordingCountInMode::ZeroBars)
        {
            recordingTransportState.store(
                RecordingTransportState::StartingRecording,
                std::memory_order_release);
            return;
        }

        recordingTransportState.store(
            RecordingTransportState::CountingIn,
            std::memory_order_release);
    }

    void cancelRecording()
    {
        recordingTransportState.store(
            RecordingTransportState::Idle,
            std::memory_order_release);
        recordingTimelineResetRequested.store(
            false,
            std::memory_order_release);
        playHeadTimelineResetRequested.store(
            false,
            std::memory_order_release);
    }

    void synchroniseRecordingState()
    {
        if (! isActiveControllerArmed())
        {
            recordingTransportState.store(
                RecordingTransportState::Idle,
                std::memory_order_release);
        }
    }

    void prepareToPlay(double sampleRate,
                       int samplesPerBlock) override
    {
        juce::ignoreUnused(samplesPerBlock);

        if (auto* controller =
                audioRecordingController.load(
                    std::memory_order_acquire))
        {
            if (controller->isRecording())
            {
                controller->stopRecording(
                    "Recording stopped: audio device was restarted");
            }
        }

        if (auto* controller =
                midiRecordingController.load(
                    std::memory_order_acquire))
        {
            if (controller->isRecording())
            {
                controller->stopRecording(
                    "Recording stopped: audio device was restarted");
            }
        }

        currentSampleRate =
            sampleRate > 0.0 ? sampleRate : 44100.0;

        playHead.prepareToPlay(currentSampleRate);

        processedSamples = 0;
        samplesRemainingInClick = 0;
        clickPhase = 0.0;
        clickFrequencyHz = 1000.0;
        beatIndex = 0;
        recordingTimelineSamples = 0;
        countInTargetSamples = 0;
        countInTempoBpm = 120.0;
        midiCaptureTimelineSamples = 0;
        currentMidiCaptureStartOffset = 0;
        recordingBeatPosition = 0.0;
        lastAudioRecordedSamples = 0;
        recordingTimelineResetRequested.store(
            false,
            std::memory_order_release);
        playHeadTimelineResetRequested.store(
            false,
            std::memory_order_release);
        recordingTransportState.store(
            RecordingTransportState::Idle,
            std::memory_order_release);
    }

    void processMidiInput(
        const juce::MidiBuffer& midiMessages) noexcept override
    {
        midiOutputController.sendInputMidi(
            midiMessages,
            currentSampleRate);

        auto transportState =
            recordingTransportState.load(
                std::memory_order_acquire);

        currentMidiCaptureStartOffset = 0;

        const bool timelineWasResetAtBlockStart =
            transportState != RecordingTransportState::Idle
            && playHeadTimelineResetRequested.exchange(
                false,
                std::memory_order_acq_rel);

        if (timelineWasResetAtBlockStart)
        {
            resetStandaloneTimeline();
        }

        if (transportState
            == RecordingTransportState::StartingRecording)
        {
            if (! isActiveControllerArmed())
            {
                auto expectedState =
                    RecordingTransportState::StartingRecording;

                recordingTransportState.compare_exchange_strong(
                    expectedState,
                    RecordingTransportState::Idle,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire);
                return;
            }

            auto expectedState =
                RecordingTransportState::StartingRecording;

            if (! recordingTransportState.compare_exchange_strong(
                    expectedState,
                    RecordingTransportState::Recording,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                return;
            }

            recordingTimelineResetRequested.store(
                true,
                std::memory_order_release);

            if (! timelineWasResetAtBlockStart)
                resetStandaloneTimeline();

            midiCaptureTimelineSamples = 0;
            beginActiveControllerCapturing();
            transportState = RecordingTransportState::Recording;
        }
        else if (transportState
                 == RecordingTransportState::WaitingForNote)
        {
            int triggeringNoteOffset = -1;

            for (const auto metadata : midiMessages)
            {
                if (metadata.numBytes > 0
                    && metadata.numBytes <= 3
                    && metadata.getMessage().isNoteOn())
                {
                    triggeringNoteOffset =
                        juce::jmax(0, metadata.samplePosition);
                    break;
                }
            }

            if (triggeringNoteOffset < 0)
                return;

            if (! isActiveControllerArmed())
            {
                auto expectedState =
                    RecordingTransportState::WaitingForNote;
                recordingTransportState.compare_exchange_strong(
                    expectedState,
                    RecordingTransportState::Idle,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire);
                return;
            }

            auto expectedState =
                RecordingTransportState::WaitingForNote;

            if (! recordingTransportState.compare_exchange_strong(
                    expectedState,
                    RecordingTransportState::Recording,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                return;
            }

            midiCaptureTimelineSamples = 0;
            currentMidiCaptureStartOffset = triggeringNoteOffset;
            recordingBeatPosition = 0.0;
            lastAudioRecordedSamples = 0;
            beginActiveControllerCapturing();
            transportState = RecordingTransportState::Recording;
        }

        if (transportState != RecordingTransportState::Recording
            || ! activeRecordingIsMidi.load(
                   std::memory_order_acquire))
        {
            return;
        }

        if (auto* controller =
                midiRecordingController.load(
                    std::memory_order_acquire))
        {
            controller->processExternalMidi(
                midiMessages,
                midiCaptureTimelineSamples,
                currentMidiCaptureStartOffset,
                playHead.getTempoBpm());
        }
    }

    void processHostedMidiOutput(
        const juce::MidiBuffer& midiMessages) noexcept override
    {
        midiOutputController.sendGeneratedMidi(
            midiMessages,
            currentSampleRate);

        if (recordingTransportState.load(
                std::memory_order_acquire)
                != RecordingTransportState::Recording
            || ! activeRecordingIsMidi.load(
                   std::memory_order_acquire))
        {
            return;
        }

        if (auto* controller =
                midiRecordingController.load(
                    std::memory_order_acquire))
        {
            controller->processHostedMidiOutput(
                midiMessages,
                midiCaptureTimelineSamples,
                currentMidiCaptureStartOffset);
        }
    }

    void processOutputBlock(
        juce::AudioBuffer<float>& buffer) override
    {
        const int numSamples = buffer.getNumSamples();

        auto transportState =
            recordingTransportState.load(
                std::memory_order_acquire);

        const bool timelineResetPending =
            transportState != RecordingTransportState::Idle
            && playHeadTimelineResetRequested.load(
                std::memory_order_acquire);

        if (timelineResetPending)
            transportState = RecordingTransportState::Idle;

        if (transportState != RecordingTransportState::Idle
            && ! isActiveControllerArmed())
        {
            auto expectedState = transportState;

            recordingTransportState.compare_exchange_strong(
                expectedState,
                RecordingTransportState::Idle,
                std::memory_order_acq_rel,
                std::memory_order_acquire);

            transportState = RecordingTransportState::Idle;
        }

        if (transportState != RecordingTransportState::Idle
            && recordingTimelineResetRequested.exchange(
                false,
                std::memory_order_acq_rel))
        {
            recordingTimelineSamples = 0;
            countInTargetSamples = 0;
            recordingBeatPosition = 0.0;
            lastAudioRecordedSamples = 0;

            if (transportState
                == RecordingTransportState::CountingIn)
            {
                countInTempoBpm =
                    juce::jmax(1.0, playHead.getTempoBpm());

                const double countInBeats =
                    static_cast<double>(
                        getCountInBars(
                            recordingCountInMode.load(
                                std::memory_order_acquire)))
                    * 4.0;

                countInTargetSamples =
                    static_cast<juce::int64>(
                        std::ceil(countInBeats
                                  * 60.0
                                  * currentSampleRate
                                  / countInTempoBpm));
            }

            samplesRemainingInClick = 0;
            clickPhase = 0.0;
            beatIndex = 0;
        }

        const bool shouldPreserveExistingTimeline =
            recordingCountInMode.load(
                std::memory_order_acquire)
            == AppSettings::RecordingCountInMode::WaitNote;

        const bool useRecordingTimeline =
            ! shouldPreserveExistingTimeline
            && (transportState == RecordingTransportState::CountingIn
                || transportState == RecordingTransportState::Recording);

        const int metronomeSamples =
            transportState == RecordingTransportState::CountingIn
                ? getCountInSamplesRemaining(numSamples)
                : numSamples;

        if (! timelineResetPending
            && shouldRenderMetronome(transportState))
        {
            renderMetronome(
                buffer,
                useRecordingTimeline
                    ? recordingTimelineSamples
                    : processedSamples,
                metronomeSamples,
                transportState == RecordingTransportState::CountingIn
                    ? countInTempoBpm
                    : playHead.getTempoBpm());

            if (metronomeSamples < numSamples)
                samplesRemainingInClick = 0;
        }
        else
            samplesRemainingInClick = 0;

        if (transportState != RecordingTransportState::Idle)
            recordingTimelineSamples += numSamples;

        if (transportState == RecordingTransportState::Recording)
        {
            const bool recordingMidi =
                activeRecordingIsMidi.load(
                    std::memory_order_acquire);

            juce::int64 capturedSamples = 0;

            if (recordingMidi)
            {
                const int capturedMidiSamples =
                    juce::jmax(0,
                               numSamples
                                   - juce::jlimit(
                                       0,
                                       numSamples,
                                       currentMidiCaptureStartOffset));

                if (auto* midiController =
                        midiRecordingController.load(
                            std::memory_order_acquire))
                {
                    midiController->advanceCapturedSamples(
                        capturedMidiSamples);
                }

                capturedSamples = capturedMidiSamples;
                midiCaptureTimelineSamples += capturedMidiSamples;
                currentMidiCaptureStartOffset = 0;
            }
            else if (auto* audioController =
                         audioRecordingController.load(
                             std::memory_order_acquire))
            {
                const auto currentRecordedSamples =
                    audioController->getRecordedSampleCount();

                capturedSamples =
                    juce::jmax<juce::int64>(
                        0,
                        currentRecordedSamples
                            - lastAudioRecordedSamples);

                lastAudioRecordedSamples =
                    currentRecordedSamples;
            }

            if (capturedSamples > 0
                && currentSampleRate > 0.0)
            {
                recordingBeatPosition +=
                    static_cast<double>(capturedSamples)
                    * juce::jmax(1.0, playHead.getTempoBpm())
                    / (60.0 * currentSampleRate);
            }

            if (recordingMidi)
            {
                if (auto* midiController =
                        midiRecordingController.load(
                            std::memory_order_acquire))
                {
                    midiController->setRecordedBeatPosition(
                        recordingBeatPosition);
                }
            }
            else if (auto* audioController =
                         audioRecordingController.load(
                             std::memory_order_acquire))
            {
                audioController->setRecordedBeatPosition(
                    recordingBeatPosition);
            }
        }

        if (transportState == RecordingTransportState::CountingIn
            && isActiveControllerArmed()
            && recordingTimelineSamples >= countInTargetSamples)
        {
            auto expectedState =
                RecordingTransportState::CountingIn;

            recordingTransportState.compare_exchange_strong(
                expectedState,
                RecordingTransportState::StartingRecording,
                std::memory_order_acq_rel,
                std::memory_order_acquire);
        }

        processedSamples += numSamples;
        playHead.advance(numSamples);
    }

    void releaseResources() override
    {
        samplesRemainingInClick = 0;
        clickPhase = 0.0;
        midiCaptureTimelineSamples = 0;
        currentMidiCaptureStartOffset = 0;
        recordingBeatPosition = 0.0;
        lastAudioRecordedSamples = 0;
        recordingTransportState.store(
            RecordingTransportState::Idle,
            std::memory_order_release);
        recordingTimelineResetRequested.store(
            false,
            std::memory_order_release);
        playHeadTimelineResetRequested.store(
            false,
            std::memory_order_release);
    }

private:
    bool isActiveControllerArmed() const noexcept
    {
        if (activeRecordingIsMidi.load(
                std::memory_order_acquire))
        {
            auto* controller =
                midiRecordingController.load(
                    std::memory_order_acquire);
            return controller != nullptr && controller->isRecording();
        }

        auto* controller =
            audioRecordingController.load(
                std::memory_order_acquire);
        return controller != nullptr && controller->isRecording();
    }

    void beginActiveControllerCapturing() noexcept
    {
        if (activeRecordingIsMidi.load(
                std::memory_order_acquire))
        {
            if (auto* controller =
                    midiRecordingController.load(
                        std::memory_order_acquire))
            {
                controller->beginCapturing(playHead.getTempoBpm());
            }

            return;
        }

        if (auto* controller =
                audioRecordingController.load(
                    std::memory_order_acquire))
        {
            controller->beginCapturing();
        }
    }

    void resetStandaloneTimeline()
    {
        playHead.resetTimeline();
        processedSamples = 0;
    }

    static int getCountInBars(
        AppSettings::RecordingCountInMode mode) noexcept
    {
        switch (mode)
        {
            case AppSettings::RecordingCountInMode::OneBar:
                return 1;
            case AppSettings::RecordingCountInMode::TwoBars:
                return 2;
            case AppSettings::RecordingCountInMode::FourBars:
                return 4;
            case AppSettings::RecordingCountInMode::EightBars:
                return 8;
            case AppSettings::RecordingCountInMode::ZeroBars:
            case AppSettings::RecordingCountInMode::WaitNote:
            default:
                return 0;
        }
    }

    bool shouldRenderMetronome(
        RecordingTransportState transportState) const
    {
        const auto mode = getMetronomeMode();

        if (mode == AppSettings::MetronomeMode::On)
            return true;

        if (mode != AppSettings::MetronomeMode::RecordOnly)
            return false;

        return transportState == RecordingTransportState::CountingIn
               || transportState == RecordingTransportState::WaitingForNote
               || transportState == RecordingTransportState::Recording;
    }

    int getCountInSamplesRemaining(int blockSize) const
    {
        if (blockSize <= 0 || currentSampleRate <= 0.0)
            return 0;

        const auto remainingSamples =
            juce::jmax<juce::int64>(
                0,
                countInTargetSamples
                    - recordingTimelineSamples);

        return static_cast<int>(
            juce::jlimit<juce::int64>(
                0,
                blockSize,
                remainingSamples));
    }

    void renderMetronome(
        juce::AudioBuffer<float>& buffer,
        juce::int64 timelineStartSample,
        int samplesToRender,
        double tempoBpm)
    {
        const int numOutputChannels =
            buffer.getNumChannels();

        const int numSamples =
            juce::jlimit(0,
                         buffer.getNumSamples(),
                         samplesToRender);

        if (numOutputChannels <= 0
            || numSamples <= 0
            || currentSampleRate <= 0.0)
        {
            return;
        }

        const double bpm = juce::jmax(1.0, tempoBpm);

        const double samplesPerBeat =
            (60.0 / bpm) * currentSampleRate;

        const int clickLengthSamples =
            static_cast<int>(
                0.050 * currentSampleRate);

        for (int sample = 0;
             sample < numSamples;
             ++sample)
        {
            const auto absoluteSample =
                timelineStartSample + sample;

            const auto previousBeat =
                static_cast<int>(
                    std::floor(
                        static_cast<double>(
                            juce::jmax<juce::int64>(
                                0,
                                absoluteSample - 1))
                        / samplesPerBeat));

            const auto currentBeat =
                static_cast<int>(
                    std::floor(
                        static_cast<double>(
                            absoluteSample)
                        / samplesPerBeat));

            if (absoluteSample == 0
                || currentBeat != previousBeat)
            {
                beatIndex =
                    ((currentBeat % 4) + 4) % 4;

                samplesRemainingInClick =
                    clickLengthSamples;

                clickPhase = 0.0;

                clickFrequencyHz =
                    beatIndex == 0
                        ? 1760.0
                        : 1320.0;
            }

            float clickSample = 0.0f;

            if (samplesRemainingInClick > 0)
            {
                const float envelope =
                    static_cast<float>(
                        samplesRemainingInClick)
                    / static_cast<float>(
                        clickLengthSamples);

                clickSample =
                    std::sin(
                        static_cast<float>(
                            clickPhase))
                    * 0.28f
                    * envelope
                    * envelope;

                clickPhase +=
                    juce::MathConstants<double>::twoPi
                    * clickFrequencyHz
                    / currentSampleRate;

                --samplesRemainingInClick;
            }

            for (int channel = 0;
                 channel < numOutputChannels;
                 ++channel)
            {
                buffer.getWritePointer(channel)[sample]
                    += clickSample;
            }
        }
    }

    StandalonePlayHead& playHead;
    StandaloneMidiOutputController& midiOutputController;
    std::atomic<AppSettings::MetronomeMode> metronomeMode {
        AppSettings::MetronomeMode::Off
    };
    std::atomic<AudioRecordingController*> audioRecordingController {
        nullptr
    };
    std::atomic<MidiRecordingController*> midiRecordingController {
        nullptr
    };
    std::atomic<RecordingTransportState> recordingTransportState {
        RecordingTransportState::Idle
    };
    std::atomic<bool> activeRecordingIsMidi { false };
    std::atomic<AppSettings::RecordingCountInMode> recordingCountInMode {
        AppSettings::RecordingCountInMode::ZeroBars
    };
    std::atomic<bool> recordingTimelineResetRequested { false };
    std::atomic<bool> playHeadTimelineResetRequested { false };
    juce::int64 processedSamples = 0;
    juce::int64 recordingTimelineSamples = 0;
    juce::int64 midiCaptureTimelineSamples = 0;
    juce::int64 countInTargetSamples = 0;
    int currentMidiCaptureStartOffset = 0;
    double recordingBeatPosition = 0.0;
    juce::int64 lastAudioRecordedSamples = 0;
    double countInTempoBpm = 120.0;
    int samplesRemainingInClick = 0;
    double clickPhase = 0.0;
    double clickFrequencyHz = 1000.0;
    int beatIndex = 0;
    double currentSampleRate = 44100.0;
};

class StandaloneTempoControls final : public juce::Component,
                                      private juce::Timer
{
public:
    static constexpr int preferredWidth = 252;
    static constexpr int preferredHeight = 32;

    StandaloneTempoControls(
        PolyHostPluginProcessor* processorIn,
        StandalonePlayHead& playHeadIn,
        StandalonePlayHeadTracker& playHeadTrackerIn)
        : processor(processorIn),
          playHead(playHeadIn),
          playHeadTracker(playHeadTrackerIn),
          beatIndicator(playHeadIn),
          tempoEditor(*this)
    {
        AppSettings settings;
        defaultTempoBpm = settings.getDefaultTempoBpm();
        playHead.setTempoBpm(defaultTempoBpm);
        playHeadTracker.setMetronomeMode(
            settings.getMetronomeMode());

        addAndMakeVisible(recordButton);
        recordButton.setName("Recording");
        recordButton.setWantsKeyboardFocus(false);
        recordButton.onClick = [this]
        {
            toggleRecording();
        };
        recordButton.onRightClick = [this]
        {
            if (recordingViewToggleCallback)
                recordingViewToggleCallback();
        };

        addAndMakeVisible(beatIndicator);

        addAndMakeVisible(tempoEditor);
        tempoEditor.setInputRestrictions(6, "0123456789.");
        tempoEditor.setJustification(juce::Justification::centred);
        tempoEditor.setSelectAllWhenFocused(true);
        tempoEditor.setColour(
            juce::TextEditor::backgroundColourId,
            juce::Colour(0xFF151B24));
        tempoEditor.setColour(
            juce::TextEditor::textColourId,
            juce::Colours::white);
        tempoEditor.setColour(
            juce::TextEditor::outlineColourId,
            juce::Colour(0xFF4C5668));
        tempoEditor.setColour(
            juce::TextEditor::focusedOutlineColourId,
            juce::Colour(0xFF6E8FB8));
        tempoEditor.onReturnKey = [this]
        {
            commitTempoFromEditor();
            tempoEditor.unfocusAllComponents();
        };

        addAndMakeVisible(resetTempoButton);
        resetTempoButton.onClick = [this]
        {
            setTempoBpm(defaultTempoBpm, true);
        };

        addAndMakeVisible(metronomeButton);
        metronomeButton.onClick = [this]
        {
            cycleMetronomeMode();
        };

        addAndMakeVisible(tapTempoButton);
        tapTempoButton.onClick = [this]
        {
            registerTapTempo();
        };

        refreshUi();
        startTimerHz(5);
    }

    ~StandaloneTempoControls() override
    {
        stopTimer();
    }

    double getTempoBpm() const
    {
        return playHead.getTempoBpm();
    }

    double getDefaultTempoBpm() const
    {
        return defaultTempoBpm;
    }

    void setTempoBpmFromPreset(double bpm)
    {
        setTempoBpm(bpm, false);
    }

    void setRecordingViewToggleCallback(
        std::function<void()> callback)
    {
        recordingViewToggleCallback = std::move(callback);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(2, 0);

        const int buttonWidth =
            ButtonStyling::defaultButtonWidth();

        const int buttonHeight =
            ButtonStyling::defaultButtonHeight();

        auto recordBounds = area.removeFromRight(buttonWidth);
        recordButton.setBounds(
            recordBounds.withSizeKeepingCentre(
                recordBounds.getWidth(),
                buttonHeight));

        area.removeFromRight(2);

        auto tapBounds = area.removeFromRight(buttonWidth);
        tapTempoButton.setBounds(
            tapBounds.withSizeKeepingCentre(
                tapBounds.getWidth(),
                buttonHeight));

        area.removeFromRight(2);

        auto metronomeBounds =
            area.removeFromRight(buttonWidth);

        metronomeButton.setBounds(
            metronomeBounds.withSizeKeepingCentre(
                metronomeBounds.getWidth(),
                buttonHeight));

        area.removeFromRight(1);

        auto resetBounds =
            area.removeFromRight(buttonWidth);

        resetTempoButton.setBounds(
            resetBounds.withSizeKeepingCentre(
                resetBounds.getWidth(),
                buttonHeight));

        area.removeFromRight(2);

        auto editorBounds = area.removeFromRight(55);

        tempoEditor.setBounds(
            editorBounds.reduced(1, 0)
                        .withSizeKeepingCentre(
                            editorBounds.getWidth() - 2,
                            22));

        area.removeFromRight(2);
        beatIndicator.setBounds(area);
    }

private:
    class TempoTextEditor final : public juce::TextEditor
    {
    public:
        explicit TempoTextEditor(
            StandaloneTempoControls& ownerIn)
            : owner(ownerIn)
        {
        }

        void mouseWheelMove(
            const juce::MouseEvent& event,
            const juce::MouseWheelDetails& wheel) override
        {
            if (wheel.deltaY == 0.0f)
            {
                juce::TextEditor::mouseWheelMove(event, wheel);
                return;
            }

            const double step =
                event.mods.isShiftDown() ? 0.1 : 1.0;

            const double direction =
                wheel.deltaY > 0.0f ? 1.0 : -1.0;

            owner.setTempoBpm(
                owner.getTempoBpm() + direction * step,
                true);
        }

        void mouseDoubleClick(
            const juce::MouseEvent& event) override
        {
            juce::ignoreUnused(event);

            owner.setTempoBpm(
                owner.getDefaultTempoBpm(),
                true);
        }

        void addPopupMenuItems(
            juce::PopupMenu& menu,
            const juce::MouseEvent* mouseClickEvent) override
        {
            juce::TextEditor::addPopupMenuItems(
                menu,
                mouseClickEvent);

            menu.addSeparator();
            menu.addItem(1001, "Set as Default");
        }

        void performPopupMenuAction(int menuItemID) override
        {
            if (menuItemID == 1001)
            {
                owner.setCurrentTempoAsDefault();
                return;
            }

            juce::TextEditor::performPopupMenuAction(
                menuItemID);
        }

        bool keyPressed(
            const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress(
                           juce::KeyPress::upKey,
                           juce::ModifierKeys::shiftModifier,
                           0))
            {
                owner.setTempoBpm(
                    owner.getTempoBpm() + 0.1,
                    true);
                return true;
            }

            if (key == juce::KeyPress(
                           juce::KeyPress::downKey,
                           juce::ModifierKeys::shiftModifier,
                           0))
            {
                owner.setTempoBpm(
                    owner.getTempoBpm() - 0.1,
                    true);
                return true;
            }

            if (key == juce::KeyPress::upKey)
            {
                owner.setTempoBpm(
                    owner.getTempoBpm() + 1.0,
                    true);
                return true;
            }

            if (key == juce::KeyPress::downKey)
            {
                owner.setTempoBpm(
                    owner.getTempoBpm() - 1.0,
                    true);
                return true;
            }

            return juce::TextEditor::keyPressed(key);
        }

        void focusLost(FocusChangeType cause) override
        {
            juce::TextEditor::focusLost(cause);
            owner.commitTempoFromEditor();
        }

    private:
        StandaloneTempoControls& owner;
    };

    class BeatIndicatorComponent final : public juce::Component,
                                         private juce::Timer
    {
    public:
        explicit BeatIndicatorComponent(
            StandalonePlayHead& playHeadIn)
            : playHead(playHeadIn)
        {
            startTimerHz(30);
        }

        void paint(juce::Graphics& g) override
        {
            auto area =
                getLocalBounds().toFloat().reduced(1.0f);

            const int beatIndex =
                playHead.getCurrentBeatInBar();

            const auto beatProgress =
                playHead.getCurrentBeatProgress();

            const float ledDiameter = 9.0f;
            const float spacing = 5.0f;

            const float totalWidth =
                (ledDiameter * 4.0f)
                + (spacing * 3.0f);

            const float startX =
                area.getCentreX() - (totalWidth * 0.5f);

            const float y =
                area.getCentreY() - (ledDiameter * 0.5f);

            for (int index = 0; index < 4; ++index)
            {
                auto bounds = juce::Rectangle<float>(
                    startX
                        + (ledDiameter + spacing)
                            * static_cast<float>(index),
                    y,
                    ledDiameter,
                    ledDiameter);

                const bool isActive = index == beatIndex;
                float pulse = 1.0f;

                if (isActive)
                {
                    pulse =
                        0.75f
                        + 0.25f
                            * (1.0f
                               - static_cast<float>(
                                   beatProgress));
                }

                const auto baseColour =
                    index == 0
                        ? juce::Colours::red
                        : juce::Colours::limegreen;

                const auto fillColour =
                    isActive
                        ? baseColour.brighter(
                            0.35f * pulse + 0.15f)
                        : baseColour.darker(0.75f)
                              .withAlpha(0.22f);

                const auto borderColour =
                    isActive
                        ? baseColour.brighter(
                            0.55f * pulse + 0.25f)
                        : baseColour.darker(0.5f)
                              .withAlpha(0.35f);

                const auto highlightColour =
                    isActive
                        ? juce::Colours::white.withAlpha(
                            0.20f + 0.12f * pulse)
                        : juce::Colours::white.withAlpha(
                            0.06f);

                g.setColour(fillColour);
                g.fillEllipse(bounds);

                auto highlight =
                    bounds.reduced(1.5f)
                          .removeFromTop(
                              bounds.getHeight() * 0.42f);

                g.setColour(highlightColour);
                g.fillEllipse(highlight);

                g.setColour(borderColour);
                g.drawEllipse(bounds, 1.0f);
            }
        }

    private:
        void timerCallback() override
        {
            repaint();
        }

        StandalonePlayHead& playHead;
    };

    void setTempoBpm(double bpm, bool markPresetDirty)
    {
        bpm = juce::jlimit(20.0, 300.0, bpm);
        bpm = std::round(bpm * 10.0) / 10.0;

        if (std::abs(playHead.getTempoBpm() - bpm) < 0.0001)
        {
            refreshUi();
            return;
        }

        playHead.setTempoBpm(bpm);
        refreshUi();

        if (markPresetDirty && processor != nullptr)
            processor->getCore().markDirty();
    }

    void commitTempoFromEditor()
    {
        const auto text = tempoEditor.getText().trim();

        if (text.isEmpty()
            || ! text.containsOnly("0123456789."))
        {
            refreshUi();
            return;
        }

        if (text.containsChar('.')
            && text.fromFirstOccurrenceOf(
                        ".",
                        false,
                        false)
                   .containsChar('.'))
        {
            refreshUi();
            return;
        }

        const double parsedTempo = text.getDoubleValue();

        if (parsedTempo <= 0.0)
        {
            refreshUi();
            return;
        }

        setTempoBpm(parsedTempo, true);
    }

    void registerTapTempo()
    {
        const double nowMs =
            juce::Time::getMillisecondCounterHiRes();

        if (! tapTimesMs.isEmpty()
            && nowMs - tapTimesMs.getLast() > 2000.0)
        {
            tapTimesMs.clear();
        }

        tapTimesMs.add(nowMs);

        while (tapTimesMs.size() > 6)
            tapTimesMs.remove(0);

        if (tapTimesMs.size() < 2)
            return;

        juce::Array<double> validIntervalsMs;

        for (int index = 1;
             index < tapTimesMs.size();
             ++index)
        {
            const double intervalMs =
                tapTimesMs[index]
                - tapTimesMs[index - 1];

            if (intervalMs >= 200.0
                && intervalMs <= 3000.0)
            {
                validIntervalsMs.add(intervalMs);
            }
        }

        if (validIntervalsMs.isEmpty())
            return;

        double totalMs = 0.0;

        for (const auto intervalMs : validIntervalsMs)
            totalMs += intervalMs;

        const double averageIntervalMs =
            totalMs
            / static_cast<double>(validIntervalsMs.size());

        setTempoBpm(
            60000.0 / averageIntervalMs,
            true);
    }

    void setCurrentTempoAsDefault()
    {
        defaultTempoBpm = getTempoBpm();

        AppSettings settings;
        settings.setDefaultTempoBpm(defaultTempoBpm);
        refreshUi();
    }

    void cycleMetronomeMode()
    {
        const auto currentMode =
            playHeadTracker.getMetronomeMode();

        AppSettings::MetronomeMode nextMode =
            AppSettings::MetronomeMode::Off;

        if (currentMode == AppSettings::MetronomeMode::Off)
            nextMode = AppSettings::MetronomeMode::On;
        else if (currentMode == AppSettings::MetronomeMode::On)
            nextMode = AppSettings::MetronomeMode::RecordOnly;

        playHeadTracker.setMetronomeMode(nextMode);

        AppSettings settings;
        settings.setMetronomeMode(nextMode);
        refreshUi();
    }

    void toggleRecording()
    {
        if (processor == nullptr)
            return;

        auto& audioController =
            processor->getAudioRecordingController();
        auto& midiController =
            processor->getMidiRecordingController();

        if (audioController.isRecording()
            || midiController.isRecording())
        {
            const bool wasCapturing =
                audioController.isCapturing()
                || midiController.isCapturing();

            playHeadTracker.cancelRecording();

            if (audioController.isRecording())
            {
                audioController.stopRecording(
                    wasCapturing
                        ? "Recording stopped"
                        : "Recording cancelled");
            }

            if (midiController.isRecording())
            {
                midiController.stopRecording(
                    wasCapturing
                        ? "Recording stopped"
                        : "Recording cancelled");
            }

            refreshRecordingButton();
            return;
        }

        const bool recordMidi =
            midiController.isMidiModeSelected();

        const auto result =
            recordMidi
                ? midiController.armRecording()
                : audioController.armRecording();

        if (result.wasOk())
        {
            AppSettings settings;
            playHeadTracker.armRecording(
                settings.getRecordingCountInMode(),
                recordMidi);
        }

        refreshRecordingButton();

        if (result.failed())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                recordMidi
                    ? "MIDI Recording Failed"
                    : "Audio Recording Failed",
                result.getErrorMessage(),
                "OK",
                this);
        }
    }

    void refreshRecordingButton()
    {
        playHeadTracker.synchroniseRecordingState();

        if (processor == nullptr)
        {
            recordButton.setEnabled(false);
            recordButton.setTooltip("Recording unavailable");
            recordButton.repaint();
            return;
        }

        const auto audioStatus =
            processor->getAudioRecordingController().getStatus();
        const auto midiStatus =
            processor->getMidiRecordingController().getStatus();
        const bool anyArmed = audioStatus.armed || midiStatus.armed;
        const bool anyRecording =
            audioStatus.recording || midiStatus.recording;
        const bool activeModeIsMidi =
            midiStatus.armed
            || (! audioStatus.armed
                && processor->getMidiRecordingController()
                       .isMidiModeSelected());
        const double selectedSampleRate =
            activeModeIsMidi
                ? midiStatus.sampleRate
                : audioStatus.sampleRate;
        const juce::String modeName =
            activeModeIsMidi ? "MIDI" : "Audio";

        recordButton.setEnabled(
            anyArmed || selectedSampleRate > 0.0);
        const juce::String primaryTooltip =
            anyRecording
                ? "Stop " + modeName + " Recording"
                : anyArmed
                    ? "Cancel " + modeName + " Recording"
                    : "Start " + modeName + " Recording";
        recordButton.setTooltip(
            primaryTooltip
            + "\nRight-click: Toggle Recording View");
        recordButton.repaint();
    }

    void timerCallback() override
    {
        refreshRecordingButton();
    }

    void refreshUi()
    {
        tempoEditor.setText(
            juce::String(getTempoBpm(), 1),
            juce::dontSendNotification);

        tempoEditor.setTooltip(
            "Scroll to adjust.\n"
            "Shift+Scroll: Fine Adjust.\n"
            "Double-click: Reset.\n"
            "Right-click: Set as Default.");

        resetTempoButton.setTooltip(
            "Reset tempo to "
            + juce::String(defaultTempoBpm, 1)
            + ".");

        const auto metronomeMode =
            playHeadTracker.getMetronomeMode();

        if (metronomeMode == AppSettings::MetronomeMode::On)
            metronomeButton.setTooltip("Metronome: On");
        else if (metronomeMode == AppSettings::MetronomeMode::RecordOnly)
            metronomeButton.setTooltip("Metronome: Record Only");
        else
            metronomeButton.setTooltip("Metronome: Off");

        metronomeButton.repaint();
        refreshRecordingButton();
    }

    PolyHostPluginProcessor* processor = nullptr;
    StandalonePlayHead& playHead;
    StandalonePlayHeadTracker& playHeadTracker;
    double defaultTempoBpm = 120.0;
    std::function<void()> recordingViewToggleCallback;
    juce::Array<double> tapTimesMs;
    BeatIndicatorComponent beatIndicator;
    TempoTextEditor tempoEditor;

    ButtonStyling::ToolbarIconButton recordButton
    {
        0,
        "Start Recording",
        juce::String::charToString((juce_wchar) 0xe7c8),
        ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
        ButtonStyling::defaultButtonWidth(),
        {},
        ButtonStyling::defaultBackground(),
        0,
        ButtonStyling::defaultIconSize(),
        [this]
        {
            if (processor != nullptr)
            {
                const auto& audioController =
                    processor->getAudioRecordingController();
                const auto& midiController =
                    processor->getMidiRecordingController();

                if (audioController.isCapturing()
                    || midiController.isCapturing())
                    return ButtonStyling::destructiveBackground();

                if (audioController.isRecording()
                    || midiController.isRecording())
                    return juce::Colour(0xFFE67E22);
            }

            return ButtonStyling::defaultBackground();
        }
    };

    ButtonStyling::ToolbarIconButton resetTempoButton
    {
        0,
        ButtonStyling::Tooltips::resetTempo(),
        ButtonStyling::Glyphs::reset(),
        ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
        ButtonStyling::defaultButtonWidth()
    };

    ButtonStyling::ToolbarIconButton metronomeButton
    {
        0,
        ButtonStyling::Tooltips::metronome(),
        ButtonStyling::Glyphs::metronome(),
        ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
        ButtonStyling::defaultButtonWidth(),
        [this]
        {
            return playHeadTracker.getMetronomeMode()
                   == AppSettings::MetronomeMode::On;
        },
        ButtonStyling::defaultBackground(),
        0,
        ButtonStyling::defaultIconSize(),
        [this]
        {
            if (playHeadTracker.getMetronomeMode()
                == AppSettings::MetronomeMode::RecordOnly)
            {
                return ButtonStyling::destructiveBackground();
            }

            return ButtonStyling::defaultBackground();
        }
    };

    ButtonStyling::ToolbarIconButton tapTempoButton
    {
        0,
        ButtonStyling::Tooltips::tapTempo(),
        ButtonStyling::Glyphs::tapTempo(),
        ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
        ButtonStyling::defaultButtonWidth(),
        {},
        ButtonStyling::defaultBackground(),
        0,
        ButtonStyling::defaultIconSize() + 2.0f
    };
};

class StandaloneSettingsLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    StandaloneSettingsLookAndFeel()
    {
        const auto windowBackground = juce::Colour(0xFF4A4A4A);
        const auto panelBackground = juce::Colour(0xFF575757);
        const auto editorBackground = juce::Colour(0xFF33404A);
        const auto editorOutline =
            juce::Colours::lightgrey.withAlpha(0.35f);

        setColour(
            juce::ResizableWindow::backgroundColourId,
            windowBackground);

        setColour(
            juce::Label::textColourId,
            juce::Colours::white);

        setColour(
            juce::ToggleButton::textColourId,
            juce::Colours::white);

        setColour(
            juce::GroupComponent::textColourId,
            juce::Colours::white);

        setColour(
            juce::GroupComponent::outlineColourId,
            juce::Colours::white.withAlpha(0.18f));

        setColour(
            juce::TextEditor::backgroundColourId,
            editorBackground);

        setColour(
            juce::TextEditor::textColourId,
            juce::Colours::white);

        setColour(
            juce::TextEditor::outlineColourId,
            editorOutline);

        setColour(
            juce::ComboBox::backgroundColourId,
            editorBackground);

        setColour(
            juce::ComboBox::textColourId,
            juce::Colours::white);

        setColour(
            juce::ComboBox::outlineColourId,
            editorOutline);

        setColour(
            juce::ComboBox::buttonColourId,
            editorBackground);

        setColour(
            juce::ComboBox::arrowColourId,
            juce::Colours::white);

        setColour(
            juce::ListBox::backgroundColourId,
            editorBackground);

        setColour(
            juce::ListBox::outlineColourId,
            editorOutline);

        setColour(
            juce::ListBox::textColourId,
            juce::Colours::white);

        setColour(
            juce::ScrollBar::backgroundColourId,
            editorBackground);

        setColour(
            juce::ScrollBar::trackColourId,
            editorBackground);

        setColour(
            juce::ScrollBar::thumbColourId,
            panelBackground);

        setColour(
            juce::PopupMenu::backgroundColourId,
            panelBackground);

        setColour(
            juce::PopupMenu::textColourId,
            juce::Colours::white);

        setColour(
            juce::PopupMenu::highlightedBackgroundColourId,
            editorBackground);

        setColour(
            juce::PopupMenu::highlightedTextColourId,
            juce::Colours::white);
    }

    void drawGroupComponentOutline(
        juce::Graphics& g,
        int width,
        int height,
        const juce::String& text,
        const juce::Justification& position,
        juce::GroupComponent& group) override
    {
        constexpr float sideInset = 4.0f;
        constexpr float topInset = 10.0f;
        constexpr float bottomInset = 4.0f;

        g.setColour(juce::Colour(0xFF575757));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(
                sideInset,
                topInset,
                juce::jmax(
                    0.0f,
                    static_cast<float>(width)
                        - (sideInset * 2.0f)),
                juce::jmax(
                    0.0f,
                    static_cast<float>(height)
                        - topInset
                        - bottomInset)),
            4.0f);

        juce::LookAndFeel_V4::drawGroupComponentOutline(
            g,
            width,
            height,
            text,
            position,
            group);
    }
};

class StandaloneAudioSettingsComponent final : public juce::Component
{
public:
    StandaloneAudioSettingsComponent(
        juce::StandalonePluginHolder& holderIn,
        int maxAudioInputChannels,
        int maxAudioOutputChannels)
        : holder(holderIn),
          deviceSelector(holder.deviceManager,
                         0,
                         maxAudioInputChannels,
                         0,
                         maxAudioOutputChannels,
                         false,
                         false,
                         true,
                         false),
          shouldMuteLabel("Feedback Loop:", "Feedback Loop:"),
          shouldMuteButton("Mute audio input")
    {
        setOpaque(true);
        setLookAndFeel(&settingsLookAndFeel);

        shouldMuteButton.setClickingTogglesState(true);
        shouldMuteButton.getToggleStateValue().referTo(
            holder.getMuteInputValue());

        addAndMakeVisible(deviceSelector);

        if (holder.getProcessorHasPotentialFeedbackLoop())
        {
            addAndMakeVisible(shouldMuteButton);
            addAndMakeVisible(shouldMuteLabel);
            shouldMuteLabel.attachToComponent(
                &shouldMuteButton,
                true);
        }
    }

    ~StandaloneAudioSettingsComponent() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(
            getLookAndFeel().findColour(
                juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        const juce::ScopedValueSetter<bool> resizingSetter(
            isResizing,
            true);

        auto area = getLocalBounds();

        if (holder.getProcessorHasPotentialFeedbackLoop())
        {
            const auto itemHeight =
                deviceSelector.getItemHeight();

            auto feedbackArea =
                area.removeFromTop(itemHeight);

            const auto separatorHeight =
                itemHeight / 2;

            shouldMuteButton.setBounds(
                juce::Rectangle<int>(
                    feedbackArea.proportionOfWidth(0.35f),
                    separatorHeight,
                    feedbackArea.proportionOfWidth(0.60f),
                    itemHeight));

            area.removeFromTop(separatorHeight);
        }

        deviceSelector.setBounds(area);
    }

    void childBoundsChanged(
        juce::Component* childComponent) override
    {
        if (! isResizing
            && childComponent == &deviceSelector)
        {
            setToRecommendedSize();
        }
    }

    void setToRecommendedSize()
    {
        int extraHeight = 0;

        if (holder.getProcessorHasPotentialFeedbackLoop())
        {
            const auto itemHeight =
                deviceSelector.getItemHeight();

            extraHeight =
                itemHeight + (itemHeight / 2);
        }

        setSize(
            getWidth(),
            deviceSelector.getHeight() + extraHeight);
    }

private:
    StandaloneSettingsLookAndFeel settingsLookAndFeel;
    juce::StandalonePluginHolder& holder;
    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::Label shouldMuteLabel;
    juce::ToggleButton shouldMuteButton;
    bool isResizing = false;
};

class StandaloneMidiSettingsComponent final
    : public juce::Component
{
public:
    explicit StandaloneMidiSettingsComponent(
        juce::StandalonePluginHolder& holderIn,
        StandaloneMidiOutputController& midiOutputControllerIn)
        : holder(holderIn),
          midiOutputController(midiOutputControllerIn),
          deviceListContent(*this)
    {
        setOpaque(true);
        setLookAndFeel(&settingsLookAndFeel);
        setSize(500, 500);

        outputDeviceGroup.setText(
            "MIDI Output Device");

        outputDeviceGroup.setColour(
            juce::GroupComponent::textColourId,
            getLookAndFeel().findColour(
                juce::Label::textColourId));

        outputDeviceGroup.setColour(
            juce::GroupComponent::outlineColourId,
            getLookAndFeel().findColour(
                juce::GroupComponent::outlineColourId));

        addAndMakeVisible(outputDeviceGroup);

        outputDeviceLabel.setText(
            "MIDI Output:",
            juce::dontSendNotification);

        outputDeviceLabel.setJustificationType(
            juce::Justification::centredLeft);

        addAndMakeVisible(outputDeviceLabel);

        outputDeviceCombo.setTooltip(
            "Choose the hardware or virtual MIDI output that PHI will use.");

        outputDeviceCombo.onChange = [this]
        {
            handleOutputDeviceSelection();
        };

        addAndMakeVisible(outputDeviceCombo);

        sendGeneratedMidiButton.setButtonText(
            "Send generated MIDI to output");

        sendGeneratedMidiButton.setTooltip(
            "Send MIDI produced by hosted arpeggiators, sequencers and MIDI effects to the selected output.");

        sendGeneratedMidiButton.setToggleState(
            midiOutputController
                .getSendGeneratedMidiToOutput(),
            juce::dontSendNotification);

        sendGeneratedMidiButton.onClick = [this]
        {
            midiOutputController
                .setSendGeneratedMidiToOutput(
                    sendGeneratedMidiButton
                        .getToggleState());
        };

        addAndMakeVisible(sendGeneratedMidiButton);

        midiThruButton.setButtonText(
            "MIDI Thru");

        midiThruButton.setTooltip(
            "Forward original MIDI from enabled input devices to the selected output.");

        midiThruButton.setToggleState(
            midiOutputController.getMidiThruEnabled(),
            juce::dontSendNotification);

        midiThruButton.onClick = [this]
        {
            midiOutputController.setMidiThruEnabled(
                midiThruButton.getToggleState());
        };

        addAndMakeVisible(midiThruButton);

        outputInfoLabel.setText(
            "Generated MIDI is produced by hosted arpeggiators, sequencers\n"
            "and MIDI effects. MIDI Thru forwards original MIDI received\n"
            "from enabled input devices. Do not route the output back to PHI,\n"
            "as this can create duplicate notes or a MIDI feedback loop.",
            juce::dontSendNotification);

        outputInfoLabel.setJustificationType(
            juce::Justification::topLeft);

        outputInfoLabel.setFont(
            juce::Font(
                juce::FontOptions(13.0f)));

        addAndMakeVisible(outputInfoLabel);

        inputDevicesGroup.setText(
            "MIDI Input Devices");

        inputDevicesGroup.setColour(
            juce::GroupComponent::textColourId,
            getLookAndFeel().findColour(
                juce::Label::textColourId));

        inputDevicesGroup.setColour(
            juce::GroupComponent::outlineColourId,
            getLookAndFeel().findColour(
                juce::GroupComponent::
                    outlineColourId));

        addAndMakeVisible(inputDevicesGroup);

        viewport.setViewedComponent(
            &deviceListContent,
            false);

        viewport.setScrollBarsShown(
            true,
            false);

        viewport.setScrollBarThickness(12);
        viewport.setWantsKeyboardFocus(false);

        addAndMakeVisible(viewport);

        refreshButton.setButtonText(
            "Refresh Devices");

        refreshButton.setTooltip(
            "Refresh the available MIDI input and output devices.");

        refreshButton.onClick = [this]
        {
            rebuildDeviceLists();
        };

        addAndMakeVisible(refreshButton);

        rebuildDeviceLists();
    }

    ~StandaloneMidiSettingsComponent() override
    {
        viewport.setViewedComponent(
            nullptr,
            false);

        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(
            getLookAndFeel().findColour(
                juce::ResizableWindow::
                    backgroundColourId));
    }

    void resized() override
    {
        auto area =
            getLocalBounds().reduced(14);

        auto controlsArea =
            area.removeFromBottom(30);

        refreshButton.setBounds(
            controlsArea.removeFromRight(130));

        area.removeFromBottom(10);

        auto outputArea =
            area.removeFromTop(216);

        outputDeviceGroup.setBounds(outputArea);

        auto outputContentArea =
            outputArea.reduced(14);

        outputContentArea.removeFromTop(10);

        auto outputRow =
            outputContentArea.removeFromTop(30);

        outputDeviceLabel.setBounds(
            outputRow.removeFromLeft(88));

        outputRow.removeFromLeft(6);

        outputDeviceCombo.setBounds(outputRow);

        outputContentArea.removeFromTop(4);

        sendGeneratedMidiButton.setBounds(
            outputContentArea.removeFromTop(26));

        outputContentArea.removeFromTop(4);

        midiThruButton.setBounds(
            outputContentArea.removeFromTop(26));

        outputContentArea.removeFromTop(6);

        outputInfoLabel.setBounds(outputContentArea);

        area.removeFromTop(10);

        inputDevicesGroup.setBounds(area);

        auto listArea =
            area.reduced(12);

        listArea.removeFromTop(10);

        viewport.setBounds(listArea);

        deviceListContent.setSize(
            juce::jmax(
                1,
                viewport.getWidth() - 14),
            deviceListContent.getRequiredHeight());
    }

private:
    class DeviceListContent final
        : public juce::Component
    {
    public:
        explicit DeviceListContent(
            StandaloneMidiSettingsComponent& ownerIn)
            : owner(ownerIn)
        {
            setOpaque(true);
        }

        void rebuild()
        {
            deviceButtons.clear(true);

            const auto devices =
                juce::MidiInput::
                    getAvailableDevices();

            for (const auto& device : devices)
            {
                auto* button =
                    deviceButtons.add(
                        new juce::ToggleButton(
                            device.name));

                button->setClickingTogglesState(
                    true);

                button->setToggleState(
                    owner.holder.deviceManager
                        .isMidiInputDeviceEnabled(
                            device.identifier),
                    juce::dontSendNotification);

                button->setColour(
                    juce::ToggleButton::textColourId,
                    getLookAndFeel().findColour(
                        juce::Label::textColourId));

                button->setTooltip(
                    "Enable or disable this MIDI input device.");

                const auto identifier =
                    device.identifier;

                button->onClick =
                    [this, identifier, button]
                    {
                        owner.setMidiInputEnabled(
                            identifier,
                            button->getToggleState());
                    };

                addAndMakeVisible(button);
            }

            setSize(
                juce::jmax(1, getWidth()),
                getRequiredHeight());

            resized();
            repaint();
        }

        int getRequiredHeight() const
        {
            if (deviceButtons.isEmpty())
                return 54;

            return 12
                + deviceButtons.size() * 32;
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(
                getLookAndFeel().findColour(
                    juce::TextEditor::
                        backgroundColourId));

            g.setColour(
                getLookAndFeel().findColour(
                    juce::TextEditor::
                        outlineColourId));

            g.drawRect(
                getLocalBounds(),
                1);

            if (deviceButtons.isEmpty())
            {
                g.setColour(
                    getLookAndFeel().findColour(
                        juce::Label::
                            textColourId));

                g.setFont(
                    juce::Font(
                        juce::FontOptions(14.0f)));

                g.drawText(
                    "No MIDI input devices found",
                    getLocalBounds().reduced(12),
                    juce::Justification::centred,
                    true);
            }
        }

        void resized() override
        {
            auto area =
                getLocalBounds().reduced(10, 6);

            for (auto* button : deviceButtons)
            {
                if (button == nullptr)
                    continue;

                button->setBounds(
                    area.removeFromTop(26));

                area.removeFromTop(6);
            }
        }

    private:
        StandaloneMidiSettingsComponent& owner;

        juce::OwnedArray<
            juce::ToggleButton>
            deviceButtons;
    };

    void rebuildDeviceLists()
    {
        midiOutputController.refreshSelectedOutputDevice();
        rebuildOutputDeviceList();
        deviceListContent.rebuild();
        resized();
    }

    void rebuildOutputDeviceList()
    {
        const juce::ScopedValueSetter<bool> rebuildingSetter(
            isRebuildingOutputDevices,
            true);

        outputDeviceCombo.clear(
            juce::dontSendNotification);

        outputDeviceIdentifiers.clear();

        outputDeviceIdentifiers.add(juce::String());
        outputDeviceCombo.addItem(
            "No MIDI Output",
            1);

        const auto selectedIdentifier =
            midiOutputController
                .getSelectedDeviceIdentifier();

        int selectedItemId = 1;

        for (const auto& device :
             juce::MidiOutput::getAvailableDevices())
        {
            outputDeviceIdentifiers.add(
                device.identifier);

            const int itemId =
                outputDeviceIdentifiers.size();

            outputDeviceCombo.addItem(
                device.name,
                itemId);

            if (device.identifier == selectedIdentifier)
                selectedItemId = itemId;
        }

        if (selectedIdentifier.isNotEmpty()
            && selectedItemId == 1)
        {
            outputDeviceIdentifiers.add(
                selectedIdentifier);

            selectedItemId =
                outputDeviceIdentifiers.size();

            outputDeviceCombo.addItem(
                "Saved MIDI output (unavailable)",
                selectedItemId);

            outputDeviceCombo.setItemEnabled(
                selectedItemId,
                false);
        }

        outputDeviceCombo.setSelectedId(
            selectedItemId,
            juce::dontSendNotification);
    }

    void handleOutputDeviceSelection()
    {
        if (isRebuildingOutputDevices)
            return;

        const int selectedIndex =
            outputDeviceCombo.getSelectedId() - 1;

        if (! juce::isPositiveAndBelow(
                selectedIndex,
                outputDeviceIdentifiers.size()))
        {
            return;
        }

        const auto identifier =
            outputDeviceIdentifiers[selectedIndex];

        if (midiOutputController.selectOutputDevice(identifier))
            return;

        rebuildOutputDeviceList();

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "MIDI Output",
            "PHI could not open the selected MIDI output. It may already be in use by another application.",
            "OK");
    }

    void setMidiInputEnabled(
        const juce::String& identifier,
        bool shouldBeEnabled)
    {
        holder.deviceManager
            .setMidiInputDeviceEnabled(
                identifier,
                shouldBeEnabled);

        saveEnabledMidiInputs();
    }

    void saveEnabledMidiInputs()
    {
        juce::StringArray enabledIdentifiers;

        for (const auto& device :
             juce::MidiInput::
                 getAvailableDevices())
        {
            if (holder.deviceManager
                    .isMidiInputDeviceEnabled(
                        device.identifier))
            {
                enabledIdentifiers
                    .addIfNotAlreadyThere(
                        device.identifier);
            }
        }

        AppSettings settings;

        settings.setEnabledMidiDeviceIdentifiers(
            enabledIdentifiers);

        holder.saveAudioDeviceState();
    }

    StandaloneSettingsLookAndFeel settingsLookAndFeel;
    juce::StandalonePluginHolder& holder;
    StandaloneMidiOutputController& midiOutputController;
    juce::GroupComponent outputDeviceGroup;
    juce::Label outputDeviceLabel;
    juce::ComboBox outputDeviceCombo;
    juce::ToggleButton sendGeneratedMidiButton;
    juce::ToggleButton midiThruButton;
    juce::Label outputInfoLabel;
    juce::StringArray outputDeviceIdentifiers;
    bool isRebuildingOutputDevices = false;
    juce::GroupComponent inputDevicesGroup;
    DeviceListContent deviceListContent;
    juce::Viewport viewport;
    juce::TextButton refreshButton;
};

class StandaloneMenuExtension final : public MainView::MenuExtension
{
public:
    StandaloneMenuExtension(
        juce::StandalonePluginHolder& holderIn,
        StandalonePlayHead& playHeadIn,
        StandalonePlayHeadTracker& playHeadTrackerIn,
        StandaloneMidiOutputController& midiOutputControllerIn)
        : holder(holderIn),
          midiOutputController(midiOutputControllerIn),
          tempoControls(
              static_cast<PolyHostPluginProcessor*>(
                  holderIn.processor.get()),
              playHeadIn,
              playHeadTrackerIn)
    {
    }

    juce::StringArray getAdditionalMenuNames() const override
    {
        return {};
    }

    juce::Component* getMenuBarRightComponent() override
    {
        return &tempoControls;
    }

    int getMenuBarRightComponentWidth() const override
    {
        return StandaloneTempoControls::preferredWidth;
    }

    int getMenuBarHeight() const override
    {
        return StandaloneTempoControls::preferredHeight;
    }

    double getCurrentHostTempoBpm() const override
    {
        return tempoControls.getTempoBpm();
    }

    double getDefaultHostTempoBpm() const override
    {
        return tempoControls.getDefaultTempoBpm();
    }

    void setCurrentHostTempoBpm(double bpm) override
    {
        tempoControls.setTempoBpmFromPreset(bpm);
    }

    void setRecordingViewToggleCallback(
        std::function<void()> callback)
    {
        tempoControls.setRecordingViewToggleCallback(
            std::move(callback));
    }

    juce::PopupMenu getAdditionalMenuForName(
        const juce::String& menuName) override
    {
        juce::ignoreUnused(menuName);
        juce::PopupMenu menu;
        return menu;
    }

    void addAdditionalItemsToMenu(
        const juce::String& menuName,
        juce::PopupMenu& menu) override
    {
        if (menuName == "Options")
        {
            menu.addItem(commandAudioSettings,
                         "Audio Settings");
            return;
        }

        if (menuName == "File")
        {
            menu.addSeparator();
            menu.addItem(commandQuit, "Quit");
            return;
        }

        if (menuName != "MIDI")
            return;

        menu.addItem(
            commandMidiSettings,
            "MIDI Settings...");

        menu.addSeparator();
    }

    bool handleAdditionalMenuCommand(
        int menuItemID) override
    {
        if (menuItemID == commandAudioSettings)
        {
            showAudioSettingsDialog();
            return true;
        }

        if (menuItemID == commandMidiSettings)
        {
            showMidiSettingsDialog();
            return true;
        }

        if (menuItemID == commandQuit)
        {
            if (auto* application =
                    juce::JUCEApplicationBase::getInstance())
            {
                application->systemRequestedQuit();
            }

            return true;
        }

        return false;
    }

private:
    enum CommandIds
    {
        commandAudioSettings = 10001,
        commandMidiSettings = 10002,
        commandQuit = 10003
    };

    void showMidiSettingsDialog()
    {
        auto content =
            std::make_unique<
                StandaloneMidiSettingsComponent>(
                    holder,
                    midiOutputController);

        content->setSize(500, 500);

        juce::DialogWindow::LaunchOptions options;

        options.content.setOwned(
            content.release());

        options.dialogTitle =
            "MIDI Settings";

        options.dialogBackgroundColour =
            options.content->getLookAndFeel()
                .findColour(
                    juce::ResizableWindow::
                        backgroundColourId);

        options.escapeKeyTriggersCloseButton =
            true;

        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

    void showAudioSettingsDialog()
    {
        int maxNumInputs = 0;
        int maxNumOutputs = 0;

        if (holder.channelConfiguration.size() > 0)
        {
            const auto& configuration =
                holder.channelConfiguration.getReference(0);

            maxNumInputs =
                juce::jmax(
                    0,
                    static_cast<int>(
                        configuration.numIns));

            maxNumOutputs =
                juce::jmax(
                    0,
                    static_cast<int>(
                        configuration.numOuts));
        }

        if (holder.processor != nullptr)
        {
            if (auto* inputBus =
                    holder.processor->getBus(true, 0))
            {
                maxNumInputs =
                    juce::jmax(
                        0,
                        inputBus->getDefaultLayout().size());
            }

            if (auto* outputBus =
                    holder.processor->getBus(false, 0))
            {
                maxNumOutputs =
                    juce::jmax(
                        0,
                        outputBus->getDefaultLayout().size());
            }
        }

        auto content =
            std::make_unique<
                StandaloneAudioSettingsComponent>(
                    holder,
                    maxNumInputs,
                    maxNumOutputs);

        content->setSize(500, 550);
        content->setToRecommendedSize();

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(content.release());
        options.dialogTitle = "Audio Settings";
        options.dialogBackgroundColour =
            options.content->getLookAndFeel()
                .findColour(
                    juce::ResizableWindow::
                        backgroundColourId);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

    juce::StandalonePluginHolder& holder;
    StandaloneMidiOutputController& midiOutputController;
    StandaloneTempoControls tempoControls;
};

class PolyHostStandaloneWindow final : public juce::DocumentWindow
{
public:
    PolyHostStandaloneWindow(
        const juce::String& title,
        std::unique_ptr<juce::StandalonePluginHolder> pluginHolderIn)
        : juce::DocumentWindow(
              title,
              juce::LookAndFeel::getDefaultLookAndFeel().findColour(
                  juce::ResizableWindow::backgroundColourId),
              juce::DocumentWindow::allButtons),
          pluginHolder(std::move(pluginHolderIn)),
          menuExtension(*pluginHolder,
                        playHead,
                        playHeadTracker,
                        midiOutputController)
    {
        setUsingNativeTitleBar(true);

        processor =
            static_cast<PolyHostPluginProcessor*>(pluginHolder->processor.get());

        jassert(processor != nullptr);

        if (processor == nullptr)
        {
            setSize(800, 500);
            centreWithSize(getWidth(), getHeight());
            return;
        }

        pluginHolder->stopPlaying();

        AppSettings settings;
        playHead.setTempoBpm(settings.getDefaultTempoBpm());
        restoreMidiInputState(settings);

        if (auto* device =
                pluginHolder->deviceManager.getCurrentAudioDevice())
        {
            playHead.prepareToPlay(device->getCurrentSampleRate());
        }

        processor->setPlayHead(&playHead);
        playHeadTracker.setAudioRecordingController(
            &processor->getAudioRecordingController());
        playHeadTracker.setMidiRecordingController(
            &processor->getMidiRecordingController());
        processor->setStandaloneAudioExtension(
            &playHeadTracker);
        pluginHolder->startPlaying();

        auto* editor =
            new PolyHostPluginEditor(*processor, &menuExtension);

        for (int index = 0;
             index < editor->getNumChildComponents();
             ++index)
        {
            if (auto* childMainView =
                    dynamic_cast<MainView*>(
                        editor->getChildComponent(index)))
            {
                mainView = childMainView;
                break;
            }
        }

        if (mainView != nullptr)
        {
            menuExtension.setRecordingViewToggleCallback(
                [safeMainView =
                     juce::Component::SafePointer<MainView>(mainView)]
                {
                    if (safeMainView != nullptr)
                        safeMainView->toggleRecordingView();
                });
        }

        constexpr int sharedMenuBarHeight = 24;
        constexpr int standaloneMenuBarHeight =
            StandaloneTempoControls::preferredHeight;

        constexpr int extraStandaloneHeight =
            standaloneMenuBarHeight - sharedMenuBarHeight;

        editor->setSize(
            editor->getWidth(),
            editor->getHeight() + extraStandaloneHeight);

        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);

        constexpr int minimumStandaloneWidth = 530;

        setResizeLimits(
            minimumStandaloneWidth,
            1,
            32768,
            32768);

        if (getWidth() < minimumStandaloneWidth)
            setSize(minimumStandaloneWidth, getHeight());

        centreWithSize(getWidth(), getHeight());
    }

    ~PolyHostStandaloneWindow() override
    {
        const auto metronomeModeToSave =
            playHeadTracker.getMetronomeMode();

        if (pluginHolder != nullptr)
            pluginHolder->stopPlaying();

        if (processor != nullptr)
        {
            processor->setStandaloneAudioExtension(nullptr);
            playHeadTracker.setAudioRecordingController(nullptr);
            playHeadTracker.setMidiRecordingController(nullptr);
            processor->setPlayHead(nullptr);
        }

        clearContentComponent();

        {
            AppSettings settings;
            settings.setMetronomeMode(metronomeModeToSave);
        }

        pluginHolder.reset();
    }

    void closeButtonPressed() override
    {
        if (auto* application = juce::JUCEApplicationBase::getInstance())
            application->systemRequestedQuit();
    }

    void savePluginState()
    {
        if (pluginHolder != nullptr)
        {
            DebugLog::write("[Shutdown] audio stop begin");
            pluginHolder->stopPlaying();
            DebugLog::write("[Shutdown] audio stop returned");

            saveMidiInputState();

            DebugLog::write("[Shutdown] hosted plugin state save begin");
            pluginHolder->savePluginState();
            DebugLog::write("[Shutdown] hosted plugin state save returned");

            DebugLog::write("[Shutdown] audio device state save begin");
            pluginHolder->saveAudioDeviceState();
            DebugLog::write("[Shutdown] audio device state save returned");
        }
    }

    bool openPluginPath(const juce::String& pluginPath,
                        bool openInNewTab)
    {
        if (pluginHolder == nullptr || mainView == nullptr)
            return false;

        pluginHolder->stopPlaying();

        const bool loaded =
            mainView->openPluginPath(pluginPath,
                                     openInNewTab);

        pluginHolder->startPlaying();

        return loaded;
    }

private:
    void restoreMidiInputState(const AppSettings& settings)
    {
        if (pluginHolder == nullptr)
            return;

        const auto enabledIdentifiers =
            settings.getEnabledMidiDeviceIdentifiers();

        for (const auto& device :
             juce::MidiInput::getAvailableDevices())
        {
            pluginHolder->deviceManager.setMidiInputDeviceEnabled(
                device.identifier,
                enabledIdentifiers.contains(device.identifier));
        }
    }

    void saveMidiInputState()
    {
        if (pluginHolder == nullptr)
            return;

        juce::StringArray enabledIdentifiers;

        for (const auto& device :
             juce::MidiInput::getAvailableDevices())
        {
            if (pluginHolder->deviceManager
                    .isMidiInputDeviceEnabled(device.identifier))
            {
                enabledIdentifiers.addIfNotAlreadyThere(
                    device.identifier);
            }
        }

        AppSettings settings;
        settings.setEnabledMidiDeviceIdentifiers(
            enabledIdentifiers);
    }

    StandaloneMidiOutputController midiOutputController;
    StandalonePlayHead playHead;
    StandalonePlayHeadTracker playHeadTracker {
        playHead,
        midiOutputController
    };
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
    StandaloneMenuExtension menuExtension;
    PolyHostPluginProcessor* processor = nullptr;
    MainView* mainView = nullptr;
};

class PolyHostStandaloneApplication final : public juce::JUCEApplication
{
public:
    PolyHostStandaloneApplication()
    {
        AppSettings settings;

        const auto savedStandaloneState =
            settings.getAudioDeviceState().trim();

        if (savedStandaloneState.isNotEmpty())
        {
            if (auto savedStateXml =
                    juce::XmlDocument::parse(
                        savedStandaloneState))
            {
                standaloneProperties.restoreFromXml(
                    *savedStateXml);
            }
        }
    }

    const juce::String getApplicationName() override
    {
        return juce::CharPointer_UTF8(JucePlugin_Name);
    }

    const juce::String getApplicationVersion() override
    {
        return JucePlugin_VersionString;
    }

    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }

    void anotherInstanceStarted(
        const juce::String& commandLine) override
    {
        if (mainWindow == nullptr)
            return;

        mainWindow->setMinimised(false);
        mainWindow->setVisible(true);
        mainWindow->toFront(true);

        openPluginPaths(commandLine, false);
    }

    void initialise(
        const juce::String& commandLine) override
    {
        showStartupSplash();

        const auto pluginPaths =
            getPluginPathsFromCommandLine(commandLine);

        if (! pluginPaths.isEmpty())
            standaloneProperties.removeValue("filterState");

        mainWindow = std::make_unique<PolyHostStandaloneWindow>(
            getApplicationName(),
            createPluginHolder());

        mainWindow->setVisible(true);

        if (! pluginPaths.isEmpty())
        {
            DebugLog::write("[ExternalOpen] startup plugin load queued until window is visible");

            juce::MessageManager::callAsync([this, commandLine]
            {
                if (mainWindow == nullptr)
                    return;

                DebugLog::write("[ExternalOpen] queued startup plugin load begin | windowShowing="
                                + juce::String(mainWindow->isShowing() ? "true" : "false"));
                openPluginPaths(commandLine, true);
                DebugLog::write("[ExternalOpen] queued startup plugin load complete");
                centreMainWindowAtCurrentSizeAsync();
            });
        }
        else
        {
            centreMainWindowAtCurrentSizeAsync();
        }
    }

    void shutdown() override
    {
        startupSplash.reset();

        if (mainWindow != nullptr)
        {
            DebugLog::write("[Shutdown] window hide begin");
            mainWindow->setVisible(false);
            DebugLog::write("[Shutdown] window hide returned");

            mainWindow->savePluginState();
        }

        DebugLog::write("[Shutdown] window destruction begin");
        mainWindow.reset();
        DebugLog::write("[Shutdown] window destruction returned");

        if (auto stateXml =
                standaloneProperties.createXml(
                    "StandaloneProperties"))
        {
            AppSettings settings;

            settings.setAudioDeviceState(
                stateXml->toString());
        }

        DebugLog::write("[Shutdown] complete");
    }

    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay(
                100,
                []
                {
                    if (auto* application =
                            juce::JUCEApplicationBase::getInstance())
                    {
                        application->systemRequestedQuit();
                    }
                });
        }
        else
        {
            quit();
        }
    }

private:
    juce::Image createStartupSplashImage()
    {
        constexpr int splashWidth = 500;
        constexpr int splashHeight = 210;

        juce::Image image(
            juce::Image::ARGB,
            splashWidth,
            splashHeight,
            true);

        juce::Graphics graphics(image);

        const auto panelBounds =
            image.getBounds().reduced(8).toFloat();

        graphics.setColour(juce::Colour(0xFF1D2230));
        graphics.fillRoundedRectangle(panelBounds, 12.0f);

        graphics.setColour(juce::Colour(0xFF566072));
        graphics.drawRoundedRectangle(panelBounds, 12.0f, 1.0f);

        juce::Path logoPath;
        logoPath.startNewSubPath(66.0f, 38.0f);
        logoPath.lineTo(94.0f, 66.0f);
        logoPath.lineTo(66.0f, 94.0f);
        logoPath.lineTo(38.0f, 66.0f);
        logoPath.closeSubPath();

        graphics.setColour(juce::Colour(0xFFB12CDB));
        graphics.fillPath(logoPath);

        graphics.setColour(juce::Colours::white);
        graphics.setFont(
            juce::Font(juce::FontOptions(
                27.0f,
                juce::Font::bold)));
        graphics.drawFittedText(
            getApplicationName(),
            112,
            37,
            344,
            38,
            juce::Justification::centredLeft,
            1);

        graphics.setColour(juce::Colours::lightgrey);
        graphics.setFont(
            juce::Font(juce::FontOptions(15.0f)));
        graphics.drawFittedText(
            "Version " + getApplicationVersion(),
            113,
            75,
            343,
            24,
            juce::Justification::centredLeft,
            1);

        graphics.setColour(juce::Colour(0xFF30384A));
        graphics.fillRect(38, 118, 424, 1);

        graphics.setColour(juce::Colours::white);
        graphics.setFont(
            juce::Font(juce::FontOptions(16.0f)));
        graphics.drawFittedText(
            "Loading last session...",
            38,
            137,
            424,
            28,
            juce::Justification::centred,
            1);

        graphics.setColour(juce::Colour(0xFF3F8AD8));
        graphics.fillRoundedRectangle(
            juce::Rectangle<float>(38.0f, 176.0f, 424.0f, 3.0f),
            1.5f);

        return image;
    }

    void showStartupSplash()
    {
        DebugLog::write("[Startup] splash display begin");

        startupSplash = std::make_unique<juce::SplashScreen>(
            getApplicationName(),
            createStartupSplashImage(),
            true);

        startupSplash->setAlwaysOnTop(true);
        startupSplash->toFront(false);

        // Allow the new top-level window to paint before hosted plug-ins are
        // restored synchronously on the message thread.
        juce::MessageManager::getInstance()
            ->runDispatchLoopUntil(20);

        DebugLog::write("[Startup] splash display complete");
    }

    void centreMainWindowAtCurrentSizeAsync()
    {
        DebugLog::write("[Window] final startup centre queued");

        juce::MessageManager::callAsync([this]
        {
            if (mainWindow == nullptr)
                return;

            DebugLog::write("[Window] final startup centre begin | size="
                            + juce::String(mainWindow->getWidth())
                            + "x"
                            + juce::String(mainWindow->getHeight()));

            mainWindow->centreWithSize(mainWindow->getWidth(),
                                       mainWindow->getHeight());

            startupSplash.reset();

            DebugLog::write("[Window] final startup centre complete");
        });
    }

    static juce::StringArray getPluginPathsFromCommandLine(
        const juce::String& commandLine)
    {
        juce::StringArray pluginPaths;

        const auto arguments =
            juce::StringArray::fromTokens(
                commandLine,
                true);

        for (auto argument : arguments)
        {
            argument = argument.trim().unquoted();

            if (argument.isEmpty())
                continue;

            const juce::File file(argument);

            const bool isAcceptedFormat =
                file.hasFileExtension(".vst3")
               #if JUCE_PLUGINHOST_VST
                || file.hasFileExtension(".dll")
               #endif
                ;

            if (isAcceptedFormat && file.exists())
            {
                pluginPaths.addIfNotAlreadyThere(
                    file.getFullPathName());
            }
        }

        return pluginPaths;
    }

    void openPluginPaths(
        const juce::String& commandLine,
        bool firstPathUsesExistingTab)
    {
        if (mainWindow == nullptr)
            return;

        const auto pluginPaths =
            getPluginPathsFromCommandLine(commandLine);

        bool openInNewTab =
            ! firstPathUsesExistingTab;

        for (const auto& pluginPath : pluginPaths)
        {
            mainWindow->openPluginPath(
                pluginPath,
                openInNewTab);

            openInNewTab = true;
        }
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts>
            channelConfiguration;

        return std::make_unique<juce::StandalonePluginHolder>(
            &standaloneProperties,
            false,
            juce::String(),
            nullptr,
            channelConfiguration,
            false);
    }

    juce::PropertySet standaloneProperties;
    std::unique_ptr<juce::SplashScreen> startupSplash;
    std::unique_ptr<PolyHostStandaloneWindow> mainWindow;
};
}

START_JUCE_APPLICATION(PolyHostStandaloneApplication)
