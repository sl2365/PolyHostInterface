#include "PluginProcessor.h"
#include "PluginEditor.h"

PolyHostPluginProcessor::MacroParameter::MacroParameter(PolyHostPluginProcessor& ownerIn, int macroIndexIn)
    : owner(ownerIn),
      macroIndex(macroIndexIn)
{
}

float PolyHostPluginProcessor::MacroParameter::getValue() const
{
    return owner.getCore().getMacroCurrentValue(macroIndex);
}

void PolyHostPluginProcessor::MacroParameter::setValue(float newValue)
{
    owner.getCore().setMacroValueFromHost(macroIndex, newValue);
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
        auto parameter = std::make_unique<MacroParameter>(*this, i);
        addParameter(parameter.get());
        macroParameters.push_back(std::move(parameter));
    }
}

PolyHostPluginProcessor::PolyHostPluginProcessor()
    : juce::AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    initialiseMacroParameters();
}

PolyHostPluginProcessor::~PolyHostPluginProcessor()
{
}

void PolyHostPluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    core.prepareToPlay(sampleRate, samplesPerBlock);
}

void PolyHostPluginProcessor::releaseResources()
{
    core.releaseResources();
}

void PolyHostPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();

        pushMidiMonitorEvent(message, "Host MIDI");

        if (message.isController())
            pushPointerMidiEvent(message.getControllerNumber(),
                                 message.getControllerValue());
    }

    core.processBlock(buffer, midiMessages);
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

    const bool inputOk = (inputSet == juce::AudioChannelSet::mono()
                          || inputSet == juce::AudioChannelSet::stereo());

    const bool outputOk = (outputSet == juce::AudioChannelSet::mono()
                           || outputSet == juce::AudioChannelSet::stereo());

    return inputOk && outputOk;
}

void PolyHostPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto stateXml = std::make_unique<juce::XmlElement>("POLYHOST_STATE");

    stateXml->setAttribute("sessionName", core.getSessionName());
    stateXml->setAttribute("currentPresetFile",
                           core.getSessionDocument().getCurrentPresetFile().getFullPathName());
    stateXml->setAttribute("selectedTabIndex", core.getSelectedTabIndex());

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
    std::unique_ptr<juce::XmlElement> stateXml(getXmlFromBinary(data, sizeInBytes));

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

    if (currentPresetFilePath.isNotEmpty())
        core.getSessionDocument().setCurrentPresetFile(juce::File(currentPresetFilePath));
    else
        core.getSessionDocument().setCurrentPresetFile({});

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

void PolyHostPluginProcessor::pushMidiMonitorEvent(const juce::MidiMessage& message,
                                                   const juce::String& sourceName)
{
    const juce::ScopedLock sl(midiMonitorQueueLock);

    MidiMonitorEvent event;
    event.message = message;
    event.sourceName = sourceName;
    event.valid = true;

    midiMonitorQueue.add(event);

    if (midiMonitorQueue.size() > 512)
        midiMonitorQueue.removeRange(0, midiMonitorQueue.size() - 512);
}

juce::Array<PolyHostPluginProcessor::MidiMonitorEvent> PolyHostPluginProcessor::popPendingMidiMonitorEvents()
{
    const juce::ScopedLock sl(midiMonitorQueueLock);

    auto result = midiMonitorQueue;
    midiMonitorQueue.clear();
    return result;
}

PluginCore& PolyHostPluginProcessor::getCore()
{
    return core;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolyHostPluginProcessor();
}