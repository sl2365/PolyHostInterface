#include "PointerControlView.h"

PointerControlView::PointerControlView()
{
    titleLabel.setText("Pointer Control", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    descriptionLabel.setText("Use a MIDI CC to send mouse-wheel style adjustments to the plugin under the cursor.",
                             juce::dontSendNotification);
    descriptionLabel.setJustificationType(juce::Justification::centredLeft);
    descriptionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    descriptionLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    addAndMakeVisible(descriptionLabel);

    enabledToggle.onClick = [this]
    {
        if (onEnabledChanged)
            onEnabledChanged(enabledToggle.getToggleState());
    };
    addAndMakeVisible(enabledToggle);

    adjustCcLabel.setText("Adjust CC", juce::dontSendNotification);
    adjustCcLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(adjustCcLabel);

    adjustCcSlider.setRange(0, 127, 1);
    adjustCcSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    adjustCcSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    adjustCcSlider.onValueChange = [this]
    {
        if (onAdjustCcChanged)
            onAdjustCcChanged((int) adjustCcSlider.getValue());
    };
    addAndMakeVisible(adjustCcSlider);

    discontinuityLabel.setText("Discontinuity Threshold", juce::dontSendNotification);
    discontinuityLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(discontinuityLabel);

    discontinuityHelpLabel.setText("Ignores large sudden CC jumps. Higher values allow bigger jumps before they are treated as invalid.",
                                   juce::dontSendNotification);
    discontinuityHelpLabel.setJustificationType(juce::Justification::centredLeft);
    discontinuityHelpLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    discontinuityHelpLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    addAndMakeVisible(discontinuityHelpLabel);

    discontinuitySlider.setRange(1, 127, 1);
    discontinuitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    discontinuitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    discontinuitySlider.onValueChange = [this]
    {
        if (onDiscontinuityThresholdChanged)
            onDiscontinuityThresholdChanged((int) discontinuitySlider.getValue());
    };
    addAndMakeVisible(discontinuitySlider);

    clampLabel.setText("Delta Clamp", juce::dontSendNotification);
    clampLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(clampLabel);

    clampHelpLabel.setText("Limits the maximum wheel movement sent from one MIDI step. Lower values feel smoother, higher values feel faster.",
                           juce::dontSendNotification);
    clampHelpLabel.setJustificationType(juce::Justification::centredLeft);
    clampHelpLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    clampHelpLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    addAndMakeVisible(clampHelpLabel);

    clampSlider.setRange(1, 32, 1);
    clampSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    clampSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    clampSlider.onValueChange = [this]
    {
        if (onDeltaClampChanged)
            onDeltaClampChanged((int) clampSlider.getValue());
    };
    addAndMakeVisible(clampSlider);

    helpLabel.setText("Current mode: wheel-style adjustment only.\nJump / pickup modes can be added next.",
                      juce::dontSendNotification);
    helpLabel.setJustificationType(juce::Justification::topLeft);
    helpLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(helpLabel);
}

void PointerControlView::setPointerControlEnabled(bool shouldEnable)
{
    enabledToggle.setToggleState(shouldEnable, juce::dontSendNotification);
}

void PointerControlView::setAdjustCcNumber(int ccNumber)
{
    adjustCcSlider.setValue(ccNumber, juce::dontSendNotification);
}

void PointerControlView::setDiscontinuityThreshold(int threshold)
{
    discontinuitySlider.setValue(threshold, juce::dontSendNotification);
}

void PointerControlView::setDeltaClamp(int clampAmount)
{
    clampSlider.setValue(clampAmount, juce::dontSendNotification);
}

void PointerControlView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1B263B));
}

void PointerControlView::resized()
{
    auto area = getLocalBounds().reduced(16);

    titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(4);
    descriptionLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(12);

    enabledToggle.setBounds(area.removeFromTop(28));
    area.removeFromTop(16);

    adjustCcLabel.setBounds(area.removeFromTop(24));
    adjustCcSlider.setBounds(area.removeFromTop(28));
    area.removeFromTop(16);

    discontinuityLabel.setBounds(area.removeFromTop(24));
    discontinuityHelpLabel.setBounds(area.removeFromTop(36));
    discontinuitySlider.setBounds(area.removeFromTop(28));
    area.removeFromTop(16);

    clampLabel.setBounds(area.removeFromTop(24));
    clampHelpLabel.setBounds(area.removeFromTop(36));
    clampSlider.setBounds(area.removeFromTop(28));
    area.removeFromTop(16);

    helpLabel.setBounds(area.removeFromTop(60));
}