#include "PluginProcessor.h"
#include "PluginEditor.h"

PolyHostPluginProcessor::PolyHostPluginProcessor()
    : juce::AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
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

PluginCore& PolyHostPluginProcessor::getCore()
{
    return core;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolyHostPluginProcessor();
}