#pragma once

#include <JuceHeader.h>
#include <vector>
#include "SessionManager.h"
#include "TabModel.h"
#include "SlotModel.h"

class PluginCore : private juce::AudioProcessorListener
{
public:
    PluginCore();
    ~PluginCore();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    const juce::String getSessionName() const;
    void setSessionName(const juce::String& newName);

    bool isDirty() const;
    void markDirty();
    void markClean();

    juce::String getStatusText() const;
    void setStatusText(const juce::String& newStatusText);

    SessionDocument& getSessionDocument();
    TabModel& getTabModel();

    int getNumTabs() const;
    int getSelectedTabIndex() const;
    void setSelectedTabIndex(int newIndex);
    bool addTab(const juce::String& tabName = {});
    bool closeSelectedTab();
    bool clearTab(int tabIndex);
    bool reloadTabPlugin(int tabIndex);
    void refreshTabModel();

    bool isTabBypassed(int tabIndex) const;
    void setTabBypassed(int tabIndex, bool shouldBeBypassed);
    bool canMoveTabUp(int tabIndex) const;
    bool canMoveTabDown(int tabIndex) const;
    bool moveTabUp(int tabIndex);
    bool moveTabDown(int tabIndex);
    int getTabPointerAdjustMethodOverride(int tabIndex) const;
    void setTabPointerAdjustMethodOverride(int tabIndex, int methodOverride);
    float getTabPointerLaneTolerance(int tabIndex) const;
    void setTabPointerLaneTolerance(int tabIndex, float tolerance);
    int getTabPointerAdjustSensitivity(int tabIndex) const;
    void setTabPointerAdjustSensitivity(int tabIndex, int sensitivity);
    bool tabHasCustomPointerMap(int tabIndex) const;
    bool tabHasGlobalPointerMap(int tabIndex) const;
    bool getTabPreferGlobalPointerMap(int tabIndex) const;
    void setTabPreferGlobalPointerMap(int tabIndex, bool shouldPreferGlobal);
    bool loadGlobalPointerMapForTab(int tabIndex);
    bool saveCurrentPointerMapToGlobalFile(int tabIndex);
    juce::String getActivePointerMapSourceText(int tabIndex) const;
    void saveCurrentPointerMapToPreset(int tabIndex);
    void clearCurrentPresetPointerMap(int tabIndex);
    const juce::Array<SessionTabData::PointerJumpPoint>& getTabPointerJumpPoints(int tabIndex) const;
    void setTabPointerJumpPoints(int tabIndex, const juce::Array<SessionTabData::PointerJumpPoint>& points);
    juce::String getRoutingTooltipForTab(int tabIndex) const;

    const juce::StringArray& getAvailableMidiInputNames() const;
    int getTabMidiAssignmentCount(int tabIndex) const;
    bool isMidiInputAssignedToTab(int tabIndex, const juce::String& inputName) const;
    void setMidiInputAssignedToTab(int tabIndex, const juce::String& inputName, bool shouldBeAssigned);
    void toggleMidiInputAssignedToTab(int tabIndex, const juce::String& inputName);
    void refreshAvailableMidiInputs();
    void sendMidiPanic();

    void setRoutingViewSize(int width, int height);
    int getRoutingViewWidth() const;
    int getRoutingViewHeight() const;
    bool hasRoutingViewSize() const;

    SlotModel& getMainSlot();
    const SlotModel& getMainSlot() const;

    bool loadMainSlotPluginFromPath(const juce::String& pluginPath);
    void unloadMainSlotPlugin();
    void resetForNewPreset();

    juce::AudioPluginInstance* getMainPluginInstance() const;
    juce::String getMainSlotPluginPath() const;

    void getMainSlotPluginState(juce::MemoryBlock& destData) const;
    bool setMainSlotPluginState(const void* data, int sizeInBytes);

    SessionTabData buildMainTabSessionData() const;
    bool restoreMainTabFromSessionData(const SessionTabData& tabData,
                                       juce::StringArray& warnings);

    SessionData buildSessionData() const;
    bool restoreSessionData(const SessionData& sessionData,
                            juce::StringArray& warnings);

    struct LastTouchedParameter
    {
        int tabIndex = -1;
        juce::String pluginName;
        int parameterIndex = -1;
        juce::String parameterName;
        float lastValue = 0.0f;
        bool valid = false;
    };

    bool hasLastTouchedParameter() const;
    LastTouchedParameter getLastTouchedParameter() const;
    juce::String getLastTouchedParameterDescription() const;

    const juce::Array<SessionData::MacroMapping>& getMacroMappings() const;
    bool isMacroMapped(int macroIndex) const;
    int getNextFreeMacroIndex() const;
    bool assignLastTouchedToNextFreeMacro();
    bool replaceMacroMappingFromLastTouched(int macroIndex);
    bool clearMacroMapping(int macroIndex);
    void clearAllMacroMappings();
    bool moveMacroMappingUp(int macroIndex);
    bool moveMacroMappingDown(int macroIndex);
    bool undoLastMacroMappingsEdit();
    bool hasMacroMappingsUndoState() const;

    float getMacroCurrentValue(int macroIndex) const;
    void setMacroValueFromHost(int macroIndex, float normalizedValue);

    void suppressDirtyMarkingFor(int milliseconds);
    bool isDirtyMarkingSuppressed() const;

    float getInputMeterLevelL() const;
    float getInputMeterLevelR() const;
    float getOutputMeterLevelL() const;
    float getOutputMeterLevelR() const;

private:
    struct HostedTabState
    {
        std::unique_ptr<SlotModel> slot;
        std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
        juce::String tabName;
        bool bypassed = false;
        int pointerAdjustMethodOverride = 0;
        juce::StringArray midiAssignedDeviceIdentifiers;
        bool hasCustomPointerMap = false;
        bool preferGlobalPointerMap = false;
        juce::Array<SessionTabData::PointerJumpPoint> pointerJumpPoints;
        juce::Array<SessionTabData::PointerJumpPoint> presetPointerJumpPoints;
        float pointerLaneTolerance = 30.0f;
        int pointerAdjustSensitivity = 1;
    };

    void audioProcessorParameterChanged(juce::AudioProcessor* processor,
                                        int parameterIndex,
                                        float newValue) override;
    void audioProcessorChanged(juce::AudioProcessor* processor,
                               const juce::AudioProcessorListener::ChangeDetails& details) override;

    void attachToHostedPlugin(juce::AudioPluginInstance* instance);
    void detachFromHostedPlugin(juce::AudioPluginInstance* instance);

    HostedTabState* getSelectedHostedTab();
    const HostedTabState* getSelectedHostedTab() const;

    SessionTabData buildSessionTabData(int tabIndex) const;
    bool restoreTabFromSessionData(int tabIndex,
                                   const SessionTabData& tabData,
                                   juce::StringArray& warnings);

    void captureLastTouchedParameter(juce::AudioProcessor* processor,
                                     int parameterIndex,
                                     float newValue);
    int findHostedTabIndexForProcessor(const juce::AudioProcessor* processor) const;
    juce::String getHostedPluginDisplayName(int tabIndex) const;
    int findMacroMappingIndexByMacroSlot(int macroIndex) const;

    void ensureValidSelectedTab();
    void rebuildTabModelFromHostedTabs();
    juce::String createDefaultTabName(int index) const;
    PluginSlotType getHostedTabType(int tabIndex) const;
    void moveHostedTab(int fromIndex, int toIndex);
    int findNextActiveFxTab(int startIndex) const;
    void buildMidiBufferForTab(int tabIndex,
                               const juce::MidiBuffer& sourceMidi,
                               juce::MidiBuffer& destMidi) const;

    void updateMeterLevels(const juce::AudioBuffer<float>& inputBuffer,
                           const juce::AudioBuffer<float>& outputBuffer);
    static float computePeakLevel(const float* samples, int numSamples);
    static float smoothMeterLevel(float current, float target);

    static juce::File getPointerMapsDirectory();
    static juce::String makeSafePointerMapFileName(const juce::String& pluginName,
                                                   const juce::String& manufacturer);
    juce::File getGlobalPointerMapFileForTab(int tabIndex) const;

public:
    struct MissingPluginEntry
    {
        int tabIndex = -1;
        juce::String pluginName;
        juce::String pluginPath;
        juce::String pluginPathRelative;
        juce::String pluginPathDriveFlexible;
        juce::String pluginStateBase64;
        juce::String pluginFormatName;
        bool isInstrument = false;
        juce::String pluginManufacturer;
        juce::String pluginVersion;
    };

    const juce::Array<MissingPluginEntry>& getMissingPlugins() const { return missingPlugins; }
    void clearMissingPlugins() { missingPlugins.clear(); }

private:
    SessionDocument sessionDocument;
    TabModel tabModel;
    juce::OwnedArray<HostedTabState> hostedTabs;
    juce::Array<MissingPluginEntry> missingPlugins;
    int selectedTabIndex = 0;
    juce::String statusText { "Ready" };

    juce::AudioPluginFormatManager formatManager;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    bool playbackPrepared = false;

    int routingViewWidth = 800;
    int routingViewHeight = 500;
    bool routingViewSizeValid = false;

    juce::StringArray availableMidiInputNames { "MIDI Ch: All" };

    juce::Array<SessionData::MacroMapping> macroMappings;
    LastTouchedParameter lastTouchedParameter;
    void storeMacroMappingsUndoState();
    juce::Array<SessionData::MacroMapping> macroMappingsUndoSnapshot;
    bool hasMacroMappingsUndoSnapshot = false;
    std::array<float, 128> macroCurrentValues {};

    juce::uint32 dirtyMarkingResumeTimeMs = 0;

    std::atomic<float> inputMeterLevelL { 0.0f };
    std::atomic<float> inputMeterLevelR { 0.0f };
    std::atomic<float> outputMeterLevelL { 0.0f };
    std::atomic<float> outputMeterLevelR { 0.0f };
};