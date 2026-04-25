#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "TransportState.h"

class TransportController
{
public:
    explicit TransportController(AudioEngine& audioEngineIn);

    const TransportState& getState() const;

    void setHostTempoBpm(double bpm);
    void setHostSyncEnabled(bool enabled);
    void registerTapTempo();
    void setDefaultTempoBpm(double bpm);

    double getHostTempoBpm() const;
    double getDefaultTempoBpm() const;
    bool isHostSyncEnabled() const;

    double getDisplayedTempoBpm() const;
    bool isTempoEditorInteractive() const;
    bool isExternalHostTempoAvailable() const;

    bool refreshFromEngineIfNeeded();

private:
    AudioEngine& audioEngine;
    TransportState state;
    juce::Array<double> tapTimesMs;
    double lastDisplayedSyncedTempoBpm = -1.0;
    bool lastDisplayedHostTempoAvailable = false;
};