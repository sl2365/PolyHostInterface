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

    bool getPointerControlDebugLoggingEnabled() const;
    void setPointerControlDebugLoggingEnabled(bool shouldEnable);

    bool getPointerControlEnabled() const;
    void setPointerControlEnabled(bool shouldEnable);

    int getPointerControlAdjustCcNumber() const;
    void setPointerControlAdjustCcNumber(int ccNumber);

    int getPointerControlDiscontinuityThreshold() const;
    void setPointerControlDiscontinuityThreshold(int threshold);

    int getPointerControlDeltaClamp() const;
    void setPointerControlDeltaClamp(int clampAmount);

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

    juce::StringArray getPluginScanFolders() const;
    void setPluginScanFolders(const juce::StringArray& folders);
    void addPluginScanFolder(const juce::String& folderPath);
    void removePluginScanFolder(const juce::String& folderPath);

    static juce::File getSettingsFile();
    static juce::File getPresetsDirectory();
    static juce::File getAppDirectory();
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
