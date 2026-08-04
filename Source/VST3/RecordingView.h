#pragma once

#include <JuceHeader.h>
#include <functional>
#include "AppSettings.h"
#include "AudioRecordingController.h"
#include "MidiRecordingController.h"
#include "ButtonStyling.h"

class RecordingView final : public juce::Component,
                            private juce::Timer,
                            private juce::ListBoxModel
{
public:
    RecordingView(AudioRecordingController& audioControllerIn,
                  MidiRecordingController& midiControllerIn,
                  AppSettings& settingsIn);
    ~RecordingView() override;

    void refreshNow();

    std::function<void()> onCloseView;
    std::function<void()> onStatusChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class RecordingModeSwitch final : public juce::Component,
                                      public juce::SettableTooltipClient
    {
    public:
        void setMidiSelected(bool shouldSelectMidi);
        bool isMidiSelected() const noexcept;
        void paint(juce::Graphics& graphics) override;
        void mouseUp(const juce::MouseEvent& event) override;

        std::function<void(bool)> onChange;

    private:
        bool midiSelected = false;
    };

    void timerCallback() override;
    int getNumRows() override;
    void paintListBoxItem(int rowNumber,
                          juce::Graphics& graphics,
                          int width,
                          int height,
                          bool rowIsSelected) override;
    void listBoxItemDoubleClicked(int row,
                                  const juce::MouseEvent& event) override;
    void refreshRecordingStatus();
    void refreshRecordingFiles();
    void updateRecordingFileLabel(bool armed);
    void updateModePresentation();
    void updateMidiOptions();
    bool isAnyRecordingArmed() const;
    void openRecordingFile(const juce::File& file);
    void openRecordingsFolder();
    static juce::String formatElapsedTime(juce::int64 recordedSamples,
                                          double sampleRate);

    AudioRecordingController& audioController;
    MidiRecordingController& midiController;
    AppSettings& settings;

    juce::Label titleLabel;
    RecordingModeSwitch modeSwitch;
    juce::Label countInLabel;
    juce::ComboBox countInCombo;
    juce::Label helpLabel;
    juce::GroupComponent recordingGroup { "recording", "Audio Recording" };
    juce::ToggleButton externalNotesButton { "External Notes" };
    juce::ToggleButton externalControllersButton { "External Controllers" };
    juce::ToggleButton hostedMidiButton { "Hosted MIDI Output" };
    juce::Label statusLabel;
    juce::Label elapsedLabel;
    juce::Label formatLabel;
    juce::Label fileLabel;
    juce::Label warningLabel;
    juce::Label recordingsLabel;
    juce::ListBox recordingsList;
    juce::TextButton openFolderButton { "Open Folder" };
    juce::TextButton closeViewButton { "Close" };

    juce::Array<juce::File> recordingFiles;

    bool lastRecordingState = false;
    bool lastArmedState = false;
    bool lastMidiMode = false;
    int lastDisplayedElapsedSecond = -1;
    juce::uint32 lastDroppedItemCount = 0;
    int recordingListRefreshTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingView)
};
