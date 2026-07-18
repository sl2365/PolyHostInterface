#include "PointerControl.h"
#include "DebugLog.h"
#include <cmath>
#include <limits>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

void PointerControl::setTargetScreenBounds(juce::Rectangle<int> bounds)
{
    const bool boundsChanged = (bounds != targetScreenBounds);

    if (boundsChanged && dragActive)
        endDrag();

    const bool hadTarget = !targetScreenBounds.isEmpty();
    targetScreenBounds = bounds;

    if (!targetScreenBounds.isEmpty())
    {
        if (!hadTarget)
        {
            syncToPhysicalCursorPosition();

            currentX = juce::jlimit(targetScreenBounds.getX(),
                                    targetScreenBounds.getRight(),
                                    currentX);

            currentY = juce::jlimit(targetScreenBounds.getY(),
                                    targetScreenBounds.getBottom(),
                                    currentY);

            virtualX = (float) currentX;
            virtualY = (float) currentY;
            hasLockedLaneX = true;
            hasLockedLaneY = true;
            lockedLaneX = (float) currentX;
            lockedLaneY = (float) currentY;
        }
        else
        {
            currentX = juce::jlimit(targetScreenBounds.getX(),
                                    targetScreenBounds.getRight(),
                                    currentX);

            currentY = juce::jlimit(targetScreenBounds.getY(),
                                    targetScreenBounds.getBottom(),
                                    currentY);

            virtualX = juce::jlimit((float) targetScreenBounds.getX(),
                                    (float) targetScreenBounds.getRight(),
                                    virtualX);

            virtualY = juce::jlimit((float) targetScreenBounds.getY(),
                                    (float) targetScreenBounds.getBottom(),
                                    virtualY);
        }

       #if JUCE_WINDOWS
        POINT p {};
        if (GetCursorPos(&p))
        {
            lastObservedPhysicalCursorX = p.x;
            lastObservedPhysicalCursorY = p.y;
            hasObservedPhysicalCursorPosition = true;
        }
       #endif
    }
}

void PointerControl::clearTarget()
{
    endDrag();
    releaseMouseButtons();
    releaseKeyboardKeys();
    restoreCursorIfHidden();

    targetScreenBounds = {};
    jumpPoints.clear();
    freeZones.clear();
    selectedJumpPoint = -1;
    lastMoveAxis = LastMoveAxis::none;
    hasLockedLaneX = false;
    hasLockedLaneY = false;
}

bool PointerControl::hasTarget() const
{
    return !targetScreenBounds.isEmpty();
}

void PointerControl::setSnapWeights(float newXWeight, float newYWeight)
{
    xSnapWeight = juce::jmax(0.001f, newXWeight);
    ySnapWeight = juce::jmax(0.001f, newYWeight);
}

float PointerControl::getXSnapWeight() const
{
    return xSnapWeight;
}

float PointerControl::getYSnapWeight() const
{
    return ySnapWeight;
}

bool PointerControl::hasJumpPoints() const
{
    return jumpPoints.size() > 0;
}

void PointerControl::setLaneTolerance(float newTolerance)
{
    laneTolerance = juce::jlimit(1.0f, 128.0f, newTolerance);

    if (hasJumpPoints())
        updateJumpSelection();
}

float PointerControl::getLaneTolerance() const
{
    return laneTolerance;
}

void PointerControl::setJumpPoints(const juce::Array<JumpPoint>& newJumpPoints,
                                   juce::Rectangle<int> sourceBounds)
{
    jumpPoints.clear();
    selectedJumpPoint = -1;
    lastMoveAxis = LastMoveAxis::none;

    if (targetScreenBounds.isEmpty() || sourceBounds.isEmpty())
        return;

    const float scaleX = (float) targetScreenBounds.getWidth() / (float) sourceBounds.getWidth();
    const float scaleY = (float) targetScreenBounds.getHeight() / (float) sourceBounds.getHeight();

    for (int i = 0; i < newJumpPoints.size(); ++i)
    {
        const auto& src = newJumpPoints.getReference(i);

        JumpPoint dst;
        dst.x = (float) targetScreenBounds.getX() + src.x * scaleX;
        dst.y = (float) targetScreenBounds.getY() + src.y * scaleY;
        jumpPoints.add(dst);
    }

    lockedLaneX = (float) currentX;
    lockedLaneY = (float) currentY;
    hasLockedLaneX = true;
    hasLockedLaneY = true;

    updateJumpSelection();
}

void PointerControl::setFreeZones(const juce::Array<juce::Rectangle<float>>& sourceFreeZones,
                                  juce::Rectangle<int> sourceBounds)
{
    freeZones.clear();

    if (targetScreenBounds.isEmpty() || sourceBounds.isEmpty())
        return;

    const float scaleX = (float) targetScreenBounds.getWidth() / (float) sourceBounds.getWidth();
    const float scaleY = (float) targetScreenBounds.getHeight() / (float) sourceBounds.getHeight();

    for (int i = 0; i < sourceFreeZones.size(); ++i)
    {
        const auto& sourceFreeZone = sourceFreeZones.getReference(i);

        if (sourceFreeZone.isEmpty())
            continue;

        auto zone = juce::Rectangle<float>(
            (float) targetScreenBounds.getX() + sourceFreeZone.getX() * scaleX,
            (float) targetScreenBounds.getY() + sourceFreeZone.getY() * scaleY,
            sourceFreeZone.getWidth() * scaleX,
            sourceFreeZone.getHeight() * scaleY);

        if (! zone.isEmpty())
            freeZones.add(zone);
    }

    if (hasJumpPoints())
        updateJumpSelection();
}

void PointerControl::setFreeZone(bool shouldHaveFreeZone,
                                 juce::Rectangle<float> sourceFreeZone,
                                 juce::Rectangle<int> sourceBounds)
{
    juce::Array<juce::Rectangle<float>> zones;

    if (shouldHaveFreeZone && ! sourceFreeZone.isEmpty())
        zones.add(sourceFreeZone);

    setFreeZones(zones, sourceBounds);
}

void PointerControl::panX(int midiValue)
{
    if (!hasTarget())
        return;

    restoreCursorIfHidden();

    auto norm = juce::jlimit(0.0f, 1.0f, (float) midiValue / 127.0f);
    virtualX = (float) targetScreenBounds.getX() + norm * (float) targetScreenBounds.getWidth();

    lockedLaneX = virtualX;
    hasLockedLaneX = true;
    lastMoveAxis = LastMoveAxis::x;

    if (isCurrentVirtualPositionInFreeZone())
    {
        moveFreelyToVirtualPosition();
        return;
    }

    if (hasJumpPoints())
    {
        updateJumpSelection();
        moveCursorToCurrentPosition();
    }
    else
    {
        currentX = (int) std::round(virtualX);
        moveCursorToCurrentPosition();
    }
}

void PointerControl::panXRelative(int delta)
{
    if (!hasTarget() || delta == 0)
        return;

    restoreCursorIfHidden();

    const float pixelsPerStep = (float) targetScreenBounds.getWidth() / 127.0f;

    virtualX = juce::jlimit((float) targetScreenBounds.getX(),
                            (float) targetScreenBounds.getRight(),
                            virtualX + ((float) delta * pixelsPerStep));

    lockedLaneX = virtualX;
    hasLockedLaneX = true;
    lastMoveAxis = LastMoveAxis::x;

    if (isCurrentVirtualPositionInFreeZone())
    {
        moveFreelyToVirtualPosition();
        return;
    }

    if (hasJumpPoints())
    {
        updateJumpSelection();
        moveCursorToCurrentPosition();
    }
    else
    {
        currentX = (int) std::round(virtualX);
        moveCursorToCurrentPosition();
    }
}
 
void PointerControl::panY(int midiValue)
{
    if (!hasTarget())
        return;

    restoreCursorIfHidden();

    auto norm = juce::jlimit(0.0f, 1.0f, (float) midiValue / 127.0f);
    virtualY = (float) targetScreenBounds.getY() + norm * (float) targetScreenBounds.getHeight();

    lockedLaneY = virtualY;
    hasLockedLaneY = true;
    lastMoveAxis = LastMoveAxis::y;

    if (isCurrentVirtualPositionInFreeZone())
    {
        moveFreelyToVirtualPosition();
        return;
    }

    if (hasJumpPoints())
    {
        updateJumpSelection();
        moveCursorToCurrentPosition();
    }
    else
    {
        currentY = (int) std::round(virtualY);
        moveCursorToCurrentPosition();
    }
}

void PointerControl::panYRelative(int delta)
{
    if (!hasTarget() || delta == 0)
        return;

    restoreCursorIfHidden();

    const float pixelsPerStep = (float) targetScreenBounds.getHeight() / 127.0f;

    virtualY = juce::jlimit((float) targetScreenBounds.getY(),
                            (float) targetScreenBounds.getBottom(),
                            virtualY + ((float) delta * pixelsPerStep));

    lockedLaneY = virtualY;
    hasLockedLaneY = true;
    lastMoveAxis = LastMoveAxis::y;

    if (isCurrentVirtualPositionInFreeZone())
    {
        moveFreelyToVirtualPosition();
        return;
    }

    if (hasJumpPoints())
    {
        updateJumpSelection();
        moveCursorToCurrentPosition();
    }
    else
    {
        currentY = (int) std::round(virtualY);
        moveCursorToCurrentPosition();
    }
}

void PointerControl::updateJumpSelection()
{
    if (isCurrentVirtualPositionInFreeZone())
    {
        const auto adjusted = getFreeZoneAdjustedVirtualPosition();

        selectedJumpPoint = -1;
        currentX = (int) std::round(adjusted.x);
        currentY = (int) std::round(adjusted.y);
        return;
    }

    if (!hasJumpPoints())
    {
        selectedJumpPoint = -1;
        return;
    }

    const float sameRowTolerance = laneTolerance;
    const float sameColumnTolerance = laneTolerance;

    int bestIndex = -1;
    float bestPrimary = (std::numeric_limits<float>::max)();
    float bestSecondary = (std::numeric_limits<float>::max)();

    if (lastMoveAxis == LastMoveAxis::x)
    {
        const float rowY = hasLockedLaneY ? lockedLaneY : (float) currentY;

        for (int i = 0; i < jumpPoints.size(); ++i)
        {
            const auto& point = jumpPoints.getReference(i);

            if (isPointInFreeZone(point.x, point.y))
                continue;

            const float rowDistance = std::abs(point.y - rowY);
            if (rowDistance > sameRowTolerance)
                continue;

            const float xDistance = std::abs(point.x - virtualX);

            if (xDistance < bestPrimary
                || (juce::approximatelyEqual(xDistance, bestPrimary) && rowDistance < bestSecondary))
            {
                bestPrimary = xDistance;
                bestSecondary = rowDistance;
                bestIndex = i;
            }
        }
    }
    else if (lastMoveAxis == LastMoveAxis::y)
    {
        const float columnX = hasLockedLaneX ? lockedLaneX : (float) currentX;

        for (int i = 0; i < jumpPoints.size(); ++i)
        {
            const auto& point = jumpPoints.getReference(i);

            if (isPointInFreeZone(point.x, point.y))
                continue;

            const float columnDistance = std::abs(point.x - columnX);
            if (columnDistance > sameColumnTolerance)
                continue;

            const float yDistance = std::abs(point.y - virtualY);

            if (yDistance < bestPrimary
                || (juce::approximatelyEqual(yDistance, bestPrimary) && columnDistance < bestSecondary))
            {
                bestPrimary = yDistance;
                bestSecondary = columnDistance;
                bestIndex = i;
            }
        }
    }
    else
    {
        for (int i = 0; i < jumpPoints.size(); ++i)
        {
            const auto& point = jumpPoints.getReference(i);

            if (isPointInFreeZone(point.x, point.y))
                continue;

            const float dx = point.x - virtualX;
            const float dy = point.y - virtualY;
            const float dist = (dx * dx * xSnapWeight) + (dy * dy * ySnapWeight);

            if (dist < bestPrimary)
            {
                bestPrimary = dist;
                bestIndex = i;
            }
        }
    }

    if (bestIndex < 0)
    {
        bestPrimary = (std::numeric_limits<float>::max)();

        for (int i = 0; i < jumpPoints.size(); ++i)
        {
            const auto& point = jumpPoints.getReference(i);

            if (isPointInFreeZone(point.x, point.y))
                continue;

            const float dx = point.x - virtualX;
            const float dy = point.y - virtualY;
            const float dist = (dx * dx * xSnapWeight) + (dy * dy * ySnapWeight);

            if (dist < bestPrimary)
            {
                bestPrimary = dist;
                bestIndex = i;
            }
        }
    }

    selectedJumpPoint = bestIndex;

    if (selectedJumpPoint >= 0)
    {
        const auto& point = jumpPoints.getReference(selectedJumpPoint);
        currentX = (int) std::round(point.x);
        currentY = (int) std::round(point.y);
    }
    else
    {
        currentX = (int) std::round(virtualX);
        currentY = (int) std::round(virtualY);
    }
}

int PointerControl::findNearestJumpPointInCurrentLane() const
{
    if (!juce::isPositiveAndBelow(selectedJumpPoint, jumpPoints.size()))
        return -1;

    const auto& currentPoint = jumpPoints.getReference(selectedJumpPoint);
    const float currentLaneTolerance = laneTolerance;

    float bestDistance = (std::numeric_limits<float>::max)();
    int bestIndex = -1;

    for (int i = 0; i < jumpPoints.size(); ++i)
    {
        const auto& point = jumpPoints.getReference(i);

        if (isPointInFreeZone(point.x, point.y))
            continue;

        if (lastMoveAxis == LastMoveAxis::x)
        {
            if (std::abs(point.y - currentPoint.y) > currentLaneTolerance)
                continue;

            const float distance = std::abs(point.x - virtualX);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = i;
            }
        }
        else if (lastMoveAxis == LastMoveAxis::y)
        {
            if (std::abs(point.x - currentPoint.x) > currentLaneTolerance)
                continue;

            const float distance = std::abs(point.y - virtualY);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = i;
            }
        }
    }

    return bestIndex;
}

bool PointerControl::isPointInFreeZone(float x, float y) const
{
    for (int i = 0; i < freeZones.size(); ++i)
    {
        if (freeZones.getReference(i).contains(x, y))
            return true;
    }

    return false;
}

int PointerControl::findFreeZoneForCurrentAxis() const
{
    if (freeZones.isEmpty())
        return -1;

    const float captureMargin = juce::jlimit(2.0f, 64.0f, laneTolerance);

    int bestIndex = -1;
    float bestPrimary = (std::numeric_limits<float>::max)();
    float bestArea = -1.0f;

    for (int i = 0; i < freeZones.size(); ++i)
    {
        const auto& zone = freeZones.getReference(i);

        if (zone.isEmpty())
            continue;

        const float left = zone.getX();
        const float right = zone.getRight();
        const float top = zone.getY();
        const float bottom = zone.getBottom();

        bool candidate = false;
        float primary = (std::numeric_limits<float>::max)();

        if (lastMoveAxis == LastMoveAxis::x)
        {
            const float laneY = hasLockedLaneY ? lockedLaneY : virtualY;

            if (laneY >= top
                && laneY <= bottom
                && virtualX >= left - captureMargin
                && virtualX <= right + captureMargin)
            {
                candidate = true;

                if (virtualX < left)
                    primary = left - virtualX;
                else if (virtualX > right)
                    primary = virtualX - right;
                else
                    primary = 0.0f;
            }
        }
        else if (lastMoveAxis == LastMoveAxis::y)
        {
            const float laneX = hasLockedLaneX ? lockedLaneX : virtualX;

            if (laneX >= left
                && laneX <= right
                && virtualY >= top - captureMargin
                && virtualY <= bottom + captureMargin)
            {
                candidate = true;

                if (virtualY < top)
                    primary = top - virtualY;
                else if (virtualY > bottom)
                    primary = virtualY - bottom;
                else
                    primary = 0.0f;
            }
        }
        else if (virtualX >= left
                 && virtualX <= right
                 && virtualY >= top
                 && virtualY <= bottom)
        {
            candidate = true;
            primary = 0.0f;
        }

        if (! candidate)
            continue;

        const float area = zone.getWidth() * zone.getHeight();

        if (primary < bestPrimary
            || (juce::approximatelyEqual(primary, bestPrimary) && area > bestArea))
        {
            bestPrimary = primary;
            bestArea = area;
            bestIndex = i;
        }
    }

    return bestIndex;
}

bool PointerControl::shouldUseFreeZoneForCurrentAxis() const
{
    return findFreeZoneForCurrentAxis() >= 0;
}

bool PointerControl::isCurrentVirtualPositionInFreeZone() const
{
    return shouldUseFreeZoneForCurrentAxis();
}

juce::Point<float> PointerControl::getFreeZoneAdjustedVirtualPosition() const
{
    const int zoneIndex = findFreeZoneForCurrentAxis();

    if (! juce::isPositiveAndBelow(zoneIndex, freeZones.size()))
        return { virtualX, virtualY };

    const auto& zone = freeZones.getReference(zoneIndex);

    const float left = zone.getX();
    const float right = zone.getRight();
    const float top = zone.getY();
    const float bottom = zone.getBottom();

    if (lastMoveAxis == LastMoveAxis::x)
    {
        const float laneY = hasLockedLaneY ? lockedLaneY : virtualY;

        return { juce::jlimit(left, right, virtualX),
                 juce::jlimit(top, bottom, laneY) };
    }

    if (lastMoveAxis == LastMoveAxis::y)
    {
        const float laneX = hasLockedLaneX ? lockedLaneX : virtualX;

        return { juce::jlimit(left, right, laneX),
                 juce::jlimit(top, bottom, virtualY) };
    }

    return { juce::jlimit(left, right, virtualX),
             juce::jlimit(top, bottom, virtualY) };
}

void PointerControl::moveFreelyToVirtualPosition()
{
    const auto adjusted = getFreeZoneAdjustedVirtualPosition();

    currentX = (int) std::round(juce::jlimit((float) targetScreenBounds.getX(),
                                            (float) targetScreenBounds.getRight(),
                                            adjusted.x));
    currentY = (int) std::round(juce::jlimit((float) targetScreenBounds.getY(),
                                            (float) targetScreenBounds.getBottom(),
                                            adjusted.y));
    selectedJumpPoint = -1;
    moveCursorToCurrentPosition();
}

void PointerControl::setMouseButtonState(MouseButton button, bool shouldBeDown)
{
    bool* buttonState = nullptr;
    juce::String buttonName;

   #if JUCE_WINDOWS
    DWORD downFlag = 0;
    DWORD upFlag = 0;
   #endif

    switch (button)
    {
        case MouseButton::left:
            buttonState = &leftMouseButtonDown;
            buttonName = "left";
           #if JUCE_WINDOWS
            downFlag = MOUSEEVENTF_LEFTDOWN;
            upFlag = MOUSEEVENTF_LEFTUP;
           #endif
            break;

        case MouseButton::middle:
            buttonState = &middleMouseButtonDown;
            buttonName = "middle";
           #if JUCE_WINDOWS
            downFlag = MOUSEEVENTF_MIDDLEDOWN;
            upFlag = MOUSEEVENTF_MIDDLEUP;
           #endif
            break;

        case MouseButton::right:
            buttonState = &rightMouseButtonDown;
            buttonName = "right";
           #if JUCE_WINDOWS
            downFlag = MOUSEEVENTF_RIGHTDOWN;
            upFlag = MOUSEEVENTF_RIGHTUP;
           #endif
            break;
    }

    if (buttonState == nullptr || *buttonState == shouldBeDown)
        return;

    if (shouldBeDown && !hasTarget())
        return;

    restoreCursorIfHidden();

    int clickX = currentX;
    int clickY = currentY;

   #if JUCE_WINDOWS
    POINT p {};
    if (GetCursorPos(&p))
    {
        clickX = p.x;
        clickY = p.y;

        currentX = clickX;
        currentY = clickY;
        virtualX = (float) currentX;
        virtualY = (float) currentY;

        lastObservedPhysicalCursorX = clickX;
        lastObservedPhysicalCursorY = clickY;
        hasObservedPhysicalCursorPosition = true;
    }

    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = shouldBeDown ? downFlag : upFlag;
    SendInput(1, &input, sizeof(INPUT));
   #endif

    *buttonState = shouldBeDown;

    DebugLog::writeAdvanced("[PointerControl] mouse button "
                            + juce::String(shouldBeDown ? "down" : "up")
                            + " | button="
                            + buttonName
                            + " | x="
                            + juce::String(clickX)
                            + " | y="
                            + juce::String(clickY));
}

void PointerControl::releaseMouseButtons()
{
    setMouseButtonState(MouseButton::left, false);
    setMouseButtonState(MouseButton::middle, false);
    setMouseButtonState(MouseButton::right, false);
}

void PointerControl::setKeyboardKeyState(KeyboardKey key, bool shouldBeDown)
{
    bool* keyState = nullptr;
    juce::String keyName;

   #if JUCE_WINDOWS
    WORD virtualKey = 0;
   #endif

    switch (key)
    {
        case KeyboardKey::cursorUp:
            keyState = &cursorUpKeyDown;
            keyName = "cursorUp";
           #if JUCE_WINDOWS
            virtualKey = VK_UP;
           #endif
            break;

        case KeyboardKey::cursorDown:
            keyState = &cursorDownKeyDown;
            keyName = "cursorDown";
           #if JUCE_WINDOWS
            virtualKey = VK_DOWN;
           #endif
            break;

        case KeyboardKey::enter:
            keyState = &enterKeyDown;
            keyName = "enter";
           #if JUCE_WINDOWS
            virtualKey = VK_RETURN;
           #endif
            break;
    }

    if (keyState == nullptr || *keyState == shouldBeDown)
        return;

    restoreCursorIfHidden();

   #if JUCE_WINDOWS
    INPUT input {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtualKey;

    if (! shouldBeDown)
        input.ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(1, &input, sizeof(INPUT));
   #endif

    *keyState = shouldBeDown;

    DebugLog::writeAdvanced("[PointerControl] keyboard key "
                            + juce::String(shouldBeDown ? "down" : "up")
                            + " | key="
                            + keyName);
}

void PointerControl::releaseKeyboardKeys()
{
    setKeyboardKeyState(KeyboardKey::cursorUp, false);
    setKeyboardKeyState(KeyboardKey::cursorDown, false);
    setKeyboardKeyState(KeyboardKey::enter, false);
}

void PointerControl::wheelAdjust(int delta)
{
    if (!hasTarget() || delta == 0)
        return;

    restoreCursorIfHidden();

    int scrollX = currentX;
    int scrollY = currentY;

   #if JUCE_WINDOWS
    POINT p {};
    if (GetCursorPos(&p))
    {
        scrollX = p.x;
        scrollY = p.y;

        currentX = scrollX;
        currentY = scrollY;
        virtualX = (float) currentX;
        virtualY = (float) currentY;

        lastObservedPhysicalCursorX = scrollX;
        lastObservedPhysicalCursorY = scrollY;
        hasObservedPhysicalCursorPosition = true;
    }
   #endif

    DebugLog::writeAdvanced("[PointerControl] wheelAdjust"
                            " | delta=" + juce::String(delta)
                            + " | x=" + juce::String(scrollX)
                            + " | y=" + juce::String(scrollY));

   #if JUCE_WINDOWS
    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = (DWORD) (delta * WHEEL_DELTA);
    SendInput(1, &input, sizeof(INPUT));
   #else
    juce::ignoreUnused(delta);
   #endif
}

void PointerControl::beginDrag()
{
   #if JUCE_WINDOWS
    if (dragActive || !hasTarget())
        return;

    syncToPhysicalCursorPosition();

    dragStartX = currentX;
    dragStartY = currentY;
    lastDragAdjustTimeMs = juce::Time::getMillisecondCounterHiRes();

    suppressNextPhysicalMoveCheck = true;
    lastProgrammaticCursorX = dragStartX;
    lastProgrammaticCursorY = dragStartY;
    SetCursorPos(dragStartX, dragStartY);

    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));

    dragActive = true;

    DebugLog::writeAdvanced("[PointerControl] beginDrag"
                            " | x=" + juce::String(dragStartX)
                            + " | y=" + juce::String(dragStartY));
   #endif
}

void PointerControl::moveDragBy(int deltaY)
{
   #if JUCE_WINDOWS
    if (!dragActive)
        return;

    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = 0;
    input.mi.dy = deltaY;
    SendInput(1, &input, sizeof(INPUT));
   #else
    juce::ignoreUnused(deltaY);
   #endif
}

void PointerControl::dragAdjust(int delta)
{
    if (!hasTarget() || delta == 0)
        return;

    restoreCursorIfHidden();

    DebugLog::writeAdvanced("[PointerControl] dragAdjust"
                            " | delta=" + juce::String(delta)
                            + " | currentX=" + juce::String(currentX)
                            + " | currentY=" + juce::String(currentY));

   #if JUCE_WINDOWS
    if (!dragActive)
        beginDrag();

    if (!dragActive)
        return;

    lastDragAdjustTimeMs = juce::Time::getMillisecondCounterHiRes();

    const int pixelsPerStep = 1;
    const int moveY = -delta * pixelsPerStep;

    moveDragBy(moveY);
   #else
    juce::ignoreUnused(delta);
   #endif
}

void PointerControl::endDrag()
{
   #if JUCE_WINDOWS
    if (!dragActive)
        return;

    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));

    suppressNextPhysicalMoveCheck = true;
    lastProgrammaticCursorX = dragStartX;
    lastProgrammaticCursorY = dragStartY;
    SetCursorPos(dragStartX, dragStartY);

    currentX = dragStartX;
    currentY = dragStartY;
    virtualX = (float) currentX;
    virtualY = (float) currentY;

    dragActive = false;
    restoreCursorIfHidden();

    DebugLog::writeAdvanced("[PointerControl] endDrag"
                            " | x=" + juce::String(dragStartX)
                            + " | y=" + juce::String(dragStartY));
   #endif
}

bool PointerControl::isDragActive() const
{
    return dragActive;
}

juce::Point<int> PointerControl::getCurrentScreenPosition() const
{
    return { currentX, currentY };
}

juce::Rectangle<int> PointerControl::getTargetScreenBounds() const
{
    return targetScreenBounds;
}

void PointerControl::moveCursorToCurrentPosition()
{
    if (!hasTarget())
        return;

   #if JUCE_WINDOWS
    suppressNextPhysicalMoveCheck = true;
    lastProgrammaticCursorX = currentX;
    lastProgrammaticCursorY = currentY;
    SetCursorPos(currentX, currentY);
   #endif
}

void PointerControl::restoreCursorIfHidden()
{
   #if JUCE_WINDOWS
    while (ShowCursor(TRUE) < 0)
    {
    }
   #endif
}

void PointerControl::handleExternalMouseMove()
{
   #if JUCE_WINDOWS
    POINT p {};
    if (!GetCursorPos(&p))
        return;

    if (!hasObservedPhysicalCursorPosition)
    {
        lastObservedPhysicalCursorX = p.x;
        lastObservedPhysicalCursorY = p.y;
        hasObservedPhysicalCursorPosition = true;
        return;
    }

    if (suppressNextPhysicalMoveCheck
        && p.x == lastProgrammaticCursorX
        && p.y == lastProgrammaticCursorY)
    {
        lastObservedPhysicalCursorX = p.x;
        lastObservedPhysicalCursorY = p.y;
        suppressNextPhysicalMoveCheck = false;
        return;
    }

    if (p.x != lastObservedPhysicalCursorX || p.y != lastObservedPhysicalCursorY)
    {
        lastObservedPhysicalCursorX = p.x;
        lastObservedPhysicalCursorY = p.y;

        if (dragActive)
            return;

        restoreCursorIfHidden();

        currentX = p.x;
        currentY = p.y;
        virtualX = (float) currentX;
        virtualY = (float) currentY;
    }
   #endif
}

void PointerControl::syncToPhysicalCursorPosition()
{
   #if JUCE_WINDOWS
    POINT p {};
    if (GetCursorPos(&p))
    {
        currentX = p.x;
        currentY = p.y;
        virtualX = (float) currentX;
        virtualY = (float) currentY;

        if (!hasObservedPhysicalCursorPosition)
        {
            lastObservedPhysicalCursorX = p.x;
            lastObservedPhysicalCursorY = p.y;
            hasObservedPhysicalCursorPosition = true;
        }
    }
   #endif
}

void PointerControl::releaseDragIfIdle(double idleThresholdMs)
{
   #if JUCE_WINDOWS
    if (!dragActive)
        return;

    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if ((nowMs - lastDragAdjustTimeMs) >= idleThresholdMs)
        endDrag();
   #else
    juce::ignoreUnused(idleThresholdMs);
   #endif
}


