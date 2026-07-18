#pragma once

#include <JuceHeader.h>
#include <functional>
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
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages,
                      juce::AudioPlayHead* playHead);

    const juce::String getSessionName() const;
    void setSessionName(const juce::String& newName);

    bool isDirty() const;
    bool hasAnyLoadedPlugin() const;
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
    bool tabHasGlobalPointerMap(int tabIndex) const;
    bool loadGlobalPointerMapForTab(int tabIndex);
    bool saveCurrentPointerMapToGlobalFile(int tabIndex, const juce::String& userLabel = {});
    bool saveCurrentPointerMapAsNewGlobalFile(int tabIndex,
                                              const juce::String& mapFileName,
                                              bool allowOverwriteExisting = false);
    bool pointerMapFileNameExists(const juce::String& mapFileName) const;
    bool deleteSelectedGlobalPointerMapFile(int tabIndex, juce::String* deletedMapName = nullptr);
    juce::String getActivePointerMapSourceText(int tabIndex) const;

    struct PointerMapChoice
    {
        juce::String displayName;
        juce::String fileName;
        juce::String relativePath;
        bool selected = false;
    };

    juce::Array<PointerMapChoice> getAvailableGlobalPointerMapsForTab(int tabIndex) const;
    bool loadGlobalPointerMapForTabByRelativePath(int tabIndex, const juce::String& relativePath);
    juce::String getSelectedGlobalPointerMapRelativePath(int tabIndex) const;
    juce::String getSelectedGlobalPointerMapDisplayName(int tabIndex) const;

    const juce::Array<SessionTabData::PointerJumpPoint>& getTabPointerJumpPoints(int tabIndex) const;
    void setTabPointerJumpPoints(int tabIndex, const juce::Array<SessionTabData::PointerJumpPoint>& points);
    bool tabHasPointerFreeZone(int tabIndex) const;
    bool tabHasPointerFreeZones(int tabIndex) const;
    juce::Rectangle<float> getTabPointerFreeZone(int tabIndex) const;
    const juce::Array<juce::Rectangle<float>>& getTabPointerFreeZones(int tabIndex) const;
    void setTabPointerFreeZone(int tabIndex, juce::Rectangle<float> freeZone);
    void setTabPointerFreeZones(int tabIndex, const juce::Array<juce::Rectangle<float>>& freeZones);
    void addTabPointerFreeZone(int tabIndex, juce::Rectangle<float> freeZone);
    bool removeTabPointerFreeZoneAt(int tabIndex, int freeZoneIndex);
    void clearTabPointerFreeZone(int tabIndex);
    juce::String getRoutingTooltipForTab(int tabIndex) const;
    bool tabNeedsAttention(int tabIndex) const;
    bool tabHasMissingPluginIssue(int tabIndex) const;
    bool tabHasFailedPluginIssue(int tabIndex) const;
    juce::String getTabAttentionTitle(int tabIndex) const;
    juce::String getTabAttentionMessage(int tabIndex) const;
    juce::String buildPluginDiagnosticsText(int tabIndex) const;
    void clearPluginQuarantine();
    juce::String getPluginQuarantineReport() const;

    void setLastPresetLoadReport(const juce::String& reportText, bool hadIssues);
    void clearLastPresetLoadReport();
    bool hasLastPresetLoadReport() const;
    bool lastPresetLoadReportHadIssues() const;
    juce::String getLastPresetLoadReport() const;

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

    bool scanPluginDescriptionsForFile(const juce::String& pluginPath,
                                       juce::OwnedArray<juce::PluginDescription>& typesFound,
                                       juce::String& errorMessage);
    bool loadMainSlotPluginFromDescription(const juce::PluginDescription& description);
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
    void setMacroCurrentValueFromPointerAutomation(int macroIndex,
                                                   float normalizedValue);

    using PointerAutomationCallback =
        std::function<void(int macroIndex, float normalizedValue)>;

    void setPointerAutomationCallback(
        PointerAutomationCallback callback);

    void armPointerAutomationCapture(int milliseconds);

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
        PluginSlotType pluginType = PluginSlotType::Empty;
        juce::MidiBuffer midiScratchBuffer;
        juce::AudioBuffer<float> audioScratchBuffer;
        int audioScratchChannelCapacity = 2;
        int audioScratchSampleCapacity = 512;
        juce::String tabName;
        bool bypassed = false;
        int pointerAdjustMethodOverride = 0;
        juce::StringArray midiAssignedDeviceIdentifiers;
        juce::String selectedGlobalPointerMapRelativePath;
        juce::String selectedGlobalPointerMapName;
        juce::Array<SessionTabData::PointerJumpPoint> pointerJumpPoints;
        juce::Array<juce::Rectangle<float>> pointerFreeZones;
        float pointerLaneTolerance = 30.0f;
        int pointerAdjustSensitivity = 1;
        bool restoreIssueActive = false;
        bool restoreIssueMissingPlugin = false;
        bool restoreIssueFailedPlugin = false;
        juce::String restoreIssueTitle;
        juce::String restoreIssueMessage;
        PluginSlotType restoreIssueType = PluginSlotType::Empty;
        bool restoreIssueHasPluginData = false;
        SessionPluginData restoreIssuePluginData;
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
    bool loadMainSlotPluginFromPathMatchingSessionData(const juce::String& pluginPath,
                                                       const SessionPluginData& pluginData);

    static juce::String makePluginQuarantineKey(const SessionPluginData& pluginData);
    static juce::String makePluginQuarantineDisplayName(const SessionPluginData& pluginData);
    bool isPluginQuarantined(const SessionPluginData& pluginData, juce::String* reason = nullptr) const;
    void quarantinePlugin(const SessionPluginData& pluginData, const juce::String& reason);
    void clearPluginQuarantine(const SessionPluginData& pluginData);
    void clearTabRestoreIssue(HostedTabState& tab);
    void setTabRestoreIssue(int tabIndex,
                            const SessionPluginData& pluginData,
                            bool isMissingPlugin,
                            const juce::String& message);
    void beginPluginLoadCrashMarker(const SessionPluginData& pluginData, int tabIndex);
    void clearPluginLoadCrashMarker();
    void importUnclosedPluginLoadCrashMarker(juce::StringArray& warnings);

    void captureLastTouchedParameter(juce::AudioProcessor* processor,
                                     int parameterIndex,
                                     float newValue);
    int findHostedTabIndexForProcessor(const juce::AudioProcessor* processor) const;
    juce::String getHostedPluginDisplayName(int tabIndex) const;
    int findMacroMappingIndexByMacroSlot(int macroIndex) const;
    int findMacroMappingIndexByTarget(int tabIndex,
                                      int parameterIndex) const;

    void ensureValidSelectedTab();
    void rebuildTabModelFromHostedTabs();
    juce::String createDefaultTabName(int index) const;
    PluginSlotType getHostedTabType(int tabIndex) const;
    void moveHostedTab(int fromIndex, int toIndex);
    int findNextActiveFxTab(int startIndex) const;
    void buildMidiBufferForTab(
        int tabIndex,
        const juce::MidiBuffer& sourceMidi,
        juce::MidiBuffer& destMidi,
        bool releaseOnly = false) const;

    void updateMeterLevels(const juce::AudioBuffer<float>& inputBuffer,
                           const juce::AudioBuffer<float>& outputBuffer);
    static float computePeakLevel(const float* samples, int numSamples);
    static float smoothMeterLevel(float current, float target);

    static juce::File getPointerMapsDirectory();
    static juce::String makeSafePointerMapFileName(const juce::String& pluginName,
                                                   const juce::String& manufacturer);
    juce::Array<juce::File> findMatchingGlobalPointerMapFilesForTab(int tabIndex) const;
    juce::File getGlobalPointerMapFileForTab(int tabIndex) const;
    juce::File getWritableGlobalPointerMapFileForTab(int tabIndex,
                                                     const juce::String& userLabel = {}) const;
    bool loadPointerMapFileIntoTab(int tabIndex, const juce::File& file);

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
    juce::StringPairArray pluginQuarantineReasons;
    juce::String lastPresetLoadReportText;
    bool lastPresetLoadReportHadProblems = false;
    int selectedTabIndex = 0;
    juce::String statusText { "Ready" };

    juce::AudioPluginFormatManager formatManager;
    bool pluginFormatsInitialised = false;
    void ensurePluginFormatsInitialised();

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    std::atomic<bool> playbackPrepared { false };

    static constexpr int hostBufferChannelCapacity = 2;
    int hostBufferSampleCapacity = 512;
    juce::AudioBuffer<float> hostInputScratchBuffer;
    juce::AudioBuffer<float> finalOutputScratchBuffer;
    juce::Array<int> fxIndexScratch;

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

    PointerAutomationCallback pointerAutomationCallback;
    std::atomic<juce::uint32> pointerAutomationCaptureExpiryMs { 0 };
    std::atomic<bool> applyingMacroValueFromHost { false };

    juce::uint32 dirtyMarkingResumeTimeMs = 0;

    std::atomic<float> inputMeterLevelL { 0.0f };
    std::atomic<float> inputMeterLevelR { 0.0f };
    std::atomic<float> outputMeterLevelL { 0.0f };
    std::atomic<float> outputMeterLevelR { 0.0f };

    std::atomic<int> lastInvalidAudioTabIndex { -1 };
    std::atomic<juce::uint32> invalidAudioBlockCount { 0 };
};