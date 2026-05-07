#include "PointerControl.h"

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
        }
    }
}

void PointerControl::clearTarget()
{
    targetScreenBounds = {};
}

bool PointerControl::hasTarget() const
{
    return !targetScreenBounds.isEmpty();
}

void PointerControl::panX(int midiValue)
{
    if (!hasTarget())
        return;

    auto norm = juce::jlimit(0.0f, 1.0f, (float) midiValue / 127.0f);
    currentX = targetScreenBounds.getX() + (int) std::round(norm * (float) targetScreenBounds.getWidth());
    moveCursorToCurrentPosition();
}

void PointerControl::panY(int midiValue)
{
    if (!hasTarget())
        return;

    auto norm = juce::jlimit(0.0f, 1.0f, (float) midiValue / 127.0f);
    currentY = targetScreenBounds.getY() + (int) std::round(norm * (float) targetScreenBounds.getHeight());
    moveCursorToCurrentPosition();
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
    }
   #endif
}

