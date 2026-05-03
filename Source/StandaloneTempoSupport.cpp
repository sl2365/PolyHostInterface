#include "StandaloneTempoSupport.h"
#include <cmath>

StandaloneTempoSupport::StandaloneTempoSupport(AudioEngine& audioEngineIn)
    : audioEngine(audioEngineIn)
{
}

const StandaloneTempoState& StandaloneTempoSupport::getState() const
{
    return state;
}

void StandaloneTempoSupport::initialise(juce::AudioDeviceManager& deviceManager)
{
    if (trackerInitialised)
        return;

    deviceManager.addAudioCallback(&playbackTracker);
    trackerInitialised = true;
}

void StandaloneTempoSupport::shutdown(juce::AudioDeviceManager& deviceManager)
{
    if (!trackerInitialised)
        return;

    deviceManager.removeAudioCallback(&playbackTracker);
    trackerInitialised = false;
}

void StandaloneTempoSupport::setDefaultTempoBpm(double bpm)
{
    state.defaultTempoBpm = juce::jlimit(20.0, 300.0, bpm);
}

void StandaloneTempoSupport::pushEffectiveTempoToEngine()
{
    hostPlayHead.setTempoBpm(state.hostTempoBpm);
}

void StandaloneTempoSupport::setHostTempoBpm(double bpm)
{
    bpm = juce::jlimit(20.0, 300.0, bpm);
    bpm = std::round(bpm * 10.0) / 10.0;

    if (state.hostTempoBpm == bpm)
        return;

    state.hostTempoBpm = bpm;
    pushEffectiveTempoToEngine();

    if (onTempoChanged)
        onTempoChanged();
}

double StandaloneTempoSupport::getHostTempoBpm() const
{
    return state.hostTempoBpm;
}

double StandaloneTempoSupport::getDefaultTempoBpm() const
{
    return state.defaultTempoBpm;
}

void StandaloneTempoSupport::registerTapTempo()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if (!tapTimesMs.isEmpty())
    {
        const double gapSinceLastTap = nowMs - tapTimesMs.getLast();

        if (gapSinceLastTap > 2000.0)
            tapTimesMs.clear();
    }

    tapTimesMs.add(nowMs);

    while (tapTimesMs.size() > 6)
        tapTimesMs.remove(0);

    if (tapTimesMs.size() < 2)
        return;

    juce::Array<double> validIntervalsMs;

    for (int i = 1; i < tapTimesMs.size(); ++i)
    {
        const double intervalMs = tapTimesMs[i] - tapTimesMs[i - 1];

        if (intervalMs >= 200.0 && intervalMs <= 3000.0)
            validIntervalsMs.add(intervalMs);
    }

    if (validIntervalsMs.isEmpty())
        return;

    double totalMs = 0.0;

    for (auto interval : validIntervalsMs)
        totalMs += interval;

    const double averageIntervalMs = totalMs / (double) validIntervalsMs.size();
    const double bpm = 60000.0 / averageIntervalMs;

    setHostTempoBpm(bpm);
}

double StandaloneTempoSupport::getDisplayedTempoBpm() const
{
    return state.hostTempoBpm;
}

bool StandaloneTempoSupport::isTempoEditorInteractive() const
{
    return true;
}

void StandaloneTempoSupport::setWrappedAudioCallback(juce::AudioIODeviceCallback* callback)
{
    playbackTracker.setWrappedCallback(callback);
}

StandaloneTempoSupport::TempoStripComponent::TempoStripComponent(StandaloneTempoSupport& ownerIn)
    : owner(ownerIn), beatIndicator(ownerIn), tempoEditor(*this)
{
    addAndMakeVisible(beatIndicator);

    addAndMakeVisible(tempoEditor);
    tempoEditor.setInputRestrictions(6, "0123456789.");
    tempoEditor.setJustification(juce::Justification::centred);
    tempoEditor.setSelectAllWhenFocused(true);
    tempoEditor.onReturnKey = [this]
    {
        commitTempoFromEditor();
        tempoEditor.unfocusAllComponents();
    };

    addAndMakeVisible(resetTempoButton);
    resetTempoButton.setTooltip("Reset tempo to " + juce::String(owner.getDefaultTempoBpm(), 1) + ".");
    resetTempoButton.onClick = [this]
    {
        resetTempo();
    };

    addAndMakeVisible(metronomeButton);
    metronomeButton.onClick = [this]
    {
        owner.toggleMetronome();
        metronomeButton.repaint();
    };

    addAndMakeVisible(tapTempoButton);
    tapTempoButton.setLookAndFeel(&tapButtonLookAndFeel);
    tapTempoButton.setColour(juce::TextButton::buttonColourId, ButtonStyling::defaultBackground());
    tapTempoButton.setColour(juce::TextButton::buttonOnColourId, ButtonStyling::defaultBackground());
    tapTempoButton.onClick = [this]
    {
        registerTapTempo();
    };

    startTimerHz(10);
    refreshUi();
}

StandaloneTempoSupport::TempoStripComponent::~TempoStripComponent()
{
    tapTempoButton.setLookAndFeel(nullptr);
}

void StandaloneTempoSupport::TempoStripComponent::resized()
{
    auto area = getLocalBounds();
    const int buttonHeight = ButtonStyling::defaultButtonHeight();

    auto tapBounds = area.removeFromRight(40).reduced(0);
    tapBounds = tapBounds.withSizeKeepingCentre(tapBounds.getWidth(), buttonHeight);
    tapTempoButton.setBounds(tapBounds);

    area.removeFromRight(2);

    auto metronomeBounds = area.removeFromRight(30).reduced(0);
    metronomeBounds = metronomeBounds.withSizeKeepingCentre(metronomeBounds.getWidth(), buttonHeight);
    metronomeButton.setBounds(metronomeBounds);

    area.removeFromRight(1);

    auto resetBounds = area.removeFromRight(30).reduced(0);
    resetBounds = resetBounds.withSizeKeepingCentre(resetBounds.getWidth(), buttonHeight);
    resetTempoButton.setBounds(resetBounds);

    area.removeFromRight(2);

    auto editorBounds = area.removeFromRight(55).reduced(1, 2);
    editorBounds = editorBounds.withSizeKeepingCentre(editorBounds.getWidth(), 22);
    tempoEditor.setBounds(editorBounds);

    area.removeFromRight(0);

    auto beatBounds = area.removeFromRight(85).reduced(2);
    beatBounds = beatBounds.withSizeKeepingCentre(beatBounds.getWidth(), 18);
    beatIndicator.setBounds(beatBounds);
}

void StandaloneTempoSupport::TempoStripComponent::timerCallback()
{
    juce::ignoreUnused(owner);
}

void StandaloneTempoSupport::TempoStripComponent::resetTempo()
{
    setTempoBpm(owner.getDefaultTempoBpm());
}

void StandaloneTempoSupport::TempoStripComponent::setTempoBpm(double bpm)
{
    owner.setHostTempoBpm(bpm);
    refreshUi();
}

void StandaloneTempoSupport::TempoStripComponent::registerTapTempo()
{
    owner.registerTapTempo();
    refreshUi();
}

void StandaloneTempoSupport::TempoStripComponent::commitTempoFromEditor()
{
    auto text = tempoEditor.getText().trim();

    if (text.isEmpty())
    {
        refreshUi();
        return;
    }

    if (text.containsOnly("0123456789.") == false
        || (text.containsChar('.')
            && text.fromFirstOccurrenceOf(".", false, false).containsChar('.')))
    {
        refreshUi();
        return;
    }

    auto parsed = text.getDoubleValue();

    if (parsed <= 0.0)
    {
        refreshUi();
        return;
    }

    setTempoBpm(parsed);
}

void StandaloneTempoSupport::TempoStripComponent::refreshUi()
{
    updateControlState();
    updateTooltip();
    tempoEditor.setText(juce::String(owner.getDisplayedTempoBpm(), 1),
                        juce::dontSendNotification);
    metronomeButton.repaint();
}

void StandaloneTempoSupport::TempoStripComponent::updateTooltip()
{
    tempoEditor.setTooltip("Scroll to adjust.\nShift+Scroll: Fine Adjust.\nRight-click: Set as Default.");
    resetTempoButton.setTooltip("Reset tempo to " + juce::String(owner.getDefaultTempoBpm(), 1) + ".");
}

void StandaloneTempoSupport::TempoStripComponent::updateControlState()
{
    const bool editable = owner.isTempoEditorInteractive();

    tempoEditor.setEnabled(editable);
    resetTempoButton.setEnabled(editable);
    tapTempoButton.setEnabled(editable);
}

void StandaloneTempoSupport::TempoStripComponent::TempoTextEditor::mouseWheelMove(
    const juce::MouseEvent& event,
    const juce::MouseWheelDetails& wheel)
{
    if (! owner.owner.isTempoEditorInteractive())
    {
        juce::TextEditor::mouseWheelMove(event, wheel);
        return;
    }

    if (wheel.deltaY == 0.0f)
    {
        juce::TextEditor::mouseWheelMove(event, wheel);
        return;
    }

    const bool fineAdjust = event.mods.isShiftDown();
    const double step = fineAdjust ? 0.1 : 1.0;
    const double direction = (wheel.deltaY > 0.0f) ? 1.0 : -1.0;

    owner.setTempoBpm(owner.owner.getHostTempoBpm() + direction * step);
}

void StandaloneTempoSupport::TempoStripComponent::TempoTextEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (! owner.owner.isTempoEditorInteractive())
        return;

    owner.setTempoBpm(owner.owner.getDefaultTempoBpm());
}

void StandaloneTempoSupport::TempoStripComponent::TempoTextEditor::addPopupMenuItems(
    juce::PopupMenu& menu,
    const juce::MouseEvent* mouseClickEvent)
{
    juce::TextEditor::addPopupMenuItems(menu, mouseClickEvent);

    menu.addSeparator();
    menu.addItem(1001, "Set as Default");
}

void StandaloneTempoSupport::TempoStripComponent::TempoTextEditor::performPopupMenuAction(int menuItemID)
{
    if (menuItemID == 1001)
    {
        if (owner.owner.onSetCurrentTempoAsDefaultRequested)
            owner.owner.onSetCurrentTempoAsDefaultRequested(owner.owner.getHostTempoBpm());

        return;
    }

    juce::TextEditor::performPopupMenuAction(menuItemID);
}

bool StandaloneTempoSupport::TempoStripComponent::TempoTextEditor::keyPressed(const juce::KeyPress& key)
{
    if (! owner.owner.isTempoEditorInteractive())
        return juce::TextEditor::keyPressed(key);

    if (key == juce::KeyPress(juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0))
    {
        owner.setTempoBpm(owner.owner.getHostTempoBpm() + 0.1);
        return true;
    }

    if (key == juce::KeyPress(juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier, 0))
    {
        owner.setTempoBpm(owner.owner.getHostTempoBpm() - 0.1);
        return true;
    }

    if (key == juce::KeyPress::upKey)
    {
        owner.setTempoBpm(owner.owner.getHostTempoBpm() + 1.0);
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        owner.setTempoBpm(owner.owner.getHostTempoBpm() - 1.0);
        return true;
    }

    return juce::TextEditor::keyPressed(key);
}

void StandaloneTempoSupport::TempoStripComponent::TempoTextEditor::focusLost(FocusChangeType cause)
{
    juce::TextEditor::focusLost(cause);
    owner.commitTempoFromEditor();
}

StandaloneTempoSupport::TempoStripComponent::BeatIndicatorComponent::BeatIndicatorComponent(StandaloneTempoSupport& ownerIn)
    : owner(ownerIn)
{
    startTimerHz(30);
}

void StandaloneTempoSupport::TempoStripComponent::BeatIndicatorComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(2.0f);

    const int beatIndex = owner.getCurrentBeatInBar();
    const auto beatProgress = owner.getCurrentBeatProgress();

    const float ledDiameter = 11.0f;  // This adjusts LED size
    const float spacing = 7.0f;
    const float totalWidth = (ledDiameter * 4.0f) + (spacing * 3.0f);
    const float startX = area.getCentreX() - (totalWidth * 0.5f);
    const float y = area.getCentreY() - (ledDiameter * 0.5f);

    for (int i = 0; i < 4; ++i)
    {
        auto r = juce::Rectangle<float>(startX + (ledDiameter + spacing) * (float) i,
                                        y,
                                        ledDiameter,
                                        ledDiameter);

        const bool isActive = (i == beatIndex);

        float pulse = 1.0f;

        if (isActive)
            pulse = 0.75f + 0.25f * (1.0f - (float) beatProgress);

        auto baseColour = (i == 0) ? juce::Colours::red : juce::Colours::limegreen;
        auto fillColour = isActive ? baseColour.brighter(0.35f * pulse + 0.15f)
                                   : baseColour.darker(0.75f).withAlpha(0.22f);

        auto borderColour = isActive ? baseColour.brighter(0.55f * pulse + 0.25f)
                                     : baseColour.darker(0.5f).withAlpha(0.35f);

        auto highlightColour = isActive ? juce::Colours::white.withAlpha(0.20f + 0.12f * pulse)
                                        : juce::Colours::white.withAlpha(0.06f);

        g.setColour(fillColour);
        g.fillEllipse(r);

        auto highlight = r.reduced(2.0f, 2.0f).removeFromTop(r.getHeight() * 0.42f);
        g.setColour(highlightColour);
        g.fillEllipse(highlight);

        g.setColour(borderColour);
        g.drawEllipse(r, 1.0f);
    }
}

void StandaloneTempoSupport::TempoStripComponent::BeatIndicatorComponent::timerCallback()
{
    repaint();
}

std::unique_ptr<juce::Component> StandaloneTempoSupport::createTempoStripComponent()
{
    auto strip = std::make_unique<TempoStripComponent>(*this);
    tempoStripComponent = strip.get();
    return strip;
}

void StandaloneTempoSupport::refreshTempoStrip()
{
    if (tempoStripComponent != nullptr)
        tempoStripComponent->refreshUi();
}

void StandaloneTempoSupport::setMetronomeEnabled(bool shouldBeEnabled)
{
    if (state.metronomeEnabled == shouldBeEnabled)
        return;

    state.metronomeEnabled = shouldBeEnabled;
    refreshTempoStrip();
}

bool StandaloneTempoSupport::isMetronomeEnabled() const
{
    return state.metronomeEnabled;
}

void StandaloneTempoSupport::toggleMetronome()
{
    setMetronomeEnabled(!state.metronomeEnabled);
}

void StandaloneTempoSupport::PlaybackTracker::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    if (wrappedCallback != nullptr)
    {
        wrappedCallback->audioDeviceIOCallbackWithContext(inputChannelData,
                                                          numInputChannels,
                                                          outputChannelData,
                                                          numOutputChannels,
                                                          numSamples,
                                                          context);
    }

    if (state.metronomeEnabled && outputChannelData != nullptr && numOutputChannels > 0)
    {
        renderMetronome(outputChannelData,
                        numOutputChannels,
                        numSamples,
                        currentSampleRate);
    }

    processedSamples += numSamples;
    playHead.advance(numSamples);
}

void StandaloneTempoSupport::PlaybackTracker::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (wrappedCallback != nullptr)
        wrappedCallback->audioDeviceAboutToStart(device);

    currentSampleRate = 44100.0;

    if (device != nullptr)
    {
        currentSampleRate = device->getCurrentSampleRate();
        playHead.prepareToPlay(currentSampleRate);
    }

    processedSamples = 0;
    samplesRemainingInClick = 0;
    clickPhase = 0.0;
    beatIndex = 0;
}

void StandaloneTempoSupport::PlaybackTracker::audioDeviceStopped()
{
    if (wrappedCallback != nullptr)
        wrappedCallback->audioDeviceStopped();
}

void StandaloneTempoSupport::PlaybackTracker::renderMetronome(float* const* outputChannelData,
                                                              int numOutputChannels,
                                                              int numSamples,
                                                              double sampleRate)
{
    if (sampleRate <= 0.0)
        return;

    const double bpm = juce::jmax(1.0, state.hostTempoBpm);
    const double samplesPerBeat = (60.0 / bpm) * sampleRate;
    const int clickLengthSamples = (int) (0.050 * sampleRate);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto absoluteSample = processedSamples + sample;
        const auto previousBeat = (int) std::floor((double) juce::jmax<juce::int64>(0, absoluteSample - 1) / samplesPerBeat);
        const auto currentBeat  = (int) std::floor((double) absoluteSample / samplesPerBeat);

        if (absoluteSample == 0 || currentBeat != previousBeat)
        {
            beatIndex = ((currentBeat % 4) + 4) % 4;
            samplesRemainingInClick = clickLengthSamples;
            clickPhase = 0.0;
            clickFrequencyHz = (beatIndex == 0) ? 1760.0 : 1320.0;
        }

        float clickSample = 0.0f;

        if (samplesRemainingInClick > 0)
        {
            const float env = (float) samplesRemainingInClick / (float) clickLengthSamples;
            clickSample = std::sin((float) clickPhase) * 0.28f * env * env;

            clickPhase += juce::MathConstants<double>::twoPi * clickFrequencyHz / sampleRate;
            --samplesRemainingInClick;
        }

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                outputChannelData[ch][sample] += clickSample;
        }
    }
}

int StandaloneTempoSupport::getCurrentBeatInBar() const
{
    auto position = hostPlayHead.getPosition();

    if (!position.hasValue())
        return 0;

    const auto ppq = position->getPpqPosition().orFallback(0.0);
    const int beatIndex = ((int) std::floor(ppq)) % 4;
    return (beatIndex + 4) % 4;
}

double StandaloneTempoSupport::getCurrentBeatProgress() const
{
    auto position = hostPlayHead.getPosition();

    if (!position.hasValue())
        return 0.0;

    const auto ppq = position->getPpqPosition().orFallback(0.0);
    const double frac = ppq - std::floor(ppq);
    return juce::jlimit(0.0, 1.0, frac);
}

