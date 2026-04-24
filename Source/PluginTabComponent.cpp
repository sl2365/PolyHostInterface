#include "PluginTabComponent.h"
#include "AudioEngine.h"

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
            loadPlugin(chooser.getResult());
    };

    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setText("Empty slot - drop or click to load", juce::dontSendNotification);
}

PluginTabComponent::~PluginTabComponent()
{
    pluginEditor.reset();
    if (nodeId != juce::AudioProcessorGraph::NodeID()) audioEngine.removePlugin(nodeId);
}

bool PluginTabComponent::loadPlugin(const juce::File& pluginFile)
{
    if (!pluginFile.existsAsFile()) return false;
    juce::OwnedArray<juce::PluginDescription> results;
    juce::String errorMessage;
    for (auto* format : formatManager.getFormats())
    {
        format->findAllTypesForFile(results, pluginFile.getFullPathName());
        if (!results.isEmpty()) break;
    }
    if (results.isEmpty())
    {
        statusLabel.setText("Could not scan: " + pluginFile.getFileName(), juce::dontSendNotification);
        return false;
    }
    auto& desc = *results.getFirst();
    auto instance = formatManager.createPluginInstance(desc, 44100.0, 512, errorMessage);
    if (instance == nullptr)
    {
        statusLabel.setText("Load failed: " + errorMessage, juce::dontSendNotification);
        return false;
    }
    clearPlugin();
    loadedPluginFile = pluginFile;
    slotType = desc.isInstrument ? SlotType::Synth : SlotType::FX;
    nodeId = audioEngine.addPlugin(std::move(instance), desc.isInstrument);
    showPluginEditor();
    sendChangeMessage();
    return true;
}

void PluginTabComponent::clearPlugin()
{
    if (pluginEditor != nullptr)
        removeChildComponent(pluginEditor.get());

    pluginEditor.reset();

    if (nodeId != juce::AudioProcessorGraph::NodeID())
    {
        audioEngine.removePlugin(nodeId);
        nodeId = {};
    }

    loadedPluginFile = {};
    slotType = SlotType::Empty;
    bypassed = false;

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
        statusLabel.setText("Plugin has no GUI", juce::dontSendNotification);
        statusLabel.setVisible(true);
        return;
    }

    pluginEditor.reset(proc->createEditor());
    addAndMakeVisible(pluginEditor.get());

    if (allowEditorWindowResize)
    {
        const auto editorBounds = pluginEditor->getLocalBounds();

        if (auto* top = getTopLevelComponent())
        {
            auto bounds = top->getBounds();
            bounds.setWidth(juce::jmax(bounds.getWidth(),  editorBounds.getWidth()  + 80));
            bounds.setHeight(juce::jmax(bounds.getHeight(), editorBounds.getHeight() + 140));
            top->setBounds(bounds);
        }
    }
    resized();
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
            loadPlugin(file);
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
            loadPlugin(file);
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
        node->getProcessor()->setStateInformation(state.getData(), (int) state.getSize());
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

