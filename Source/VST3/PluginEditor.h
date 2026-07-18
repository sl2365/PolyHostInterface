#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MainView.h"

class PolyHostPluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PolyHostPluginEditor(PolyHostPluginProcessor&,
                                  MainView::MenuExtension* menuExtension = nullptr);
    ~PolyHostPluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    void resizeToFitContent(int contentWidth, int contentHeight);
    void resizeToRoutingView();

private:

    static constexpr int defaultWidth = 800;
    static constexpr int defaultHeight = 500;
    static constexpr int routingWidth = 800;
    static constexpr int routingHeight = 500;
    static constexpr int minWidth = 500;
    static constexpr int minHeight = 300;
    static constexpr int maxWidth = 1600;
    static constexpr int maxHeight = 1000;

    PolyHostPluginProcessor& audioProcessor;
    MainView mainView;
};