#pragma once
#include <JuceHeader.h>
#include "SessionManager.h"
#include "ClapPluginWrapper.h"

class AudioEngine;

class PluginTabComponent final : public juce::Component,
                                 public juce::FileDragAndDropTarget,
                                 public juce::ChangeBroadcaster,
                                 private juce::AudioProcessorListener
{
public:
    using SlotType = PluginSlotType;

    static juce::Colour colourForType(SlotType t)
    {
        switch (t)
        {
            case SlotType::Synth: return juce::Colour(0xFF3A7BD5);
            case SlotType::FX:    return juce::Colour(0xFFE67E22);
            case SlotType::Empty: return juce::Colour(0xFF555555);
        }

        return juce::Colour(0xFF555555);
    }

    PluginTabComponent(AudioEngine& engine, int slotIndex);
    ~PluginTabComponent() override;

    bool loadPlugin(const juce::File& pluginFile);
    bool loadPlugin(const juce::File& pluginFile, const juce::PluginDescription& desc);
    bool loadPluginFromSessionData(const juce::File& pluginFile,
                                   const SessionPluginData& sessionPluginData);
    void clearPlugin();

    bool hasPlugin() const { return nodeId != juce::AudioProcessorGraph::NodeID(); }
    SlotType getType() const { return slotType; }
    int getSlotIndex() const { return slotIndex; }
    void setSlotIndex(int newIndex) { slotIndex = newIndex; }
    juce::AudioProcessorGraph::NodeID getNodeID() const { return nodeId; }
    juce::String getPluginName() const;
    bool isBypassed() const { return bypassed; }
    void setBypassed(bool shouldBeBypassed);

    void setAllowEditorWindowResize(bool shouldAllow) { allowEditorWindowResize = shouldAllow; }

    juce::Rectangle<int> getPreferredContentBounds() const;
    juce::File getPluginFile() const;
    juce::PluginDescription getLoadedPluginDescription() const;
    juce::MemoryBlock getPluginState() const;
    bool restorePluginState(const juce::MemoryBlock& state);

    void setSavedWindowBounds(int width, int height);
    void clearSavedWindowBounds();
    bool hasSavedWindowBounds() const;
    juce::Rectangle<int> getSavedWindowBounds() const;

    std::function<void(const juce::File&)> onOpenDroppedPluginInNewTab;

    void resized() override;
    void paint(juce::Graphics& g) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    static bool isPluginFile(const juce::File& f);

private:
    bool scanPluginDescriptions(const juce::File& pluginFile,
                                juce::OwnedArray<juce::PluginDescription>& results);
    bool choosePluginDescription(const juce::OwnedArray<juce::PluginDescription>& results,
                                 juce::PluginDescription& chosenDesc) const;
    bool findMatchingPluginDescription(const juce::OwnedArray<juce::PluginDescription>& results,
                                       const SessionPluginData& sessionPluginData,
                                       juce::PluginDescription& matchedDesc) const;
    bool loadClapPlugin(const juce::File& pluginFile);
    void showPluginEditor();
    void attachToCurrentProcessor();
    void detachFromCurrentProcessor();
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override;
    void audioProcessorChanged(juce::AudioProcessor*, const juce::AudioProcessorListener::ChangeDetails&) override;

    AudioEngine& audioEngine;
    SlotType slotType { SlotType::Empty };
    int slotIndex;

    juce::AudioPluginFormatManager formatManager;
    juce::AudioProcessorGraph::NodeID nodeId;
    std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;
    juce::Rectangle<int> preferredEditorBounds { 0, 0, 360, 220 };
    juce::File loadedPluginFile;
    juce::PluginDescription loadedPluginDescription;
    bool hasManualWindowBounds = false;
    juce::Rectangle<int> savedWindowBounds;

    juce::TextButton loadButton { "Click to Load Plugin..." };
    juce::Label statusLabel;
    bool allowEditorWindowResize = true;
    bool bypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTabComponent)
};