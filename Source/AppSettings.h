#pragma once
#include <JuceHeader.h>

class AppSettings
{
public:
    static constexpr int defaultWindowWidth  = 900;
    static constexpr int defaultWindowHeight = 520;

    AppSettings();
    void load();
    void save();

    int getWindowX() const;
    int getWindowY() const;
    void setWindowPosition(int x, int y);

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

    bool getAutoSaveAfterPluginRepair() const;
    void setAutoSaveAfterPluginRepair(bool shouldAutoSave);

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
