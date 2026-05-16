#include "PointerControl.h"
#include "DebugLog.h"
#include <cmath>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

void PointerControl::setTargetScreenBounds(juce::Rectangle<int> bounds)
{
    const bool hadTarget = !targetScreenBounds.isEmpty();
    targetScreenBounds = bounds;

    if (!targetScreenBounds.isEmpty())
    {
        if (!hadTarget)
        {
            currentX = targetScreenBounds.getCentreX();
            currentY = targetScreenBounds.getCentreY();
            virtualX = (float) currentX;
            virtualY = (float) currentY;
            hasLockedLaneX = true;
            hasLockedLaneY = true;
            lockedLaneX = (float) currentX;
            lockedLaneY = (float) currentY;
            moveCursorToCurrentPosition();
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
    }
}

void PointerControl::clearTarget()
{
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

    DebugLog::writeAdvanced("[PointerControl] wheelAdjust"
                            " | delta=" + juce::String(delta)
                            + " | currentX=" + juce::String(currentX)
                            + " | currentY=" + juce::String(currentY));

    syncToPhysicalCursorPosition();

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

void PointerControl::dragAdjust(int delta)
{
    if (!hasTarget() || delta == 0)
        return;

    DebugLog::writeAdvanced("[PointerControl] dragAdjust"
                            " | delta=" + juce::String(delta)
                            + " | currentX=" + juce::String(currentX)
                            + " | currentY=" + juce::String(currentY));

   #if JUCE_WINDOWS
    syncToPhysicalCursorPosition();

    const int anchorX = currentX;
    const int anchorY = currentY;
    const int pixelsPerStep = 1;
    const int moveY = -delta * pixelsPerStep;

    SetCursorPos(anchorX, anchorY);

    INPUT inputs[3] {};

    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_MOVE;
    inputs[1].mi.dx = 0;
    inputs[1].mi.dy = moveY;

    inputs[2].type = INPUT_MOUSE;
    inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    SendInput(3, inputs, sizeof(INPUT));

    SetCursorPos(anchorX, anchorY);

    currentX = anchorX;
    currentY = anchorY;
    virtualX = (float) currentX;
    virtualY = (float) currentY;
   #else
    juce::ignoreUnused(delta);
   #endif
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
    SetCursorPos(currentX, currentY);
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
    }
   #endif
}