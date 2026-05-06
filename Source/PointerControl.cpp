#include "PointerControl.h"
#include "PluginTabComponent.h"
#include "DebugLog.h"
#include <cmath>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

void PointerControl::logDebug(const juce::String& message) const
{
    if (isPointerDebugLoggingEnabled && isPointerDebugLoggingEnabled())
        DebugLog::write(message);
}

void PointerControl::resetInteractionState()
{
    if (lastCcValue != -1)
        logDebug("PointerControl: resetting interaction state | lastCcValue was " + juce::String(lastCcValue));

    lastCcValue = -1;
}

bool PointerControl::handleMidiMessageForCurrentTab(const juce::MidiMessage& message,
                                                    PluginTabComponent* currentTab)
{
    if (!enabled)
    {
        resetInteractionState();
        logDebug("PointerControl: ignored MIDI message because Pointer Control is disabled");
        return false;
    }

    if (currentTab == nullptr)
    {
        resetInteractionState();
        logDebug("PointerControl: ignored MIDI message because currentTab is null");
        return false;
    }

    if (!message.isController())
        return false;

    const int controllerNumber = message.getControllerNumber();
    const int ccValue = message.getControllerValue();

    if (controllerNumber != adjustCcNumber)
        return false;

    auto pluginName = currentTab->getPluginName().trim();
    if (pluginName.isEmpty())
        pluginName = "<unnamed plugin>";

    logDebug("PointerControl: CC match"
             " | plugin=" + pluginName
             + " | cc=" + juce::String(controllerNumber)
             + " | value=" + juce::String(ccValue)
             + " | lastCcValue=" + juce::String(lastCcValue));

    const bool mouseOverEditor = isMouseOverEditor(currentTab);

    logDebug("PointerControl: editor hit-test"
             " | plugin=" + pluginName
             + " | mouseOverEditor=" + juce::String(mouseOverEditor ? "true" : "false"));

    if (!mouseOverEditor)
    {
        resetInteractionState();
        return false;
    }

    if (lastCcValue < 0)
    {
        lastCcValue = ccValue;
        logDebug("PointerControl: primed initial CC value"
                 " | plugin=" + pluginName
                 + " | storedLastCcValue=" + juce::String(lastCcValue));
        return true;
    }

    int rawDelta = ccValue - lastCcValue;
    int adjustedDelta = rawDelta;

    if (std::abs(rawDelta) > discontinuityThreshold)
    {
        logDebug("PointerControl: discontinuity detected"
                 " | plugin=" + pluginName
                 + " | rawDelta=" + juce::String(rawDelta)
                 + " | threshold=" + juce::String(discontinuityThreshold)
                 + " | oldLastCcValue=" + juce::String(lastCcValue)
                 + " | newCcValue=" + juce::String(ccValue));

        lastCcValue = ccValue;
        return true;
    }

    lastCcValue = ccValue;
    adjustedDelta *= stepMultiplier;
    adjustedDelta = juce::jlimit(-deltaClamp, deltaClamp, adjustedDelta);

    logDebug("PointerControl: computed delta"
             " | plugin=" + pluginName
             + " | rawDelta=" + juce::String(rawDelta)
             + " | stepMultiplier=" + juce::String(stepMultiplier)
             + " | adjustedDelta=" + juce::String(adjustedDelta)
             + " | clamp=" + juce::String(deltaClamp));

    if (adjustedDelta == 0)
    {
        logDebug("PointerControl: zero delta, no wheel sent"
                 " | plugin=" + pluginName);
        return true;
    }

    sendWheelAtCurrentCursor(adjustedDelta);

    logDebug("PointerControl: wheel sent"
             " | plugin=" + pluginName
             + " | wheelSteps=" + juce::String(adjustedDelta));

    return true;
}

bool PointerControl::isMouseOverEditor(PluginTabComponent* currentTab) const
{
    auto* editor = currentTab->getPluginEditor();
    if (editor == nullptr)
    {
        logDebug("PointerControl: editor hit-test failed because editor is null");
        return false;
    }

    if (!editor->isShowing())
    {
        logDebug("PointerControl: editor hit-test failed because editor is not showing");
        return false;
    }

    const auto mouseScreenPos = juce::Desktop::getMousePosition();
    const auto editorScreenBounds = editor->getScreenBounds();
    const bool contains = editorScreenBounds.contains(mouseScreenPos);

    logDebug("PointerControl: editor bounds"
                            " | mouseX=" + juce::String(mouseScreenPos.x)
                            + " | mouseY=" + juce::String(mouseScreenPos.y)
                            + " | editorX=" + juce::String(editorScreenBounds.getX())
                            + " | editorY=" + juce::String(editorScreenBounds.getY())
                            + " | editorW=" + juce::String(editorScreenBounds.getWidth())
                            + " | editorH=" + juce::String(editorScreenBounds.getHeight())
                            + " | contains=" + juce::String(contains ? "true" : "false"));

    return contains;
}

void PointerControl::sendWheelAtCurrentCursor(int deltaSteps)
{
   #if JUCE_WINDOWS
    logDebug("PointerControl: sendWheelAtCurrentCursor"
                            " | deltaSteps=" + juce::String(deltaSteps)
                            + " | mouseData=" + juce::String(deltaSteps * WHEEL_DELTA));

    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(deltaSteps * WHEEL_DELTA);

    const auto sent = SendInput(1, &input, sizeof(INPUT));

    logDebug("PointerControl: SendInput result=" + juce::String((int) sent));
   #else
    juce::ignoreUnused(deltaSteps);
   #endif
}

