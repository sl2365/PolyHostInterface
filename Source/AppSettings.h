#pragma once
#include <JuceHeader.h>

class AppSettings
{
public:
    AppSettings();
    void load();
    void save();

    juce::String getMidiDeviceName() const;
    void setMidiDeviceName(const juce::String& name);

    juce::StringArray getEnabledMidiDeviceIdentifiers() const;
    void setEnabledMidiDeviceIdentifiers(const juce::StringArray& identifiers);

    juce::String getAudioDeviceName() const;
    void setAudioDeviceName(const juce::String& name);

    int getWindowWidth() const;
    int getWindowHeight() const;
    void setWindowSize(int w, int h);

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
