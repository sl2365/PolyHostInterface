#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>

class MidiKeyboardPanel final : public juce::Component,
                                private juce::Timer
{
public:
    static constexpr int preferredHeight = 112;
    static constexpr int fixedKeyWidthMode = 0;

    explicit MidiKeyboardPanel(juce::MidiKeyboardState& stateToUse);
    ~MidiKeyboardPanel() override;

    std::function<void(int)> onPitchBendChanged;
    std::function<void(int)> onModulationChanged;

    void setWidthMode(int newWidthMode);
    int getWidthMode() const noexcept { return widthMode; }
    void setNoteNamesVisible(bool shouldShow);
    bool areNoteNamesVisible() const noexcept { return noteNamesVisible; }
    void releaseAllOwnedNotes();
    void displayExternalPitchBend(int value);
    void displayExternalModulation(int value);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

private:
    enum class DragTarget
    {
        none,
        keys,
        pitchBend,
        modulation
    };

    static constexpr int midiChannel = 1;
    static constexpr int allMidiChannels = 0xffff;
    static constexpr int initialFirstNote = 24;
    static constexpr int whiteKeysPerOctave = 7;
    static constexpr int maximumWhiteKeyCount = 61;
    static constexpr float fixedWhiteKeyWidth = 20.0f;
    static constexpr int controllerAreaWidth = 72;

    juce::Rectangle<int> getControllerArea() const;
    juce::Rectangle<int> getPitchBendBounds() const;
    juce::Rectangle<int> getModulationBounds() const;
    juce::Rectangle<int> getAvailableKeysBounds() const;
    juce::Rectangle<int> getKeysBounds() const;
    int getVisibleWhiteKeyCount() const;
    float getWhiteKeyWidth() const;

    int noteForWhiteKey(int whiteKey) const;
    static bool hasBlackKeyAfter(int whiteKey);
    int getMaximumFirstNote() const;
    void scrollByOctaves(int octaveDelta);
    juce::Rectangle<float> getBlackKeyBounds(int whiteKey) const;
    int noteAtPosition(juce::Point<float> position) const;

    juce::Colour getWhiteKeyColour(int note) const;
    juce::Colour getBlackKeyColour(int note) const;
    void drawControllerWheel(juce::Graphics& graphics,
                             juce::Rectangle<int> bounds,
                             const juce::String& label,
                             float normalizedValue,
                             bool showCentreLine) const;

    void updateMouseCursor(juce::Point<int> position);
    void updateControllerFromMouse(juce::Point<float> position);
    void startMomentaryNote(int note);
    void releaseMomentaryNote();
    void latchNote(int note);
    void releaseLatchedNote(int note);
    void setPitchBendValue(int value);
    void setModulationValue(int value);
    void timerCallback() override;

    juce::MidiKeyboardState& keyboardState;
    std::array<bool, 128> latchedNotes {};
    std::array<bool, 128> momentaryNotes {};
    int momentaryNote = -1;
    int widthMode = 5;
    int firstVisibleNote = initialFirstNote;
    bool noteNamesVisible = true;
    int pitchBendValue = 8192;
    int modulationValue = 0;
    juce::uint32 pitchBendScrollResetDeadlineMs = 0;
    DragTarget dragTarget = DragTarget::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiKeyboardPanel)
};
