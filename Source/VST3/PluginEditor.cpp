#include "PluginEditor.h"

PolyHostPluginEditor::PolyHostPluginEditor(PolyHostPluginProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      mainView(p)
{
    addAndMakeVisible(mainView);
    setResizable(true, false);
    setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);
    setSize(defaultWidth, defaultHeight);
}

PolyHostPluginEditor::~PolyHostPluginEditor()
{
}

void PolyHostPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void PolyHostPluginEditor::resized()
{
    mainView.setBounds(getLocalBounds());
    mainView.resized();
    mainView.repaint();
}

void PolyHostPluginEditor::visibilityChanged()
{
}

void PolyHostPluginEditor::resizeToFitContent(int contentWidth, int contentHeight)
{
    constexpr int topMenuHeight = 28;
    constexpr int topRowHeight = 32;
    constexpr int tabBarHeight = 34;
    constexpr int statusBarHeight = 24;

    constexpr int outerPaddingX = 24;
    constexpr int outerPaddingTop = 8;
    constexpr int outerPaddingBottom = 8;
    constexpr int contentInnerPadding = 16;

    const int fixedChromeHeight =
        topMenuHeight
        + topRowHeight
        + tabBarHeight
        + statusBarHeight
        + outerPaddingTop
        + outerPaddingBottom
        + contentInnerPadding;

    const int desiredWidth =
        contentWidth
        + outerPaddingX
        + contentInnerPadding;

    const int desiredHeight =
        fixedChromeHeight
        + contentHeight;

    const int clampedWidth = juce::jlimit(minWidth, maxWidth, desiredWidth);
    const int clampedHeight = juce::jlimit(minHeight, maxHeight, desiredHeight);

    if (getWidth() != clampedWidth || getHeight() != clampedHeight)
    {
        setSize(clampedWidth, clampedHeight);
        resized();
        repaint();
    }
}

void PolyHostPluginEditor::resizeToRoutingView()
{
    setSize(routingWidth, routingHeight);
    resized();
    repaint();
}
