#include "PointerControl.h"
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
}

bool PointerControl::hasTarget() const
{
    return !targetScreenBounds.isEmpty();
}

bool PointerControl::hasJumpPoints() const
{
    return jumpPoints.size() > 0;
}

void PointerControl::setJumpPoints(const juce::Array<JumpPoint>& newJumpPoints,
                                   juce::Rectangle<int> sourceBounds)
{
    jumpPoints.clear();

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

    updateJumpSelection();
}

void PointerControl::panX(int midiValue)
{
    if (!hasTarget())
        return;

    auto norm = juce::jlimit(0.0f, 1.0f, (float) midiValue / 127.0f);
    virtualX = (float) targetScreenBounds.getX() + norm * (float) targetScreenBounds.getWidth();

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

    float bestDist = (std::numeric_limits<float>::max)();
    int bestIndex = -1;

    for (int i = 0; i < jumpPoints.size(); ++i)
    {
        const auto& point = jumpPoints.getReference(i);
        const float dx = point.x - virtualX;
        const float dy = point.y - virtualY;
        const float dist = dx * dx + dy * dy;

        if (dist < bestDist)
        {
            bestDist = dist;
            bestIndex = i;
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

void PointerControl::wheelAdjust(int delta)
{
    if (!hasTarget() || delta == 0)
        return;

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