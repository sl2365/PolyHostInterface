#include "PluginEditor.h"

PolyHostPluginEditor::PolyHostPluginEditor(PolyHostPluginProcessor& p,
                                           MainView::MenuExtension* menuExtension)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      mainView(p, menuExtension)
{
    addAndMakeVisible(mainView);
    routingViewHeight =
        mainView.getAppSettings().getRoutingWindowHeight();
    macroMappingsViewHeight =
        mainView.getAppSettings().getMacroMappingsViewHeight();
    setResizable(true, false);
    updateResizeLimits();
    setSize(defaultWidth, defaultHeight + getMidiKeyboardExtraHeight());
}

PolyHostPluginEditor::~PolyHostPluginEditor()
{
    mainView.getAppSettings().setRoutingWindowSize(
        routingWidth, routingViewHeight);
    mainView.getAppSettings().setMacroMappingsViewHeight(
        macroMappingsViewHeight);
}

void PolyHostPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void PolyHostPluginEditor::resized()
{
    const int contentHeight = getHeight() - getMidiKeyboardExtraHeight();

    if (mainView.isShowingRoutingView()
        && ! applyingRoutingViewSize)
    {
        routingViewHeight = juce::jlimit(
            minHeight,
            maxHeight - getMidiKeyboardExtraHeight(),
            contentHeight);
    }

    if (mainView.isShowingMacroMappingsView()
        && ! applyingMacroMappingsViewSize)
    {
        macroMappingsViewHeight = juce::jlimit(
            minHeight,
            maxHeight - getMidiKeyboardExtraHeight(),
            contentHeight);
    }

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
    const juce::ScopedValueSetter<bool> applyingSize(
        applyingRoutingViewSize, true);
    updateResizeLimits();
    setSize(routingWidth,
            juce::jlimit(minHeight + getMidiKeyboardExtraHeight(),
                         maxHeight,
                         routingViewHeight + getMidiKeyboardExtraHeight()));
    resized();
    repaint();
}

void PolyHostPluginEditor::resizeToMacroMappingsView()
{
    const juce::ScopedValueSetter<bool> applyingSize(
        applyingMacroMappingsViewSize, true);
    updateResizeLimits();
    setSize(macroMappingsWidth,
            juce::jlimit(minHeight + getMidiKeyboardExtraHeight(),
                         maxHeight,
                         macroMappingsViewHeight
                             + getMidiKeyboardExtraHeight()));
    resized();
    repaint();
}

void PolyHostPluginEditor::updateMidiKeyboardVisibility()
{
    updateResizeLimits();

    if (mainView.isShowingRoutingView())
        resizeToRoutingView();
    else if (mainView.isShowingMacroMappingsView())
        resizeToMacroMappingsView();
}

int PolyHostPluginEditor::getMidiKeyboardExtraHeight() const
{
    return mainView.getAppSettings().getMidiKeyboardVisible()
        ? MidiKeyboardPanel::preferredHeight + 8
        : 0;
}

void PolyHostPluginEditor::updateResizeLimits()
{
    setResizeLimits(minWidth,
                    minHeight + getMidiKeyboardExtraHeight(),
                    maxWidth,
                    maxHeight);
}
