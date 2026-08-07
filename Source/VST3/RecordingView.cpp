#include "RecordingView.h"
#include "ButtonStyling.h"
#include <algorithm>
#include <cmath>

namespace
{
    enum CountInItemIds
    {
        countInZeroBars = 1,
        countInOneBar,
        countInTwoBars,
        countInFourBars,
        countInEightBars,
        countInWaitNote
    };

    int getCountInItemId(
        AppSettings::RecordingCountInMode mode)
    {
        switch (mode)
        {
            case AppSettings::RecordingCountInMode::OneBar:
                return countInOneBar;
            case AppSettings::RecordingCountInMode::TwoBars:
                return countInTwoBars;
            case AppSettings::RecordingCountInMode::FourBars:
                return countInFourBars;
            case AppSettings::RecordingCountInMode::EightBars:
                return countInEightBars;
            case AppSettings::RecordingCountInMode::WaitNote:
                return countInWaitNote;
            case AppSettings::RecordingCountInMode::ZeroBars:
            default:
                return countInZeroBars;
        }
    }

    AppSettings::RecordingCountInMode getCountInModeForItemId(
        int itemId)
    {
        switch (itemId)
        {
            case countInOneBar:
                return AppSettings::RecordingCountInMode::OneBar;
            case countInTwoBars:
                return AppSettings::RecordingCountInMode::TwoBars;
            case countInFourBars:
                return AppSettings::RecordingCountInMode::FourBars;
            case countInEightBars:
                return AppSettings::RecordingCountInMode::EightBars;
            case countInWaitNote:
                return AppSettings::RecordingCountInMode::WaitNote;
            case countInZeroBars:
            default:
                return AppSettings::RecordingCountInMode::ZeroBars;
        }
    }

    void configureLabel(juce::Label& label,
                        float fontHeight,
                        juce::Justification justification)
    {
        label.setFont(juce::Font(juce::FontOptions(fontHeight)));
        label.setJustificationType(justification);
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    }
}

void RecordingView::RecordingModeSwitch::setMidiSelected(
    bool shouldSelectMidi)
{
    if (midiSelected == shouldSelectMidi)
        return;

    midiSelected = shouldSelectMidi;
    repaint();
}

bool RecordingView::RecordingModeSwitch::isMidiSelected() const noexcept
{
    return midiSelected;
}

void RecordingView::RecordingModeSwitch::paint(juce::Graphics& graphics)
{
    auto area = getLocalBounds();
    auto audioArea = area.removeFromLeft(42);
    auto midiArea = area.removeFromRight(38);
    auto switchArea =
        area.withSizeKeepingCentre(38, juce::jmin(18, area.getHeight()));

    const auto activeTextColour =
        isEnabled() ? juce::Colours::white
                    : juce::Colours::grey;
    const auto inactiveTextColour =
        activeTextColour.withAlpha(0.55f);

    graphics.setFont(juce::Font(juce::FontOptions(13.0f)));
    graphics.setColour(midiSelected
                           ? inactiveTextColour
                           : activeTextColour);
    graphics.drawText("Audio",
                      audioArea,
                      juce::Justification::centredRight,
                      false);

    graphics.setColour(midiSelected
                           ? activeTextColour
                           : inactiveTextColour);
    graphics.drawText("MIDI",
                      midiArea,
                      juce::Justification::centredLeft,
                      false);

    const float cornerRadius =
        switchArea.getHeight() * 0.5f;
    graphics.setColour(
        isEnabled()
            ? juce::Colour(0xFF3A6EA5)
            : juce::Colour(0xFF4A4E58));
    graphics.fillRoundedRectangle(switchArea.toFloat(),
                                  cornerRadius);

    const float knobDiameter =
        static_cast<float>(switchArea.getHeight() - 6);
    const float knobX =
        midiSelected
            ? static_cast<float>(switchArea.getRight())
                  - knobDiameter - 3.0f
            : static_cast<float>(switchArea.getX()) + 3.0f;

    graphics.setColour(
        isEnabled()
            ? juce::Colours::white
            : juce::Colours::lightgrey.withAlpha(0.65f));
    graphics.fillEllipse(knobX,
                         static_cast<float>(switchArea.getY()) + 3.0f,
                         knobDiameter,
                         knobDiameter);
}

void RecordingView::RecordingModeSwitch::mouseUp(
    const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (! isEnabled())
        return;

    midiSelected = ! midiSelected;
    repaint();

    if (onChange)
        onChange(midiSelected);
}

RecordingView::RecordingView(AudioRecordingController& audioControllerIn,
                             MidiRecordingController& midiControllerIn,
                             AppSettings& settingsIn)
    : audioController(audioControllerIn),
      midiController(midiControllerIn),
      settings(settingsIn),
      recordingsList("Recordings")
{
    recordingsList.setModel(this);

    MidiRecordingController::Options midiOptions;
    midiOptions.externalNotes = settings.getRecordExternalMidiNotes();
    midiOptions.externalControllers =
        settings.getRecordExternalMidiControllers();
    midiOptions.hostedOutput = settings.getRecordHostedMidiOutput();

    if (! midiOptions.externalNotes
        && ! midiOptions.externalControllers
        && ! midiOptions.hostedOutput)
    {
        midiOptions.externalNotes = true;
    }

    midiController.setOptions(midiOptions);
    midiController.setMidiModeSelected(
        settings.getMidiRecordingModeSelected());

    titleLabel.setText("Recording", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    modeSwitch.setMidiSelected(midiController.isMidiModeSelected());
    modeSwitch.setTooltip("Choose Audio or MIDI recording.");
    modeSwitch.onChange = [this](bool shouldSelectMidi)
    {
        if (isAnyRecordingArmed())
        {
            modeSwitch.setMidiSelected(
                midiController.isMidiModeSelected());
            return;
        }

        midiController.setMidiModeSelected(shouldSelectMidi);
        settings.setMidiRecordingModeSelected(shouldSelectMidi);
        updateModePresentation();
        refreshRecordingStatus();
        refreshRecordingFiles();
    };
    addAndMakeVisible(modeSwitch);

    countInLabel.setText("Count-in:", juce::dontSendNotification);
    countInLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    countInLabel.setJustificationType(juce::Justification::centredRight);
    countInLabel.setColour(juce::Label::textColourId,
                           juce::Colours::lightgrey);
    addAndMakeVisible(countInLabel);

    countInCombo.addItem("0 Bars", countInZeroBars);
    countInCombo.addItem("1 Bar", countInOneBar);
    countInCombo.addItem("2 Bars", countInTwoBars);
    countInCombo.addItem("4 Bars", countInFourBars);
    countInCombo.addItem("8 Bars", countInEightBars);
    countInCombo.addItem("Wait Note", countInWaitNote);
    countInCombo.setSelectedId(
        getCountInItemId(settings.getRecordingCountInMode()),
        juce::dontSendNotification);
    countInCombo.setTooltip(
        "Choose the delay before recording begins.\nWait Note starts on the first MIDI note-on.");
    countInCombo.onChange = [this]
    {
        settings.setRecordingCountInMode(
            getCountInModeForItemId(
                countInCombo.getSelectedId()));
    };
    addAndMakeVisible(countInCombo);

    configureLabel(helpLabel, 14.0f, juce::Justification::centredLeft);
    helpLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(helpLabel);

    recordingGroup.setColour(juce::GroupComponent::outlineColourId,
                             juce::Colour(0xFF526A86));
    recordingGroup.setColour(juce::GroupComponent::textColourId,
                             juce::Colours::white);
    addAndMakeVisible(recordingGroup);

    externalNotesButton.setToggleState(
        midiOptions.externalNotes,
        juce::dontSendNotification);
    externalNotesButton.setTooltip(
        "Record note-on and note-off messages from external MIDI hardware.");
    externalNotesButton.onClick = [this] { updateMidiOptions(); };
    addAndMakeVisible(externalNotesButton);

    externalControllersButton.setToggleState(
        midiOptions.externalControllers,
        juce::dontSendNotification);
    externalControllersButton.setTooltip(
        "Record external CC, pedal, pitch bend, aftertouch and program-change messages.");
    externalControllersButton.onClick = [this] { updateMidiOptions(); };
    addAndMakeVisible(externalControllersButton);

    hostedMidiButton.setToggleState(
        midiOptions.hostedOutput,
        juce::dontSendNotification);
    hostedMidiButton.setTooltip(
        "Record MIDI generated by hosted plug-ins, independently of MIDI Thru.");
    hostedMidiButton.onClick = [this] { updateMidiOptions(); };
    addAndMakeVisible(hostedMidiButton);

    configureLabel(statusLabel, 20.0f, juce::Justification::centred);
    statusLabel.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    addAndMakeVisible(statusLabel);

    configureLabel(elapsedLabel, 26.0f, juce::Justification::centred);
    elapsedLabel.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    addAndMakeVisible(elapsedLabel);

    configureLabel(musicalPositionLabel,
                   15.0f,
                   juce::Justification::centred);
    musicalPositionLabel.setFont(
        juce::Font(
            juce::FontOptions(15.0f, juce::Font::bold)));
    addAndMakeVisible(musicalPositionLabel);

    configureLabel(formatLabel, 14.0f, juce::Justification::centred);
    addAndMakeVisible(formatLabel);

    configureLabel(fileLabel, 13.0f, juce::Justification::centred);
    fileLabel.setMinimumHorizontalScale(0.55f);
    addAndMakeVisible(fileLabel);

    configureLabel(warningLabel, 13.0f, juce::Justification::centred);
    warningLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFFFB347));
    addAndMakeVisible(warningLabel);

    recordingsLabel.setText("Recordings", juce::dontSendNotification);
    recordingsLabel.setFont(
        juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    recordingsLabel.setJustificationType(juce::Justification::centredLeft);
    recordingsLabel.setColour(juce::Label::textColourId,
                              juce::Colours::white);
    addAndMakeVisible(recordingsLabel);

    recordingsList.setRowHeight(26);
    recordingsList.setColour(juce::ListBox::backgroundColourId,
                             juce::Colour(0xFF171B24));
    recordingsList.setColour(juce::ListBox::outlineColourId,
                             juce::Colours::white.withAlpha(0.18f));
    recordingsList.setOutlineThickness(1);
    addAndMakeVisible(recordingsList);

    openFolderButton.setColour(juce::TextButton::buttonColourId,
                               ButtonStyling::defaultBackground());
    openFolderButton.onClick = [this] { openRecordingsFolder(); };
    addAndMakeVisible(openFolderButton);

    closeViewButton.setColour(juce::TextButton::buttonColourId,
                              ButtonStyling::defaultBackground());
    closeViewButton.onClick = [this]
    {
        if (onCloseView)
            onCloseView();
    };
    addAndMakeVisible(closeViewButton);

    updateModePresentation();
    refreshNow();
    startTimerHz(30);
}

RecordingView::~RecordingView()
{
    stopTimer();
    recordingsList.setModel(nullptr);
}

void RecordingView::refreshNow()
{
    refreshRecordingStatus();
    refreshRecordingFiles();
}

void RecordingView::refreshRecordingStatus()
{
    const bool midiMode = midiController.isMidiModeSelected();
    const auto audioStatus = audioController.getStatus();
    const auto midiStatus = midiController.getStatus();
    const bool anyArmed = audioStatus.armed || midiStatus.armed;
    const bool recording =
        midiMode ? midiStatus.recording : audioStatus.recording;
    const bool armed =
        midiMode ? midiStatus.armed : audioStatus.armed;
    const double sampleRate =
        midiMode ? midiStatus.sampleRate : audioStatus.sampleRate;
    const juce::int64 recordedSampleCount =
        midiMode ? midiStatus.recordedSamples
                 : audioStatus.recordedSamples;
    const double recordedBeatCount =
        midiMode ? midiStatus.recordedBeats
                 : audioStatus.recordedBeats;
    const juce::uint32 droppedItemCount =
        midiMode ? midiStatus.droppedEvents
                 : audioStatus.droppedBlocks;
    const auto statusMessage =
        midiMode ? midiStatus.message : audioStatus.message;
    const int elapsedSecond =
        sampleRate > 0.0
            ? static_cast<int>(recordedSampleCount / sampleRate)
            : 0;

    statusLabel.setText(recording ? "RECORDING" : statusMessage,
                        juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId,
                          recording ? juce::Colour(0xFFFF4D5A)
                                    : juce::Colours::lightgrey);

    elapsedLabel.setText(formatElapsedTime(recordedSampleCount,
                                           sampleRate),
                         juce::dontSendNotification);

    musicalPositionLabel.setText(
        formatBarsAndBeats(recordedBeatCount),
        juce::dontSendNotification);

    if (midiMode)
    {
        formatLabel.setText("MIDI  |  Type 1  |  960 PPQ  |  4/4  |  "
                                + juce::String(midiStatus.recordedEvents)
                                + " event"
                                + juce::String(
                                    midiStatus.recordedEvents == 1
                                        ? ""
                                        : "s"),
                            juce::dontSendNotification);
    }
    else if (sampleRate > 0.0)
    {
        formatLabel.setText("WAV  |  Stereo  |  24-bit  |  "
                                + juce::String(sampleRate, 0)
                                + " Hz",
                            juce::dontSendNotification);
    }
    else
    {
        formatLabel.setText("Audio device unavailable",
                            juce::dontSendNotification);
    }

    updateRecordingFileLabel(armed);
    modeSwitch.setEnabled(! anyArmed);
    countInCombo.setEnabled(! anyArmed);
    externalNotesButton.setEnabled(midiMode && ! anyArmed);
    externalControllersButton.setEnabled(midiMode && ! anyArmed);
    hostedMidiButton.setEnabled(midiMode && ! anyArmed);

    if (droppedItemCount > 0)
    {
        warningLabel.setText("Warning: "
                                 + juce::String(droppedItemCount)
                                 + (midiMode ? " MIDI event" : " audio block")
                                 + juce::String(droppedItemCount == 1
                                                    ? " was"
                                                    : "s were")
                                 + " dropped because the recording buffer was full.",
                             juce::dontSendNotification);
    }
    else
    {
        warningLabel.setText({}, juce::dontSendNotification);
    }

    const bool recordingStateChanged =
        recording != lastRecordingState
        || armed != lastArmedState;

    const bool modeChanged = midiMode != lastMidiMode;

    const bool visibleStatusChanged =
        recordingStateChanged
        || modeChanged
        || elapsedSecond != lastDisplayedElapsedSecond
        || droppedItemCount != lastDroppedItemCount;

    lastRecordingState = recording;
    lastArmedState = armed;
    lastMidiMode = midiMode;
    lastDisplayedElapsedSecond = elapsedSecond;
    lastDroppedItemCount = droppedItemCount;

    if (recordingStateChanged || modeChanged)
        refreshRecordingFiles();

    if (visibleStatusChanged && onStatusChanged)
        onStatusChanged();
}

void RecordingView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF23283A));
}

void RecordingView::resized()
{
    auto area = getLocalBounds().reduced(18);

    auto headerRow = area.removeFromTop(30);
    auto headerButtons = headerRow.removeFromRight(164);
    closeViewButton.setBounds(headerButtons.removeFromRight(64).reduced(0, 2));
    headerButtons.removeFromRight(8);
    openFolderButton.setBounds(headerButtons.removeFromRight(92).reduced(0, 2));

    auto countInArea = headerRow.removeFromRight(190);
    countInArea.removeFromRight(8);
    countInLabel.setBounds(countInArea.removeFromLeft(68));
    countInCombo.setBounds(countInArea.reduced(0, 2));

    auto modeArea = headerRow.removeFromRight(134);
    modeArea.removeFromRight(8);
    modeSwitch.setBounds(modeArea);
    titleLabel.setBounds(headerRow);

    area.removeFromTop(6);

    const int availableGroupHeight = juce::jmax(0, area.getHeight());
    auto groupArea = area.removeFromTop(juce::jmin(282, availableGroupHeight));
    groupArea = groupArea.withSizeKeepingCentre(juce::jmin(760, groupArea.getWidth()),
                                                groupArea.getHeight());
    recordingGroup.setBounds(groupArea);

    auto controls = groupArea.reduced(18, 14);
    controls.removeFromTop(10);

    constexpr int columnGap = 14;
    const int recordingsWidth =
        juce::jlimit(180,
                     320,
                     (controls.getWidth() - columnGap) * 42 / 100);

    auto recordingsArea =
        controls.removeFromRight(recordingsWidth);
    controls.removeFromRight(columnGap);

    auto informationArea = controls;

    if (midiController.isMidiModeSelected())
    {
        helpLabel.setBounds(informationArea.removeFromTop(46));

        auto optionsArea = informationArea.removeFromTop(50);
        auto firstOptionsRow = optionsArea.removeFromTop(24);
        externalNotesButton.setBounds(
            firstOptionsRow.removeFromLeft(
                juce::jmin(132, firstOptionsRow.getWidth() / 2)));
        externalControllersButton.setBounds(firstOptionsRow);
        hostedMidiButton.setBounds(optionsArea.removeFromTop(24));

        statusLabel.setBounds(informationArea.removeFromTop(28));
        elapsedLabel.setBounds(informationArea.removeFromTop(32));
        musicalPositionLabel.setBounds(
            informationArea.removeFromTop(18));
        formatLabel.setBounds(informationArea.removeFromTop(22));
        fileLabel.setBounds(informationArea.removeFromTop(24));
        warningLabel.setBounds(informationArea.removeFromTop(24));
    }
    else
    {
        helpLabel.setBounds(informationArea.removeFromTop(60));

        informationArea.removeFromTop(6);
        statusLabel.setBounds(informationArea.removeFromTop(30));
        elapsedLabel.setBounds(informationArea.removeFromTop(34));
        musicalPositionLabel.setBounds(
            informationArea.removeFromTop(18));
        formatLabel.setBounds(informationArea.removeFromTop(22));
        fileLabel.setBounds(informationArea.removeFromTop(32));
        warningLabel.setBounds(informationArea.removeFromTop(24));
    }

    recordingsLabel.setBounds(recordingsArea.removeFromTop(22));
    recordingsArea.removeFromTop(4);
    recordingsList.setBounds(recordingsArea);
}

void RecordingView::timerCallback()
{
    refreshRecordingStatus();

    if (! isVisible())
    {
        recordingListRefreshTicks = 0;
        return;
    }

    ++recordingListRefreshTicks;

    if (recordingListRefreshTicks >= 60)
    {
        recordingListRefreshTicks = 0;
        refreshRecordingFiles();
    }
}

int RecordingView::getNumRows()
{
    return juce::jmax(1, recordingFiles.size());
}

void RecordingView::paintListBoxItem(int rowNumber,
                                     juce::Graphics& graphics,
                                     int width,
                                     int height,
                                     bool rowIsSelected)
{
    auto rowBounds =
        juce::Rectangle<int>(0, 0, width, height).reduced(3, 2);

    if (recordingFiles.isEmpty())
    {
        if (rowNumber != 0)
            return;

        graphics.setColour(juce::Colours::lightgrey.withAlpha(0.65f));
        graphics.setFont(juce::Font(juce::FontOptions(13.0f)));
        graphics.drawFittedText(
                                midiController.isMidiModeSelected()
                                    ? "No MIDI recordings found."
                                    : "No WAV recordings found.",
                                rowBounds.reduced(7, 0),
                                juce::Justification::centredLeft,
                                1);
        return;
    }

    if (! juce::isPositiveAndBelow(rowNumber, recordingFiles.size()))
        return;

    if (rowIsSelected)
    {
        graphics.setColour(juce::Colour(0xFF3A6EA5));
        graphics.fillRoundedRectangle(rowBounds.toFloat(), 4.0f);
    }

    graphics.setColour(rowIsSelected
                           ? juce::Colours::white
                           : juce::Colours::lightgrey);
    graphics.setFont(
        juce::Font(juce::FontOptions(13.0f,
                                     rowIsSelected
                                         ? juce::Font::bold
                                         : juce::Font::plain)));
    graphics.drawFittedText(recordingFiles.getReference(rowNumber).getFileName(),
                            rowBounds.reduced(7, 0),
                            juce::Justification::centredLeft,
                            1);
}

void RecordingView::listBoxItemDoubleClicked(
    int row,
    const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (! juce::isPositiveAndBelow(row, recordingFiles.size()))
        return;

    openRecordingFile(recordingFiles.getReference(row));
}

void RecordingView::refreshRecordingFiles()
{
    const auto directory =
        AudioRecordingController::getRecordingsDirectory();

    const juce::String filePattern =
        midiController.isMidiModeSelected()
            ? "*.mid"
            : "*.wav";

    juce::Array<juce::File> discoveredFiles;

    if (directory.isDirectory())
    {
        discoveredFiles =
            directory.findChildFiles(juce::File::findFiles,
                                     false,
                                     filePattern);
    }

    std::sort(discoveredFiles.begin(),
              discoveredFiles.end(),
              [](const juce::File& first,
                 const juce::File& second)
              {
                  const auto firstTime =
                      first.getLastModificationTime().toMilliseconds();
                  const auto secondTime =
                      second.getLastModificationTime().toMilliseconds();

                  if (firstTime != secondTime)
                      return firstTime > secondTime;

                  return first.getFileName().compareIgnoreCase(
                             second.getFileName()) > 0;
              });

    bool filesChanged =
        discoveredFiles.size() != recordingFiles.size();

    if (! filesChanged)
    {
        for (int index = 0;
             index < discoveredFiles.size();
             ++index)
        {
            if (discoveredFiles.getReference(index).getFullPathName()
                != recordingFiles.getReference(index).getFullPathName())
            {
                filesChanged = true;
                break;
            }
        }
    }

    if (! filesChanged)
        return;

    recordingFiles = discoveredFiles;
    recordingsList.updateContent();
    recordingsList.repaint();
    updateRecordingFileLabel(isAnyRecordingArmed());
}

void RecordingView::updateRecordingFileLabel(
    bool armed)
{
    if (armed)
    {
        fileLabel.setText({}, juce::dontSendNotification);
        fileLabel.setTooltip({});
        return;
    }

    if (recordingFiles.isEmpty())
    {
        fileLabel.setText("No recordings have been made yet.",
                          juce::dontSendNotification);
    }
    else
    {
        fileLabel.setText({}, juce::dontSendNotification);
    }

    fileLabel.setTooltip({});
}

void RecordingView::updateModePresentation()
{
    const bool midiMode = midiController.isMidiModeSelected();
    modeSwitch.setMidiSelected(midiMode);

    recordingGroup.setText(
        midiMode ? "MIDI Recording" : "Audio Recording");

    if (midiMode)
    {
        helpLabel.setText(
            "Use the Record button in the tempo toolbar to record a standard "
            "MIDI file. The metronome is not included in the MIDI file. "
            "Recording continues when this view is closed or another tab selected.",
            juce::dontSendNotification);
    }
    else
    {
        helpLabel.setText(
            "Use the Record button in the tempo toolbar to record the stereo "
            "output to a 24-bit WAV file. The metronome is not recorded. "
            "Recording continues when this view is closed or another tab selected.",
            juce::dontSendNotification);
    }

    externalNotesButton.setVisible(midiMode);
    externalControllersButton.setVisible(midiMode);
    hostedMidiButton.setVisible(midiMode);

    recordingsLabel.setText(
        midiMode ? "MIDI Recordings" : "Audio Recordings",
        juce::dontSendNotification);

    const auto fileDescription =
        midiMode
            ? "Double-click a completed MIDI file to open it in the Windows default application."
            : "Double-click a completed WAV file to open it in the Windows default player.";

    recordingsLabel.setTooltip(fileDescription);
    recordingsList.setTooltip(fileDescription);
    recordingsList.updateContent();
    recordingsList.repaint();
    resized();
}

void RecordingView::updateMidiOptions()
{
    if (isAnyRecordingArmed())
        return;

    MidiRecordingController::Options options;
    options.externalNotes = externalNotesButton.getToggleState();
    options.externalControllers =
        externalControllersButton.getToggleState();
    options.hostedOutput = hostedMidiButton.getToggleState();

    if (! options.externalNotes
        && ! options.externalControllers
        && ! options.hostedOutput)
    {
        const auto previousOptions = midiController.getOptions();
        externalNotesButton.setToggleState(
            previousOptions.externalNotes,
            juce::dontSendNotification);
        externalControllersButton.setToggleState(
            previousOptions.externalControllers,
            juce::dontSendNotification);
        hostedMidiButton.setToggleState(
            previousOptions.hostedOutput,
            juce::dontSendNotification);
        return;
    }

    midiController.setOptions(options);
    settings.setMidiRecordingSources(options.externalNotes,
                                     options.externalControllers,
                                     options.hostedOutput);
    refreshRecordingStatus();
}

bool RecordingView::isAnyRecordingArmed() const
{
    return audioController.isRecording()
           || midiController.isRecording();
}

void RecordingView::openRecordingFile(const juce::File& file)
{
    const bool midiMode = midiController.isMidiModeSelected();
    const auto audioStatus = audioController.getStatus();
    const auto midiStatus = midiController.getStatus();
    const bool activeFileIsArmed =
        midiMode
            ? midiStatus.armed
              && midiStatus.activeFile.getFullPathName()
                     == file.getFullPathName()
            : audioStatus.armed
              && audioStatus.activeFile.getFullPathName()
                     == file.getFullPathName();
    const juce::String fileType = midiMode ? "MIDI" : "WAV";

    if (activeFileIsArmed)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Recording Active",
            "Cancel or stop recording before opening this "
                + fileType
                + " file.",
            "OK",
            this);
        return;
    }

    if (! file.existsAsFile())
    {
        refreshRecordingFiles();
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Open Recording Failed",
            "The " + fileType + " file no longer exists:\n\n"
                + file.getFullPathName(),
            "OK",
            this);
        return;
    }

    if (! file.startAsProcess())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Open Recording Failed",
            "Windows could not open the "
                + fileType
                + " file with its default application:\n\n"
                + file.getFullPathName(),
            "OK",
            this);
    }
}

void RecordingView::openRecordingsFolder()
{
    auto directory = AudioRecordingController::getRecordingsDirectory();
    const auto result = directory.createDirectory();

    if (result.failed() || ! directory.startAsProcess())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Open Recordings Folder Failed",
            "The Recordings folder could not be opened:\n\n"
                + directory.getFullPathName(),
            "OK",
            this);
    }
}

juce::String RecordingView::formatElapsedTime(juce::int64 samples,
                                               double sampleRate)
{
    const auto totalSeconds =
        sampleRate > 0.0
            ? static_cast<juce::int64>(samples / sampleRate)
            : 0;

    const int hours = static_cast<int>(totalSeconds / 3600);
    const int minutes = static_cast<int>((totalSeconds / 60) % 60);
    const int seconds = static_cast<int>(totalSeconds % 60);

    return juce::String(hours).paddedLeft('0', 2)
         + ":"
         + juce::String(minutes).paddedLeft('0', 2)
         + ":"
         + juce::String(seconds).paddedLeft('0', 2);
}

juce::String RecordingView::formatBarsAndBeats(
    double recordedBeats)
{
    constexpr juce::int64 beatsPerBar = 4;

    const auto completedBeats =
        static_cast<juce::int64>(
            std::floor(juce::jmax(0.0, recordedBeats)
                       + 0.0000001));

    const auto barNumber =
        completedBeats / beatsPerBar + 1;

    const auto beatNumber =
        completedBeats % beatsPerBar + 1;

    return "Beats / Bars: "
         + juce::String(barNumber).paddedLeft('0', 3)
         + "."
         + juce::String(beatNumber);
}
