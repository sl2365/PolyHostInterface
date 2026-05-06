#pragma once
#include <JuceHeader.h>

namespace ButtonStyling
{
    enum class Variant
    {
        Standard,
        Active,
        Destructive
    };

    namespace Glyphs
    {
        juce::String save();
        juce::String saveAs();
        juce::String revert();
        juce::String panic();
        juce::String routing();
        juce::String fitWindow();
        juce::String saveWindowSize();
        juce::String clearWindowSize();
        juce::String close();
        juce::String reset();
        juce::String metronome();
        juce::String tapTempo();
        juce::String add();
        juce::String arrowUp();
        juce::String arrowDown();
        juce::String activeTick();
        juce::String bypassCross();
        juce::String info();
        juce::String pointerControl();
        juce::String pointerTabSettings();
    }

    namespace Tooltips
    {
        juce::String savePreset();
        juce::String savePresetAs();
        juce::String revertPreset();
        juce::String midiPanic();
        juce::String routing();
        juce::String fitWindow();
        juce::String saveWindowSize();
        juce::String clearWindowSize();
        juce::String closeTab();
        juce::String resetTempo();
        juce::String metronome();
        juce::String tapTempo();
        juce::String addTab();
        juce::String refreshMidi();
        juce::String viewTab();
        juce::String midiAssignments();
        juce::String toggleBypass();
        juce::String moveUp();
        juce::String moveDown();
        juce::String routingInfo();
        juce::String pointerControl();
        juce::String resetPointerSettings();
        juce::String useTabPointerSettings();
    }

    namespace Labels
    {
        juce::String midi();
        juce::String synth();
        juce::String fx();
        juce::String empty();
    }

    juce::Colour defaultBackground();
    juce::Colour activeBackground();
    juce::Colour destructiveBackground();
    juce::Colour bypassActiveBackground();
    juce::Colour bypassInactiveBackground();
    juce::Colour outlineColour();
    juce::Colour textColour(bool isActive);

    float defaultCornerRadius();
    float badgeCornerRadius();
    int defaultButtonHeight();
    int defaultButtonWidth();
    float defaultIconSize();

    juce::Colour backgroundForVariant(Variant variant);
    juce::Colour resolveBackgroundColour(juce::Colour baseColour,
                                         bool isMouseOver,
                                         bool isMouseDown,
                                         bool isActive);

    juce::Font iconFont(float height = defaultIconSize());
    juce::Font textFont(float height, bool bold = false);

    void drawButtonBackground(juce::Graphics& g,
                              juce::Rectangle<float> area,
                              juce::Colour baseColour,
                              bool isMouseOver,
                              bool isMouseDown,
                              bool isActive = false,
                              float cornerRadius = 4.0f);

    void drawButtonBackgroundForVariant(juce::Graphics& g,
                                        juce::Rectangle<float> area,
                                        Variant variant,
                                        bool isMouseOver,
                                        bool isMouseDown,
                                        bool isActive = false,
                                        float cornerRadius = 4.0f);

    void drawIcon(juce::Graphics& g,
                  const juce::String& glyph,
                  juce::Rectangle<int> area,
                  bool isActive = false,
                  float fontHeight = 14.0f,
                  int yOffset = 0);

    void drawLabel(juce::Graphics& g,
                   const juce::String& text,
                   juce::Rectangle<int> area,
                   bool isActive = false,
                   float fontHeight = 14.0f,
                   bool bold = true);
                   
    class ToolbarIconButton final : public juce::ToolbarButton
    {
    public:
        enum class ContentType
        {
            IconGlyph,
            TextLabel
        };

        ToolbarIconButton(int itemId,
                          const juce::String& tooltipText,
                          const juce::String& contentText,
                          ContentType contentTypeIn,
                          int preferredWidthIn,
                          std::function<bool()> isActiveProviderIn = {},
                          juce::Colour baseColourIn = defaultBackground(),
                          int iconYOffsetIn = 0,
                          float iconFontHeightIn = defaultIconSize());

        bool isVisuallyActive() const;

        bool getToolbarItemSizes(int toolbarDepth, bool isVertical,
                                 int& preferredSizeOut, int& minSize, int& maxSize) override;

        void paintButtonArea(juce::Graphics& g,
                             int width, int height,
                             bool isMouseOver, bool isMouseDown) override;

        void contentAreaChanged(const juce::Rectangle<int>& newArea) override;
        void paint(juce::Graphics& g) override;

    private:
        juce::String tooltip;
        juce::String content;
        ContentType contentType;
        int preferredWidth = 32;
        int iconYOffset = 0;
        float iconFontHeight = defaultIconSize();
        juce::Rectangle<int> contentArea;
        std::function<bool()> isActiveProvider;
        juce::Colour baseColour;
    };

    class SmallIconButton final : public juce::Button
    {
    public:
        explicit SmallIconButton(const juce::String& glyphText,
                                 juce::Colour baseColourIn = defaultBackground(),
                                 float cornerRadiusIn = defaultCornerRadius(),
                                 float iconFontHeightIn = defaultIconSize(),
                                 int iconYOffsetIn = 1);

        void paintButton(juce::Graphics& g,
                         bool isMouseOverButton,
                         bool isButtonDown) override;

    private:
        juce::String glyph;
        juce::Colour baseColour;
        float cornerRadius = defaultCornerRadius();
        float iconFontHeight = defaultIconSize();
        int iconYOffset = 1;
    };

    class RoundedTextButtonLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        explicit RoundedTextButtonLookAndFeel(float cornerRadiusIn = defaultCornerRadius());

        void drawButtonBackground(juce::Graphics& g,
                                  juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool isMouseOverButton,
                                  bool isButtonDown) override;

        void drawButtonText(juce::Graphics& g,
                            juce::TextButton& button,
                            bool isMouseOverButton,
                            bool isButtonDown) override;

    private:
        float cornerRadius = defaultCornerRadius();
    };

    class StatusIconButton final : public juce::Button
    {
    public:
        explicit StatusIconButton(const juce::String& activeGlyphText,
                                  const juce::String& inactiveGlyphText,
                                  juce::Colour activeColourIn,
                                  juce::Colour inactiveColourIn,
                                  float cornerRadiusIn = defaultCornerRadius(),
                                  float iconFontHeightIn = defaultIconSize(),
                                  int iconYOffsetIn = 0);

        void setVisualState(bool shouldShowActiveState);
        void paintButton(juce::Graphics& g,
                         bool isMouseOverButton,
                         bool isButtonDown) override;

    private:
        juce::String activeGlyph;
        juce::String inactiveGlyph;
        juce::Colour activeColour;
        juce::Colour inactiveColour;
        float cornerRadius = defaultCornerRadius();
        float iconFontHeight = defaultIconSize();
        int iconYOffset = 0;
        bool visualStateActive = true;
    };

    class TypeBadgeButton final : public juce::TextButton
    {
    public:
        TypeBadgeButton();

        void paintButton(juce::Graphics& g,
                         bool isMouseOverButton,
                         bool isButtonDown) override;
    };
}