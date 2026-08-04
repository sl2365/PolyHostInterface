#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

PolyHostPluginProcessor::MacroParameter::MacroParameter(PolyHostPluginProcessor& ownerIn, int macroIndexIn)
    : owner(ownerIn),
      macroIndex(macroIndexIn)
{
}

float PolyHostPluginProcessor::MacroParameter::getValue() const
{
    return owner.getCore().getMacroCurrentValue(macroIndex);
}

void PolyHostPluginProcessor::MacroParameter::setValue(
    float newValue)
{
    owner.setMacroParameterValue(
        macroIndex,
        newValue);
}

float PolyHostPluginProcessor::MacroParameter::getDefaultValue() const
{
    return 0.0f;
}

juce::String PolyHostPluginProcessor::MacroParameter::getName(int maximumStringLength) const
{
    juce::String name = "Macro " + juce::String(macroIndex + 1).paddedLeft('0', 3);
    return name.substring(0, maximumStringLength);
}

juce::String PolyHostPluginProcessor::MacroParameter::getLabel() const
{
    return {};
}

juce::String PolyHostPluginProcessor::MacroParameter::getText(float value, int maximumStringLength) const
{
    juce::String text = juce::String(juce::jlimit(0.0f, 1.0f, value), 3);
    return text.substring(0, maximumStringLength);
}

float PolyHostPluginProcessor::MacroParameter::getValueForText(const juce::String& text) const
{
    return juce::jlimit(0.0f, 1.0f, text.getFloatValue());
}

bool PolyHostPluginProcessor::MacroParameter::isAutomatable() const
{
    return true;
}

bool PolyHostPluginProcessor::MacroParameter::isDiscrete() const
{
    return false;
}

bool PolyHostPluginProcessor::MacroParameter::isBoolean() const
{
    return false;
}

int PolyHostPluginProcessor::MacroParameter::getNumSteps() const
{
    return juce::AudioProcessor::getDefaultNumParameterSteps();
}

bool PolyHostPluginProcessor::MacroParameter::isOrientationInverted() const
{
    return false;
}

bool PolyHostPluginProcessor::MacroParameter::isMetaParameter() const
{
    return false;
}

juce::AudioProcessorParameter::Category PolyHostPluginProcessor::MacroParameter::getCategory() const
{
    return juce::AudioProcessorParameter::genericParameter;
}

void PolyHostPluginProcessor::initialiseMacroParameters()
{
    constexpr int macroCount = 128;
    macroParameters.reserve((size_t) macroCount);

    for (int i = 0; i < macroCount; ++i)
    {
        auto* parameter =
            new MacroParameter(*this, i);

        addParameter(parameter);
        macroParameters.push_back(parameter);
    }
}

PolyHostPluginProcessor::PolyHostPluginProcessor()
    : juce::AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    initialiseMacroParameters();

    core.setPointerAutomationCallback(
        [this](int macroIndex,
               float normalizedValue)
        {
            queuePointerAutomationHostNotification(
                macroIndex,
                normalizedValue);
        });
}

PolyHostPluginProcessor::~PolyHostPluginProcessor()
{
    audioRecordingController.stopRecording("Recording stopped: PHI is closing");
    core.setPointerAutomationCallback({});

    cancelPendingUpdate();
    stopTimer();
    endPointerAutomationGesture();
}

void PolyHostPluginProcessor::prepareToPlay(double sampleRate,
                                            int samplesPerBlock)
{
    diagnosticPrepareCalls.fetch_add(
        1,
        std::memory_order_relaxed);

    processorMidiInputScratchBuffer.clear();
    processorMidiInputScratchBuffer.ensureSize(
        64 * 1024);

    midiOutputResetScratchBuffer.clear();
    midiOutputResetScratchBuffer.ensureSize(
        8 * 1024);

    for (int channel = 1;
         channel <= 16;
         ++channel)
    {
        midiOutputResetScratchBuffer.addEvent(
            juce::MidiMessage::controllerEvent(
                channel,
                64,
                0),
            0);

        midiOutputResetScratchBuffer.addEvent(
            juce::MidiMessage::controllerEvent(
                channel,
                66,
                0),
            0);

        midiOutputResetScratchBuffer.addEvent(
            juce::MidiMessage::controllerEvent(
                channel,
                67,
                0),
            0);

        midiOutputResetScratchBuffer.addEvent(
            juce::MidiMessage::controllerEvent(
                channel,
                121,
                0),
            0);

        midiOutputResetScratchBuffer.addEvent(
            juce::MidiMessage::controllerEvent(
                channel,
                123,
                0),
            0);

        midiOutputResetScratchBuffer.addEvent(
            juce::MidiMessage::controllerEvent(
                channel,
                120,
                0),
            0);
    }

    pendingMidiOutputReset.store(
        false,
        std::memory_order_relaxed);

    core.prepareToPlay(sampleRate, samplesPerBlock);

    if (standaloneAudioExtension != nullptr)
    {
        standaloneAudioExtension->prepareToPlay(
            sampleRate,
            samplesPerBlock);

        audioRecordingController.prepareToPlay(
            sampleRate,
            samplesPerBlock);

        midiRecordingController.prepareToPlay(
            sampleRate,
            samplesPerBlock);
    }
    else
    {
        audioRecordingController.releaseResources();
        midiRecordingController.releaseResources();
    }
}

void PolyHostPluginProcessor::releaseResources()
{
    diagnosticReleaseCalls.fetch_add(
        1,
        std::memory_order_relaxed);

    audioRecordingController.releaseResources();
    midiRecordingController.releaseResources();

    if (standaloneAudioExtension != nullptr)
        standaloneAudioExtension->releaseResources();

    core.releaseResources();
}

void PolyHostPluginProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    auto ticksToMicros =
        [](juce::int64 tickCount) noexcept
        {
            return juce::roundToInt(
                juce::jlimit(
                    0.0,
                    2147483647.0,
                    juce::Time::highResolutionTicksToSeconds(
                        tickCount)
                        * 1000000.0));
        };

    auto updateMaximum =
        [](std::atomic<int>& target,
           int value) noexcept
        {
            int recordedMaximum =
                target.load(
                    std::memory_order_relaxed);

            while (value > recordedMaximum
                   && ! target.compare_exchange_weak(
                        recordedMaximum,
                        value,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed))
            {
            }
        };

    diagnosticProcessCallsStarted.fetch_add(
        1,
        std::memory_order_relaxed);

    diagnosticLastProcessActivityMs.store(
        juce::Time::getMillisecondCounter(),
        std::memory_order_relaxed);

    diagnosticLastInputMidiEventCount.store(
        midiMessages.getNumEvents(),
        std::memory_order_relaxed);

    const auto processStartTicks =
        juce::Time::getHighResolutionTicks();

    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels();
         i < getTotalNumOutputChannels();
         ++i)
    {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    processorMidiInputScratchBuffer.clear();

    if (buffer.getNumSamples() > 0)
    {
        processorMidiInputScratchBuffer.addEvents(
            midiMessages,
            0,
            buffer.getNumSamples(),
            0);
    }

    if (standaloneAudioExtension != nullptr)
    {
        standaloneAudioExtension->processMidiInput(
            processorMidiInputScratchBuffer);
    }

    for (const auto metadata : midiMessages)
    {
        const auto& message =
            metadata.getMessage();

        pushMidiMonitorEvent(message);

        if (message.isController())
        {
            pushPointerMidiEvent(
                message.getControllerNumber(),
                message.getControllerValue());
        }
    }

    const auto coreStartTicks =
        juce::Time::getHighResolutionTicks();

    core.processBlock(
        buffer,
        midiMessages,
        getPlayHead());

    const auto coreEndTicks =
        juce::Time::getHighResolutionTicks();

    const int coreMicros =
        ticksToMicros(
            coreEndTicks
            - coreStartTicks);

    diagnosticLastCoreMicros.store(
        coreMicros,
        std::memory_order_relaxed);

    updateMaximum(
        diagnosticMaxCoreMicros,
        coreMicros);

    if (standaloneAudioExtension != nullptr)
    {
        standaloneAudioExtension->processHostedMidiOutput(
            midiMessages);
    }

    const bool coreOutputResetRequested =
        core.consumeMidiOutputResetRequested();

    if (coreOutputResetRequested)
    {
        diagnosticCoreOutputResetRequests.fetch_add(
            1,
            std::memory_order_relaxed);
    }

    if (JucePlugin_ProducesMidiOutput != 0)
    {
        const bool localOutputResetRequested =
            buffer.getNumSamples() > 0
            && pendingMidiOutputReset.exchange(
                false,
                std::memory_order_acq_rel);

        const bool outputResetRequested =
            coreOutputResetRequested
            || localOutputResetRequested;

        if (outputResetRequested)
        {
            midiMessages.clear();

            midiMessages.addEvents(
                midiOutputResetScratchBuffer,
                0,
                buffer.getNumSamples(),
                0);
        }
        else
        {
            if (! sendGeneratedMidiToHost.load(
                    std::memory_order_relaxed))
            {
                midiMessages.clear();
            }

            if (midiThruEnabled.load(
                    std::memory_order_relaxed))
            {
                midiMessages.addEvents(
                    processorMidiInputScratchBuffer,
                    0,
                    buffer.getNumSamples(),
                    0);
            }
        }
    }

    if (standaloneAudioExtension != nullptr)
    {
        audioRecordingController.processAudioBlock(buffer);

        standaloneAudioExtension
            ->processOutputBlock(buffer);
    }

    const auto processEndTicks =
        juce::Time::getHighResolutionTicks();

    const int processMicros =
        ticksToMicros(
            processEndTicks
            - processStartTicks);

    const int postCoreMicros =
        ticksToMicros(
            processEndTicks
            - coreEndTicks);

    diagnosticLastProcessMicros.store(
        processMicros,
        std::memory_order_relaxed);

    diagnosticLastPostCoreMicros.store(
        postCoreMicros,
        std::memory_order_relaxed);

    updateMaximum(
        diagnosticMaxProcessMicros,
        processMicros);

    updateMaximum(
        diagnosticMaxPostCoreMicros,
        postCoreMicros);

    if (processMicros >= 5000)
    {
        diagnosticBlocksOver5Ms.fetch_add(
            1,
            std::memory_order_relaxed);

        diagnosticLastSlowProcessMicros.store(
            processMicros,
            std::memory_order_relaxed);

        diagnosticLastSlowCoreMicros.store(
            coreMicros,
            std::memory_order_relaxed);

        diagnosticLastSlowPostCoreMicros.store(
            postCoreMicros,
            std::memory_order_relaxed);
    }

    if (processMicros >= 10000)
    {
        diagnosticBlocksOver10Ms.fetch_add(
            1,
            std::memory_order_relaxed);
    }

    if (processMicros >= 20000)
    {
        diagnosticBlocksOver20Ms.fetch_add(
            1,
            std::memory_order_relaxed);
    }

    diagnosticLastOutputMidiEventCount.store(
        midiMessages.getNumEvents(),
        std::memory_order_relaxed);

    diagnosticProcessCallsCompleted.fetch_add(
        1,
        std::memory_order_relaxed);
}

juce::AudioProcessorEditor* PolyHostPluginProcessor::createEditor()
{
    return new PolyHostPluginEditor(*this);
}

bool PolyHostPluginProcessor::hasEditor() const
{
    return true;
}

const juce::String PolyHostPluginProcessor::getName() const
{
    return "PolyHostInterface";
}

bool PolyHostPluginProcessor::acceptsMidi() const
{
    return true;
}

bool PolyHostPluginProcessor::producesMidi() const
{
    return JucePlugin_ProducesMidiOutput != 0;
}

bool PolyHostPluginProcessor::isMidiEffect() const
{
    return false;
}

double PolyHostPluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PolyHostPluginProcessor::getNumPrograms()
{
    return 1;
}

int PolyHostPluginProcessor::getCurrentProgram()
{
    return 0;
}

void PolyHostPluginProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String PolyHostPluginProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void PolyHostPluginProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

bool PolyHostPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputSet = layouts.getMainInputChannelSet();
    const auto outputSet = layouts.getMainOutputChannelSet();

    const bool inputOk =
        inputSet == juce::AudioChannelSet::mono()
        || inputSet == juce::AudioChannelSet::stereo();

    const bool outputOk =
        outputSet == juce::AudioChannelSet::stereo();

    return inputOk && outputOk;
}

void PolyHostPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    diagnosticGetStateCalls.fetch_add(
        1,
        std::memory_order_relaxed);

    auto stateXml = std::make_unique<juce::XmlElement>("POLYHOST_STATE");

    stateXml->setAttribute(
        "sessionName",
        core.getSessionName());

    stateXml->setAttribute(
        "currentPresetFile",
        AppSettings::makePresetPathPortable(
            core.getSessionDocument()
                .getCurrentPresetFile()));

    stateXml->setAttribute(
        "selectedTabIndex",
        core.getSelectedTabIndex());

    stateXml->setAttribute(
        "sendGeneratedMidiToHost",
        getSendGeneratedMidiToHost());

    stateXml->setAttribute(
        "midiThruEnabled",
        getMidiThruEnabled());

    auto sessionData = core.buildSessionData();

    if (auto sessionXml = SessionManager::createXmlFromSessionData(sessionData))
    {
        auto sessionXmlString = sessionXml->toString();
        juce::MemoryBlock xmlData(sessionXmlString.toRawUTF8(),
                                  sessionXmlString.getNumBytesAsUTF8());

        stateXml->setAttribute("sessionDataXmlBase64",
                               xmlData.toBase64Encoding());
    }

    copyXmlToBinary(*stateXml, destData);
}

void PolyHostPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    diagnosticSetStateCalls.fetch_add(
        1,
        std::memory_order_relaxed);

    const juce::ScopedLock stateRestoreLock(
        getCallbackLock());

    std::unique_ptr<juce::XmlElement> stateXml(
        getXmlFromBinary(
            data,
            sizeInBytes));

    if (stateXml == nullptr)
        return;

    if (! stateXml->hasTagName("POLYHOST_STATE"))
        return;

    auto sessionName = stateXml->getStringAttribute("sessionName");
    auto currentPresetFilePath = stateXml->getStringAttribute("currentPresetFile").trim();
    auto sessionDataXmlBase64 = stateXml->getStringAttribute("sessionDataXmlBase64");
    auto selectedTabIndex = stateXml->getIntAttribute("selectedTabIndex", 0);

    sendGeneratedMidiToHost.store(
        stateXml->getBoolAttribute(
            "sendGeneratedMidiToHost",
            true),
        std::memory_order_relaxed);

    midiThruEnabled.store(
        stateXml->getBoolAttribute(
            "midiThruEnabled",
            false),
        std::memory_order_relaxed);

    if (sessionName.isNotEmpty())
        core.setSessionName(sessionName);

    bool restoredSession = false;

    if (sessionDataXmlBase64.isNotEmpty())
    {
        juce::MemoryBlock xmlData;

        if (xmlData.fromBase64Encoding(sessionDataXmlBase64))
        {
            auto xmlText = juce::String::fromUTF8(static_cast<const char*>(xmlData.getData()),
                                                  static_cast<int>(xmlData.getSize()));

            std::unique_ptr<juce::XmlElement> sessionXml(juce::XmlDocument::parse(xmlText));

            if (sessionXml != nullptr)
            {
                SessionData sessionData;
                juce::StringArray warnings;

                if (SessionManager::restoreSessionDataFromXml(*sessionXml, sessionData, warnings))
                {
                    if (core.restoreSessionData(sessionData, warnings))
                    {
                        restoredSession = true;

                        if (juce::isPositiveAndBelow(selectedTabIndex, core.getNumTabs()))
                            core.setSelectedTabIndex(selectedTabIndex);
                    }
                }
            }
        }
    }

    if (! restoredSession)
        core.resetForNewPreset();

    const auto currentPresetFile =
        AppSettings::resolvePresetPath(
            currentPresetFilePath);

    if (currentPresetFile.existsAsFile())
    {
        core.getSessionDocument()
            .setCurrentPresetFile(
                currentPresetFile);
    }
    else
    {
        core.getSessionDocument()
            .setCurrentPresetFile({});
    }

    core.markClean();
    core.suppressDirtyMarkingFor(4000);
}

void PolyHostPluginProcessor::pushPointerMidiEvent(int controllerNumber, int controllerValue)
{
    const int write = pointerMidiWriteIndex.load(std::memory_order_relaxed);
    const int nextWrite = (write + 1) % pointerMidiQueueSize;
    const int read = pointerMidiReadIndex.load(std::memory_order_acquire);

    if (nextWrite == read)
        return;

    pointerMidiQueue[write].controllerNumber = controllerNumber;
    pointerMidiQueue[write].controllerValue = controllerValue;
    pointerMidiQueue[write].valid = true;

    pointerMidiWriteIndex.store(nextWrite, std::memory_order_release);
}

bool PolyHostPluginProcessor::popNextPointerMidiEvent(PointerMidiEvent& dest)
{
    const int read = pointerMidiReadIndex.load(std::memory_order_relaxed);
    const int write = pointerMidiWriteIndex.load(std::memory_order_acquire);

    if (read == write)
        return false;

    dest = pointerMidiQueue[read];
    pointerMidiQueue[read].valid = false;

    pointerMidiReadIndex.store((read + 1) % pointerMidiQueueSize, std::memory_order_release);
    return true;
}

void PolyHostPluginProcessor::pushMidiMonitorEvent(
    const juce::MidiMessage& message)
{
    const int dataSize =
        message.getRawDataSize();

    if (dataSize <= 0
        || dataSize > midiMonitorMaxMessageBytes)
    {
        return;
    }

    const int write =
        midiMonitorWriteIndex.load(
            std::memory_order_relaxed);

    const int nextWrite =
        (write + 1) % midiMonitorQueueSize;

    const int read =
        midiMonitorReadIndex.load(
            std::memory_order_acquire);

    if (nextWrite == read)
        return;

    auto& event =
        midiMonitorQueue[write];

    std::memcpy(
        event.data.data(),
        message.getRawData(),
        static_cast<size_t>(dataSize));

    event.dataSize = dataSize;
    event.timeStamp = message.getTimeStamp();

    midiMonitorWriteIndex.store(
        nextWrite,
        std::memory_order_release);
}

juce::Array<
    PolyHostPluginProcessor::MidiMonitorEvent>
PolyHostPluginProcessor::popPendingMidiMonitorEvents()
{
    juce::Array<MidiMonitorEvent> result;

    for (;;)
    {
        const int read =
            midiMonitorReadIndex.load(
                std::memory_order_relaxed);

        const int write =
            midiMonitorWriteIndex.load(
                std::memory_order_acquire);

        if (read == write)
            break;

        auto& rawEvent =
            midiMonitorQueue[read];

        if (rawEvent.dataSize > 0
            && rawEvent.dataSize
                <= midiMonitorMaxMessageBytes)
        {
            MidiMonitorEvent event;

            event.message =
                juce::MidiMessage(
                    rawEvent.data.data(),
                    rawEvent.dataSize,
                    rawEvent.timeStamp);

            event.sourceName = "Host MIDI";
            event.valid = true;

            result.add(event);
        }

        rawEvent.dataSize = 0;

        midiMonitorReadIndex.store(
            (read + 1) % midiMonitorQueueSize,
            std::memory_order_release);
    }

    return result;
}

PluginCore& PolyHostPluginProcessor::getCore()
{
    return core;
}

AudioRecordingController& PolyHostPluginProcessor::getAudioRecordingController()
{
    return audioRecordingController;
}

MidiRecordingController& PolyHostPluginProcessor::getMidiRecordingController()
{
    return midiRecordingController;
}

void PolyHostPluginProcessor::
    sampleSuspensionDiagnostics()
{
    diagnosticSuspensionSamples.fetch_add(
        1,
        std::memory_order_relaxed);

    const bool outerSuspended =
        isSuspended();

    const bool previousOuterSuspended =
        diagnosticOuterSuspendedNow.exchange(
            outerSuspended,
            std::memory_order_relaxed);

    if (outerSuspended)
    {
        diagnosticOuterSuspendedObserved.store(
            true,
            std::memory_order_relaxed);
    }

    if (outerSuspended
        != previousOuterSuspended)
    {
        diagnosticOuterSuspensionTransitions.fetch_add(
            1,
            std::memory_order_relaxed);
    }

    auto* hostedInstance =
        core.getMainPluginInstance();

    const bool hostedInstancePresent =
        hostedInstance != nullptr;

    const bool hostedSuspended =
        hostedInstancePresent
        && hostedInstance->isSuspended();

    diagnosticHostedInstancePresentNow.store(
        hostedInstancePresent,
        std::memory_order_relaxed);

    const bool previousHostedSuspended =
        diagnosticHostedSuspendedNow.exchange(
            hostedSuspended,
            std::memory_order_relaxed);

    if (hostedSuspended)
    {
        diagnosticHostedSuspendedObserved.store(
            true,
            std::memory_order_relaxed);
    }

    if (hostedSuspended
        != previousHostedSuspended)
    {
        diagnosticHostedSuspensionTransitions.fetch_add(
            1,
            std::memory_order_relaxed);
    }

    const auto lastProcessActivityMs =
        diagnosticLastProcessActivityMs.load(
            std::memory_order_relaxed);

    if (lastProcessActivityMs == 0)
        return;

    const auto currentTimeMs =
        juce::Time::getMillisecondCounter();

    const auto elapsedMs =
        currentTimeMs
        - lastProcessActivityMs;

    const int processGapMs =
        static_cast<int>(
            juce::jmin(
                elapsedMs,
                static_cast<juce::uint32>(
                    2147483647u)));

    diagnosticCurrentProcessGapMs.store(
        processGapMs,
        std::memory_order_relaxed);

    int recordedMaximumGap =
        diagnosticMaximumProcessGapMs.load(
            std::memory_order_relaxed);

    while (processGapMs > recordedMaximumGap
           && ! diagnosticMaximumProcessGapMs
                    .compare_exchange_weak(
                        recordedMaximumGap,
                        processGapMs,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed))
    {
    }

    constexpr int inactiveThresholdMs =
        250;

    const bool processInactive =
        processGapMs
        >= inactiveThresholdMs;

    const bool previousProcessInactive =
        diagnosticProcessInactiveNow.exchange(
            processInactive,
            std::memory_order_relaxed);

    if (processInactive)
    {
        diagnosticProcessInactiveObserved.store(
            true,
            std::memory_order_relaxed);
    }

    if (processInactive
        != previousProcessInactive)
    {
        diagnosticProcessInactiveTransitions.fetch_add(
            1,
            std::memory_order_relaxed);
    }
}

juce::String
PolyHostPluginProcessor::
    buildProcessorDiagnosticsText() const
{
    juce::String text;

    auto addLine =
        [&text](
            const juce::String& name,
            const juce::String& value)
        {
            text << name
                 << ": "
                 << value
                 << "\n";
        };

    const auto started =
        diagnosticProcessCallsStarted.load(
            std::memory_order_relaxed);

    const auto completed =
        diagnosticProcessCallsCompleted.load(
            std::memory_order_relaxed);

    text << "Processor Runtime Diagnostics\n"
         << "-----------------------------\n";

    addLine(
        "Current Sample Rate",
        juce::String(
            getSampleRate(),
            1));

    addLine(
        "Current Block Size",
        juce::String(
            getBlockSize()));

    addLine(
        "Prepare Calls",
        juce::String(
            (juce::int64)
                diagnosticPrepareCalls.load(
                    std::memory_order_relaxed)));

    addLine(
        "Release Calls",
        juce::String(
            (juce::int64)
                diagnosticReleaseCalls.load(
                    std::memory_order_relaxed)));

    addLine(
        "Process Calls Started",
        juce::String(
            (juce::int64) started));

    addLine(
        "Process Calls Completed",
        juce::String(
            (juce::int64) completed));

    addLine(
        "Process Calls In Flight",
        juce::String(
            (juce::int64) started
            - (juce::int64) completed));

    addLine(
        "Suspension Samples",
        juce::String(
            (juce::int64)
                diagnosticSuspensionSamples.load(
                    std::memory_order_relaxed)));

    addLine(
        "Outer Suspended Now",
        diagnosticOuterSuspendedNow.load(
            std::memory_order_relaxed)
                ? "Yes"
                : "No");

    addLine(
        "Outer Suspension Observed",
        diagnosticOuterSuspendedObserved.load(
            std::memory_order_relaxed)
                ? "Yes"
                : "No");

    addLine(
        "Outer Suspension Transitions",
        juce::String(
            (juce::int64)
                diagnosticOuterSuspensionTransitions.load(
                    std::memory_order_relaxed)));

    addLine(
        "Hosted Instance Present Now",
        diagnosticHostedInstancePresentNow.load(
            std::memory_order_relaxed)
                ? "Yes"
                : "No");

    addLine(
        "Hosted Suspended Now",
        diagnosticHostedSuspendedNow.load(
            std::memory_order_relaxed)
                ? "Yes"
                : "No");

    addLine(
        "Hosted Suspension Observed",
        diagnosticHostedSuspendedObserved.load(
            std::memory_order_relaxed)
                ? "Yes"
                : "No");

    addLine(
        "Hosted Suspension Transitions",
        juce::String(
            (juce::int64)
                diagnosticHostedSuspensionTransitions.load(
                    std::memory_order_relaxed)));

    addLine(
        "Current Process Activity Gap ms",
        juce::String(
            diagnosticCurrentProcessGapMs.load(
                std::memory_order_relaxed)));

    addLine(
        "Maximum Process Activity Gap ms",
        juce::String(
            diagnosticMaximumProcessGapMs.load(
                std::memory_order_relaxed)));

    addLine(
        "Process Inactive Now",
        diagnosticProcessInactiveNow.load(
            std::memory_order_relaxed)
                ? "Yes"
                : "No");

    addLine(
        "Process Inactive Observed",
        diagnosticProcessInactiveObserved.load(
            std::memory_order_relaxed)
                ? "Yes"
                : "No");

    addLine(
        "Process Inactive Transitions",
        juce::String(
            (juce::int64)
                diagnosticProcessInactiveTransitions.load(
                    std::memory_order_relaxed)));

    addLine(
        "Last Outer Process Time us",
        juce::String(
            diagnosticLastProcessMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Maximum Outer Process Time us",
        juce::String(
            diagnosticMaxProcessMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Last Core Time us",
        juce::String(
            diagnosticLastCoreMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Maximum Core Time us",
        juce::String(
            diagnosticMaxCoreMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Last Post-Core Time us",
        juce::String(
            diagnosticLastPostCoreMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Maximum Post-Core Time us",
        juce::String(
            diagnosticMaxPostCoreMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Blocks At Least 5 ms",
        juce::String(
            (juce::int64)
                diagnosticBlocksOver5Ms.load(
                    std::memory_order_relaxed)));

    addLine(
        "Blocks At Least 10 ms",
        juce::String(
            (juce::int64)
                diagnosticBlocksOver10Ms.load(
                    std::memory_order_relaxed)));

    addLine(
        "Blocks At Least 20 ms",
        juce::String(
            (juce::int64)
                diagnosticBlocksOver20Ms.load(
                    std::memory_order_relaxed)));

    addLine(
        "Last Slow Outer Time us",
        juce::String(
            diagnosticLastSlowProcessMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Last Slow Core Time us",
        juce::String(
            diagnosticLastSlowCoreMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Last Slow Post-Core Time us",
        juce::String(
            diagnosticLastSlowPostCoreMicros.load(
                std::memory_order_relaxed)));

    addLine(
        "Last Input MIDI Event Count",
        juce::String(
            diagnosticLastInputMidiEventCount.load(
                std::memory_order_relaxed)));

    addLine(
        "Last Output MIDI Event Count",
        juce::String(
            diagnosticLastOutputMidiEventCount.load(
                std::memory_order_relaxed)));

    addLine(
        "Core Output Reset Requests",
        juce::String(
            (juce::int64)
                diagnosticCoreOutputResetRequests.load(
                    std::memory_order_relaxed)));

    addLine(
        "Get State Calls",
        juce::String(
            (juce::int64)
                diagnosticGetStateCalls.load(
                    std::memory_order_relaxed)));

    addLine(
        "Set State Calls",
        juce::String(
            (juce::int64)
                diagnosticSetStateCalls.load(
                    std::memory_order_relaxed)));

    return text;
}

bool PolyHostPluginProcessor::
    getSendGeneratedMidiToHost() const
{
    return sendGeneratedMidiToHost.load(
        std::memory_order_relaxed);
}

void PolyHostPluginProcessor::
    setSendGeneratedMidiToHost(
        bool shouldSend)
{
    const bool previousValue =
        sendGeneratedMidiToHost.exchange(
            shouldSend,
            std::memory_order_relaxed);

    if (previousValue != shouldSend)
    {
        core.requestMidiReleaseReset();

        pendingMidiOutputReset.store(
            true,
            std::memory_order_release);

        updateHostDisplay();
    }
}

bool PolyHostPluginProcessor::
    getMidiThruEnabled() const
{
    return midiThruEnabled.load(
        std::memory_order_relaxed);
}

void PolyHostPluginProcessor::
    setMidiThruEnabled(
        bool shouldEnable)
{
    const bool previousValue =
        midiThruEnabled.exchange(
            shouldEnable,
            std::memory_order_relaxed);

    if (previousValue != shouldEnable)
    {
        core.requestMidiReleaseReset();

        pendingMidiOutputReset.store(
            true,
            std::memory_order_release);

        updateHostDisplay();
    }
}

void PolyHostPluginProcessor::setMacroParameterValue(
    int macroIndex,
    float normalizedValue)
{
    if (pointerAutomationNotificationMacroIndex.load(
            std::memory_order_acquire) == macroIndex)
    {
        core.setMacroCurrentValueFromPointerAutomation(
            macroIndex,
            normalizedValue);

        return;
    }

    core.setMacroValueFromHost(
        macroIndex,
        normalizedValue);
}

void PolyHostPluginProcessor::
    queuePointerAutomationHostNotification(
        int macroIndex,
        float normalizedValue)
{
    if (macroIndex < 0
        || macroIndex >= (int) macroParameters.size())
    {
        return;
    }

    pendingPointerAutomationValue.store(
        juce::jlimit(
            0.0f,
            1.0f,
            normalizedValue),
        std::memory_order_relaxed);

    pendingPointerAutomationMacroIndex.store(
        macroIndex,
        std::memory_order_relaxed);

    pendingPointerAutomationSequence.fetch_add(
        1,
        std::memory_order_release);

    triggerAsyncUpdate();
}

void PolyHostPluginProcessor::handleAsyncUpdate()
{
    juce::uint32 sequenceBefore = 0;
    juce::uint32 sequenceAfter = 0;
    int macroIndex = -1;
    float normalizedValue = 0.0f;

    do
    {
        sequenceBefore =
            pendingPointerAutomationSequence.load(
                std::memory_order_acquire);

        if (sequenceBefore
            == lastHandledPointerAutomationSequence)
        {
            return;
        }

        macroIndex =
            pendingPointerAutomationMacroIndex.load(
                std::memory_order_relaxed);

        normalizedValue =
            pendingPointerAutomationValue.load(
                std::memory_order_relaxed);

        sequenceAfter =
            pendingPointerAutomationSequence.load(
                std::memory_order_acquire);
    }
    while (sequenceBefore != sequenceAfter);

    lastHandledPointerAutomationSequence =
        sequenceAfter;

    notifyHostOfPointerAutomation(
        macroIndex,
        normalizedValue);

    if (pendingPointerAutomationSequence.load(
            std::memory_order_acquire)
        != lastHandledPointerAutomationSequence)
    {
        triggerAsyncUpdate();
    }
}

void PolyHostPluginProcessor::
    notifyHostOfPointerAutomation(
        int macroIndex,
        float normalizedValue)
{
    if (! juce::isPositiveAndBelow(
            macroIndex,
            (int) macroParameters.size()))
    {
        return;
    }

    auto* parameter =
        macroParameters[(size_t) macroIndex];

    if (parameter == nullptr)
        return;

    if (activePointerAutomationMacroIndex
        != macroIndex)
    {
        endPointerAutomationGesture();

        parameter->beginChangeGesture();

        activePointerAutomationMacroIndex =
            macroIndex;
    }

    normalizedValue =
        juce::jlimit(
            0.0f,
            1.0f,
            normalizedValue);

    pointerAutomationNotificationMacroIndex.store(
        macroIndex,
        std::memory_order_release);

    parameter->setValueNotifyingHost(
        normalizedValue);

    pointerAutomationNotificationMacroIndex.store(
        -1,
        std::memory_order_release);

    lastPointerAutomationChangeMs =
        juce::Time::getMillisecondCounter();

    startTimer(50);
}

void PolyHostPluginProcessor::timerCallback()
{
    if (activePointerAutomationMacroIndex < 0)
    {
        stopTimer();
        return;
    }

    const auto elapsedMs =
        juce::Time::getMillisecondCounter()
        - lastPointerAutomationChangeMs;

    if (elapsedMs >= 300)
    {
        endPointerAutomationGesture();
        stopTimer();
    }
}

void PolyHostPluginProcessor::
    endPointerAutomationGesture()
{
    if (! juce::isPositiveAndBelow(
            activePointerAutomationMacroIndex,
            (int) macroParameters.size()))
    {
        activePointerAutomationMacroIndex = -1;
        return;
    }

    if (auto* parameter =
            macroParameters[
                (size_t) activePointerAutomationMacroIndex])
    {
        parameter->endChangeGesture();
    }

    activePointerAutomationMacroIndex = -1;
}

void PolyHostPluginProcessor::setStandaloneAudioExtension(
    StandaloneAudioExtension* extension)
{
    if (extension == nullptr && standaloneAudioExtension != nullptr)
    {
        audioRecordingController.releaseResources();
        midiRecordingController.releaseResources();
    }

    standaloneAudioExtension = extension;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolyHostPluginProcessor();
}
