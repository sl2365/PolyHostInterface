#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "ButtonStyling.h"

struct StandaloneTempoState
{
    double hostTempoBpm = 120.0;
    double defaultTempoBpm = 120.0;
    bool metronomeEnabled = false;
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
        class TempoTextEditor final : public juce::TextEditor
        {
        public:
            explicit TempoTextEditor(TempoStripComponent& ownerIn) : owner(ownerIn) {}

            void mouseWheelMove(const juce::MouseEvent& event,
                                const juce::MouseWheelDetails& wheel) override;

            void mouseDoubleClick(const juce::MouseEvent& event) override;
            void addPopupMenuItems(juce::PopupMenu& menu,
                                   const juce::MouseEvent* mouseClickEvent) override;
            void performPopupMenuAction(int menuItemID) override;
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
        ButtonStyling::ToolbarIconButton resetTempoButton
        {
            0,
            ButtonStyling::Tooltips::resetTempo(),
            ButtonStyling::Glyphs::reset(),
            ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
            ButtonStyling::defaultButtonWidth()
        };
        ButtonStyling::ToolbarIconButton metronomeButton
        {
            0,
            ButtonStyling::Tooltips::metronome(),
            ButtonStyling::Glyphs::metronome(),
            ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
            ButtonStyling::defaultButtonWidth(),
            [this] { return owner.isMetronomeEnabled(); }
        };
        ButtonStyling::ToolbarIconButton tapTempoButton
        {
            0,
            ButtonStyling::Tooltips::tapTempo(),
            ButtonStyling::Glyphs::tapTempo(),
            ButtonStyling::ToolbarIconButton::ContentType::IconGlyph,
            ButtonStyling::defaultButtonWidth(),
            {},
            ButtonStyling::defaultBackground(),
            0,
            ButtonStyling::defaultIconSize() + 2.0f
        };
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
    std::function<void(double)> onSetCurrentTempoAsDefaultRequested;

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