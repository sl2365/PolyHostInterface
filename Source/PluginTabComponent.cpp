#include "PluginTabComponent.h"
#include "AudioEngine.h"
#include "DebugLog.h"

PluginTabComponent::PluginTabComponent(AudioEngine& engine, int index)
    : audioEngine(engine), slotIndex(index)
{
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
   #if JUCE_PLUGINHOST_VST
    formatManager.addFormat(std::make_unique<juce::VSTPluginFormat>());
   #endif

    addAndMakeVisible(loadButton);
    loadButton.setColour(juce::TextButton::buttonColourId, colourForType(slotType).withAlpha(0.6f));
    loadButton.onClick = [this]
    {
        juce::FileChooser chooser("Open Plugin", juce::File(), "*.vst3;*.dll;*.clap");
        if (chooser.browseForFileToOpen())
        {
            if (loadPlugin(chooser.getResult()))
            {
                if (onPluginLoadedDirectly)
                    onPluginLoadedDirectly();
            }
        }
    };

    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setText("Empty slot - drop or click to load", juce::dontSendNotification);
}

PluginTabComponent::~PluginTabComponent()
{
    detachFromCurrentProcessor();
    pluginEditor.reset();

    if (nodeId != juce::AudioProcessorGraph::NodeID())
        audioEngine.removePlugin(nodeId);
}

bool PluginTabComponent::scanPluginDescriptions(const juce::File& pluginFile,
                                                juce::OwnedArray<juce::PluginDescription>& results)
{
    if (!pluginFile.existsAsFile())
        return false;

    results.clear();

    for (auto* format : formatManager.getFormats())
    {
        format->findAllTypesForFile(results, pluginFile.getFullPathName());

        if (!results.isEmpty())
            return true;
    }

    return false;
}

bool PluginTabComponent::choosePluginDescription(const juce::OwnedArray<juce::PluginDescription>& results,
                                                 juce::PluginDescription& chosenDesc) const
{
    if (results.isEmpty())
        return false;

    if (results.size() == 1)
    {
        chosenDesc = *results.getFirst();
        return true;
    }

    juce::StringArray pluginNames;

    for (auto* desc : results)
    {
        juce::String typeLabel = desc->isInstrument ? "Synth" : "FX";
        juce::String label = desc->name + " (" + typeLabel + ")";

        if (desc->descriptiveName.isNotEmpty() && desc->descriptiveName != desc->name)
            label += " - " + desc->descriptiveName;

        if (desc->manufacturerName.isNotEmpty())
            label += " [" + desc->manufacturerName + "]";

        pluginNames.add(label);
    }

    juce::AlertWindow chooser("Select Plugin",
                              "This plugin file contains multiple plugins. Choose one to open:",
                              juce::AlertWindow::QuestionIcon);

    chooser.addComboBox("pluginChoice", pluginNames, "Plugin");
    chooser.addButton("Open", 1, juce::KeyPress(juce::KeyPress::returnKey));
    chooser.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (auto* combo = chooser.getComboBoxComponent("pluginChoice"))
        combo->setSelectedItemIndex(0);

    if (chooser.runModalLoop() != 1)
        return false;

    auto* combo = chooser.getComboBoxComponent("pluginChoice");
    if (combo == nullptr)
        return false;

    const int selectedIndex = combo->getSelectedItemIndex();

    if (!juce::isPositiveAndBelow(selectedIndex, results.size()))
        return false;

    chosenDesc = *results.getUnchecked(selectedIndex);
    return true;
}

bool PluginTabComponent::findMatchingPluginDescription(const juce::OwnedArray<juce::PluginDescription>& results,
                                                       const SessionPluginData& sessionPluginData,
                                                       juce::PluginDescription& matchedDesc) const
{
    if (results.isEmpty())
        return false;

    // 1) Strongest for shell plugins: exact uniqueId
    if (sessionPluginData.pluginUniqueId != 0)
    {
        for (auto* desc : results)
        {
            if (desc->uniqueId == sessionPluginData.pluginUniqueId)
            {
                matchedDesc = *desc;
                return true;
            }
        }
    }

    // 2) Exact name + descriptive name + manufacturer + format
    for (auto* desc : results)
    {
        const bool nameMatches =
            sessionPluginData.pluginName.isNotEmpty()
            && desc->name == sessionPluginData.pluginName;

        const bool descriptiveMatches =
            sessionPluginData.pluginDescriptiveName.isEmpty()
            || desc->descriptiveName == sessionPluginData.pluginDescriptiveName;

        const bool manufacturerMatches =
            sessionPluginData.pluginManufacturer.isEmpty()
            || desc->manufacturerName == sessionPluginData.pluginManufacturer;

        const bool formatMatches =
            sessionPluginData.pluginFormatName.isEmpty()
            || desc->pluginFormatName == sessionPluginData.pluginFormatName;

        if (nameMatches && descriptiveMatches && manufacturerMatches && formatMatches)
        {
            matchedDesc = *desc;
            return true;
        }
    }

    // 3) Exact descriptive name
    if (sessionPluginData.pluginDescriptiveName.isNotEmpty())
    {
        for (auto* desc : results)
        {
            if (desc->descriptiveName == sessionPluginData.pluginDescriptiveName)
            {
                matchedDesc = *desc;
                return true;
            }
        }
    }

    // 4) Exact name only
    if (sessionPluginData.pluginName.isNotEmpty())
    {
        for (auto* desc : results)
        {
            if (desc->name == sessionPluginData.pluginName)
            {
                matchedDesc = *desc;
                return true;
            }
        }
    }

    return false;
}

bool PluginTabComponent::loadPluginFromSessionData(const juce::File& pluginFile,
                                                   const SessionPluginData& sessionPluginData)
{
    if (!pluginFile.existsAsFile())
        return false;

    if (pluginFile.getFileExtension().equalsIgnoreCase(".clap")
        || sessionPluginData.pluginFormatName.equalsIgnoreCase("CLAP"))
    {
        return loadClapPlugin(pluginFile);
    }

    juce::OwnedArray<juce::PluginDescription> results;

    if (!scanPluginDescriptions(pluginFile, results) || results.isEmpty())
    {
        statusLabel.setText("Could not scan: " + pluginFile.getFileName(), juce::dontSendNotification);
        return false;
    }

    juce::PluginDescription matchedDesc;

    if (findMatchingPluginDescription(results, sessionPluginData, matchedDesc))
    {
        return loadPlugin(pluginFile, matchedDesc);
    }

    juce::PluginDescription chosenDesc;

    if (!choosePluginDescription(results, chosenDesc))
        return false;

    return loadPlugin(pluginFile, chosenDesc);
}

bool PluginTabComponent::loadClapPlugin(const juce::File& pluginFile)
{
    DebugLog::write("PluginTabComponent::loadClapPlugin - start: " + pluginFile.getFullPathName());

    std::unique_ptr<juce::AudioPluginInstance> clapWrapper = std::make_unique<ClapPluginWrapper>(pluginFile);
    DebugLog::write("PluginTabComponent::loadClapPlugin - wrapper object created");

    auto* clap = dynamic_cast<ClapPluginWrapper*>(clapWrapper.get());

    if (clap == nullptr || !clap->isLoaded())
    {
        juce::String error = "Unknown CLAP load failure";
        if (clap != nullptr)
            error = clap->getLastError();

        DebugLog::write("PluginTabComponent::loadClapPlugin - wrapper failed: " + error);
        statusLabel.setText("Load failed: " + error, juce::dontSendNotification);
        return false;
    }

    DebugLog::write("PluginTabComponent::loadClapPlugin - wrapper loaded OK");

    const bool isSynthLike = clap->isSynthLikePlugin();
    const juce::String clapName = clap->getPluginName();

    DebugLog::write("PluginTabComponent::loadClapPlugin - plugin name: " + clapName);
    DebugLog::write("PluginTabComponent::loadClapPlugin - clearing current plugin");
    clearPlugin();

    loadedPluginFile = pluginFile;

    loadedPluginDescription = {};
    loadedPluginDescription.name = clapName;
    loadedPluginDescription.descriptiveName = clapName;
    loadedPluginDescription.pluginFormatName = "CLAP";
    loadedPluginDescription.fileOrIdentifier = pluginFile.getFullPathName();
    loadedPluginDescription.isInstrument = isSynthLike;

    slotType = isSynthLike ? SlotType::Synth : SlotType::FX;

    DebugLog::write("PluginTabComponent::loadClapPlugin - calling audioEngine.addPlugin");
    nodeId = audioEngine.addPlugin(std::move(clapWrapper), isSynthLike);

    if (nodeId == juce::AudioProcessorGraph::NodeID())
    {
        DebugLog::write("PluginTabComponent::loadClapPlugin - addPlugin returned invalid node");
        statusLabel.setText("Load failed: audio engine rejected plugin", juce::dontSendNotification);
        return false;
    }

    DebugLog::write("PluginTabComponent::loadClapPlugin - addPlugin OK");
    DebugLog::write("PluginTabComponent::loadClapPlugin - attaching to processor");
    attachToCurrentProcessor();

    DebugLog::write("PluginTabComponent::loadClapPlugin - showing plugin editor");
    showPluginEditor();

    DebugLog::write("PluginTabComponent::loadClapPlugin - done");
    sendChangeMessage();
    return true;
}

bool PluginTabComponent::loadPlugin(const juce::File& pluginFile)
{
    if (!pluginFile.existsAsFile())
        return false;

    if (pluginFile.getFileExtension().equalsIgnoreCase(".clap"))
        return loadClapPlugin(pluginFile);

    juce::OwnedArray<juce::PluginDescription> results;

    if (!scanPluginDescriptions(pluginFile, results) || results.isEmpty())
    {
        statusLabel.setText("Could not scan: " + pluginFile.getFileName(), juce::dontSendNotification);
        return false;
    }

    juce::PluginDescription chosenDesc;

    if (!choosePluginDescription(results, chosenDesc))
        return false;

    return loadPlugin(pluginFile, chosenDesc);
}

bool PluginTabComponent::loadPlugin(const juce::File& pluginFile, const juce::PluginDescription& desc)
{
    if (!pluginFile.existsAsFile())
        return false;

    juce::String errorMessage;
    auto instance = formatManager.createPluginInstance(desc, 44100.0, 512, errorMessage);

    if (instance == nullptr)
    {
        statusLabel.setText("Load failed: " + errorMessage, juce::dontSendNotification);
        return false;
    }

    clearPlugin();
    loadedPluginFile = pluginFile;
    loadedPluginDescription = desc;
    slotType = desc.isInstrument ? SlotType::Synth : SlotType::FX;
    nodeId = audioEngine.addPlugin(std::move(instance), desc.isInstrument);
    attachToCurrentProcessor();
    showPluginEditor();
    sendChangeMessage();
    return true;
}

void PluginTabComponent::clearPlugin()
{
    detachFromCurrentProcessor();

    if (pluginEditor != nullptr)
        removeChildComponent(pluginEditor.get());

    pluginEditor.reset();

    if (nodeId != juce::AudioProcessorGraph::NodeID())
    {
        audioEngine.removePlugin(nodeId);
        nodeId = {};
    }

    loadedPluginFile = {};
    loadedPluginDescription = {};
    slotType = SlotType::Empty;
    bypassed = false;
    clearPluginStateBaseline();
    preferredEditorBounds = { 0, 0, 360, 220 };

    loadButton.setVisible(true);
    statusLabel.setVisible(true);
    loadButton.setColour(juce::TextButton::buttonColourId, colourForType(slotType).withAlpha(0.6f));
    statusLabel.setText("Empty slot - drop or click to load", juce::dontSendNotification);

    resized();
    sendChangeMessage();
}

void PluginTabComponent::showPluginEditor()
{
    pluginEditor.reset();
    loadButton.setVisible(false);
    statusLabel.setVisible(false);

    auto* node = audioEngine.getGraph().getNodeForId(nodeId);
    if (node == nullptr)
        return;

    auto* proc = node->getProcessor();
    if (proc == nullptr || !proc->hasEditor())
    {
        juce::String message = "Plugin has no GUI";

        if (auto* clap = dynamic_cast<ClapPluginWrapper*>(proc))
        {
            if (clap->supportsGui() && !clap->supportsEmbeddedGui())
                message = "CLAP GUI exists but embedded Win32 GUI is not available";
        }

        statusLabel.setText(message, juce::dontSendNotification);
        statusLabel.setVisible(true);
        return;
    }

    pluginEditor.reset(proc->createEditor());

    if (pluginEditor != nullptr)
    {
        int w = pluginEditor->getWidth();
        int h = pluginEditor->getHeight();

        if (auto* clap = dynamic_cast<ClapPluginWrapper*>(proc))
        {
            clap->refreshEmbeddedGuiSize();
            auto guiSize = clap->getEmbeddedGuiSize();

            if (guiSize.getWidth() > 0)
                w = guiSize.getWidth();

            if (guiSize.getHeight() > 0)
                h = guiSize.getHeight();
        }

        preferredEditorBounds = { 0, 0, w, h };
    }

    addAndMakeVisible(pluginEditor.get());
    resized();
}

juce::Rectangle<int> PluginTabComponent::getPreferredContentBounds() const
{
    return preferredEditorBounds;
}

void PluginTabComponent::attachToCurrentProcessor()
{
    auto* node = audioEngine.getGraph().getNodeForId(nodeId);
    if (node == nullptr)
        return;

    if (auto* proc = node->getProcessor())
        proc->addListener(this);
}

void PluginTabComponent::detachFromCurrentProcessor()
{
    auto* node = audioEngine.getGraph().getNodeForId(nodeId);
    if (node == nullptr)
        return;

    if (auto* proc = node->getProcessor())
        proc->removeListener(this);
}

void PluginTabComponent::audioProcessorParameterChanged(juce::AudioProcessor*, int, float)
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<PluginTabComponent>(this)]
    {
        if (safe != nullptr)
            safe->sendChangeMessage();
    });
}

void PluginTabComponent::audioProcessorChanged(juce::AudioProcessor*,
                                               const juce::AudioProcessorListener::ChangeDetails&)
{
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<PluginTabComponent>(this)]
    {
        if (safe != nullptr)
            safe->sendChangeMessage();
    });
}

juce::String PluginTabComponent::getPluginName() const
{
    if (auto* node = audioEngine.getGraph().getNodeForId(nodeId))
        return node->getProcessor()->getName();
    return {};
}

void PluginTabComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E2E));
    if (!hasPlugin())
    {
        g.setColour(colourForType(slotType).withAlpha(0.3f));
        float dashes[] = { 6.f, 4.f };
        g.drawDashedLine({ 20.f, 20.f, (float)getWidth()-20.f, (float)getHeight()-20.f }, dashes, 2, 1.5f);
    }
}

void PluginTabComponent::resized()
{
    auto area = getLocalBounds().reduced(12);

    if (pluginEditor != nullptr)
    {
        pluginEditor->setBounds(area);
        return;
    }

    statusLabel.setBounds(area.removeFromTop(40));
    loadButton.setBounds(area.withSizeKeepingCentre(240, 44));
}

bool PluginTabComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
        if (isPluginFile(juce::File(f)))
            return true;

    return false;
}

void PluginTabComponent::filesDropped(const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        juce::File file(f);

        if (!isPluginFile(file))
            continue;

        if (!hasPlugin())
        {
            if (loadPlugin(file))
            {
                if (onPluginLoadedDirectly)
                    onPluginLoadedDirectly();
            }

            break;
        }

        auto currentPluginName = getPluginName().trim();
        if (currentPluginName.isEmpty())
            currentPluginName = "Current Plugin";

        juce::String message;
        message << "Replace this tabs plugin:\n"
                << currentPluginName
                << "\n\nWith dropped plugin:\n"
                << file.getFileName()
                << "\n\nWhat would you like to do?";

        juce::AlertWindow w("Plugin Drop",
                            message,
                            juce::AlertWindow::QuestionIcon);

        w.addButton("Open New Tab", 1);
        w.addButton("Replace Existing Plugin", 2);
        w.addButton("Cancel", 0);

        const int result = w.runModalLoop();

        if (result == 1)
        {
            if (onOpenDroppedPluginInNewTab)
                onOpenDroppedPluginInNewTab(file);
        }
        else if (result == 2)
        {
            if (loadPlugin(file))
            {
                if (onPluginLoadedDirectly)
                    onPluginLoadedDirectly();
            }
        }

        break;
    }
}

bool PluginTabComponent::isPluginFile(const juce::File& f)
{
    auto ext = f.getFileExtension().toLowerCase();
    return ext == ".vst3" || ext == ".dll" || ext == ".clap";
}

juce::File PluginTabComponent::getPluginFile() const
{
    return loadedPluginFile;
}

juce::PluginDescription PluginTabComponent::getLoadedPluginDescription() const
{
    return loadedPluginDescription;
}

void PluginTabComponent::setSavedWindowBounds(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    hasManualWindowBounds = true;
    savedWindowBounds = { 0, 0, width, height };
    sendChangeMessage();
}

void PluginTabComponent::clearSavedWindowBounds()
{
    if (!hasManualWindowBounds)
        return;

    hasManualWindowBounds = false;
    savedWindowBounds = {};
    sendChangeMessage();
}

bool PluginTabComponent::hasSavedWindowBounds() const
{
    return hasManualWindowBounds;
}

juce::Rectangle<int> PluginTabComponent::getSavedWindowBounds() const
{
    return savedWindowBounds;
}

juce::MemoryBlock PluginTabComponent::getPluginState() const
{
    juce::MemoryBlock state;

    if (auto* node = audioEngine.getGraph().getNodeForId(nodeId))
        node->getProcessor()->getStateInformation(state);

    return state;
}

bool PluginTabComponent::restorePluginState(const juce::MemoryBlock& state)
{
    if (auto* node = audioEngine.getGraph().getNodeForId(nodeId))
    {
        auto* processor = node->getProcessor();
        if (processor == nullptr)
            return false;

        if (auto* clap = dynamic_cast<ClapPluginWrapper*>(processor))
        {
            clap->clearLastStateLoadResult();
            processor->setStateInformation(state.getData(), (int) state.getSize());
            return clap->didLastStateLoadSucceed();
        }

        processor->setStateInformation(state.getData(), (int) state.getSize());
        return true;
    }

    return false;
}

void PluginTabComponent::setBypassed(bool shouldBeBypassed)
{
    if (bypassed == shouldBeBypassed)
        return;

    bypassed = shouldBeBypassed;
    sendChangeMessage();
}

bool PluginTabComponent::capturePluginStateBaseline()
{
    auto* node = audioEngine.getGraph().getNodeForId(nodeId);
    if (node == nullptr)
    {
        pluginStateBaseline.reset();
        return false;
    }

    auto* proc = node->getProcessor();
    if (proc == nullptr)
    {
        pluginStateBaseline.reset();
        return false;
    }

    juce::MemoryBlock state;
    proc->getStateInformation(state);
    pluginStateBaseline = state;
    return true;
}

bool PluginTabComponent::doesCurrentPluginStateDifferFromBaseline() const
{
    auto* node = audioEngine.getGraph().getNodeForId(nodeId);
    if (node == nullptr)
        return false;

    auto* proc = node->getProcessor();
    if (proc == nullptr)
        return false;

    juce::MemoryBlock currentState;
    proc->getStateInformation(currentState);

    return currentState != pluginStateBaseline;
}

void PluginTabComponent::clearPluginStateBaseline()
{
    pluginStateBaseline.reset();
}

