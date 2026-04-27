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

    addAndMakeVisible(tapTempoButton);
    tapTempoButton.onClick = [this]
    {
        registerTapTempo();
    };

    startTimerHz(10);
    refreshUi();
}

void StandaloneTempoSupport::TempoStripComponent::resized()
{
    auto area = getLocalBounds();

    auto tapBounds = area.removeFromRight(48).reduced(2);
    tapBounds = tapBounds.withSizeKeepingCentre(tapBounds.getWidth(), area.getHeight() - 4);
    tapTempoButton.setBounds(tapBounds);

    area.removeFromRight(4);

    auto resetBounds = area.removeFromRight(32).reduced(2);
    resetBounds = resetBounds.withSizeKeepingCentre(resetBounds.getWidth(), area.getHeight() - 4);
    resetTempoButton.setBounds(resetBounds);

    area.removeFromRight(4);

    auto editorBounds = area.removeFromRight(70).reduced(2);
    editorBounds = editorBounds.withSizeKeepingCentre(editorBounds.getWidth(), 22);
    tempoEditor.setBounds(editorBounds);

    area.removeFromRight(6);

    auto beatBounds = area.removeFromRight(96).reduced(2);
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
}

void StandaloneTempoSupport::TempoStripComponent::updateTooltip()
{
    tempoEditor.setTooltip("Scroll to adjust.\nShift+Scroll: Fine Adjust.");
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
    auto beatWidth = area.getWidth() / 4.0f;

    const auto bpm = owner.getHostTempoBpm();
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto beatLengthMs = 60000.0 / juce::jmax(1.0, bpm);
    const int beatIndex = (int) std::floor(nowMs / beatLengthMs) % 4;

    for (int i = 0; i < 4; ++i)
    {
        auto r = juce::Rectangle<float>(area.getX() + beatWidth * (float) i + 1.5f,
                                        area.getY() + 2.0f,
                                        beatWidth - 6.0f,
                                        area.getHeight() - 4.0f);

        const bool isActive = (i == beatIndex);

        float pulse = 1.0f;

        if (isActive)
        {
            auto beatProgress = std::fmod(nowMs, beatLengthMs) / beatLengthMs;
            pulse = 0.75f + 0.25f * (1.0f - (float) beatProgress);
        }

        auto baseColour = (i == 0) ? juce::Colours::red : juce::Colours::limegreen;
        auto fillColour = isActive ? baseColour.brighter(0.35f * pulse + 0.15f)
                                   : baseColour.darker(0.75f).withAlpha(0.22f);

        auto borderColour = isActive ? baseColour.brighter(0.55f * pulse + 0.25f)
                                     : baseColour.darker(0.5f).withAlpha(0.35f);

        auto highlightColour = isActive ? juce::Colours::white.withAlpha(0.20f + 0.12f * pulse)
                                        : juce::Colours::white.withAlpha(0.06f);

        const float cornerSize = 5.0f;

        g.setColour(fillColour);
        g.fillRoundedRectangle(r, cornerSize);

        auto highlight = r.reduced(2.0f, 2.0f).removeFromTop(r.getHeight() * 0.38f);
        g.setColour(highlightColour);
        g.fillRoundedRectangle(highlight, cornerSize * 0.75f);

        g.setColour(borderColour);
        g.drawRoundedRectangle(r, cornerSize, 1.0f);
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

