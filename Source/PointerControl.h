#pragma once
#include <JuceHeader.h>

class PluginTabComponent;

class PointerControl
{
public:
    PointerControl() = default;

    void setEnabled(bool shouldEnable) { enabled = shouldEnable; }
    bool isEnabled() const { return enabled; }

    void setAdjustCcNumber(int cc)
    {
        adjustCcNumber = juce::jlimit(0, 127, cc);
    }

    int getAdjustCcNumber() const { return adjustCcNumber; }

    void setDiscontinuityThreshold(int threshold)
    {
        discontinuityThreshold = juce::jlimit(1, 127, threshold);
    }

    int getDiscontinuityThreshold() const { return discontinuityThreshold; }

    void setDeltaClamp(int clampAmount)
    {
        deltaClamp = juce::jlimit(1, 32, clampAmount);
    }

    int getDeltaClamp() const { return deltaClamp; }

    void setStepMultiplier(int multiplier)
    {
        stepMultiplier = juce::jlimit(1, 8, multiplier);
    }

    int getStepMultiplier() const { return stepMultiplier; }

    void setPointerDebugLoggingEnabledProvider(std::function<bool()> provider)
    {
        isPointerDebugLoggingEnabled = std::move(provider);
    }

    bool handleMidiMessageForCurrentTab(const juce::MidiMessage& message,
                                        PluginTabComponent* currentTab);
    void resetInteractionState();

private:
    bool isMouseOverEditor(PluginTabComponent* currentTab) const;
    void sendWheelAtCurrentCursor(int deltaSteps);
    void logDebug(const juce::String& message) const;

    bool enabled = true;
    int adjustCcNumber = 74;
    int discontinuityThreshold = 12;
    int deltaClamp = 4;
    int stepMultiplier = 1;
    int lastCcValue = -1;
    std::function<bool()> isPointerDebugLoggingEnabled;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PointerControl)
};