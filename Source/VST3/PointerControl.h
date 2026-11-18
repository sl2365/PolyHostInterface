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

    enum class MouseButton
    {
        left,
        middle,
        right
    };

    enum class KeyboardKey
    {
        cursorUp,
        cursorDown,
        enter
    };

    void setTargetScreenBounds(juce::Rectangle<int> bounds);
    void clearTarget();

    bool hasTarget() const;

    void setSnapWeights(float newXWeight, float newYWeight);
    float getXSnapWeight() const;
    float getYSnapWeight() const;

    void setLaneTolerance(float newTolerance);
    float getLaneTolerance() const;

    void setJumpPoints(const juce::Array<JumpPoint>& newJumpPoints,
                       juce::Rectangle<int> sourceBounds);
    void setFreeZones(const juce::Array<juce::Rectangle<float>>& sourceFreeZones,
                      juce::Rectangle<int> sourceBounds);
    void setFreeZone(bool shouldHaveFreeZone,
                     juce::Rectangle<float> sourceFreeZone,
                     juce::Rectangle<int> sourceBounds);

    void panX(int midiValue);
    void panY(int midiValue);
    void panXRelative(int delta);
    void panYRelative(int delta);

    void setMouseButtonState(MouseButton button, bool shouldBeDown);
    void setKeyboardKeyState(KeyboardKey key, bool shouldBeDown);

    void wheelAdjust(int delta);
    void dragAdjust(int delta);
    void endDrag();
    bool isDragActive() const;
    void handleExternalMouseMove();
    void releaseDragIfIdle(double idleThresholdMs);

    juce::Point<int> getCurrentScreenPosition() const;
    juce::Rectangle<int> getTargetScreenBounds() const;

private:
    void moveCursorToCurrentPosition();
    void syncToPhysicalCursorPosition();
    void updateJumpSelection();
    int findNearestJumpPointInCurrentLane() const;
    bool hasJumpPoints() const;
    bool isPointInFreeZone(float x, float y) const;
    int findFreeZoneForCurrentAxis() const;
    bool shouldUseFreeZoneForCurrentAxis() const;
    bool isCurrentVirtualPositionInFreeZone() const;
    juce::Point<float> getFreeZoneAdjustedVirtualPosition() const;
    void moveFreelyToVirtualPosition();
    void beginDrag();
    void moveDragBy(int deltaY);
    void restoreCursorIfHidden();
    void releaseMouseButtons();
    void releaseKeyboardKeys();

    enum class LastMoveAxis
    {
        none,
        x,
        y
    };

    juce::Rectangle<int> targetScreenBounds;
    juce::Array<JumpPoint> jumpPoints;
    juce::Array<juce::Rectangle<float>> freeZones;
    float virtualX = 0.0f;
    float virtualY = 0.0f;

    int selectedJumpPoint = -1;
    int currentX = 0;
    int currentY = 0;
    float xSnapWeight = 1.0f;
    float ySnapWeight = 0.20f;
    float laneLockStrength = 4.0f;
    float lockedLaneX = 0.0f;
    float lockedLaneY = 0.0f;
    bool hasLockedLaneX = false;
    bool hasLockedLaneY = false;
    float laneTolerance = 10.0f;
    LastMoveAxis lastMoveAxis = LastMoveAxis::none;

    bool dragActive = false;
    int dragStartX = 0;
    int dragStartY = 0;
    double lastDragAdjustTimeMs = 0.0;

    bool leftMouseButtonDown = false;
    bool middleMouseButtonDown = false;
    bool rightMouseButtonDown = false;

    bool cursorUpKeyDown = false;
    bool cursorDownKeyDown = false;
    bool enterKeyDown = false;

    bool suppressNextPhysicalMoveCheck = false;
    int lastProgrammaticCursorX = 0;
    int lastProgrammaticCursorY = 0;
    int lastObservedPhysicalCursorX = 0;
    int lastObservedPhysicalCursorY = 0;
    bool hasObservedPhysicalCursorPosition = false;
};
