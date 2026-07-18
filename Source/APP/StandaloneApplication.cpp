#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include <atomic>
#include <cmath>

#include "AppSettings.h"
#include "ButtonStyling.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
class StandalonePlayHead final : public juce::AudioPlayHead
{
public:
    void prepareToPlay(double newSampleRate)
    {
        const juce::ScopedLock scopedLock(lock);

        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        samplePosition = 0;
    }

    void setTempoBpm(double newTempoBpm)
    {
        const juce::ScopedLock scopedLock(lock);
        tempoBpm = juce::jlimit(20.0, 300.0, newTempoBpm);
    }

    double getTempoBpm() const
    {
        const juce::ScopedLock scopedLock(lock);
        return tempoBpm;
    }

    void advance(int numSamples)
    {
        const juce::ScopedLock scopedLock(lock);
        samplePosition += juce::jmax(0, numSamples);
    }

    int getCurrentBeatInBar() const
    {
        const juce::ScopedLock scopedLock(lock);

        const double ppqPosition =
            (static_cast<double>(samplePosition) / sampleRate)
            * (tempoBpm / 60.0);

        const int beatIndex =
            static_cast<int>(std::floor(ppqPosition)) % 4;

        return (beatIndex + 4) % 4;
    }

    double getCurrentBeatProgress() const
    {
        const juce::ScopedLock scopedLock(lock);

        const double ppqPosition =
            (static_cast<double>(samplePosition) / sampleRate)
            * (tempoBpm / 60.0);

        return juce::jlimit(
            0.0,
            1.0,
            ppqPosition - std::floor(ppqPosition));
    }

    Optional<PositionInfo> getPosition() const override
    {
        const juce::ScopedLock scopedLock(lock);

        PositionInfo info;
        info.setIsPlaying(true);
        info.setIsRecording(false);
        info.setIsLooping(false);
        info.setBpm(tempoBpm);
        info.setTimeSignature(
            juce::AudioPlayHead::TimeSignature { 4, 4 });
        info.setTimeInSamples(samplePosition);
        info.setTimeInSeconds(
            static_cast<double>(samplePosition) / sampleRate);

        const double ppqPosition =
            (static_cast<double>(samplePosition) / sampleRate)
            * (tempoBpm / 60.0);

        info.setPpqPosition(ppqPosition);
        info.setPpqPositionOfLastBarStart(
            std::floor(ppqPosition / 4.0) * 4.0);

        return info;
    }

private:
    mutable juce::CriticalSection lock;
    double sampleRate = 44100.0;
    double tempoBpm = 120.0;
    juce::int64 samplePosition = 0;
};

class StandalonePlayHeadTracker final
    : public PolyHostPluginProcessor::StandaloneAudioExtension
{
public:
    explicit StandalonePlayHeadTracker(
        StandalonePlayHead& playHeadIn)
        : playHead(playHeadIn)
    {
    }

    void setMetronomeEnabled(bool shouldBeEnabled)
    {
        metronomeEnabled.store(shouldBeEnabled);
    }

    bool isMetronomeEnabled() const
    {
        return metronomeEnabled.load();
    }

    void prepareToPlay(double sampleRate,
                       int samplesPerBlock) override
    {
        juce::ignoreUnused(samplesPerBlock);

        currentSampleRate =
            sampleRate > 0.0 ? sampleRate : 44100.0;

        playHead.prepareToPlay(currentSampleRate);

        processedSamples = 0;
        samplesRemainingInClick = 0;
        clickPhase = 0.0;
        clickFrequencyHz = 1000.0;
        beatIndex = 0;
    }

    void processOutputBlock(
        juce::AudioBuffer<float>& buffer) override
    {
        const int numSamples = buffer.getNumSamples();

        if (metronomeEnabled.load())
            renderMetronome(buffer);
        else
            samplesRemainingInClick = 0;

        processedSamples += numSamples;
        playHead.advance(numSamples);
    }

    void releaseResources() override
    {
        samplesRemainingInClick = 0;
        clickPhase = 0.0;
    }

private:
    void renderMetronome(
        juce::AudioBuffer<float>& buffer)
    {
        const int numOutputChannels =
            buffer.getNumChannels();

        const int numSamples =
            buffer.getNumSamples();

        if (numOutputChannels <= 0
            || numSamples <= 0
            || currentSampleRate <= 0.0)
        {
            return;
        }

        const double bpm =
            juce::jmax(1.0, playHead.getTempoBpm());

        const double samplesPerBeat =
            (60.0 / bpm) * currentSampleRate;

        const int clickLengthSamples =
            static_cast<int>(
                0.050 * currentSampleRate);

        for (int sample = 0;
             sample < numSamples;
             ++sample)
        {
            const auto absoluteSample =
                processedSamples + sample;

            const auto previousBeat =
                static_cast<int>(
                    std::floor(
                        static_cast<double>(
                            juce::jmax<juce::int64>(
                                0,
                                absoluteSample - 1))
                        / samplesPerBeat));

            const auto currentBeat =
                static_cast<int>(
                    std::floor(
                        static_cast<double>(
                            absoluteSample)
                        / samplesPerBeat));

            if (absoluteSample == 0
                || currentBeat != previousBeat)
            {
                beatIndex =
                    ((currentBeat % 4) + 4) % 4;

                samplesRemainingInClick =
                    clickLengthSamples;

                clickPhase = 0.0;

                clickFrequencyHz =
                    beatIndex == 0
                        ? 1760.0
                        : 1320.0;
            }

            float clickSample = 0.0f;

            if (samplesRemainingInClick > 0)
            {
                const float envelope =
                    static_cast<float>(
                        samplesRemainingInClick)
                    / static_cast<float>(
                        clickLengthSamples);

                clickSample =
                    std::sin(
                        static_cast<float>(
                            clickPhase))
                    * 0.28f
                    * envelope
                    * envelope;

                clickPhase +=
                    juce::MathConstants<double>::twoPi
                    * clickFrequencyHz
                    / currentSampleRate;

                --samplesRemainingInClick;
            }

            for (int channel = 0;
                 channel < numOutputChannels;
                 ++channel)
            {
                buffer.getWritePointer(channel)[sample]
                    += clickSample;
            }
        }
    }

    StandalonePlayHead& playHead;
    std::atomic<bool> metronomeEnabled { false };
    juce::int64 processedSamples = 0;
    int samplesRemainingInClick = 0;
    double clickPhase = 0.0;
    double clickFrequencyHz = 1000.0;
    int beatIndex = 0;
    double currentSampleRate = 44100.0;
};

class StandaloneTempoControls final : public juce::Component
{
public:
    static constexpr int preferredWidth = 218;
    static constexpr int preferredHeight = 32;

    StandaloneTempoControls(
        PolyHostPluginProcessor* processorIn,
        StandalonePlayHead& playHeadIn,
        StandalonePlayHeadTracker& playHeadTrackerIn)
        : processor(processorIn),
          playHead(playHeadIn),
          playHeadTracker(playHeadTrackerIn),
          beatIndicator(playHeadIn),
          tempoEditor(*this)
    {
        AppSettings settings;
        defaultTempoBpm = settings.getDefaultTempoBpm();
        playHead.setTempoBpm(defaultTempoBpm);

        addAndMakeVisible(beatIndicator);

        addAndMakeVisible(tempoEditor);
        tempoEditor.setInputRestrictions(6, "0123456789.");
        tempoEditor.setJustification(juce::Justification::centred);
        tempoEditor.setSelectAllWhenFocused(true);
        tempoEditor.setColour(
            juce::TextEditor::backgroundColourId,
            juce::Colour(0xFF151B24));
        tempoEditor.setColour(
            juce::TextEditor::textColourId,
            juce::Colours::white);
        tempoEditor.setColour(
            juce::TextEditor::outlineColourId,
            juce::Colour(0xFF4C5668));
        tempoEditor.setColour(
            juce::TextEditor::focusedOutlineColourId,
            juce::Colour(0xFF6E8FB8));
        tempoEditor.onReturnKey = [this]
        {
            commitTempoFromEditor();
            tempoEditor.unfocusAllComponents();
        };

        addAndMakeVisible(resetTempoButton);
        resetTempoButton.onClick = [this]
        {
            setTempoBpm(defaultTempoBpm, true);
        };

        addAndMakeVisible(metronomeButton);
        metronomeButton.onClick = [this]
        {
            playHeadTracker.setMetronomeEnabled(
                ! playHeadTracker.isMetronomeEnabled());

            metronomeButton.repaint();
        };

        addAndMakeVisible(tapTempoButton);
        tapTempoButton.onClick = [this]
        {
            registerTapTempo();
        };

        refreshUi();
    }

    double getTempoBpm() const
    {
        return playHead.getTempoBpm();
    }

    double getDefaultTempoBpm() const
    {
        return defaultTempoBpm;
    }

    void setTempoBpmFromPreset(double bpm)
    {
        setTempoBpm(bpm, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(2, 0);

        const int buttonWidth =
            ButtonStyling::defaultButtonWidth();

        const int buttonHeight =
            ButtonStyling::defaultButtonHeight();

        auto tapBounds = area.removeFromRight(buttonWidth);
        tapTempoButton.setBounds(
            tapBounds.withSizeKeepingCentre(
                tapBounds.getWidth(),
                buttonHeight));

        area.removeFromRight(2);

        auto metronomeBounds =
            area.removeFromRight(buttonWidth);

        metronomeButton.setBounds(
            metronomeBounds.withSizeKeepingCentre(
                metronomeBounds.getWidth(),
                buttonHeight));

        area.removeFromRight(1);

        auto resetBounds =
            area.removeFromRight(buttonWidth);

        resetTempoButton.setBounds(
            resetBounds.withSizeKeepingCentre(
                resetBounds.getWidth(),
                buttonHeight));

        area.removeFromRight(2);

        auto editorBounds = area.removeFromRight(55);

        tempoEditor.setBounds(
            editorBounds.reduced(1, 0)
                        .withSizeKeepingCentre(
                            editorBounds.getWidth() - 2,
                            22));

        area.removeFromRight(2);
        beatIndicator.setBounds(area);
    }

private:
    class TempoTextEditor final : public juce::TextEditor
    {
    public:
        explicit TempoTextEditor(
            StandaloneTempoControls& ownerIn)
            : owner(ownerIn)
        {
        }

        void mouseWheelMove(
            const juce::MouseEvent& event,
            const juce::MouseWheelDetails& wheel) override
        {
            if (wheel.deltaY == 0.0f)
            {
                juce::TextEditor::mouseWheelMove(event, wheel);
                return;
            }

            const double step =
                event.mods.isShiftDown() ? 0.1 : 1.0;

            const double direction =
                wheel.deltaY > 0.0f ? 1.0 : -1.0;

            owner.setTempoBpm(
                owner.getTempoBpm() + direction * step,
                true);
        }

        void mouseDoubleClick(
            const juce::MouseEvent& event) override
        {
            juce::ignoreUnused(event);

            owner.setTempoBpm(
                owner.getDefaultTempoBpm(),
                true);
        }

        void addPopupMenuItems(
            juce::PopupMenu& menu,
            const juce::MouseEvent* mouseClickEvent) override
        {
            juce::TextEditor::addPopupMenuItems(
                menu,
                mouseClickEvent);

            menu.addSeparator();
            menu.addItem(1001, "Set as Default");
        }

        void performPopupMenuAction(int menuItemID) override
        {
            if (menuItemID == 1001)
            {
                owner.setCurrentTempoAsDefault();
                return;
            }

            juce::TextEditor::performPopupMenuAction(
                menuItemID);
        }

        bool keyPressed(
            const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress(
                           juce::KeyPress::upKey,
                           juce::ModifierKeys::shiftModifier,
                           0))
            {
                owner.setTempoBpm(
                    owner.getTempoBpm() + 0.1,
                    true);
                return true;
            }

            if (key == juce::KeyPress(
                           juce::KeyPress::downKey,
                           juce::ModifierKeys::shiftModifier,
                           0))
            {
                owner.setTempoBpm(
                    owner.getTempoBpm() - 0.1,
                    true);
                return true;
            }

            if (key == juce::KeyPress::upKey)
            {
                owner.setTempoBpm(
                    owner.getTempoBpm() + 1.0,
                    true);
                return true;
            }

            if (key == juce::KeyPress::downKey)
            {
                owner.setTempoBpm(
                    owner.getTempoBpm() - 1.0,
                    true);
                return true;
            }

            return juce::TextEditor::keyPressed(key);
        }

        void focusLost(FocusChangeType cause) override
        {
            juce::TextEditor::focusLost(cause);
            owner.commitTempoFromEditor();
        }

    private:
        StandaloneTempoControls& owner;
    };

    class BeatIndicatorComponent final : public juce::Component,
                                         private juce::Timer
    {
    public:
        explicit BeatIndicatorComponent(
            StandalonePlayHead& playHeadIn)
            : playHead(playHeadIn)
        {
            startTimerHz(30);
        }

        void paint(juce::Graphics& g) override
        {
            auto area =
                getLocalBounds().toFloat().reduced(1.0f);

            const int beatIndex =
                playHead.getCurrentBeatInBar();

            const auto beatProgress =
                playHead.getCurrentBeatProgress();

            const float ledDiameter = 9.0f;
            const float spacing = 5.0f;

            const float totalWidth =
                (ledDiameter * 4.0f)
                + (spacing * 3.0f);

            const float startX =
                area.getCentreX() - (totalWidth * 0.5f);

            const float y =
                area.getCentreY() - (ledDiameter * 0.5f);

            for (int index = 0; index < 4; ++index)
            {
                auto bounds = juce::Rectangle<float>(
                    startX
                        + (ledDiameter + spacing)
                            * static_cast<float>(index),
                    y,
                    ledDiameter,
                    ledDiameter);

                const bool isActive = index == beatIndex;
                float pulse = 1.0f;

                if (isActive)
                {
                    pulse =
                        0.75f
                        + 0.25f
                            * (1.0f
                               - static_cast<float>(
                                   beatProgress));
                }

                const auto baseColour =
                    index == 0
                        ? juce::Colours::red
                        : juce::Colours::limegreen;

                const auto fillColour =
                    isActive
                        ? baseColour.brighter(
                            0.35f * pulse + 0.15f)
                        : baseColour.darker(0.75f)
                              .withAlpha(0.22f);

                const auto borderColour =
                    isActive
                        ? baseColour.brighter(
                            0.55f * pulse + 0.25f)
                        : baseColour.darker(0.5f)
                              .withAlpha(0.35f);

                const auto highlightColour =
                    isActive
                        ? juce::Colours::white.withAlpha(
                            0.20f + 0.12f * pulse)
                        : juce::Colours::white.withAlpha(
                            0.06f);

                g.setColour(fillColour);
                g.fillEllipse(bounds);

                auto highlight =
                    bounds.reduced(1.5f)
                          .removeFromTop(
                              bounds.getHeight() * 0.42f);

                g.setColour(highlightColour);
                g.fillEllipse(highlight);

                g.setColour(borderColour);
                g.drawEllipse(bounds, 1.0f);
            }
        }

    private:
        void timerCallback() override
        {
            repaint();
        }

        StandalonePlayHead& playHead;
    };

    void setTempoBpm(double bpm, bool markPresetDirty)
    {
        bpm = juce::jlimit(20.0, 300.0, bpm);
        bpm = std::round(bpm * 10.0) / 10.0;

        if (std::abs(playHead.getTempoBpm() - bpm) < 0.0001)
        {
            refreshUi();
            return;
        }

        playHead.setTempoBpm(bpm);
        refreshUi();

        if (markPresetDirty && processor != nullptr)
            processor->getCore().markDirty();
    }

    void commitTempoFromEditor()
    {
        const auto text = tempoEditor.getText().trim();

        if (text.isEmpty()
            || ! text.containsOnly("0123456789."))
        {
            refreshUi();
            return;
        }

        if (text.containsChar('.')
            && text.fromFirstOccurrenceOf(
                        ".",
                        false,
                        false)
                   .containsChar('.'))
        {
            refreshUi();
            return;
        }

        const double parsedTempo = text.getDoubleValue();

        if (parsedTempo <= 0.0)
        {
            refreshUi();
            return;
        }

        setTempoBpm(parsedTempo, true);
    }

    void registerTapTempo()
    {
        const double nowMs =
            juce::Time::getMillisecondCounterHiRes();

        if (! tapTimesMs.isEmpty()
            && nowMs - tapTimesMs.getLast() > 2000.0)
        {
            tapTimesMs.clear();
        }

        tapTimesMs.add(nowMs);

        while (tapTimesMs.size() > 6)
            tapTimesMs.remove(0);

        if (tapTimesMs.size() < 2)
            return;

        juce::Array<double> validIntervalsMs;

        for (int index = 1;
             index < tapTimesMs.size();
             ++index)
        {
            const double intervalMs =
                tapTimesMs[index]
                - tapTimesMs[index - 1];

            if (intervalMs >= 200.0
                && intervalMs <= 3000.0)
            {
                validIntervalsMs.add(intervalMs);
            }
        }

        if (validIntervalsMs.isEmpty())
            return;

        double totalMs = 0.0;

        for (const auto intervalMs : validIntervalsMs)
            totalMs += intervalMs;

        const double averageIntervalMs =
            totalMs
            / static_cast<double>(validIntervalsMs.size());

        setTempoBpm(
            60000.0 / averageIntervalMs,
            true);
    }

    void setCurrentTempoAsDefault()
    {
        defaultTempoBpm = getTempoBpm();

        AppSettings settings;
        settings.setDefaultTempoBpm(defaultTempoBpm);
        refreshUi();
    }

    void refreshUi()
    {
        tempoEditor.setText(
            juce::String(getTempoBpm(), 1),
            juce::dontSendNotification);

        tempoEditor.setTooltip(
            "Scroll to adjust.\n"
            "Shift+Scroll: Fine Adjust.\n"
            "Double-click: Reset.\n"
            "Right-click: Set as Default.");

        resetTempoButton.setTooltip(
            "Reset tempo to "
            + juce::String(defaultTempoBpm, 1)
            + ".");

        metronomeButton.repaint();
    }

    PolyHostPluginProcessor* processor = nullptr;
    StandalonePlayHead& playHead;
    StandalonePlayHeadTracker& playHeadTracker;
    double defaultTempoBpm = 120.0;
    juce::Array<double> tapTimesMs;
    BeatIndicatorComponent beatIndicator;
    TempoTextEditor tempoEditor;

    ButtonStyling::ToolbarIconButton resetTempoButton
    {
        0,
        ButtonStyling::Tooltips::resetTempo(),
        ButtonStyling::Glyphs::reset(),
        ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
        ButtonStyling::defaultButtonWidth()
    };

    ButtonStyling::ToolbarIconButton metronomeButton
    {
        0,
        ButtonStyling::Tooltips::metronome(),
        ButtonStyling::Glyphs::metronome(),
        ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
        ButtonStyling::defaultButtonWidth(),
        [this]
        {
            return playHeadTracker.isMetronomeEnabled();
        }
    };

    ButtonStyling::ToolbarIconButton tapTempoButton
    {
        0,
        ButtonStyling::Tooltips::tapTempo(),
        ButtonStyling::Glyphs::tapTempo(),
        ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
        ButtonStyling::defaultButtonWidth(),
        {},
        ButtonStyling::defaultBackground(),
        0,
        ButtonStyling::defaultIconSize() + 2.0f
    };
};

class StandaloneAudioSettingsComponent final : public juce::Component
{
public:
    StandaloneAudioSettingsComponent(
        juce::StandalonePluginHolder& holderIn,
        int maxAudioInputChannels,
        int maxAudioOutputChannels)
        : holder(holderIn),
          deviceSelector(holder.deviceManager,
                         0,
                         maxAudioInputChannels,
                         0,
                         maxAudioOutputChannels,
                         false,
                         false,
                         true,
                         false),
          shouldMuteLabel("Feedback Loop:", "Feedback Loop:"),
          shouldMuteButton("Mute audio input")
    {
        setOpaque(true);

        shouldMuteButton.setClickingTogglesState(true);
        shouldMuteButton.getToggleStateValue().referTo(
            holder.getMuteInputValue());

        addAndMakeVisible(deviceSelector);

        if (holder.getProcessorHasPotentialFeedbackLoop())
        {
            addAndMakeVisible(shouldMuteButton);
            addAndMakeVisible(shouldMuteLabel);
            shouldMuteLabel.attachToComponent(
                &shouldMuteButton,
                true);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(
            getLookAndFeel().findColour(
                juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        const juce::ScopedValueSetter<bool> resizingSetter(
            isResizing,
            true);

        auto area = getLocalBounds();

        if (holder.getProcessorHasPotentialFeedbackLoop())
        {
            const auto itemHeight =
                deviceSelector.getItemHeight();

            auto feedbackArea =
                area.removeFromTop(itemHeight);

            const auto separatorHeight =
                itemHeight / 2;

            shouldMuteButton.setBounds(
                juce::Rectangle<int>(
                    feedbackArea.proportionOfWidth(0.35f),
                    separatorHeight,
                    feedbackArea.proportionOfWidth(0.60f),
                    itemHeight));

            area.removeFromTop(separatorHeight);
        }

        deviceSelector.setBounds(area);
    }

    void childBoundsChanged(
        juce::Component* childComponent) override
    {
        if (! isResizing
            && childComponent == &deviceSelector)
        {
            setToRecommendedSize();
        }
    }

    void setToRecommendedSize()
    {
        int extraHeight = 0;

        if (holder.getProcessorHasPotentialFeedbackLoop())
        {
            const auto itemHeight =
                deviceSelector.getItemHeight();

            extraHeight =
                itemHeight + (itemHeight / 2);
        }

        setSize(
            getWidth(),
            deviceSelector.getHeight() + extraHeight);
    }

private:
    juce::StandalonePluginHolder& holder;
    juce::AudioDeviceSelectorComponent deviceSelector;
    juce::Label shouldMuteLabel;
    juce::ToggleButton shouldMuteButton;
    bool isResizing = false;
};

class StandaloneMenuExtension final : public MainView::MenuExtension
{
public:
    StandaloneMenuExtension(
        juce::StandalonePluginHolder& holderIn,
        StandalonePlayHead& playHeadIn,
        StandalonePlayHeadTracker& playHeadTrackerIn)
        : holder(holderIn),
          tempoControls(
              static_cast<PolyHostPluginProcessor*>(
                  holderIn.processor.get()),
              playHeadIn,
              playHeadTrackerIn)
    {
    }

    juce::StringArray getAdditionalMenuNames() const override
    {
        return { "Audio" };
    }

    juce::Component* getMenuBarRightComponent() override
    {
        return &tempoControls;
    }

    int getMenuBarRightComponentWidth() const override
    {
        return StandaloneTempoControls::preferredWidth;
    }

    int getMenuBarHeight() const override
    {
        return StandaloneTempoControls::preferredHeight;
    }

    double getCurrentHostTempoBpm() const override
    {
        return tempoControls.getTempoBpm();
    }

    double getDefaultHostTempoBpm() const override
    {
        return tempoControls.getDefaultTempoBpm();
    }

    void setCurrentHostTempoBpm(double bpm) override
    {
        tempoControls.setTempoBpmFromPreset(bpm);
    }

    juce::PopupMenu getAdditionalMenuForName(
        const juce::String& menuName) override
    {
        juce::PopupMenu menu;

        if (menuName == "Audio")
            menu.addItem(commandAudioSettings,
                         "Audio Settings...");

        return menu;
    }

    void addAdditionalItemsToMenu(
        const juce::String& menuName,
        juce::PopupMenu& menu) override
    {
        if (menuName == "File")
        {
            menu.addSeparator();
            menu.addItem(commandQuit, "Quit");
            return;
        }

        if (menuName != "MIDI")
            return;

        midiInputDevices =
            juce::MidiInput::getAvailableDevices();

        juce::PopupMenu devicesMenu;

        if (midiInputDevices.isEmpty())
        {
            devicesMenu.addItem(
                commandNoMidiInputs,
                "No MIDI input devices found",
                false,
                false);
        }
        else
        {
            for (int index = 0;
                 index < midiInputDevices.size();
                 ++index)
            {
                const auto& device =
                    midiInputDevices.getReference(index);

                const bool isEnabled =
                    holder.deviceManager
                        .isMidiInputDeviceEnabled(
                            device.identifier);

                devicesMenu.addItem(
                    commandMidiInputBase + index,
                    device.name,
                    true,
                    isEnabled);
            }
        }

        menu.addSubMenu(
            "Select MIDI Devices",
            devicesMenu);

        menu.addSeparator();
    }

    bool handleAdditionalMenuCommand(
        int menuItemID) override
    {
        if (menuItemID == commandAudioSettings)
        {
            showAudioSettingsDialog();
            return true;
        }

        if (menuItemID == commandQuit)
        {
            if (auto* application =
                    juce::JUCEApplicationBase::getInstance())
            {
                application->systemRequestedQuit();
            }

            return true;
        }

        const int deviceIndex =
            menuItemID - commandMidiInputBase;

        if (deviceIndex >= 0
            && deviceIndex < midiInputDevices.size())
        {
            const auto& device =
                midiInputDevices.getReference(deviceIndex);

            const bool currentlyEnabled =
                holder.deviceManager
                    .isMidiInputDeviceEnabled(
                        device.identifier);

            holder.deviceManager
                .setMidiInputDeviceEnabled(
                    device.identifier,
                    ! currentlyEnabled);

            saveEnabledMidiInputs();
            return true;
        }

        return false;
    }

private:
    enum CommandIds
    {
        commandAudioSettings = 10001,
        commandNoMidiInputs = 10002,
        commandQuit = 10003,
        commandMidiInputBase = 10100
    };

    void showAudioSettingsDialog()
    {
        int maxNumInputs = 0;
        int maxNumOutputs = 0;

        if (holder.channelConfiguration.size() > 0)
        {
            const auto& configuration =
                holder.channelConfiguration.getReference(0);

            maxNumInputs =
                juce::jmax(
                    0,
                    static_cast<int>(
                        configuration.numIns));

            maxNumOutputs =
                juce::jmax(
                    0,
                    static_cast<int>(
                        configuration.numOuts));
        }

        if (holder.processor != nullptr)
        {
            if (auto* inputBus =
                    holder.processor->getBus(true, 0))
            {
                maxNumInputs =
                    juce::jmax(
                        0,
                        inputBus->getDefaultLayout().size());
            }

            if (auto* outputBus =
                    holder.processor->getBus(false, 0))
            {
                maxNumOutputs =
                    juce::jmax(
                        0,
                        outputBus->getDefaultLayout().size());
            }
        }

        auto content =
            std::make_unique<
                StandaloneAudioSettingsComponent>(
                    holder,
                    maxNumInputs,
                    maxNumOutputs);

        content->setSize(500, 550);
        content->setToRecommendedSize();

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(content.release());
        options.dialogTitle = "Audio Settings";
        options.dialogBackgroundColour =
            options.content->getLookAndFeel()
                .findColour(
                    juce::ResizableWindow::
                        backgroundColourId);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

    void saveEnabledMidiInputs()
    {
        juce::StringArray enabledIdentifiers;

        for (const auto& device :
             juce::MidiInput::getAvailableDevices())
        {
            if (holder.deviceManager
                    .isMidiInputDeviceEnabled(
                        device.identifier))
            {
                enabledIdentifiers
                    .addIfNotAlreadyThere(
                        device.identifier);
            }
        }

        AppSettings settings;
        settings.setEnabledMidiDeviceIdentifiers(
            enabledIdentifiers);

        holder.saveAudioDeviceState();
    }

    juce::StandalonePluginHolder& holder;
    StandaloneTempoControls tempoControls;
    juce::Array<juce::MidiDeviceInfo>
        midiInputDevices;
};

class PolyHostStandaloneWindow final : public juce::DocumentWindow
{
public:
    PolyHostStandaloneWindow(
        const juce::String& title,
        std::unique_ptr<juce::StandalonePluginHolder> pluginHolderIn)
        : juce::DocumentWindow(
              title,
              juce::LookAndFeel::getDefaultLookAndFeel().findColour(
                  juce::ResizableWindow::backgroundColourId),
              juce::DocumentWindow::allButtons),
          pluginHolder(std::move(pluginHolderIn)),
          menuExtension(*pluginHolder,
                        playHead,
                        playHeadTracker)
    {
        setUsingNativeTitleBar(true);

        processor =
            static_cast<PolyHostPluginProcessor*>(pluginHolder->processor.get());

        jassert(processor != nullptr);

        if (processor == nullptr)
        {
            setSize(800, 500);
            centreWithSize(getWidth(), getHeight());
            return;
        }

        pluginHolder->stopPlaying();

        AppSettings settings;
        playHead.setTempoBpm(settings.getDefaultTempoBpm());
        restoreMidiInputState(settings);

        if (auto* device =
                pluginHolder->deviceManager.getCurrentAudioDevice())
        {
            playHead.prepareToPlay(device->getCurrentSampleRate());
        }

        processor->setPlayHead(&playHead);
        processor->setStandaloneAudioExtension(
            &playHeadTracker);
        pluginHolder->startPlaying();

        auto* editor =
            new PolyHostPluginEditor(*processor, &menuExtension);

        for (int index = 0;
             index < editor->getNumChildComponents();
             ++index)
        {
            if (auto* childMainView =
                    dynamic_cast<MainView*>(
                        editor->getChildComponent(index)))
            {
                mainView = childMainView;
                break;
            }
        }

        constexpr int sharedMenuBarHeight = 24;
        constexpr int standaloneMenuBarHeight =
            StandaloneTempoControls::preferredHeight;

        constexpr int extraStandaloneHeight =
            standaloneMenuBarHeight - sharedMenuBarHeight;

        editor->setSize(
            editor->getWidth(),
            editor->getHeight() + extraStandaloneHeight);

        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);

        constexpr int minimumStandaloneWidth = 530;

        setResizeLimits(
            minimumStandaloneWidth,
            1,
            32768,
            32768);

        if (getWidth() < minimumStandaloneWidth)
            setSize(minimumStandaloneWidth, getHeight());

        centreWithSize(getWidth(), getHeight());
    }

    ~PolyHostStandaloneWindow() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->stopPlaying();

        if (processor != nullptr)
        {
            processor->setStandaloneAudioExtension(nullptr);
            processor->setPlayHead(nullptr);
        }

        clearContentComponent();
        pluginHolder.reset();
    }

    void closeButtonPressed() override
    {
        if (auto* application = juce::JUCEApplicationBase::getInstance())
            application->systemRequestedQuit();
    }

    void savePluginState()
    {
        if (pluginHolder != nullptr)
        {
            saveMidiInputState();
            pluginHolder->savePluginState();
            pluginHolder->saveAudioDeviceState();
        }
    }

    bool openPluginPath(const juce::String& pluginPath,
                        bool openInNewTab)
    {
        if (pluginHolder == nullptr || mainView == nullptr)
            return false;

        if (! openInNewTab)
            pluginHolder->stopPlaying();

        const bool loaded =
            mainView->openPluginPath(pluginPath,
                                     openInNewTab);

        if (! openInNewTab)
            pluginHolder->startPlaying();

        return loaded;
    }

private:
    void restoreMidiInputState(const AppSettings& settings)
    {
        if (pluginHolder == nullptr)
            return;

        const auto enabledIdentifiers =
            settings.getEnabledMidiDeviceIdentifiers();

        for (const auto& device :
             juce::MidiInput::getAvailableDevices())
        {
            pluginHolder->deviceManager.setMidiInputDeviceEnabled(
                device.identifier,
                enabledIdentifiers.contains(device.identifier));
        }
    }

    void saveMidiInputState()
    {
        if (pluginHolder == nullptr)
            return;

        juce::StringArray enabledIdentifiers;

        for (const auto& device :
             juce::MidiInput::getAvailableDevices())
        {
            if (pluginHolder->deviceManager
                    .isMidiInputDeviceEnabled(device.identifier))
            {
                enabledIdentifiers.addIfNotAlreadyThere(
                    device.identifier);
            }
        }

        AppSettings settings;
        settings.setEnabledMidiDeviceIdentifiers(
            enabledIdentifiers);
    }

    StandalonePlayHead playHead;
    StandalonePlayHeadTracker playHeadTracker { playHead };
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
    StandaloneMenuExtension menuExtension;
    PolyHostPluginProcessor* processor = nullptr;
    MainView* mainView = nullptr;
};

class PolyHostStandaloneApplication final : public juce::JUCEApplication
{
public:
    PolyHostStandaloneApplication()
    {
        AppSettings settings;

        const auto savedStandaloneState =
            settings.getAudioDeviceState().trim();

        if (savedStandaloneState.isNotEmpty())
        {
            if (auto savedStateXml =
                    juce::XmlDocument::parse(
                        savedStandaloneState))
            {
                standaloneProperties.restoreFromXml(
                    *savedStateXml);
            }
        }
    }

    const juce::String getApplicationName() override
    {
        return juce::CharPointer_UTF8(JucePlugin_Name);
    }

    const juce::String getApplicationVersion() override
    {
        return JucePlugin_VersionString;
    }

    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }

    void anotherInstanceStarted(
        const juce::String& commandLine) override
    {
        if (mainWindow == nullptr)
            return;

        mainWindow->setMinimised(false);
        mainWindow->setVisible(true);
        mainWindow->toFront(true);

        openPluginPaths(commandLine, false);
    }

    void initialise(
        const juce::String& commandLine) override
    {
        mainWindow = std::make_unique<PolyHostStandaloneWindow>(
            getApplicationName(),
            createPluginHolder());

        mainWindow->setVisible(true);
        openPluginPaths(commandLine, true);
    }

    void shutdown() override
    {
        if (mainWindow != nullptr)
            mainWindow->savePluginState();

        mainWindow.reset();

        if (auto stateXml =
                standaloneProperties.createXml(
                    "StandaloneProperties"))
        {
            AppSettings settings;

            settings.setAudioDeviceState(
                stateXml->toString());
        }
    }

    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay(
                100,
                []
                {
                    if (auto* application =
                            juce::JUCEApplicationBase::getInstance())
                    {
                        application->systemRequestedQuit();
                    }
                });
        }
        else
        {
            quit();
        }
    }

private:
    static juce::StringArray getPluginPathsFromCommandLine(
        const juce::String& commandLine)
    {
        juce::StringArray pluginPaths;

        const auto arguments =
            juce::StringArray::fromTokens(
                commandLine,
                true);

        for (auto argument : arguments)
        {
            argument = argument.trim().unquoted();

            if (argument.isEmpty())
                continue;

            const juce::File file(argument);

            const bool isAcceptedFormat =
                file.hasFileExtension(".vst3")
               #if JUCE_PLUGINHOST_VST
                || file.hasFileExtension(".dll")
               #endif
                ;

            if (isAcceptedFormat && file.exists())
            {
                pluginPaths.addIfNotAlreadyThere(
                    file.getFullPathName());
            }
        }

        return pluginPaths;
    }

    void openPluginPaths(
        const juce::String& commandLine,
        bool firstPathUsesExistingTab)
    {
        if (mainWindow == nullptr)
            return;

        const auto pluginPaths =
            getPluginPathsFromCommandLine(commandLine);

        bool openInNewTab =
            ! firstPathUsesExistingTab;

        for (const auto& pluginPath : pluginPaths)
        {
            mainWindow->openPluginPath(
                pluginPath,
                openInNewTab);

            openInNewTab = true;
        }
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts>
            channelConfiguration;

        return std::make_unique<juce::StandalonePluginHolder>(
            &standaloneProperties,
            false,
            juce::String(),
            nullptr,
            channelConfiguration,
            false);
    }

    juce::PropertySet standaloneProperties;
    std::unique_ptr<PolyHostStandaloneWindow> mainWindow;
};
}

START_JUCE_APPLICATION(PolyHostStandaloneApplication)