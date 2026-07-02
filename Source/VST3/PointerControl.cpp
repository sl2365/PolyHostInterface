#include "PointerControl.h"
#include "DebugLog.h"
#include <cmath>

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
    restoreCursorIfHidden();

    targetScreenBounds = {};
    jumpPoints.clear();
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
        for (int i = 0; i < jumpPoints.size(); ++i)
        {
            const auto& point = jumpPoints.getReference(i);
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

void PointerControl::wheelAdjust(int delta)
{
    if (!hasTarget() || delta == 0)
        return;

    restoreCursorIfHidden();

    DebugLog::writeAdvanced("[PointerControl] wheelAdjust"
                            " | delta=" + juce::String(delta)
                            + " | currentX=" + juce::String(currentX)
                            + " | currentY=" + juce::String(currentY));

   #if JUCE_WINDOWS
    suppressNextPhysicalMoveCheck = true;
    lastProgrammaticCursorX = currentX;
    lastProgrammaticCursorY = currentY;
    SetCursorPos(currentX, currentY);

    POINT p { currentX, currentY };
    HWND hwnd = WindowFromPoint(p);

    if (hwnd != nullptr)
    {
        const WPARAM wParam = MAKEWPARAM(0, (short) (delta * WHEEL_DELTA));
        const LPARAM lParam = MAKELPARAM(currentX, currentY);
        SendMessage(hwnd, WM_MOUSEWHEEL, wParam, lParam);
    }

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


