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
    juce::String getAudioDeviceName() const;
    void setAudioDeviceName(const juce::String& name);
    int getWindowWidth() const;
    int getWindowHeight() const;
    void setWindowSize(int w, int h);
    static juce::File getSettingsFile();
private:
    std::unique_ptr<juce::XmlElement> xml;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppSettings)
};
