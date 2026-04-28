#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

struct StandaloneTempoState
{
    double hostTempoBpm = 120.0;
    double defaultTempoBpm = 120.0;
    bool metronomeEnabled = false;
};

class TempoToolbarButton final : public juce::ToolbarButton
{
public:
    enum class ContentType
    {
        IconGlyph,
        TextLabel
    };

    bool isVisuallyActive() const
    {
        return isActiveProvider ? isActiveProvider() : false;
    }

    TempoToolbarButton(int itemId,
                       const juce::String& tooltipText,
                       const juce::String& contentText,
                       ContentType contentTypeIn,
                       int preferredWidthIn,
                       std::function<bool()> isActiveProviderIn = {})
        : juce::ToolbarButton(itemId, "", nullptr, nullptr),
          tooltip(tooltipText),
          content(contentText),
          contentType(contentTypeIn),
          preferredWidth(preferredWidthIn),
          isActiveProvider(std::move(isActiveProviderIn))
    {
        setTooltip(tooltip);
        setWantsKeyboardFocus(false);
    }

    bool getToolbarItemSizes(int toolbarDepth, bool isVertical,
                             int& preferredSizeOut, int& minSize, int& maxSize) override
    {
        juce::ignoreUnused(toolbarDepth, isVertical);
        preferredSizeOut = preferredWidth;
        minSize = preferredWidth;
        maxSize = preferredWidth;
        return true;
    }

    void paintButtonArea(juce::Graphics& g,
                         int width, int height,
                         bool isMouseOver, bool isMouseDown) override
    {
        auto area = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(2.0f);
        // Tempo reset colour
        auto base = juce::Colour(0xFF3C3C3C);

        if (isVisuallyActive())
            base = juce::Colour(0xFF3A7BD5);
        else if (isMouseDown)
            base = base.darker(0.15f);
        else if (isMouseOver)
            base = base.brighter(0.12f);

        g.setColour(base);
        g.fillRoundedRectangle(area, 4.0f);

        g.setColour(juce::Colours::white.withAlpha(0.14f));
        g.drawRoundedRectangle(area, 4.0f, 1.0f);
    }

    void contentAreaChanged(const juce::Rectangle<int>& newArea) override
    {
        contentArea = newArea;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        auto isOver = isMouseOver(true);
        auto isDown = isMouseButtonDown();

        paintButtonArea(g, bounds.getWidth(), bounds.getHeight(), isOver, isDown);

        auto drawArea = contentArea.isEmpty() ? bounds : contentArea;
        g.setColour(isVisuallyActive() ? juce::Colours::white
                                       : juce::Colours::lightgrey);

        if (contentType == ContentType::IconGlyph)
        {
           #if JUCE_WINDOWS
            juce::Font font(juce::FontOptions("Segoe Fluent Icons", 18.0f, juce::Font::plain));
           #else
            juce::Font font(juce::FontOptions(18.0f));
           #endif
            g.setFont(font);
        }
        else
        {
            g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        }

        g.drawFittedText(content,
                         drawArea.reduced(4, 1),
                         juce::Justification::centred,
                         1);
    }

private:
    juce::String tooltip;
    juce::String content;
    ContentType contentType;
    int preferredWidth = 32;
    juce::Rectangle<int> contentArea;
    std::function<bool()> isActiveProvider;
};

class StandaloneTempoSupport
{
public:
    class HostPlayHead final : public juce::AudioPlayHead
    {
    public:
        void prepareToPlay(double newSampleRate)
        {
            const juce::ScopedLock sl(lock);
            sampleRate = (newSampleRate > 0.0 ? newSampleRate : 44100.0);
            samplePosition = 0.0;
        }

        void setTempoBpm(double newTempoBpm)
        {
            const juce::ScopedLock sl(lock);
            tempoBpm = juce::jlimit(20.0, 300.0, newTempoBpm);
        }

        void advance(int numSamples)
        {
            const juce::ScopedLock sl(lock);
            samplePosition += juce::jmax(0, numSamples);
        }

        double getSampleRate() const
        {
            const juce::ScopedLock sl(lock);
            return sampleRate;
        }

        Optional<PositionInfo> getPosition() const override
        {
            const juce::ScopedLock sl(lock);

            PositionInfo info;
            info.setIsPlaying(true);
            info.setIsRecording(false);
            info.setBpm(tempoBpm);
            info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 4, 4 });
            info.setTimeInSamples((juce::int64) samplePosition);
            info.setTimeInSeconds(samplePosition / sampleRate);

            const auto ppq = (samplePosition / sampleRate) * (tempoBpm / 60.0);
            info.setPpqPosition(ppq);
            info.setPpqPositionOfLastBarStart(std::floor(ppq / 4.0) * 4.0);

            return info;
        }

    private:
        mutable juce::CriticalSection lock;
        double sampleRate = 44100.0;
        double samplePosition = 0.0;
        double tempoBpm = 120.0;
    };

    class PlaybackTracker final : public juce::AudioIODeviceCallback
    {
    public:
        PlaybackTracker(HostPlayHead& playHeadIn, StandaloneTempoState& stateIn)
            : playHead(playHeadIn), state(stateIn) {}

        void setWrappedCallback(juce::AudioIODeviceCallback* callbackToWrap)
        {
            wrappedCallback = callbackToWrap;
        }

        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override;

        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;

    private:
        void renderMetronome(float* const* outputChannelData,
                             int numOutputChannels,
                             int numSamples,
                             double sampleRate);

        HostPlayHead& playHead;
        StandaloneTempoState& state;
        juce::AudioIODeviceCallback* wrappedCallback = nullptr;
        juce::int64 processedSamples = 0;
        int samplesRemainingInClick = 0;
        double clickPhase = 0.0;
        double clickFrequencyHz = 1000.0;
        int beatIndex = 0;
        double currentSampleRate = 44100.0;
    };

    class TempoStripComponent final : public juce::Component,
                                      private juce::Timer
    {
    public:
        explicit TempoStripComponent(StandaloneTempoSupport& ownerIn);
        ~TempoStripComponent() override;

        void resized() override;
        void refreshUi();

    private:
        class TapButtonLookAndFeel final : public juce::LookAndFeel_V4
        {
        public:
            void drawButtonBackground(juce::Graphics& g,
                                      juce::Button& button,
                                      const juce::Colour&,
                                      bool isMouseOverButton,
                                      bool isButtonDown) override
            {
                auto area = button.getLocalBounds().toFloat().reduced(1.0f);

                auto base = juce::Colour(0xFF3C3C3C);

                if (isButtonDown)
                    base = base.darker(0.15f);
                else if (isMouseOverButton)
                    base = base.brighter(0.12f);

                g.setColour(base);
                g.fillRoundedRectangle(area, 4.0f);

                g.setColour(juce::Colours::white.withAlpha(0.14f));
                g.drawRoundedRectangle(area, 4.0f, 1.0f);
            }
        };

        class TempoTextEditor final : public juce::TextEditor
        {
        public:
            explicit TempoTextEditor(TempoStripComponent& ownerIn) : owner(ownerIn) {}

            void mouseWheelMove(const juce::MouseEvent& event,
                                const juce::MouseWheelDetails& wheel) override;

            void mouseDoubleClick(const juce::MouseEvent& event) override;
            bool keyPressed(const juce::KeyPress& key) override;
            void focusLost(FocusChangeType cause) override;

        private:
            TempoStripComponent& owner;
        };

        class BeatIndicatorComponent final : public juce::Component,
                                             private juce::Timer
        {
        public:
            explicit BeatIndicatorComponent(StandaloneTempoSupport& ownerIn);

            void paint(juce::Graphics& g) override;

        private:
            void timerCallback() override;

            StandaloneTempoSupport& owner;
        };

        void timerCallback() override;
        void setTempoBpm(double bpm);
        void resetTempo();
        void registerTapTempo();
        void commitTempoFromEditor();
        void updateTooltip();
        void updateControlState();

        StandaloneTempoSupport& owner;
        BeatIndicatorComponent beatIndicator;
        TempoTextEditor tempoEditor;
        TempoToolbarButton resetTempoButton
        {
            0,
            "Reset tempo",
            juce::String::charToString((juce_wchar) 0xe777),
            TempoToolbarButton::ContentType::IconGlyph,
            32
        };
        TempoToolbarButton metronomeButton
        {
            0,
            "Metronome",
            juce::String::charToString((juce_wchar) 0xe15d),
            TempoToolbarButton::ContentType::IconGlyph,
            32,
            [this] { return owner.isMetronomeEnabled(); }
        };
        juce::TextButton tapTempoButton { "Tap" };
        TapButtonLookAndFeel tapButtonLookAndFeel;
    };

    explicit StandaloneTempoSupport(AudioEngine& audioEngineIn);

    const StandaloneTempoState& getState() const;

    void initialise(juce::AudioDeviceManager& deviceManager);
    void shutdown(juce::AudioDeviceManager& deviceManager);
    void setWrappedAudioCallback(juce::AudioIODeviceCallback* callback);
    std::unique_ptr<juce::Component> createTempoStripComponent();

    void setHostTempoBpm(double bpm);
    void registerTapTempo();
    void setDefaultTempoBpm(double bpm);
    void setMetronomeEnabled(bool shouldBeEnabled);
    bool isMetronomeEnabled() const;
    void toggleMetronome();
    double getHostTempoBpm() const;
    double getDefaultTempoBpm() const;
    double getDisplayedTempoBpm() const;
    int getCurrentBeatInBar() const;
    double getCurrentBeatProgress() const;
    bool isTempoEditorInteractive() const;
    void refreshTempoStrip();

    juce::AudioPlayHead* getPlayHead() { return &hostPlayHead; }
    std::function<void()> onTempoChanged;

private:
    void pushEffectiveTempoToEngine();

    AudioEngine& audioEngine;
    StandaloneTempoState state;
    HostPlayHead hostPlayHead;
    PlaybackTracker playbackTracker { hostPlayHead, state };
    juce::Array<double> tapTimesMs;
    bool trackerInitialised = false;
    TempoStripComponent* tempoStripComponent = nullptr;
};