#include "PluginCore.h"

namespace
{
    PluginSlotType slotTypeFromDescription(const juce::PluginDescription& description)
    {
        return description.isInstrument ? PluginSlotType::Synth
                                        : PluginSlotType::FX;
    }

    juce::String memoryBlockToBase64(const juce::MemoryBlock& block)
    {
        if (block.getSize() == 0)
            return {};

        return block.toBase64Encoding();
    }

    juce::String sanitisePointerMapToken(juce::String text)
    {
        text = text.trim();

        if (text.isEmpty())
            text = "Unknown";

        text = text.replaceCharacter(':', '_')
                   .replaceCharacter('/', '_')
                   .replaceCharacter('\\', '_')
                   .replaceCharacter('*', '_')
                   .replaceCharacter('?', '_')
                   .replaceCharacter('"', '_')
                   .replaceCharacter('<', '_')
                   .replaceCharacter('>', '_')
                   .replaceCharacter('|', '_');

        return text;
    }
}

PluginCore::PluginCore()
{
    macroCurrentValues.fill(0.0f);

    sessionDocument.clear();
    statusText = "Ready";
    refreshAvailableMidiInputs();

   #if JUCE_PLUGINHOST_VST3
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
   #endif

   #if JUCE_PLUGINHOST_VST
    formatManager.addFormat(std::make_unique<juce::VSTPluginFormat>());
   #endif

    addTab("Empty");
    setSelectedTabIndex(0);
    tabModel.selectTab(0);
    rebuildTabModelFromHostedTabs();
}

PluginCore::~PluginCore()
{
    for (auto* tab : hostedTabs)
        detachFromHostedPlugin(tab->pluginInstance.get());
}

void PluginCore::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    playbackPrepared = true;

    for (auto* tab : hostedTabs)
    {
        if (tab->pluginInstance != nullptr)
            tab->pluginInstance->prepareToPlay(sampleRate, samplesPerBlock);
    }
}

void PluginCore::releaseResources()
{
    for (auto* tab : hostedTabs)
    {
        if (tab->pluginInstance != nullptr)
            tab->pluginInstance->releaseResources();
    }

    playbackPrepared = false;
}

void PluginCore::processBlock(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midiMessages)
{
    if (hostedTabs.isEmpty())
        return;

    const int numSamples = buffer.getNumSamples();
    const int hostChannels = buffer.getNumChannels();

    if (numSamples <= 0 || hostChannels <= 0)
        return;

    juce::AudioBuffer<float> hostInput(hostChannels, numSamples);
    hostInput.makeCopyOf(buffer, true);

    juce::AudioBuffer<float> finalOutput(hostChannels, numSamples);
    finalOutput.clear();

    juce::Array<juce::AudioBuffer<float>> fxInputBuffers;
    juce::Array<int> fxIndices;

    for (int i = 0; i < hostedTabs.size(); ++i)
    {
        if (hostedTabs[i] == nullptr || hostedTabs[i]->bypassed)
            continue;

        if (getHostedTabType(i) == PluginSlotType::FX && hostedTabs[i]->pluginInstance != nullptr)
        {
            fxIndices.add(i);
            fxInputBuffers.add(juce::AudioBuffer<float>(hostChannels, numSamples));
            fxInputBuffers.getReference(fxInputBuffers.size() - 1).clear();
        }
    }

    auto getFxBufferIndexForTab = [&fxIndices](int tabIndex) -> int
    {
        for (int i = 0; i < fxIndices.size(); ++i)
        {
            if (fxIndices[i] == tabIndex)
                return i;
        }

        return -1;
    };

    const int firstActiveFxTab = findNextActiveFxTab(-1);

    if (firstActiveFxTab >= 0)
    {
        const int firstFxBufferIndex = getFxBufferIndexForTab(firstActiveFxTab);

        if (juce::isPositiveAndBelow(firstFxBufferIndex, fxInputBuffers.size()))
        {
            auto& firstFxBuffer = fxInputBuffers.getReference(firstFxBufferIndex);

            for (int ch = 0; ch < hostChannels; ++ch)
                firstFxBuffer.addFrom(ch, 0, hostInput, ch, 0, numSamples);
        }
    }
    else
    {
        finalOutput.makeCopyOf(hostInput, true);
    }

    for (int i = 0; i < hostedTabs.size(); ++i)
    {
        auto* tab = hostedTabs[i];

        if (tab == nullptr || tab->pluginInstance == nullptr || tab->bypassed)
            continue;

        if (getHostedTabType(i) != PluginSlotType::Synth)
            continue;

        auto* instance = tab->pluginInstance.get();

        juce::AudioBuffer<float> synthBuffer(hostChannels, numSamples);
        synthBuffer.clear();

        juce::MidiBuffer synthMidi;
        buildMidiBufferForTab(i, midiMessages, synthMidi);

        const int totalInputChannels = instance->getTotalNumInputChannels();
        const int totalOutputChannels = instance->getTotalNumOutputChannels();

        if (hostChannels < totalOutputChannels)
            continue;

        for (int ch = totalInputChannels; ch < hostChannels; ++ch)
            synthBuffer.clear(ch, 0, numSamples);

        instance->processBlock(synthBuffer, synthMidi);

        const int nextFxTab = findNextActiveFxTab(i);

        if (nextFxTab >= 0)
        {
            const int fxBufferIndex = getFxBufferIndexForTab(nextFxTab);

            if (juce::isPositiveAndBelow(fxBufferIndex, fxInputBuffers.size()))
            {
                auto& targetBuffer = fxInputBuffers.getReference(fxBufferIndex);

                for (int ch = 0; ch < hostChannels; ++ch)
                    targetBuffer.addFrom(ch, 0, synthBuffer, ch, 0, numSamples);
            }
        }
        else
        {
            for (int ch = 0; ch < hostChannels; ++ch)
                finalOutput.addFrom(ch, 0, synthBuffer, ch, 0, numSamples);
        }
    }

    for (int fxListIndex = 0; fxListIndex < fxIndices.size(); ++fxListIndex)
    {
        const int tabIndex = fxIndices[fxListIndex];
        auto* tab = hostedTabs[tabIndex];

        if (tab == nullptr || tab->pluginInstance == nullptr || tab->bypassed)
            continue;

        auto* instance = tab->pluginInstance.get();
        auto& fxBuffer = fxInputBuffers.getReference(fxListIndex);
        juce::MidiBuffer fxMidi;
        buildMidiBufferForTab(tabIndex, midiMessages, fxMidi);

        const int totalInputChannels = instance->getTotalNumInputChannels();
        const int totalOutputChannels = instance->getTotalNumOutputChannels();

        if (hostChannels < totalOutputChannels)
            continue;

        for (int ch = totalInputChannels; ch < hostChannels; ++ch)
            fxBuffer.clear(ch, 0, numSamples);

        instance->processBlock(fxBuffer, fxMidi);

        const int nextFxTab = findNextActiveFxTab(tabIndex);

        if (nextFxTab >= 0)
        {
            const int nextFxBufferIndex = getFxBufferIndexForTab(nextFxTab);

            if (juce::isPositiveAndBelow(nextFxBufferIndex, fxInputBuffers.size()))
            {
                auto& targetBuffer = fxInputBuffers.getReference(nextFxBufferIndex);

                for (int ch = 0; ch < hostChannels; ++ch)
                    targetBuffer.addFrom(ch, 0, fxBuffer, ch, 0, numSamples);
            }
        }
        else
        {
            for (int ch = 0; ch < hostChannels; ++ch)
                finalOutput.addFrom(ch, 0, fxBuffer, ch, 0, numSamples);
        }
    }

    buffer.makeCopyOf(finalOutput, true);
    updateMeterLevels(hostInput, finalOutput);
}

float PluginCore::computePeakLevel(const float* samples, int numSamples)
{
    if (samples == nullptr || numSamples <= 0)
        return 0.0f;

    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
        peak = juce::jmax(peak, std::abs(samples[i]));

    return peak;
}

float PluginCore::smoothMeterLevel(float current, float target)
{
    constexpr float attackCoeff = 0.6f;
    constexpr float releaseCoeff = 0.08f;

    const float coeff = target > current ? attackCoeff : releaseCoeff;
    return current + (target - current) * coeff;
}

juce::File PluginCore::getPointerMapsDirectory()
{
    auto dir = AppSettings::getAppDirectory().getChildFile("PointerMaps");
    dir.createDirectory();
    return dir;
}

juce::String PluginCore::makeSafePointerMapFileName(const juce::String& pluginName,
                                                    const juce::String& manufacturer)
{
    auto safePlugin = sanitisePointerMapToken(pluginName);
    auto safeManufacturer = sanitisePointerMapToken(manufacturer);

    return safeManufacturer + " - " + safePlugin;
}

juce::File PluginCore::getGlobalPointerMapFileForTab(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return {};

    const auto* hostedTab = hostedTabs[tabIndex];

    if (hostedTab == nullptr || hostedTab->pluginInstance == nullptr)
        return {};

    juce::PluginDescription description;
    hostedTab->pluginInstance->fillInPluginDescription(description);

    return getPointerMapsDirectory()
        .getChildFile(makeSafePointerMapFileName(description.name,
                                                 description.manufacturerName))
        .withFileExtension(".xml");
}

void PluginCore::updateMeterLevels(const juce::AudioBuffer<float>& inputBuffer,
                                   const juce::AudioBuffer<float>& outputBuffer)
{
    const int inputSamples = inputBuffer.getNumSamples();
    const int outputSamples = outputBuffer.getNumSamples();

    float inL = 0.0f;
    float inR = 0.0f;
    float outL = 0.0f;
    float outR = 0.0f;

    if (inputBuffer.getNumChannels() > 0)
        inL = computePeakLevel(inputBuffer.getReadPointer(0), inputSamples);

    if (inputBuffer.getNumChannels() > 1)
        inR = computePeakLevel(inputBuffer.getReadPointer(1), inputSamples);
    else
        inR = inL;

    if (outputBuffer.getNumChannels() > 0)
        outL = computePeakLevel(outputBuffer.getReadPointer(0), outputSamples);

    if (outputBuffer.getNumChannels() > 1)
        outR = computePeakLevel(outputBuffer.getReadPointer(1), outputSamples);
    else
        outR = outL;

    inputMeterLevelL.store(smoothMeterLevel(inputMeterLevelL.load(std::memory_order_relaxed), inL),
                           std::memory_order_relaxed);
    inputMeterLevelR.store(smoothMeterLevel(inputMeterLevelR.load(std::memory_order_relaxed), inR),
                           std::memory_order_relaxed);
    outputMeterLevelL.store(smoothMeterLevel(outputMeterLevelL.load(std::memory_order_relaxed), outL),
                            std::memory_order_relaxed);
    outputMeterLevelR.store(smoothMeterLevel(outputMeterLevelR.load(std::memory_order_relaxed), outR),
                            std::memory_order_relaxed);
}

float PluginCore::getInputMeterLevelL() const
{
    return inputMeterLevelL.load(std::memory_order_relaxed);
}

float PluginCore::getInputMeterLevelR() const
{
    return inputMeterLevelR.load(std::memory_order_relaxed);
}

float PluginCore::getOutputMeterLevelL() const
{
    return outputMeterLevelL.load(std::memory_order_relaxed);
}

float PluginCore::getOutputMeterLevelR() const
{
    return outputMeterLevelR.load(std::memory_order_relaxed);
}

const juce::String PluginCore::getSessionName() const
{
    return sessionDocument.getDisplayName();
}

void PluginCore::setSessionName(const juce::String& newName)
{
    juce::ignoreUnused(newName);
}

bool PluginCore::isDirty() const
{
    return sessionDocument.isDirty();
}

void PluginCore::markDirty()
{
    sessionDocument.markDirty();
}

void PluginCore::markClean()
{
    sessionDocument.markClean();
}

juce::String PluginCore::getStatusText() const
{
    return statusText;
}

void PluginCore::setStatusText(const juce::String& newStatusText)
{
    statusText = newStatusText;
}

SessionDocument& PluginCore::getSessionDocument()
{
    return sessionDocument;
}

TabModel& PluginCore::getTabModel()
{
    return tabModel;
}

int PluginCore::getNumTabs() const
{
    return hostedTabs.size();
}

int PluginCore::getSelectedTabIndex() const
{
    return selectedTabIndex;
}

void PluginCore::setSelectedTabIndex(int newIndex)
{
    if (! juce::isPositiveAndBelow(newIndex, hostedTabs.size()))
        return;

    selectedTabIndex = newIndex;
    rebuildTabModelFromHostedTabs();
}

bool PluginCore::addTab(const juce::String& tabName)
{
    auto* tab = new HostedTabState();
    tab->slot = std::make_unique<SlotModel>();
    tab->slot->setSlotName("Main Slot");
    tab->slot->setPluginLoaded(false);
    tab->tabName = tabName.isNotEmpty() ? tabName
                                        : "Empty";
    tab->midiAssignedDeviceIdentifiers.addIfNotAlreadyThere("MIDI Ch: All");
    tab->hasCustomPointerMap = false;
    tab->preferGlobalPointerMap = false;
    tab->pointerLaneTolerance = 30.0f;
    tab->pointerAdjustSensitivity = 1;

    hostedTabs.add(tab);
    selectedTabIndex = hostedTabs.size() - 1;
    rebuildTabModelFromHostedTabs();
    markDirty();
    return true;
}

bool PluginCore::closeSelectedTab()
{
    if (! juce::isPositiveAndBelow(selectedTabIndex, hostedTabs.size()))
        return false;

    if (hostedTabs.size() <= 1)
    {
        resetForNewPreset();
        return true;
    }

    auto* tab = hostedTabs[selectedTabIndex];
    detachFromHostedPlugin(tab->pluginInstance.get());
    hostedTabs.remove(selectedTabIndex);

    ensureValidSelectedTab();
    rebuildTabModelFromHostedTabs();
    markDirty();
    statusText = "Tab closed";
    return true;
}

bool PluginCore::clearTab(int tabIndex)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    auto previousSelected = selectedTabIndex;
    selectedTabIndex = tabIndex;

    auto* selectedTab = getSelectedHostedTab();

    if (selectedTab == nullptr)
    {
        selectedTabIndex = previousSelected;
        return false;
    }

    detachFromHostedPlugin(selectedTab->pluginInstance.get());

    if (selectedTab->pluginInstance != nullptr)
    {
        selectedTab->pluginInstance->releaseResources();
        selectedTab->pluginInstance.reset();
    }

    selectedTab->slot->clearPlugin();
    selectedTab->tabName = "Empty";
    selectedTab->bypassed = false;
    selectedTab->pointerAdjustMethodOverride = 0;
    selectedTab->hasCustomPointerMap = false;
    selectedTab->preferGlobalPointerMap = false;
    selectedTab->pointerJumpPoints.clear();
    selectedTab->presetPointerJumpPoints.clear();
    selectedTab->pointerLaneTolerance = 30.0f;
    selectedTab->pointerAdjustSensitivity = 1;
    selectedTab->midiAssignedDeviceIdentifiers.clear();
    selectedTab->midiAssignedDeviceIdentifiers.addIfNotAlreadyThere("MIDI Ch: All");

    selectedTabIndex = previousSelected;
    rebuildTabModelFromHostedTabs();
    markDirty();
    statusText = "Tab cleared";
    return true;
}

bool PluginCore::reloadTabPlugin(int tabIndex)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    auto previousSelected = selectedTabIndex;
    selectedTabIndex = tabIndex;

    auto* selectedTab = getSelectedHostedTab();

    if (selectedTab == nullptr)
    {
        selectedTabIndex = previousSelected;
        return false;
    }

    if (selectedTab->pluginInstance == nullptr || selectedTab->slot == nullptr)
    {
        selectedTabIndex = previousSelected;
        return false;
    }

    const auto pluginPath = selectedTab->slot->getPluginPath();

    if (pluginPath.isEmpty())
    {
        selectedTabIndex = previousSelected;
        return false;
    }

    juce::MemoryBlock pluginState;
    selectedTab->pluginInstance->getStateInformation(pluginState);

    const auto tabName = selectedTab->tabName;
    const auto bypassed = selectedTab->bypassed;
    const auto pointerAdjustMethodOverride = selectedTab->pointerAdjustMethodOverride;
    const auto pointerAdjustSensitivity = selectedTab->pointerAdjustSensitivity;
    const auto hasCustomPointerMap = selectedTab->hasCustomPointerMap;
    const auto preferGlobalPointerMap = selectedTab->preferGlobalPointerMap;
    const auto pointerJumpPoints = selectedTab->pointerJumpPoints;
    const auto presetPointerJumpPoints = selectedTab->presetPointerJumpPoints;
    const auto midiAssignments = selectedTab->midiAssignedDeviceIdentifiers;

    unloadMainSlotPlugin();

    if (! loadMainSlotPluginFromPath(pluginPath))
    {
        selectedTab->tabName = tabName;
        selectedTab->bypassed = bypassed;
        selectedTab->pointerAdjustMethodOverride = pointerAdjustMethodOverride;
        selectedTab->pointerAdjustSensitivity = pointerAdjustSensitivity;
        selectedTab->hasCustomPointerMap = hasCustomPointerMap;
        selectedTab->preferGlobalPointerMap = preferGlobalPointerMap;
        selectedTab->pointerJumpPoints = pointerJumpPoints;
        selectedTab->presetPointerJumpPoints = presetPointerJumpPoints;
        selectedTab->midiAssignedDeviceIdentifiers = midiAssignments;

        selectedTabIndex = previousSelected;
        rebuildTabModelFromHostedTabs();
        statusText = "Plugin reload failed";
        return false;
    }

    if (pluginState.getSize() > 0)
        setMainSlotPluginState(pluginState.getData(), static_cast<int>(pluginState.getSize()));

    selectedTab->tabName = tabName;
    selectedTab->bypassed = bypassed;
    selectedTab->pointerAdjustMethodOverride = pointerAdjustMethodOverride;
    selectedTab->pointerAdjustSensitivity = pointerAdjustSensitivity;
    selectedTab->hasCustomPointerMap = hasCustomPointerMap;
    selectedTab->preferGlobalPointerMap = preferGlobalPointerMap;
    selectedTab->pointerJumpPoints = pointerJumpPoints;
    selectedTab->presetPointerJumpPoints = presetPointerJumpPoints;
    selectedTab->midiAssignedDeviceIdentifiers = midiAssignments;

    selectedTabIndex = previousSelected;
    rebuildTabModelFromHostedTabs();
    markDirty();
    statusText = "Plugin reloaded";
    return true;
}

void PluginCore::refreshTabModel()
{
    rebuildTabModelFromHostedTabs();
}

bool PluginCore::isTabBypassed(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    return hostedTabs[tabIndex]->bypassed;
}

void PluginCore::setTabBypassed(int tabIndex, bool shouldBeBypassed)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    hostedTabs[tabIndex]->bypassed = shouldBeBypassed;
    markDirty();
}

bool PluginCore::canMoveTabUp(int tabIndex) const
{
    return juce::isPositiveAndBelow(tabIndex, hostedTabs.size()) && tabIndex > 0;
}

bool PluginCore::canMoveTabDown(int tabIndex) const
{
    return juce::isPositiveAndBelow(tabIndex, hostedTabs.size()) && tabIndex < hostedTabs.size() - 1;
}

void PluginCore::moveHostedTab(int fromIndex, int toIndex)
{
    if (! juce::isPositiveAndBelow(fromIndex, hostedTabs.size()))
        return;

    if (! juce::isPositiveAndBelow(toIndex, hostedTabs.size()))
        return;

    if (fromIndex == toIndex)
        return;

    auto* moved = hostedTabs.removeAndReturn(fromIndex);
    hostedTabs.insert(toIndex, moved);

    if (selectedTabIndex == fromIndex)
    {
        selectedTabIndex = toIndex;
    }
    else if (fromIndex < selectedTabIndex && toIndex >= selectedTabIndex)
    {
        --selectedTabIndex;
    }
    else if (fromIndex > selectedTabIndex && toIndex <= selectedTabIndex)
    {
        ++selectedTabIndex;
    }
}

bool PluginCore::moveTabUp(int tabIndex)
{
    if (! canMoveTabUp(tabIndex))
        return false;

    moveHostedTab(tabIndex, tabIndex - 1);
    rebuildTabModelFromHostedTabs();
    markDirty();
    statusText = "Tab moved up";
    return true;
}

bool PluginCore::moveTabDown(int tabIndex)
{
    if (! canMoveTabDown(tabIndex))
        return false;

    moveHostedTab(tabIndex, tabIndex + 1);
    rebuildTabModelFromHostedTabs();
    markDirty();
    statusText = "Tab moved down";
    return true;
}

int PluginCore::getTabPointerAdjustMethodOverride(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return 0;

    return hostedTabs[tabIndex]->pointerAdjustMethodOverride;
}

void PluginCore::setTabPointerAdjustMethodOverride(int tabIndex, int methodOverride)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    hostedTabs[tabIndex]->pointerAdjustMethodOverride = juce::jlimit(0, 2, methodOverride);
    markDirty();
}

float PluginCore::getTabPointerLaneTolerance(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return 30.0f;

    return hostedTabs[tabIndex]->pointerLaneTolerance;
}

void PluginCore::setTabPointerLaneTolerance(int tabIndex, float tolerance)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    hostedTabs[tabIndex]->pointerLaneTolerance = juce::jlimit(1.0f, 128.0f, tolerance);
    markDirty();
}

int PluginCore::getTabPointerAdjustSensitivity(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return 1;

    return hostedTabs[tabIndex]->pointerAdjustSensitivity;
}

bool PluginCore::tabHasCustomPointerMap(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    return hostedTabs[tabIndex]->hasCustomPointerMap;
}

bool PluginCore::tabHasGlobalPointerMap(int tabIndex) const
{
    auto file = getGlobalPointerMapFileForTab(tabIndex);
    return file.existsAsFile();
}

bool PluginCore::getTabPreferGlobalPointerMap(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    return hostedTabs[tabIndex]->preferGlobalPointerMap;
}

void PluginCore::setTabPreferGlobalPointerMap(int tabIndex, bool shouldPreferGlobal)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    auto* tab = hostedTabs[tabIndex];
    tab->preferGlobalPointerMap = shouldPreferGlobal;

    tab->pointerJumpPoints.clear();

    if (shouldPreferGlobal)
    {
        loadGlobalPointerMapForTab(tabIndex);
    }
    else if (tab->hasCustomPointerMap)
    {
        tab->pointerJumpPoints = tab->presetPointerJumpPoints;
    }
    else
    {
        loadGlobalPointerMapForTab(tabIndex);
    }

    markDirty();
}

bool PluginCore::loadGlobalPointerMapForTab(int tabIndex)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    auto file = getGlobalPointerMapFileForTab(tabIndex);

    if (! file.existsAsFile())
        return false;

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));

    if (xml == nullptr || ! xml->hasTagName("PointerMap"))
        return false;

    juce::Array<SessionTabData::PointerJumpPoint> points;

    for (auto* pointXml : xml->getChildIterator())
    {
        if (! pointXml->hasTagName("Point"))
            continue;

        SessionTabData::PointerJumpPoint point;
        point.x = (float) pointXml->getDoubleAttribute("x", 0.0);
        point.y = (float) pointXml->getDoubleAttribute("y", 0.0);
        points.add(point);
    }

    hostedTabs[tabIndex]->pointerJumpPoints = points;
    return true;
}

bool PluginCore::saveCurrentPointerMapToGlobalFile(int tabIndex)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    auto file = getGlobalPointerMapFileForTab(tabIndex);

    if (file == juce::File())
        return false;

    juce::XmlElement xml("PointerMap");

    for (auto& point : hostedTabs[tabIndex]->pointerJumpPoints)
    {
        auto* pointXml = xml.createNewChildElement("Point");
        pointXml->setAttribute("x", point.x);
        pointXml->setAttribute("y", point.y);
    }

    return xml.writeTo(file, {});
}

juce::String PluginCore::getActivePointerMapSourceText(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return "None";

    const auto* tab = hostedTabs[tabIndex];

    if (tab->preferGlobalPointerMap)
        return tabHasGlobalPointerMap(tabIndex) ? "Global" : "None";

    if (tab->hasCustomPointerMap)
        return "Preset";

    if (tabHasGlobalPointerMap(tabIndex))
        return "Global";

    return "None";
}

void PluginCore::saveCurrentPointerMapToPreset(int tabIndex)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    auto* tab = hostedTabs[tabIndex];
    tab->hasCustomPointerMap = true;
    tab->presetPointerJumpPoints = tab->pointerJumpPoints;

    if (! tab->preferGlobalPointerMap)
        tab->pointerJumpPoints = tab->presetPointerJumpPoints;

    markDirty();
}

void PluginCore::clearCurrentPresetPointerMap(int tabIndex)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    auto* tab = hostedTabs[tabIndex];
    tab->hasCustomPointerMap = false;
    tab->presetPointerJumpPoints.clear();
    tab->pointerJumpPoints.clear();

    loadGlobalPointerMapForTab(tabIndex);
    markDirty();
}

void PluginCore::setTabPointerAdjustSensitivity(int tabIndex, int sensitivity)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    hostedTabs[tabIndex]->pointerAdjustSensitivity = juce::jlimit(1, 20, sensitivity);
    markDirty();
}

const juce::Array<SessionTabData::PointerJumpPoint>& PluginCore::getTabPointerJumpPoints(int tabIndex) const
{
    static const juce::Array<SessionTabData::PointerJumpPoint> emptyPoints;

    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return emptyPoints;

    return hostedTabs[tabIndex]->pointerJumpPoints;
}

void PluginCore::setTabPointerJumpPoints(int tabIndex,
                                         const juce::Array<SessionTabData::PointerJumpPoint>& points)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    hostedTabs[tabIndex]->pointerJumpPoints = points;
    markDirty();
}

PluginSlotType PluginCore::getHostedTabType(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return PluginSlotType::Empty;

    auto* hostedTab = hostedTabs[tabIndex];

    if (hostedTab == nullptr || hostedTab->pluginInstance == nullptr)
        return PluginSlotType::Empty;

    juce::PluginDescription description;
    hostedTab->pluginInstance->fillInPluginDescription(description);
    return slotTypeFromDescription(description);
}

int PluginCore::findNextActiveFxTab(int startIndex) const
{
    for (int i = startIndex + 1; i < hostedTabs.size(); ++i)
    {
        if (hostedTabs[i] == nullptr || hostedTabs[i]->bypassed)
            continue;

        if (getHostedTabType(i) == PluginSlotType::FX && hostedTabs[i]->pluginInstance != nullptr)
            return i;
    }

    return -1;
}

juce::String PluginCore::getRoutingTooltipForTab(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return {};

    const auto thisType = getHostedTabType(tabIndex);

    if (thisType == PluginSlotType::Synth)
    {
        for (int i = tabIndex + 1; i < hostedTabs.size(); ++i)
        {
            if (getHostedTabType(i) == PluginSlotType::FX)
                return "Audio routes into downstream FX:\n" + tabModel.getTab(i).name;
        }

        return "Audio routes directly to output";
    }

    if (thisType == PluginSlotType::FX)
    {
        juce::StringArray synthNames;

        for (int i = 0; i < tabIndex; ++i)
        {
            if (getHostedTabType(i) == PluginSlotType::Synth)
                synthNames.add(tabModel.getTab(i).name);
        }

        if (synthNames.isEmpty())
            return "No synths currently route into this FX";

        return "Receiving audio from:\n" + synthNames.joinIntoString(", ");
    }

    return "Empty tab";
}

void PluginCore::buildMidiBufferForTab(int tabIndex,
                                       const juce::MidiBuffer& sourceMidi,
                                       juce::MidiBuffer& destMidi) const
{
    destMidi.clear();

    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    auto* hostedTab = hostedTabs[tabIndex];
    if (hostedTab == nullptr)
        return;

    const auto& assignments = hostedTab->midiAssignedDeviceIdentifiers;

    if (assignments.contains("MIDI Ch: All"))
    {
        destMidi = sourceMidi;
        return;
    }

    for (const auto metadata : sourceMidi)
    {
        const auto& message = metadata.getMessage();
        const int channel = message.getChannel();

        if (channel <= 0)
            continue;

        const juce::String channelName = "MIDI Ch: " + juce::String(channel);

        if (assignments.contains(channelName))
            destMidi.addEvent(message, metadata.samplePosition);
    }
}

const juce::StringArray& PluginCore::getAvailableMidiInputNames() const
{
    return availableMidiInputNames;
}

int PluginCore::getTabMidiAssignmentCount(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return 0;

    return hostedTabs[tabIndex]->midiAssignedDeviceIdentifiers.size();
}

bool PluginCore::isMidiInputAssignedToTab(int tabIndex, const juce::String& inputName) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    return hostedTabs[tabIndex]->midiAssignedDeviceIdentifiers.contains(inputName);
}

void PluginCore::setMidiInputAssignedToTab(int tabIndex,
                                           const juce::String& inputName,
                                           bool shouldBeAssigned)
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    auto& assignments = hostedTabs[tabIndex]->midiAssignedDeviceIdentifiers;

    if (shouldBeAssigned)
    {
        assignments.addIfNotAlreadyThere(inputName);
    }
    else
    {
        assignments.removeString(inputName);
    }

    markDirty();
}

void PluginCore::toggleMidiInputAssignedToTab(int tabIndex, const juce::String& inputName)
{
    const bool isAssigned = isMidiInputAssignedToTab(tabIndex, inputName);
    setMidiInputAssignedToTab(tabIndex, inputName, ! isAssigned);
}

void PluginCore::refreshAvailableMidiInputs()
{
    availableMidiInputNames.clear();
    availableMidiInputNames.add("MIDI Ch: All");

    for (int ch = 1; ch <= 16; ++ch)
        availableMidiInputNames.add("MIDI Ch: " + juce::String(ch));
}

void PluginCore::setRoutingViewSize(int width, int height)
{
    routingViewWidth = juce::jmax(100, width);
    routingViewHeight = juce::jmax(100, height);
    routingViewSizeValid = true;
}

int PluginCore::getRoutingViewWidth() const
{
    return routingViewWidth;
}

int PluginCore::getRoutingViewHeight() const
{
    return routingViewHeight;
}

bool PluginCore::hasRoutingViewSize() const
{
    return routingViewSizeValid;
}

SlotModel& PluginCore::getMainSlot()
{
    auto* tab = getSelectedHostedTab();
    jassert(tab != nullptr);
    return *tab->slot;
}

const SlotModel& PluginCore::getMainSlot() const
{
    auto* tab = getSelectedHostedTab();
    jassert(tab != nullptr);
    return *tab->slot;
}

void PluginCore::attachToHostedPlugin(juce::AudioPluginInstance* instance)
{
    if (instance != nullptr)
        instance->addListener(this);
}

void PluginCore::detachFromHostedPlugin(juce::AudioPluginInstance* instance)
{
    if (instance != nullptr)
        instance->removeListener(this);
}

PluginCore::HostedTabState* PluginCore::getSelectedHostedTab()
{
    if (! juce::isPositiveAndBelow(selectedTabIndex, hostedTabs.size()))
        return nullptr;

    return hostedTabs[selectedTabIndex];
}

const PluginCore::HostedTabState* PluginCore::getSelectedHostedTab() const
{
    if (! juce::isPositiveAndBelow(selectedTabIndex, hostedTabs.size()))
        return nullptr;

    return hostedTabs[selectedTabIndex];
}

int PluginCore::findHostedTabIndexForProcessor(const juce::AudioProcessor* processor) const
{
    if (processor == nullptr)
        return -1;

    for (int i = 0; i < hostedTabs.size(); ++i)
    {
        const auto* tab = hostedTabs[i];

        if (tab != nullptr && tab->pluginInstance.get() == processor)
            return i;
    }

    return -1;
}

juce::String PluginCore::getHostedPluginDisplayName(int tabIndex) const
{
    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return {};

    const auto* tab = hostedTabs[tabIndex];

    if (tab == nullptr)
        return {};

    if (tab->pluginInstance != nullptr)
    {
        juce::PluginDescription description;
        tab->pluginInstance->fillInPluginDescription(description);

        if (description.name.isNotEmpty())
            return description.name;
    }

    if (tab->slot != nullptr)
    {
        auto loadedName = tab->slot->getLoadedPluginName().trim();

        if (loadedName.isNotEmpty())
            return loadedName;
    }

    return tab->tabName;
}

int PluginCore::findMacroMappingIndexByMacroSlot(int macroIndex) const
{
    for (int i = 0; i < macroMappings.size(); ++i)
    {
        if (macroMappings.getReference(i).macroIndex == macroIndex)
            return i;
    }

    return -1;
}

void PluginCore::captureLastTouchedParameter(juce::AudioProcessor* processor,
                                             int parameterIndex,
                                             float newValue)
{
    const int tabIndex = findHostedTabIndexForProcessor(processor);

    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return;

    auto* tab = hostedTabs[tabIndex];

    if (tab == nullptr || tab->pluginInstance == nullptr)
        return;

    if (! juce::isPositiveAndBelow(parameterIndex, tab->pluginInstance->getParameters().size()))
        return;

    auto* parameter = tab->pluginInstance->getParameters()[parameterIndex];

    if (parameter == nullptr)
        return;

    lastTouchedParameter.tabIndex = tabIndex;
    lastTouchedParameter.pluginName = getHostedPluginDisplayName(tabIndex);
    lastTouchedParameter.parameterIndex = parameterIndex;
    lastTouchedParameter.parameterName = parameter->getName(128);
    lastTouchedParameter.lastValue = newValue;
    lastTouchedParameter.valid = true;
}

bool PluginCore::loadMainSlotPluginFromPath(const juce::String& pluginPath)
{
    auto* selectedTab = getSelectedHostedTab();

    if (selectedTab == nullptr)
        return false;

    unloadMainSlotPlugin();

    const juce::File pluginFile(pluginPath);

    if (! pluginFile.exists())
    {
        selectedTab->slot->clearPlugin();
        selectedTab->slot->setPluginPath(pluginPath);
        selectedTab->slot->setLastError("Plugin file does not exist");
        statusText = "Plugin load failed";
        return false;
    }

    // Determine which format to try based on file extension
    const bool isVST3 = pluginFile.hasFileExtension(".vst3");
    const bool isVST2 = pluginFile.hasFileExtension(".dll");

    if (! isVST3 && ! isVST2)
    {
        selectedTab->slot->clearPlugin();
        selectedTab->slot->setPluginPath(pluginPath);
        selectedTab->slot->setLastError("Unsupported plugin format");
        statusText = "Plugin load failed";
        return false;
    }

   #if ! JUCE_PLUGINHOST_VST3
    if (isVST3)
    {
        selectedTab->slot->clearPlugin();
        selectedTab->slot->setPluginPath(pluginPath);
        selectedTab->slot->setLastError("VST3 hosting is not enabled");
        statusText = "Plugin load failed";
        return false;
    }
   #endif

   #if ! JUCE_PLUGINHOST_VST
    if (isVST2)
    {
        selectedTab->slot->clearPlugin();
        selectedTab->slot->setPluginPath(pluginPath);
        selectedTab->slot->setLastError("VST2 hosting is not enabled");
        statusText = "Plugin load failed";
        return false;
    }
   #endif

    juce::OwnedArray<juce::PluginDescription> typesFound;
    juce::String errorMessage;

    // Ensure safe fallback values if host hasn't called prepareToPlay yet
    const double sampleRateToUse = (currentSampleRate > 0.0) ? currentSampleRate : 44100.0;
    const int blockSizeToUse = (currentBlockSize > 0) ? currentBlockSize : 512;

    // Find the right format and scan for types
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormat(i);

        if (format == nullptr)
            continue;

        const bool formatIsVST3 = format->getName().containsIgnoreCase("VST3");
        const bool formatIsVST2 = format->getName().containsIgnoreCase("VST")
                                  && ! formatIsVST3;

        if (isVST3 && formatIsVST3)
        {
            format->findAllTypesForFile(typesFound, pluginPath);
            break;
        }

        if (isVST2 && formatIsVST2)
        {
            format->findAllTypesForFile(typesFound, pluginPath);
            break;
        }
    }

    if (typesFound.isEmpty())
    {
        selectedTab->slot->clearPlugin();
        selectedTab->slot->setPluginPath(pluginPath);
        selectedTab->slot->setLastError("No plugin type found in file");
        statusText = "Load failed: No plugin type found in file";
        return false;
    }

    auto description = *typesFound[0];
    std::unique_ptr<juce::AudioPluginInstance> instance;

    // Instantiate using the right format
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormat(i);

        if (format == nullptr)
            continue;

        const bool formatIsVST3 = format->getName().containsIgnoreCase("VST3");
        const bool formatIsVST2 = format->getName().containsIgnoreCase("VST")
                                  && ! formatIsVST3;

        if (isVST3 && ! formatIsVST3)
            continue;

        if (isVST2 && ! formatIsVST2)
            continue;

        instance = format->createInstanceFromDescription(description,
                                                         sampleRateToUse,
                                                         blockSizeToUse,
                                                         errorMessage);

        if (instance != nullptr)
            break;
    }

    if (instance == nullptr)
    {
        juce::String fullError = errorMessage.isNotEmpty() ? errorMessage
                                                           : "Unknown instantiation failure";
        selectedTab->slot->clearPlugin();
        selectedTab->slot->setPluginPath(pluginPath);
        selectedTab->slot->setLastError(fullError);
        statusText = "Load failed: " + fullError;
        return false;
    }

    if (playbackPrepared)
        instance->prepareToPlay(currentSampleRate, currentBlockSize);

    selectedTab->pluginInstance = std::move(instance);
    attachToHostedPlugin(selectedTab->pluginInstance.get());

    selectedTab->slot->setPluginLoaded(true);
    selectedTab->slot->setLoadedPluginName(description.name);
    selectedTab->slot->setPluginPath(pluginPath);
    selectedTab->slot->setLastError({});

    if (! selectedTab->hasCustomPointerMap)
    {
        selectedTab->pointerJumpPoints.clear();
        loadGlobalPointerMapForTab(selectedTabIndex);
    }

    statusText = "Plugin instantiated";
    markDirty();
    suppressDirtyMarkingFor(1500);
    return true;
}

void PluginCore::unloadMainSlotPlugin()
{
    auto* selectedTab = getSelectedHostedTab();

    if (selectedTab == nullptr)
        return;

    detachFromHostedPlugin(selectedTab->pluginInstance.get());

    if (selectedTab->pluginInstance != nullptr)
    {
        selectedTab->pluginInstance->releaseResources();
        selectedTab->pluginInstance.reset();
    }

    selectedTab->slot->clearPlugin();
    dirtyMarkingResumeTimeMs = 0;
}

void PluginCore::resetForNewPreset()
{
    for (auto* tab : hostedTabs)
        detachFromHostedPlugin(tab->pluginInstance.get());

    hostedTabs.clear();
    selectedTabIndex = 0;

    sessionDocument.clear();
    statusText = "Ready";
    macroMappings.clear();
    lastTouchedParameter = {};
    macroCurrentValues.fill(0.0f);
    dirtyMarkingResumeTimeMs = 0;
    routingViewWidth = 800;
    routingViewHeight = 500;
    routingViewSizeValid = false;

    addTab("Empty");
    markClean();
}

juce::AudioPluginInstance* PluginCore::getMainPluginInstance() const
{
    auto* selectedTab = getSelectedHostedTab();
    return selectedTab != nullptr ? selectedTab->pluginInstance.get() : nullptr;
}

juce::String PluginCore::getMainSlotPluginPath() const
{
    auto* selectedTab = getSelectedHostedTab();
    return selectedTab != nullptr ? selectedTab->slot->getPluginPath()
                                  : juce::String();
}

void PluginCore::getMainSlotPluginState(juce::MemoryBlock& destData) const
{
    destData.reset();

    auto* selectedTab = getSelectedHostedTab();

    if (selectedTab == nullptr || selectedTab->pluginInstance == nullptr)
        return;

    selectedTab->pluginInstance->getStateInformation(destData);
}

bool PluginCore::setMainSlotPluginState(const void* data, int sizeInBytes)
{
    auto* selectedTab = getSelectedHostedTab();

    if (selectedTab == nullptr || selectedTab->pluginInstance == nullptr)
        return false;

    if (data == nullptr || sizeInBytes <= 0)
        return false;

    selectedTab->pluginInstance->setStateInformation(data, sizeInBytes);
    return true;
}

SessionTabData PluginCore::buildSessionTabData(int tabIndex) const
{
    SessionTabData tabData;
    tabData.index = tabIndex;
    tabData.tabName = createDefaultTabName(tabIndex);
    tabData.type = PluginSlotType::Empty;
    tabData.bypassed = false;
    tabData.hasSavedWindowBounds = routingViewSizeValid;
    tabData.savedWindowWidth = routingViewWidth;
    tabData.savedWindowHeight = routingViewHeight;
    tabData.pointerLaneTolerance = 30.0f;
    tabData.pointerAdjustMethodOverride = 0;
    tabData.hasPlugin = false;

    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return tabData;

    auto* hostedTab = hostedTabs[tabIndex];
    tabData.tabName = hostedTab->tabName;
    tabData.bypassed = hostedTab->bypassed;
    tabData.pointerAdjustMethodOverride = hostedTab->pointerAdjustMethodOverride;
    tabData.pointerLaneTolerance = hostedTab->pointerLaneTolerance;
    tabData.pointerAdjustSensitivity = hostedTab->pointerAdjustSensitivity;
    tabData.hasCustomPointerMap = hostedTab->hasCustomPointerMap;
    tabData.preferGlobalPointerMap = hostedTab->preferGlobalPointerMap;
    if (hostedTab->hasCustomPointerMap)
        tabData.pointerJumpPoints = hostedTab->presetPointerJumpPoints;
    tabData.midiAssignedDeviceIdentifiers = hostedTab->midiAssignedDeviceIdentifiers;

    auto* instance = hostedTab->pluginInstance.get();

    if (instance == nullptr)
        return tabData;

    juce::MemoryBlock pluginState;
    instance->getStateInformation(pluginState);

    juce::PluginDescription description;
    instance->fillInPluginDescription(description);

    const juce::File pluginFile(description.fileOrIdentifier);

    tabData.type = slotTypeFromDescription(description);
    tabData.hasPlugin = true;

    tabData.plugin.pluginName = description.name;
    tabData.plugin.pluginDescriptiveName = description.descriptiveName;
    tabData.plugin.pluginPath = pluginFile.getFullPathName();
    tabData.plugin.pluginPathRelative = AppSettings::makePathPortable(pluginFile);
    tabData.plugin.pluginPathDriveFlexible = AppSettings::getDriveFlexiblePath(pluginFile);
    tabData.plugin.pluginStateBase64 = memoryBlockToBase64(pluginState);
    tabData.plugin.pluginFormatName = description.pluginFormatName;
    tabData.plugin.pluginFileOrIdentifier = description.fileOrIdentifier;
    tabData.plugin.pluginUniqueId = description.uniqueId;
    tabData.plugin.isInstrument = description.isInstrument;
    tabData.plugin.pluginManufacturer = description.manufacturerName;
    tabData.plugin.pluginVersion = description.version;

    return tabData;
}

SessionTabData PluginCore::buildMainTabSessionData() const
{
    return buildSessionTabData(selectedTabIndex);
}

bool PluginCore::restoreTabFromSessionData(int tabIndex,
                                           const SessionTabData& tabData,
                                           juce::StringArray& warnings)
{
    warnings.clear();

    if (! juce::isPositiveAndBelow(tabIndex, hostedTabs.size()))
        return false;

    auto previousSelected = selectedTabIndex;
    selectedTabIndex = tabIndex;

    auto* selectedTab = getSelectedHostedTab();
    if (selectedTab == nullptr)
    {
        selectedTabIndex = previousSelected;
        return false;
    }

    selectedTab->tabName = tabData.tabName.isNotEmpty() ? tabData.tabName
                                                        : "Empty";
    selectedTab->bypassed = tabData.bypassed;
    selectedTab->pointerAdjustMethodOverride = tabData.pointerAdjustMethodOverride;
    selectedTab->pointerLaneTolerance = tabData.pointerLaneTolerance;
    selectedTab->pointerAdjustSensitivity = juce::jlimit(1, 20, tabData.pointerAdjustSensitivity);
    selectedTab->hasCustomPointerMap = tabData.hasCustomPointerMap;
    selectedTab->preferGlobalPointerMap = tabData.preferGlobalPointerMap;
    selectedTab->presetPointerJumpPoints = tabData.pointerJumpPoints;
    selectedTab->pointerJumpPoints.clear();

    if (selectedTab->preferGlobalPointerMap)
    {
        loadGlobalPointerMapForTab(tabIndex);
    }
    else if (selectedTab->hasCustomPointerMap)
    {
        selectedTab->pointerJumpPoints = selectedTab->presetPointerJumpPoints;
    }
    else
    {
        loadGlobalPointerMapForTab(tabIndex);
    }

    selectedTab->midiAssignedDeviceIdentifiers = tabData.midiAssignedDeviceIdentifiers;

    if (selectedTab->midiAssignedDeviceIdentifiers.isEmpty())
        selectedTab->midiAssignedDeviceIdentifiers.add("MIDI Ch: All");

    if (tabData.hasSavedWindowBounds
        && tabData.savedWindowWidth > 0
        && tabData.savedWindowHeight > 0)
    {
        routingViewWidth = tabData.savedWindowWidth;
        routingViewHeight = tabData.savedWindowHeight;
        routingViewSizeValid = true;
    }

    unloadMainSlotPlugin();

    if (! tabData.hasPlugin)
    {
        rebuildTabModelFromHostedTabs();
        selectedTabIndex = previousSelected;
        return true;
    }

    const auto resolvedPluginFile = AppSettings::resolvePluginPath(
        tabData.plugin.pluginPath,
        tabData.plugin.pluginPathRelative,
        tabData.plugin.pluginPathDriveFlexible);

    if (! resolvedPluginFile.existsAsFile())
    {
        warnings.add("Could not locate plugin for tab: " + tabData.tabName);
        statusText = "Plugin file missing";

        MissingPluginEntry entry;
        entry.tabIndex = tabIndex;
        entry.pluginName = tabData.plugin.pluginName;
        entry.pluginPath = tabData.plugin.pluginPath;
        entry.pluginPathRelative = tabData.plugin.pluginPathRelative;
        entry.pluginPathDriveFlexible = tabData.plugin.pluginPathDriveFlexible;
        entry.pluginStateBase64 = tabData.plugin.pluginStateBase64;
        entry.pluginFormatName = tabData.plugin.pluginFormatName;
        entry.isInstrument = tabData.plugin.isInstrument;
        entry.pluginManufacturer = tabData.plugin.pluginManufacturer;
        entry.pluginVersion = tabData.plugin.pluginVersion;
        missingPlugins.add(entry);

        selectedTabIndex = previousSelected;
        return false;
    }

    if (! loadMainSlotPluginFromPath(resolvedPluginFile.getFullPathName()))
    {
        warnings.add("Failed to load plugin for tab: " + tabData.tabName);
        selectedTabIndex = previousSelected;
        return false;
    }

    if (tabData.plugin.pluginStateBase64.isNotEmpty())
    {
        juce::MemoryBlock pluginState;

        if (pluginState.fromBase64Encoding(tabData.plugin.pluginStateBase64))
        {
            if (! setMainSlotPluginState(pluginState.getData(),
                                         static_cast<int>(pluginState.getSize())))
            {
                warnings.add("Failed to restore plugin state for tab: " + tabData.tabName);
            }
        }
        else
        {
            warnings.add("Invalid plugin state data for tab: " + tabData.tabName);
        }
    }

    rebuildTabModelFromHostedTabs();
    selectedTabIndex = previousSelected;
    return true;
}

bool PluginCore::restoreMainTabFromSessionData(const SessionTabData& tabData,
                                               juce::StringArray& warnings)
{
    return restoreTabFromSessionData(selectedTabIndex, tabData, warnings);
}

SessionData PluginCore::buildSessionData() const
{
    SessionData sessionData;
    sessionData.name = getSessionName();
    sessionData.hostTempoBpm = 120.0;
    sessionData.selectedTabIndex = selectedTabIndex;

    for (int i = 0; i < hostedTabs.size(); ++i)
        sessionData.tabs.add(buildSessionTabData(i));

    sessionData.macroMappings = macroMappings;
    return sessionData;
}

bool PluginCore::restoreSessionData(const SessionData& sessionData,
                                    juce::StringArray& warnings)
{
    warnings.clear();
    missingPlugins.clear();

    for (auto* tab : hostedTabs)
        detachFromHostedPlugin(tab->pluginInstance.get());

    hostedTabs.clear();
    selectedTabIndex = 0;
    routingViewWidth = 800;
    routingViewHeight = 500;
    routingViewSizeValid = false;
    macroMappings.clear();
    lastTouchedParameter = {};
    macroCurrentValues.fill(0.0f);

    if (sessionData.tabs.isEmpty())
    {
        addTab("Empty");
        statusText = "Preset loaded";
        markClean();
        suppressDirtyMarkingFor(1500);
        return true;
    }

    for (int i = 0; i < sessionData.tabs.size(); ++i)
    {
        auto tabName = sessionData.tabs.getReference(i).tabName;
        addTab(tabName.isNotEmpty() ? tabName : createDefaultTabName(i));
    }

    markClean();

    for (int i = 0; i < sessionData.tabs.size(); ++i)
    {
        juce::StringArray tabWarnings;
        restoreTabFromSessionData(i, sessionData.tabs.getReference(i), tabWarnings);
        warnings.addArray(tabWarnings);
    }

    selectedTabIndex = juce::jlimit(0,
                                    juce::jmax(0, hostedTabs.size() - 1),
                                    sessionData.selectedTabIndex);

    macroMappings = sessionData.macroMappings;

    rebuildTabModelFromHostedTabs();
    statusText = "Preset loaded";
    markClean();
    suppressDirtyMarkingFor(1500);
    return true;
}

void PluginCore::ensureValidSelectedTab()
{
    if (hostedTabs.isEmpty())
    {
        selectedTabIndex = 0;
        return;
    }

    selectedTabIndex = juce::jlimit(0, hostedTabs.size() - 1, selectedTabIndex);
}

void PluginCore::rebuildTabModelFromHostedTabs()
{
    tabModel.clear();

    for (int i = 0; i < hostedTabs.size(); ++i)
    {
        auto* hostedTab = hostedTabs[i];

        PluginSlotType tabType = PluginSlotType::Empty;
        juce::String tabName = "Empty";

        if (hostedTab->tabName.isNotEmpty())
            tabName = hostedTab->tabName;

        if (hostedTab->slot != nullptr && hostedTab->slot->isPluginLoaded())
        {
            if (hostedTab->pluginInstance != nullptr)
            {
                juce::PluginDescription description;
                hostedTab->pluginInstance->fillInPluginDescription(description);

                tabType = description.isInstrument ? PluginSlotType::Synth
                                                   : PluginSlotType::FX;

                if (description.name.isNotEmpty())
                    tabName = description.name;
            }
        }

        tabModel.addTab(tabName, tabType, i == selectedTabIndex);
    }

    ensureValidSelectedTab();

    if (tabModel.getNumTabs() > 0)
        tabModel.selectTab(selectedTabIndex);
}

juce::String PluginCore::createDefaultTabName(int index) const
{
    juce::ignoreUnused(index);
    return "Empty";
}

void PluginCore::suppressDirtyMarkingFor(int milliseconds)
{
    dirtyMarkingResumeTimeMs = juce::Time::getMillisecondCounter()
                             + (juce::uint32) juce::jmax(0, milliseconds);
}

bool PluginCore::isDirtyMarkingSuppressed() const
{
    return dirtyMarkingResumeTimeMs != 0
        && juce::Time::getMillisecondCounter() < dirtyMarkingResumeTimeMs;
}

bool PluginCore::hasLastTouchedParameter() const
{
    return lastTouchedParameter.valid;
}

PluginCore::LastTouchedParameter PluginCore::getLastTouchedParameter() const
{
    return lastTouchedParameter;
}

juce::String PluginCore::getLastTouchedParameterDescription() const
{
    if (! lastTouchedParameter.valid)
        return "None";

    juce::String tabText = "Tab " + juce::String(lastTouchedParameter.tabIndex + 1);
    juce::String pluginText = lastTouchedParameter.pluginName.isNotEmpty()
                                ? lastTouchedParameter.pluginName
                                : "Unknown Plugin";
    juce::String parameterText = lastTouchedParameter.parameterName.isNotEmpty()
                                ? lastTouchedParameter.parameterName
                                : ("Parameter " + juce::String(lastTouchedParameter.parameterIndex));

    return tabText + " / " + pluginText + " / " + parameterText;
}

const juce::Array<SessionData::MacroMapping>& PluginCore::getMacroMappings() const
{
    return macroMappings;
}

bool PluginCore::isMacroMapped(int macroIndex) const
{
    return findMacroMappingIndexByMacroSlot(macroIndex) >= 0;
}

int PluginCore::getNextFreeMacroIndex() const
{
    constexpr int maxMacroCount = 128;

    for (int macroIndex = 0; macroIndex < maxMacroCount; ++macroIndex)
    {
        if (! isMacroMapped(macroIndex))
            return macroIndex;
    }

    return -1;
}

bool PluginCore::assignLastTouchedToNextFreeMacro()
{
    if (! lastTouchedParameter.valid)
        return false;

    for (int i = 0; i < macroMappings.size(); ++i)
    {
        const auto& existing = macroMappings.getReference(i);

        if (existing.tabIndex == lastTouchedParameter.tabIndex
            && existing.parameterIndex == lastTouchedParameter.parameterIndex)
        {
            return false;
        }
    }

    const int macroIndex = getNextFreeMacroIndex();

    if (macroIndex < 0)
        return false;

    SessionData::MacroMapping mapping;
    mapping.macroIndex = macroIndex;
    mapping.tabIndex = lastTouchedParameter.tabIndex;
    mapping.pluginName = lastTouchedParameter.pluginName;
    mapping.parameterIndex = lastTouchedParameter.parameterIndex;
    mapping.parameterName = lastTouchedParameter.parameterName;
    mapping.enabled = true;

    if (mapping.pluginName.isNotEmpty() && mapping.parameterName.isNotEmpty())
        mapping.label = mapping.pluginName + " " + mapping.parameterName;
    else if (mapping.parameterName.isNotEmpty())
        mapping.label = mapping.parameterName;
    else
        mapping.label = "Macro " + juce::String(macroIndex + 1);

    macroMappings.add(mapping);
    markDirty();
    return true;
}

bool PluginCore::replaceMacroMappingFromLastTouched(int macroIndex)
{
    if (! lastTouchedParameter.valid)
        return false;

    const int mappingIndex = findMacroMappingIndexByMacroSlot(macroIndex);

    if (mappingIndex < 0)
        return false;

    for (int i = 0; i < macroMappings.size(); ++i)
    {
        if (i == mappingIndex)
            continue;

        const auto& existing = macroMappings.getReference(i);

        if (existing.tabIndex == lastTouchedParameter.tabIndex
            && existing.parameterIndex == lastTouchedParameter.parameterIndex)
        {
            return false;
        }
    }

    auto mapping = macroMappings.getReference(mappingIndex);

    if (mapping.tabIndex == lastTouchedParameter.tabIndex
        && mapping.parameterIndex == lastTouchedParameter.parameterIndex)
    {
        return false;
    }

    storeMacroMappingsUndoState();

    mapping.tabIndex = lastTouchedParameter.tabIndex;
    mapping.pluginName = lastTouchedParameter.pluginName;
    mapping.parameterIndex = lastTouchedParameter.parameterIndex;
    mapping.parameterName = lastTouchedParameter.parameterName;
    mapping.enabled = true;

    if (mapping.pluginName.isNotEmpty() && mapping.parameterName.isNotEmpty())
        mapping.label = mapping.pluginName + " " + mapping.parameterName;
    else if (mapping.parameterName.isNotEmpty())
        mapping.label = mapping.parameterName;
    else
        mapping.label = "Macro " + juce::String(macroIndex + 1);

    macroMappings.set(mappingIndex, mapping);
    markDirty();
    return true;
}

bool PluginCore::clearMacroMapping(int macroIndex)
{
    const int mappingIndex = findMacroMappingIndexByMacroSlot(macroIndex);

    if (mappingIndex < 0)
        return false;

    storeMacroMappingsUndoState();
    macroMappings.remove(mappingIndex);
    markDirty();
    return true;
}

void PluginCore::storeMacroMappingsUndoState()
{
    macroMappingsUndoSnapshot = macroMappings;
    hasMacroMappingsUndoSnapshot = true;
}

bool PluginCore::hasMacroMappingsUndoState() const
{
    return hasMacroMappingsUndoSnapshot;
}

bool PluginCore::undoLastMacroMappingsEdit()
{
    if (! hasMacroMappingsUndoSnapshot)
        return false;

    macroMappings = macroMappingsUndoSnapshot;
    hasMacroMappingsUndoSnapshot = false;
    markDirty();
    return true;
}

void PluginCore::clearAllMacroMappings()
{
    if (macroMappings.isEmpty())
        return;

    storeMacroMappingsUndoState();
    macroMappings.clear();
    markDirty();
}

bool PluginCore::moveMacroMappingUp(int macroIndex)
{
    if (macroIndex <= 0)
        return false;

    const int currentIndex = findMacroMappingIndexByMacroSlot(macroIndex);
    const int previousIndex = findMacroMappingIndexByMacroSlot(macroIndex - 1);

    if (currentIndex < 0 || previousIndex < 0)
        return false;

    auto current = macroMappings.getReference(currentIndex);
    auto previous = macroMappings.getReference(previousIndex);

    const int currentMacroSlot = current.macroIndex;
    const int previousMacroSlot = previous.macroIndex;

    current.macroIndex = previousMacroSlot;
    previous.macroIndex = currentMacroSlot;

    macroMappings.set(currentIndex, previous);
    macroMappings.set(previousIndex, current);

    markDirty();
    return true;
}

bool PluginCore::moveMacroMappingDown(int macroIndex)
{
    constexpr int maxMacroCount = 128;

    if (macroIndex < 0 || macroIndex >= maxMacroCount - 1)
        return false;

    const int currentIndex = findMacroMappingIndexByMacroSlot(macroIndex);
    const int nextIndex = findMacroMappingIndexByMacroSlot(macroIndex + 1);

    if (currentIndex < 0 || nextIndex < 0)
        return false;

    auto current = macroMappings.getReference(currentIndex);
    auto next = macroMappings.getReference(nextIndex);

    const int currentMacroSlot = current.macroIndex;
    const int nextMacroSlot = next.macroIndex;

    current.macroIndex = nextMacroSlot;
    next.macroIndex = currentMacroSlot;

    macroMappings.set(currentIndex, next);
    macroMappings.set(nextIndex, current);

    markDirty();
    return true;
}

float PluginCore::getMacroCurrentValue(int macroIndex) const
{
    if (macroIndex < 0 || macroIndex >= (int) macroCurrentValues.size())
        return 0.0f;

    return macroCurrentValues[(size_t) macroIndex];
}

void PluginCore::setMacroValueFromHost(int macroIndex, float normalizedValue)
{
    if (macroIndex < 0 || macroIndex >= (int) macroCurrentValues.size())
        return;

    normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
    macroCurrentValues[(size_t) macroIndex] = normalizedValue;

    const int mappingIndex = findMacroMappingIndexByMacroSlot(macroIndex);

    if (mappingIndex < 0)
        return;

    const auto& mapping = macroMappings.getReference(mappingIndex);

    if (! mapping.enabled)
        return;

    if (! juce::isPositiveAndBelow(mapping.tabIndex, hostedTabs.size()))
        return;

    auto* tab = hostedTabs[mapping.tabIndex];

    if (tab == nullptr || tab->pluginInstance == nullptr)
        return;

    auto& parameters = tab->pluginInstance->getParameters();

    if (! juce::isPositiveAndBelow(mapping.parameterIndex, parameters.size()))
        return;

    if (auto* parameter = parameters[mapping.parameterIndex])
        parameter->setValueNotifyingHost(normalizedValue);
}

void PluginCore::audioProcessorParameterChanged(juce::AudioProcessor* processor,
                                                int parameterIndex,
                                                float newValue)
{
    captureLastTouchedParameter(processor, parameterIndex, newValue);

    const int tabIndex = findHostedTabIndexForProcessor(processor);

    if (tabIndex < 0)
        return;

    if (isDirtyMarkingSuppressed())
        return;

    markDirty();
}

void PluginCore::audioProcessorChanged(juce::AudioProcessor* processor,
                                       const juce::AudioProcessorListener::ChangeDetails& details)
{
    juce::ignoreUnused(details);

    auto* selectedTab = getSelectedHostedTab();

    if (selectedTab == nullptr || processor != selectedTab->pluginInstance.get())
        return;

    if (isDirtyMarkingSuppressed())
        return;

    markDirty();
}

void PluginCore::sendMidiPanic()
{
    juce::MidiBuffer panicMidi;

    for (int channel = 1; channel <= 16; ++channel)
    {
        panicMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), 0);
        panicMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 123, 0), 0);
        panicMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0);
    }

    const int numSamples = juce::jmax(1, currentBlockSize);
    const int numChannels = 2;

    for (int i = 0; i < hostedTabs.size(); ++i)
    {
        auto* tab = hostedTabs[i];

        if (tab == nullptr || tab->pluginInstance == nullptr || tab->bypassed)
            continue;

        auto* instance = tab->pluginInstance.get();

        juce::AudioBuffer<float> tempAudio(numChannels, numSamples);
        tempAudio.clear();

        juce::MidiBuffer panicCopy = panicMidi;
        instance->processBlock(tempAudio, panicCopy);
        instance->reset();
    }

    setStatusText("MIDI Panic sent");
}
