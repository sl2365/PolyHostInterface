#pragma once
#include <JuceHeader.h>

class AppSettings
{
public:
    static constexpr int defaultWindowWidth  = 850;
    static constexpr int defaultWindowHeight = 400;

    bool getDebugLoggingEnabled() const;
    void setDebugLoggingEnabled(bool shouldEnable);

    bool getAdvancedDebugLoggingEnabled() const;
    void setAdvancedDebugLoggingEnabled(bool shouldEnable);

    bool getClearDebugLogOnStartup() const;
    void setClearDebugLogOnStartup(bool shouldClear);

    AppSettings();
    void load();
    void save();

    int getWindowX() const;
    int getWindowY() const;
    void setWindowPosition(int x, int y);

    int getRoutingWindowWidth() const;
    int getRoutingWindowHeight() const;
    void setRoutingWindowSize(int width, int height);
    void clearRoutingWindowSize();

    juce::String getLastPresetPath() const;
    void setLastPresetPath(const juce::String& path);

    juce::StringArray getRecentPresetPaths() const;
    void addRecentPresetPath(const juce::String& path);
    void clearRecentPresetPaths();
    void removeMissingRecentPresetPaths();

    juce::String getMidiDeviceName() const;
    void setMidiDeviceName(const juce::String& name);

    juce::StringArray getEnabledMidiDeviceIdentifiers() const;
    void setEnabledMidiDeviceIdentifiers(const juce::StringArray& identifiers);

    juce::String getAudioDeviceName() const;
    void setAudioDeviceName(const juce::String& name);

    juce::String getAudioDeviceState() const;
    void setAudioDeviceState(const juce::String& xmlText);

    double getDefaultTempoBpm() const;
    void setDefaultTempoBpm(double bpm);

    bool getAutoSaveAfterPluginRepair() const;
    void setAutoSaveAfterPluginRepair(bool shouldAutoSave);

    juce::String getMidiAutoAssignMode() const;
    void setMidiAutoAssignMode(const juce::String& mode);

    int getPointerControlXccNumber() const;
    void setPointerControlXccNumber(int ccNumber);

    int getPointerControlYccNumber() const;
    void setPointerControlYccNumber(int ccNumber);

    int getPointerControlAdjustCcNumber() const;
    void setPointerControlAdjustCcNumber(int ccNumber);

    int getPointerControlTabCcNumber() const;
    void setPointerControlTabCcNumber(int ccNumber);

    int getPointerControlTabSwitchCooldownMs() const;
    void setPointerControlTabSwitchCooldownMs(int delayMs);

    int getPointerControlLeftMouseCcNumber() const;
    void setPointerControlLeftMouseCcNumber(int ccNumber);

    int getPointerControlMiddleMouseCcNumber() const;
    void setPointerControlMiddleMouseCcNumber(int ccNumber);

    int getPointerControlRightMouseCcNumber() const;
    void setPointerControlRightMouseCcNumber(int ccNumber);

    int getPointerControlCursorUpKeyCcNumber() const;
    void setPointerControlCursorUpKeyCcNumber(int ccNumber);

    int getPointerControlCursorDownKeyCcNumber() const;
    void setPointerControlCursorDownKeyCcNumber(int ccNumber);

    int getPointerControlEnterKeyCcNumber() const;
    void setPointerControlEnterKeyCcNumber(int ccNumber);

    float getPointerControlXSnapWeight() const;
    void setPointerControlXSnapWeight(float weight);

    float getPointerControlYSnapWeight() const;
    void setPointerControlYSnapWeight(float weight);

    int getPointerControlOverlayTransparency() const;
    void setPointerControlOverlayTransparency(int amount);

    bool getPointerControlShowCrosshair() const;
    void setPointerControlShowCrosshair(bool shouldShow);

    int getPointerControlPointSize() const;
    void setPointerControlPointSize(int sizePixels);

    int getPointerControlToleranceCcNumber() const;
    void setPointerControlToleranceCcNumber(int ccNumber);

    int getPointerControlSensitivityCcNumber() const;
    void setPointerControlSensitivityCcNumber(int ccNumber);

    int getPointerControlAdjustSensitivity() const;
    void setPointerControlAdjustSensitivity(int amount);

    int getPointerControlAdjustCcMode() const;
    void setPointerControlAdjustCcMode(int mode);

    int getPointerControlAdjustMethod() const;
    void setPointerControlAdjustMethod(int method);

    int getPointerControlDragReturnDelayMs() const;
    void setPointerControlDragReturnDelayMs(int delayMs);

    bool getPointerControlSnapXEnabled() const;
    void setPointerControlSnapXEnabled(bool shouldEnable);

    bool getPointerControlSnapYEnabled() const;
    void setPointerControlSnapYEnabled(bool shouldEnable);

    int getMidiMonitorWindowWidth() const;
    int getMidiMonitorWindowHeight() const;
    void setMidiMonitorWindowSize(int width, int height);

    bool getMidiMonitorHideClock() const;
    bool getMidiMonitorHideActiveSense() const;
    bool getMidiMonitorShowNote() const;
    bool getMidiMonitorShowPitchBend() const;
    bool getMidiMonitorShowCc() const;
    bool getMidiMonitorShowNrpnRpn() const;
    bool getMidiMonitorShowProgramChange() const;
    bool getMidiMonitorShowAftertouch() const;
    bool getMidiMonitorShowSysEx() const;
    bool getMidiMonitorShowRealtime() const;
    bool getMidiMonitorShowSystemCommon() const;

    void setMidiMonitorFilterSettings(bool hideClock,
                                      bool hideActiveSense,
                                      bool showNote,
                                      bool showPitchBend,
                                      bool showCc,
                                      bool showNrpnRpn,
                                      bool showProgramChange,
                                      bool showAftertouch,
                                      bool showSysEx,
                                      bool showRealtime,
                                      bool showSystemCommon);

    int getMidiMonitorColumnWidth(int columnId, int fallbackWidth) const;
    void setMidiMonitorColumnWidths(int timeWidth,
                                    int sourceWidth,
                                    int typeWidth,
                                    int channelWidth,
                                    int data1Width,
                                    int data2Width,
                                    int descriptionWidth,
                                    int rawHexWidth);

    juce::Colour getPointerControlCrosshairColour() const;
    void setPointerControlCrosshairColour(juce::Colour colour);

    juce::Colour getPointerControlPointColour() const;
    void setPointerControlPointColour(juce::Colour colour);

    juce::Colour getPointerControlPreviewColour() const;
    void setPointerControlPreviewColour(juce::Colour colour);

    juce::Colour getPointerControlFreeZoneColour() const;
    void setPointerControlFreeZoneColour(juce::Colour colour);

    juce::StringArray getPluginScanFolders() const;
    void setPluginScanFolders(const juce::StringArray& folders);
    void addPluginScanFolder(const juce::String& folderPath);
    void removePluginScanFolder(const juce::String& folderPath);

    static juce::File getSettingsFile();
    static juce::File getPresetsDirectory();
    static juce::File getPluginMapsDirectory();
    static juce::File getAppDirectory();

    static juce::String makePresetPathPortable(
        const juce::File& file);

    static juce::File resolvePresetPath(
        const juce::String& path);

    static juce::String makePathPortable(const juce::File& file);
    static juce::File resolvePortablePath(const juce::String& path);
    static juce::String getDriveFlexiblePath(const juce::File& file);
    static juce::File resolvePluginPath(const juce::String& absolutePath,
                                        const juce::String& relativePath,
                                        const juce::String& driveFlexiblePath);

private:
    std::unique_ptr<juce::XmlElement> xml;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppSettings)
};
