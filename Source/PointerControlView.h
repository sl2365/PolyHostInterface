#pragma once
#include <JuceHeader.h>

class PointerControlView final : public juce::Component
{
public:
    PointerControlView();

    void setPointerControlEnabled(bool shouldEnable);
    void setAdjustCcNumber(int ccNumber);
    void setDiscontinuityThreshold(int threshold);
    void setDeltaClamp(int clampAmount);

    std::function<void(bool)> onEnabledChanged;
    std::function<void(int)> onAdjustCcChanged;
    std::function<void(int)> onDiscontinuityThresholdChanged;
    std::function<void(int)> onDeltaClampChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label descriptionLabel;

    juce::ToggleButton enabledToggle { "Enable Pointer Control" };

    juce::Label adjustCcLabel;
    juce::Slider adjustCcSlider;

    juce::Label discontinuityLabel;
    juce::Label discontinuityHelpLabel;
    juce::Slider discontinuitySlider;

    juce::Label clampLabel;
    juce::Label clampHelpLabel;
    juce::Slider clampSlider;

    juce::Label helpLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PointerControlView)
};