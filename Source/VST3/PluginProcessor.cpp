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
    core.setPointerAutomationCallback({});

    cancelPendingUpdate();
    stopTimer();
    endPointerAutomationGesture();
}

void PolyHostPluginProcessor::prepareToPlay(double sampleRate,
                                            int samplesPerBlock)
{
    core.prepareToPlay(sampleRate, samplesPerBlock);

    if (standaloneAudioExtension != nullptr)
    {
        standaloneAudioExtension->prepareToPlay(
            sampleRate,
            samplesPerBlock);
    }
}

void PolyHostPluginProcessor::releaseResources()
{
    if (standaloneAudioExtension != nullptr)
        standaloneAudioExtension->releaseResources();

    core.releaseResources();
}

void PolyHostPluginProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels();
         i < getTotalNumOutputChannels();
         ++i)
    {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();

        pushMidiMonitorEvent(message);

        if (message.isController())
        {
            pushPointerMidiEvent(
                message.getControllerNumber(),
                message.getControllerValue());
        }
    }

    core.processBlock(buffer, midiMessages, getPlayHead());

    if (standaloneAudioExtension != nullptr)
        standaloneAudioExtension->processOutputBlock(buffer);
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
    return false;
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
    standaloneAudioExtension = extension;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolyHostPluginProcessor();
}