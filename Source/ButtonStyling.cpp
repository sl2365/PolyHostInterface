#include "ButtonStyling.h"

namespace ButtonStyling
{
    namespace Glyphs
    {
        juce::String save()            { return juce::String::charToString((juce_wchar) 0xe74e); }
        juce::String saveAs()          { return juce::String::charToString((juce_wchar) 0xe792); }
        juce::String revert()          { return juce::String::charToString((juce_wchar) 0xe72c); }
        juce::String panic()           { return juce::String::charToString((juce_wchar) 0xe783); }
        juce::String routing()         { return juce::String::charToString((juce_wchar) 0xf003); }
        juce::String fitWindow()       { return juce::String::charToString((juce_wchar) 0xe9a6); }
        juce::String saveWindowSize()  { return juce::String::charToString((juce_wchar) 0xe78c); }
        juce::String clearWindowSize() { return juce::String::charToString((juce_wchar) 0xea39); }
        juce::String close()           { return juce::String::charToString((juce_wchar) 0xea39); }
        juce::String reset()           { return juce::String::charToString((juce_wchar) 0xe777); }
        juce::String metronome()       { return juce::String::charToString((juce_wchar) 0xe15d); }
        juce::String tapTempo()        { return juce::String::charToString((juce_wchar) 0xf271); }
        juce::String add()             { return "+"; }
        juce::String arrowUp()         { return juce::String::charToString((juce_wchar) 0xe70e); }
        juce::String arrowDown()       { return juce::String::charToString((juce_wchar) 0xe70d); }
        juce::String activeTick()      { return juce::String::charToString((juce_wchar) 0xe930); }
        juce::String bypassCross()     { return juce::String::charToString((juce_wchar) 0xf140); }
        juce::String info()            { return juce::String::charToString((juce_wchar) 0xe946); }
        juce::String pointerControl()  { return juce::String::charToString((juce_wchar) 0xe7c9); }
        juce::String pointerTabSettings() { return juce::String::charToString((juce_wchar) 0xe7c9); }
    }

    namespace Tooltips
    {
        juce::String savePreset()       { return "Save Preset"; }
        juce::String savePresetAs()     { return "Save Preset As"; }
        juce::String revertPreset()     { return "Revert preset"; }
        juce::String midiPanic()        { return "MIDI Panic"; }
        juce::String routing()          { return "Toggle Routing View"; }
        juce::String fitWindow()        { return "Size window to current plugin"; }
        juce::String saveWindowSize()   { return "Save window size for this tab"; }
        juce::String clearWindowSize()  { return "Clear saved window size for this tab"; }
        juce::String closeTab()         { return "Close Tab"; }
        juce::String metronome()        { return "Metronome"; }
        juce::String resetTempo()       { return "Reset tempo"; }
        juce::String tapTempo()         { return "Tap Tempo"; }
        juce::String addTab()           { return "New Tab"; }
        juce::String refreshMidi()      { return "Refresh MIDI Devices"; }
        juce::String viewTab()          { return "View Tab"; }
        juce::String midiAssignments()  { return "MIDI Assignments"; }
        juce::String toggleBypass()     { return "Toggle Bypass"; }
        juce::String moveUp()           { return "Move Up"; }
        juce::String moveDown()         { return "Move Down"; }
        juce::String routingInfo()      { return "Routing Info"; }
        juce::String pointerControl()   { return "Toggle Pointer Control View"; }
        juce::String resetPointerSettings()  { return "Reset Pointer Settings"; }
        juce::String useTabPointerSettings() { return "Use Tab Pointer Settings"; }
    }

    namespace Labels
    {
        juce::String midi()      { return "MIDI"; }
        juce::String synth()     { return "Synth"; }
        juce::String fx()        { return "FX"; }
        juce::String empty()     { return "Empty"; }
    }

    juce::Colour defaultBackground()
    {
        return juce::Colour::fromRGB(38, 50, 56);
    }

    juce::Colour activeBackground()
    {
        return juce::Colour(0xFF3A7BD5);
    }

    juce::Colour destructiveBackground()
    {
        return juce::Colour(0xFF8B2E2E);
    }

    juce::Colour bypassActiveBackground()
    {
        return juce::Colour(0xFF1E854A);
    }

    juce::Colour bypassInactiveBackground()
    {
        return juce::Colour(0xFF8B2E2E);
    }

    juce::Colour outlineColour()
    {
        return juce::Colours::white.withAlpha(0.14f);
    }

    juce::Colour textColour(bool isActive)
    {
        return isActive ? juce::Colours::white
                        : juce::Colours::lightgrey;
    }

    float defaultCornerRadius()
    {
        return 4.0f;
    }

    float badgeCornerRadius()
    {
        return 9.0f;
    }

    int defaultButtonHeight()
    {
        return 29;
    }

    int defaultButtonWidth()
    {
        return 30;
    }

    float defaultIconSize()
    {
        return 16.0f;
    }

    juce::Colour backgroundForVariant(Variant variant)
    {
        switch (variant)
        {
            case Variant::Standard:    return defaultBackground();
            case Variant::Active:      return activeBackground();
            case Variant::Destructive: return destructiveBackground();
        }

        return defaultBackground();
    }

    juce::Colour resolveBackgroundColour(juce::Colour baseColour,
                                         bool isMouseOver,
                                         bool isMouseDown,
                                         bool isActive)
    {
        if (isActive)
            return activeBackground();

        if (isMouseDown)
            return baseColour.darker(0.15f);

        if (isMouseOver)
            return baseColour.brighter(0.12f);

        return baseColour;
    }

    juce::Font iconFont(float height)
    {
       #if JUCE_WINDOWS
        return juce::Font(juce::FontOptions("Segoe Fluent Icons", height, juce::Font::plain));
       #else
        return juce::Font(juce::FontOptions(height));
       #endif
    }

    juce::Font textFont(float height, bool bold)
    {
        return juce::Font(juce::FontOptions(height,
                                            bold ? juce::Font::bold
                                                 : juce::Font::plain));
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Rectangle<float> area,
                              juce::Colour baseColour,
                              bool isMouseOver,
                              bool isMouseDown,
                              bool isActive,
                              float cornerRadius)
    {
        auto background = resolveBackgroundColour(baseColour, isMouseOver, isMouseDown, isActive);

        g.setColour(background);
        g.fillRoundedRectangle(area, cornerRadius);

        g.setColour(outlineColour());
        g.drawRoundedRectangle(area, cornerRadius, 1.0f);
    }

    void drawButtonBackgroundForVariant(juce::Graphics& g,
                                        juce::Rectangle<float> area,
                                        Variant variant,
                                        bool isMouseOver,
                                        bool isMouseDown,
                                        bool isActive,
                                        float cornerRadius)
    {
        drawButtonBackground(g,
                             area,
                             backgroundForVariant(variant),
                             isMouseOver,
                             isMouseDown,
                             isActive,
                             cornerRadius);
    }

    void drawIcon(juce::Graphics& g,
                  const juce::String& glyph,
                  juce::Rectangle<int> area,
                  bool isActive,
                  float fontHeight,
                  int yOffset)
    {
        g.setColour(textColour(isActive));
        g.setFont(iconFont(fontHeight));
        g.drawFittedText(glyph,
                         area.translated(0, yOffset),
                         juce::Justification::centred,
                         1);
    }

    void drawLabel(juce::Graphics& g,
                   const juce::String& text,
                   juce::Rectangle<int> area,
                   bool isActive,
                   float fontHeight,
                   bool bold)
    {
        g.setColour(textColour(isActive));
        g.setFont(textFont(fontHeight, bold));
        g.drawFittedText(text,
                         area,
                         juce::Justification::centred,
                         1);
    }

    ToolbarIconButton::ToolbarIconButton(int itemId,
                                         const juce::String& tooltipText,
                                         const juce::String& contentText,
                                         ContentType contentTypeIn,
                                         int preferredWidthIn,
                                         std::function<bool()> isActiveProviderIn,
                                         juce::Colour baseColourIn,
                                         int iconYOffsetIn,
                                         float iconFontHeightIn)
        : juce::ToolbarButton(itemId, "", nullptr, nullptr),
          tooltip(tooltipText),
          content(contentText),
          contentType(contentTypeIn),
          preferredWidth(preferredWidthIn),
          iconYOffset(iconYOffsetIn),
          iconFontHeight(iconFontHeightIn),
          isActiveProvider(std::move(isActiveProviderIn)),
          baseColour(baseColourIn)
    {
        setTooltip(tooltip);
        setWantsKeyboardFocus(false);
    }

    bool ToolbarIconButton::isVisuallyActive() const
    {
        return isActiveProvider ? isActiveProvider() : false;
    }

    bool ToolbarIconButton::getToolbarItemSizes(int toolbarDepth, bool isVertical,
                                                int& preferredSizeOut, int& minSize, int& maxSize)
    {
        juce::ignoreUnused(toolbarDepth, isVertical);
        preferredSizeOut = preferredWidth;
        minSize = preferredWidth;
        maxSize = preferredWidth;
        return true;
    }

    void ToolbarIconButton::paintButtonArea(juce::Graphics& g,
                                            int width, int height,
                                            bool isMouseOver, bool isMouseDown)
    {
        constexpr float verticalGap = 2.0f;

        auto area = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(2.0f, 0.0f);
        area = area.withHeight((float) defaultButtonHeight())
                   .withCentre(area.getCentre())
                   .reduced(0.0f, verticalGap);

        drawButtonBackground(g,
                             area,
                             baseColour,
                             isMouseOver,
                             isMouseDown,
                             isVisuallyActive(),
                             defaultCornerRadius());
    }
    
    void ToolbarIconButton::contentAreaChanged(const juce::Rectangle<int>& newArea)
    {
        contentArea = newArea;
    }

    void ToolbarIconButton::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        auto isOver = isMouseOver(true);
        auto isDown = isMouseButtonDown();

        paintButtonArea(g, bounds.getWidth(), bounds.getHeight(), isOver, isDown);

        auto drawArea = contentArea.isEmpty() ? bounds : contentArea;
        auto contentBounds = drawArea.reduced(4, 1);

        if (contentType == ContentType::IconGlyph)
        {
            drawIcon(g,
                     content,
                     contentBounds,
                     isVisuallyActive(),
                     iconFontHeight,
                     iconYOffset);
        }
        else
        {
            drawLabel(g,
                      content,
                      contentBounds,
                      isVisuallyActive(),
                      14.0f,
                      true);
        }
    }

    SmallIconButton::SmallIconButton(const juce::String& glyphText,
                                     juce::Colour baseColourIn,
                                     float cornerRadiusIn,
                                     float iconFontHeightIn,
                                     int iconYOffsetIn)
        : juce::Button("SmallIconButton"),
          glyph(glyphText),
          baseColour(baseColourIn),
          cornerRadius(cornerRadiusIn),
          iconFontHeight(iconFontHeightIn),
          iconYOffset(iconYOffsetIn)
    {
    }

    void SmallIconButton::paintButton(juce::Graphics& g,
                                      bool isMouseOverButton,
                                      bool isButtonDown)
    {
        auto area = getLocalBounds().toFloat().reduced(0.5f);

        auto drawColour = baseColour;
        const bool enabled = isEnabled();

        if (! enabled)
            drawColour = drawColour.withAlpha(0.5f);

        drawButtonBackground(g,
                             area,
                             drawColour,
                             enabled ? isMouseOverButton : false,
                             enabled ? isButtonDown : false,
                             false,
                             cornerRadius);

        auto iconColour = textColour(false);

        if (! enabled)
            iconColour = iconColour.withAlpha(0.5f);

        g.setColour(iconColour);
        g.setFont(iconFont(iconFontHeight));
        g.drawFittedText(glyph,
                         getLocalBounds().translated(0, iconYOffset),
                         juce::Justification::centred,
                         1);
    }

    RoundedTextButtonLookAndFeel::RoundedTextButtonLookAndFeel(float cornerRadiusIn)
        : cornerRadius(cornerRadiusIn)
    {
    }

    void RoundedTextButtonLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                            juce::Button& button,
                                                            const juce::Colour& backgroundColour,
                                                            bool isMouseOverButton,
                                                            bool isButtonDown)
    {
        juce::ignoreUnused(backgroundColour);

        auto area = button.getLocalBounds().toFloat().reduced(1.0f);

        auto baseColour = button.findColour(button.getToggleState() ? juce::TextButton::buttonOnColourId
                                                                    : juce::TextButton::buttonColourId);

        if (! button.isEnabled())
            baseColour = baseColour.withAlpha(0.5f);

        ButtonStyling::drawButtonBackground(g,
                                            area,
                                            baseColour,
                                            isMouseOverButton,
                                            isButtonDown,
                                            button.getToggleState(),
                                            cornerRadius);
    }

    void RoundedTextButtonLookAndFeel::drawButtonText(juce::Graphics& g,
                                                      juce::TextButton& button,
                                                      bool isMouseOverButton,
                                                      bool isButtonDown)
    {
        juce::ignoreUnused(isMouseOverButton, isButtonDown);

        auto textColour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                    : juce::TextButton::textColourOffId);

        if (! button.isEnabled())
            textColour = textColour.withAlpha(0.5f);

        g.setColour(textColour);

        auto font = getTextButtonFont(button, button.getHeight());
        g.setFont(font);

        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().reduced(6, 2),
                         juce::Justification::centred,
                         1);
    }

    StatusIconButton::StatusIconButton(const juce::String& activeGlyphText,
                                       const juce::String& inactiveGlyphText,
                                       juce::Colour activeColourIn,
                                       juce::Colour inactiveColourIn,
                                       float cornerRadiusIn,
                                       float iconFontHeightIn,
                                       int iconYOffsetIn)
        : juce::Button("StatusIconButton"),
          activeGlyph(activeGlyphText),
          inactiveGlyph(inactiveGlyphText),
          activeColour(activeColourIn),
          inactiveColour(inactiveColourIn),
          cornerRadius(cornerRadiusIn),
          iconFontHeight(iconFontHeightIn),
          iconYOffset(iconYOffsetIn)
    {
    }

    void StatusIconButton::setVisualState(bool shouldShowActiveState)
    {
        if (visualStateActive == shouldShowActiveState)
            return;

        visualStateActive = shouldShowActiveState;
        repaint();
    }

    void StatusIconButton::paintButton(juce::Graphics& g,
                                       bool isMouseOverButton,
                                       bool isButtonDown)
    {
        auto area = getLocalBounds().toFloat().reduced(0.5f);

        auto baseColour = visualStateActive ? activeColour
                                            : inactiveColour;

        if (! isEnabled())
            baseColour = baseColour.withAlpha(0.5f);

        drawButtonBackground(g,
                             area,
                             baseColour,
                             isEnabled() ? isMouseOverButton : false,
                             isEnabled() ? isButtonDown : false,
                             false,
                             cornerRadius);

        auto iconColour = textColour(false);

        if (! isEnabled())
            iconColour = iconColour.withAlpha(0.5f);

        g.setColour(iconColour);
        g.setFont(iconFont(iconFontHeight));

        g.drawFittedText(visualStateActive ? activeGlyph : inactiveGlyph,
                         getLocalBounds().translated(0, iconYOffset),
                         juce::Justification::centred,
                         1);
    }

    TypeBadgeButton::TypeBadgeButton()
    {
    }

    void TypeBadgeButton::paintButton(juce::Graphics& g,
                                      bool isMouseOverButton,
                                      bool isButtonDown)
    {
        auto area = getLocalBounds().toFloat().reduced(0.5f);

        auto baseColour = findColour(juce::TextButton::buttonColourId);

        if (isButtonDown)
            baseColour = baseColour.darker(0.15f);
        else if (isMouseOverButton)
            baseColour = baseColour.brighter(0.15f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(area, badgeCornerRadius());

        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.drawRoundedRectangle(area, badgeCornerRadius(), 1.0f);

        g.setColour(findColour(juce::TextButton::textColourOffId));
        g.setFont(textFont(14.0f, true));
        g.drawFittedText(getButtonText(),
                         getLocalBounds().reduced(6, 2),
                         juce::Justification::centred,
                         1);
    }
}