#include "MidiKeyboardPanel.h"

namespace
{
    constexpr std::array<int, 7> whiteNotePattern {
        0, 2, 4, 5, 7, 9, 11
    };

    constexpr std::array<bool, 7> blackKeyAfterWhitePattern {
        true, true, false, true, true, true, false
    };
}

MidiKeyboardPanel::MidiKeyboardPanel(juce::MidiKeyboardState& stateToUse)
    : keyboardState(stateToUse)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
    startTimerHz(30);
}

MidiKeyboardPanel::~MidiKeyboardPanel()
{
    stopTimer();
    releaseAllOwnedNotes();

    if (pitchBendValue != 8192)
        setPitchBendValue(8192);
}

void MidiKeyboardPanel::setWidthMode(int newWidthMode)
{
    if (newWidthMode != fixedKeyWidthMode
        && (newWidthMode < 3 || newWidthMode > 8))
    {
        newWidthMode = 5;
    }

    if (widthMode == newWidthMode)
        return;

    releaseAllOwnedNotes();
    widthMode = newWidthMode;
    repaint();
}

void MidiKeyboardPanel::releaseAllOwnedNotes()
{
    releaseMomentaryNote();

    for (int note = 0; note < static_cast<int>(latchedNotes.size()); ++note)
    {
        if (! latchedNotes[static_cast<size_t>(note)])
            continue;

        latchedNotes[static_cast<size_t>(note)] = false;
        keyboardState.noteOff(midiChannel, note, 0.0f);
    }

    repaint();
}

void MidiKeyboardPanel::paint(juce::Graphics& graphics)
{
    auto panelBounds = getLocalBounds().toFloat();

    graphics.setColour(juce::Colour(0xff1d2230));
    graphics.fillRoundedRectangle(panelBounds, 5.0f);
    graphics.setColour(juce::Colour(0xff3e556f));
    graphics.drawRoundedRectangle(panelBounds.reduced(0.5f), 5.0f, 1.0f);

    drawControllerWheel(graphics,
                        getPitchBendBounds(),
                        "PITCH",
                        static_cast<float>(pitchBendValue) / 16383.0f,
                        true);
    drawControllerWheel(graphics,
                        getModulationBounds(),
                        "MOD",
                        static_cast<float>(modulationValue) / 127.0f,
                        false);

    const auto keys = getKeysBounds();
    const int whiteKeyCount = getVisibleWhiteKeyCount();
    const float whiteWidth = getWhiteKeyWidth();

    if (keys.isEmpty() || whiteKeyCount <= 0 || whiteWidth <= 0.0f)
        return;

    for (int key = 0; key < whiteKeyCount; ++key)
    {
        const int note = noteForWhiteKey(key);
        const float x = static_cast<float>(keys.getX())
                      + whiteWidth * static_cast<float>(key);
        const auto keyBounds = juce::Rectangle<float> {
            x,
            static_cast<float>(keys.getY()),
            whiteWidth,
            static_cast<float>(keys.getHeight())
        };

        const auto keyColour = getWhiteKeyColour(note);
        juce::ColourGradient whiteKeyGradient(
            keyColour.brighter(0.035f),
            keyBounds.getX(),
            keyBounds.getY(),
            keyColour.darker(0.025f),
            keyBounds.getX(),
            keyBounds.getBottom(),
            false);

        graphics.setGradientFill(whiteKeyGradient);
        graphics.fillRect(keyBounds);
        graphics.setColour(juce::Colour(0xff25272a));
        graphics.drawRect(keyBounds, 0.8f);
    }

    for (int key = 0; key + 1 < whiteKeyCount; ++key)
    {
        if (! hasBlackKeyAfter(key))
            continue;

        const int note = noteForWhiteKey(key) + 1;
        const auto keyBounds = getBlackKeyBounds(key);
        const auto keyColour = getBlackKeyColour(note);

        graphics.setColour(juce::Colours::black.withAlpha(0.24f));
        graphics.fillRoundedRectangle(keyBounds.translated(0.7f, 1.1f), 1.5f);

        juce::ColourGradient blackKeyGradient(
            keyColour.darker(0.62f),
            keyBounds.getX(),
            keyBounds.getY(),
            keyColour.darker(0.48f),
            keyBounds.getRight(),
            keyBounds.getY(),
            false);
        blackKeyGradient.addColour(0.50, keyColour.brighter(0.72f));

        graphics.setGradientFill(blackKeyGradient);
        graphics.fillRoundedRectangle(keyBounds, 1.5f);
        graphics.setColour(juce::Colour(0xff111126));
        graphics.drawRoundedRectangle(keyBounds.reduced(0.45f), 1.5f, 0.9f);
    }
}

void MidiKeyboardPanel::mouseDown(const juce::MouseEvent& event)
{
    updateMouseCursor(event.getPosition());

    if (event.mods.isLeftButtonDown()
        && getPitchBendBounds().contains(event.getPosition()))
    {
        dragTarget = DragTarget::pitchBend;
        updateControllerFromMouse(event.position);
        return;
    }

    if (event.mods.isLeftButtonDown()
        && getModulationBounds().contains(event.getPosition()))
    {
        dragTarget = DragTarget::modulation;
        updateControllerFromMouse(event.position);
        return;
    }

    const int note = noteAtPosition(event.position);

    if (note < 0)
        return;

    if (latchedNotes[static_cast<size_t>(note)])
    {
        releaseLatchedNote(note);
        return;
    }

    if (event.mods.isRightButtonDown())
    {
        latchNote(note);
        return;
    }

    if (event.mods.isLeftButtonDown())
    {
        dragTarget = DragTarget::keys;
        startMomentaryNote(note);
    }
}

void MidiKeyboardPanel::mouseDrag(const juce::MouseEvent& event)
{
    updateMouseCursor(event.getPosition());

    if (dragTarget == DragTarget::pitchBend
        || dragTarget == DragTarget::modulation)
    {
        updateControllerFromMouse(event.position);
        return;
    }

    if (dragTarget != DragTarget::keys
        || ! event.mods.isLeftButtonDown()
        || momentaryNote < 0)
    {
        return;
    }

    const int note = noteAtPosition(event.position);

    if (note == momentaryNote)
        return;

    releaseMomentaryNote();

    if (note >= 0 && ! latchedNotes[static_cast<size_t>(note)])
        startMomentaryNote(note);
}

void MidiKeyboardPanel::mouseUp(const juce::MouseEvent& event)
{
    if (dragTarget == DragTarget::keys)
        releaseMomentaryNote();
    else if (dragTarget == DragTarget::pitchBend)
        setPitchBendValue(8192);

    dragTarget = DragTarget::none;
    updateMouseCursor(event.getPosition());
}

void MidiKeyboardPanel::mouseMove(const juce::MouseEvent& event)
{
    updateMouseCursor(event.getPosition());
}

void MidiKeyboardPanel::mouseEnter(const juce::MouseEvent& event)
{
    updateMouseCursor(event.getPosition());
}

void MidiKeyboardPanel::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

juce::Rectangle<int> MidiKeyboardPanel::getControllerArea() const
{
    auto area = getLocalBounds().reduced(5);
    return area.removeFromLeft(juce::jmin(controllerAreaWidth, area.getWidth()));
}

juce::Rectangle<int> MidiKeyboardPanel::getPitchBendBounds() const
{
    auto area = getControllerArea();
    area.removeFromRight(4);
    return area.removeFromLeft(area.getWidth() / 2);
}

juce::Rectangle<int> MidiKeyboardPanel::getModulationBounds() const
{
    auto area = getControllerArea();
    area.removeFromLeft(area.getWidth() / 2 + 2);
    return area;
}

juce::Rectangle<int> MidiKeyboardPanel::getAvailableKeysBounds() const
{
    auto area = getLocalBounds().reduced(5);
    area.removeFromLeft(juce::jmin(controllerAreaWidth, area.getWidth()));
    return area;
}

juce::Rectangle<int> MidiKeyboardPanel::getKeysBounds() const
{
    auto keys = getAvailableKeysBounds();

    if (widthMode == fixedKeyWidthMode)
    {
        const int fixedWidth = juce::roundToInt(
            static_cast<float>(getVisibleWhiteKeyCount()) * fixedWhiteKeyWidth);
        keys.setWidth(juce::jmin(keys.getWidth(), fixedWidth));
    }

    return keys;
}

int MidiKeyboardPanel::getVisibleWhiteKeyCount() const
{
    if (widthMode >= 3 && widthMode <= 8)
        return widthMode * whiteKeysPerOctave;

    return juce::jlimit(
        1,
        maximumWhiteKeyCount,
        static_cast<int>(static_cast<float>(getAvailableKeysBounds().getWidth())
                         / fixedWhiteKeyWidth));
}

float MidiKeyboardPanel::getWhiteKeyWidth() const
{
    if (widthMode == fixedKeyWidthMode)
        return fixedWhiteKeyWidth;

    const int whiteKeyCount = getVisibleWhiteKeyCount();
    return whiteKeyCount > 0
               ? static_cast<float>(getAvailableKeysBounds().getWidth())
                     / static_cast<float>(whiteKeyCount)
               : 0.0f;
}

int MidiKeyboardPanel::noteForWhiteKey(int whiteKey)
{
    return firstNote
         + (whiteKey / whiteKeysPerOctave) * 12
         + whiteNotePattern[static_cast<size_t>(whiteKey % whiteKeysPerOctave)];
}

bool MidiKeyboardPanel::hasBlackKeyAfter(int whiteKey)
{
    return blackKeyAfterWhitePattern[
        static_cast<size_t>(whiteKey % whiteKeysPerOctave)];
}

juce::Rectangle<float> MidiKeyboardPanel::getBlackKeyBounds(int whiteKey) const
{
    const auto keys = getKeysBounds();
    const float whiteWidth = getWhiteKeyWidth();
    const float x = static_cast<float>(keys.getX())
                  + whiteWidth * static_cast<float>(whiteKey + 1)
                  - whiteWidth * 0.31f;

    return {
        x,
        static_cast<float>(keys.getY()),
        whiteWidth * 0.62f,
        static_cast<float>(keys.getHeight()) * 0.62f
    };
}

int MidiKeyboardPanel::noteAtPosition(juce::Point<float> position) const
{
    const auto keys = getKeysBounds();

    if (! keys.toFloat().contains(position))
        return -1;

    const int whiteKeyCount = getVisibleWhiteKeyCount();

    for (int key = 0; key + 1 < whiteKeyCount; ++key)
    {
        if (hasBlackKeyAfter(key)
            && getBlackKeyBounds(key).contains(position))
        {
            return noteForWhiteKey(key) + 1;
        }
    }

    const float whiteWidth = getWhiteKeyWidth();
    const int whiteKey = juce::jlimit(
        0,
        whiteKeyCount - 1,
        static_cast<int>((position.x - static_cast<float>(keys.getX()))
                         / juce::jmax(1.0f, whiteWidth)));

    return noteForWhiteKey(whiteKey);
}

juce::Colour MidiKeyboardPanel::getWhiteKeyColour(int note) const
{
    if (latchedNotes[static_cast<size_t>(note)])
        return juce::Colour(0xffff9fcf);

    if (momentaryNotes[static_cast<size_t>(note)]
        || keyboardState.isNoteOnForChannels(allMidiChannels, note))
    {
        return juce::Colour(0xffffd2e8);
    }

    return juce::Colour(0xffffffe3);
}

juce::Colour MidiKeyboardPanel::getBlackKeyColour(int note) const
{
    if (latchedNotes[static_cast<size_t>(note)])
        return juce::Colour(0xffa83462);

    if (momentaryNotes[static_cast<size_t>(note)]
        || keyboardState.isNoteOnForChannels(allMidiChannels, note))
    {
        return juce::Colour(0xffcf6290);
    }

    return juce::Colour(0xff24223f);
}

void MidiKeyboardPanel::drawControllerWheel(
    juce::Graphics& graphics,
    juce::Rectangle<int> bounds,
    const juce::String& label,
    float normalizedValue,
    bool showCentreLine) const
{
    auto labelBounds = bounds.removeFromTop(15);
    graphics.setColour(juce::Colour(0xffd8dee9));
    graphics.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    graphics.drawFittedText(label,
                            labelBounds,
                            juce::Justification::centred,
                            1);

    auto wheelBounds = bounds.reduced(6, 3).toFloat();
    graphics.setColour(juce::Colour(0xff10141e));
    graphics.fillRoundedRectangle(wheelBounds, 4.0f);
    graphics.setColour(juce::Colour(0xff4f5f78));
    graphics.drawRoundedRectangle(wheelBounds.reduced(0.5f), 4.0f, 1.0f);

    auto slot = wheelBounds.withSizeKeepingCentre(5.0f,
                                                   wheelBounds.getHeight() - 10.0f);
    graphics.setColour(juce::Colour(0xff080a10));
    graphics.fillRoundedRectangle(slot, 2.5f);

    if (showCentreLine)
    {
        graphics.setColour(juce::Colour(0xff8090aa).withAlpha(0.60f));
        graphics.drawHorizontalLine(juce::roundToInt(slot.getCentreY()),
                                    slot.getX() - 4.0f,
                                    slot.getRight() + 4.0f);
    }

    normalizedValue = juce::jlimit(0.0f, 1.0f, normalizedValue);
    const float thumbY = slot.getBottom()
                       - normalizedValue * slot.getHeight();
    auto thumb = juce::Rectangle<float>(wheelBounds.getX() + 3.0f,
                                        thumbY - 4.0f,
                                        wheelBounds.getWidth() - 6.0f,
                                        8.0f);

    graphics.setColour(juce::Colour(0xffcf6290));
    graphics.fillRoundedRectangle(thumb, 2.0f);
    graphics.setColour(juce::Colour(0xfff3b4d1));
    graphics.drawRoundedRectangle(thumb.reduced(0.5f), 2.0f, 1.0f);
}

void MidiKeyboardPanel::updateMouseCursor(juce::Point<int> position)
{
    const bool interactive = getPitchBendBounds().contains(position)
                          || getModulationBounds().contains(position)
                          || getKeysBounds().contains(position);

    setMouseCursor(interactive
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
}

void MidiKeyboardPanel::updateControllerFromMouse(juce::Point<float> position)
{
    auto bounds = dragTarget == DragTarget::pitchBend
                      ? getPitchBendBounds()
                      : getModulationBounds();
    bounds.removeFromTop(15);
    const auto wheelBounds = bounds.reduced(6, 3).toFloat();
    const float normalized = juce::jlimit(
        0.0f,
        1.0f,
        (wheelBounds.getBottom() - position.y)
            / juce::jmax(1.0f, wheelBounds.getHeight()));

    if (dragTarget == DragTarget::pitchBend)
        setPitchBendValue(juce::roundToInt(normalized * 16383.0f));
    else if (dragTarget == DragTarget::modulation)
        setModulationValue(juce::roundToInt(normalized * 127.0f));
}

void MidiKeyboardPanel::startMomentaryNote(int note)
{
    if (momentaryNote == note
        || latchedNotes[static_cast<size_t>(note)])
    {
        return;
    }

    releaseMomentaryNote();
    momentaryNote = note;
    momentaryNotes[static_cast<size_t>(note)] = true;
    keyboardState.noteOn(midiChannel, note, 0.9f);
    repaint();
}

void MidiKeyboardPanel::releaseMomentaryNote()
{
    if (momentaryNote < 0)
        return;

    momentaryNotes[static_cast<size_t>(momentaryNote)] = false;
    keyboardState.noteOff(midiChannel, momentaryNote, 0.0f);
    momentaryNote = -1;
    repaint();
}

void MidiKeyboardPanel::latchNote(int note)
{
    latchedNotes[static_cast<size_t>(note)] = true;
    keyboardState.noteOn(midiChannel, note, 0.9f);
    repaint();
}

void MidiKeyboardPanel::releaseLatchedNote(int note)
{
    latchedNotes[static_cast<size_t>(note)] = false;
    keyboardState.noteOff(midiChannel, note, 0.0f);
    repaint();
}

void MidiKeyboardPanel::setPitchBendValue(int value)
{
    value = juce::jlimit(0, 16383, value);

    if (pitchBendValue == value)
        return;

    pitchBendValue = value;

    if (onPitchBendChanged)
        onPitchBendChanged(value);

    repaint();
}

void MidiKeyboardPanel::setModulationValue(int value)
{
    value = juce::jlimit(0, 127, value);

    if (modulationValue == value)
        return;

    modulationValue = value;

    if (onModulationChanged)
        onModulationChanged(value);

    repaint();
}

void MidiKeyboardPanel::timerCallback()
{
    repaint();
}
