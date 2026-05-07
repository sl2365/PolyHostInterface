#pragma once
#include <JuceHeader.h>

class PointerControl
{
public:
    struct JumpPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    void setTargetScreenBounds(juce::Rectangle<int> bounds);
    void clearTarget();

    bool hasTarget() const;

    void setJumpPoints(const juce::Array<JumpPoint>& newJumpPoints,
                       juce::Rectangle<int> sourceBounds);

    void panX(int midiValue);
    void panY(int midiValue);

    void wheelAdjust(int delta);

    juce::Point<int> getCurrentScreenPosition() const;
    juce::Rectangle<int> getTargetScreenBounds() const;

private:
    void moveCursorToCurrentPosition();
    void syncToPhysicalCursorPosition();
    void updateJumpSelection();
    bool hasJumpPoints() const;

    juce::Rectangle<int> targetScreenBounds;
    juce::Array<JumpPoint> jumpPoints;
    float virtualX = 0.0f;
    float virtualY = 0.0f;
    int selectedJumpPoint = -1;
    int currentX = 0;
    int currentY = 0;
};