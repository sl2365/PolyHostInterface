#pragma once
#include <JuceHeader.h>

class PointerControl
{
public:
    void setTargetScreenBounds(juce::Rectangle<int> bounds);
    void clearTarget();

    bool hasTarget() const;

    void panX(int midiValue);
    void panY(int midiValue);

    void wheelAdjust(int delta);

    juce::Point<int> getCurrentScreenPosition() const;
    juce::Rectangle<int> getTargetScreenBounds() const;

private:
    void moveCursorToCurrentPosition();
    void syncToPhysicalCursorPosition();

    juce::Rectangle<int> targetScreenBounds;
    int currentX = 0;
    int currentY = 0;
};