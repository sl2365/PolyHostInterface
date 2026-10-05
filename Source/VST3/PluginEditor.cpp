#include "PluginEditor.h"

PolyHostPluginEditor::PolyHostPluginEditor(PolyHostPluginProcessor& p,
                                           MainView::MenuExtension* menuExtension)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      mainView(p, menuExtension)
{
    addAndMakeVisible(mainView);
    simpleRoutingViewWidth =
        mainView.getAppSettings().getRoutingWindowWidth();
    simpleRoutingViewHeight =
        mainView.getAppSettings().getRoutingWindowHeight();
    advancedRoutingViewWidth =
        mainView.getAppSettings().getAdvancedRoutingWindowWidth();
    advancedRoutingViewHeight =
        mainView.getAppSettings().getAdvancedRoutingWindowHeight();
    macroMappingsViewHeight =
        mainView.getAppSettings().getMacroMappingsViewHeight();
    setResizable(true, false);
    updateResizeLimits();
    setSize(defaultWidth, defaultHeight + getMidiKeyboardExtraHeight());
}

PolyHostPluginEditor::~PolyHostPluginEditor()
{
    mainView.getAppSettings().setRoutingWindowSize(
        simpleRoutingViewWidth,
        simpleRoutingViewHeight);
    mainView.getAppSettings().setAdvancedRoutingWindowSize(
        advancedRoutingViewWidth,
        advancedRoutingViewHeight);
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
        const int width = juce::jlimit(
            minWidth,
            maxWidth,
            getWidth());
        const int height = juce::jlimit(
            minHeight,
            maxHeight - getMidiKeyboardExtraHeight(),
            contentHeight);

        if (mainView.isShowingAdvancedRoutingView())
        {
            advancedRoutingViewWidth = width;
            advancedRoutingViewHeight = height;
            audioProcessor.getCore().setAdvancedRoutingViewSize(
                width,
                height);
        }
        else
        {
            simpleRoutingViewWidth = width;
            simpleRoutingViewHeight = height;
            audioProcessor.getCore().setRoutingViewSize(
                width,
                height);
        }
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
    updateResizeLimits();

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

    const auto maximumSize = getMaximumEditorSizeForCurrentDisplay();
    const int clampedWidth = juce::jlimit(
        minWidth, maximumSize.x, desiredWidth);
    const int clampedHeight = juce::jlimit(
        minHeight, maximumSize.y, desiredHeight);

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

    const bool advanced =
        mainView.isShowingAdvancedRoutingView();
    const int targetWidth =
        advanced
            ? mainView.getAppSettings()
                  .getAdvancedRoutingWindowWidth()
            : mainView.getAppSettings()
                  .getRoutingWindowWidth();
    const int targetHeight =
        advanced
            ? mainView.getAppSettings()
                  .getAdvancedRoutingWindowHeight()
            : mainView.getAppSettings()
                  .getRoutingWindowHeight();

    if (advanced)
    {
        advancedRoutingViewWidth = targetWidth;
        advancedRoutingViewHeight = targetHeight;
    }
    else
    {
        simpleRoutingViewWidth = targetWidth;
        simpleRoutingViewHeight = targetHeight;
    }

    setSize(juce::jlimit(minWidth, maxWidth, targetWidth),
            juce::jlimit(minHeight + getMidiKeyboardExtraHeight(),
                         maxHeight,
                         targetHeight + getMidiKeyboardExtraHeight()));
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

juce::Point<int>
PolyHostPluginEditor::getMaximumEditorSizeForCurrentDisplay() const
{
    auto maximumWidth = maxWidth;
    auto maximumHeight = maxHeight;

    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = displays.getDisplayForRect(getScreenBounds());

    if (display != nullptr)
    {
        auto usableBounds =
            display->userBounds.getLargestIntegerWithin();

        if (auto* peer = getPeer())
        {
            if (const auto frameSize = peer->getFrameSizeIfPresent())
                frameSize->subtractFrom(usableBounds);
        }

        maximumWidth = juce::jmax(maximumWidth,
                                  usableBounds.getWidth());
        maximumHeight = juce::jmax(maximumHeight,
                                   usableBounds.getHeight());
    }

    return { maximumWidth, maximumHeight };
}

void PolyHostPluginEditor::updateResizeLimits()
{
    const auto maximumSize = getMaximumEditorSizeForCurrentDisplay();

    setResizeLimits(minWidth,
                    minHeight + getMidiKeyboardExtraHeight(),
                    maximumSize.x,
                    maximumSize.y);
}
