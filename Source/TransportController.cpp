#include "TransportController.h"
#include <cmath>

TransportController::TransportController(AudioEngine& audioEngineIn)
    : audioEngine(audioEngineIn)
{
}

const TransportState& TransportController::getState() const
{
    return state;
}

void TransportController::setDefaultTempoBpm(double bpm)
{
    state.defaultTempoBpm = juce::jlimit(20.0, 300.0, bpm);
}

void TransportController::setHostTempoBpm(double bpm)
{
    bpm = juce::jlimit(20.0, 300.0, bpm);
    bpm = std::round(bpm * 10.0) / 10.0;

    state.hostTempoBpm = bpm;
    audioEngine.setTempoBpm(state.hostTempoBpm);
}

double TransportController::getHostTempoBpm() const
{
    return state.hostTempoBpm;
}

double TransportController::getDefaultTempoBpm() const
{
    return state.defaultTempoBpm;
}

bool TransportController::isHostSyncEnabled() const
{
    return state.hostSyncEnabled;
}

void TransportController::setHostSyncEnabled(bool enabled)
{
    state.hostSyncEnabled = enabled;
    audioEngine.setHostSyncEnabled(enabled);
    lastDisplayedSyncedTempoBpm = -1.0;
    lastDisplayedHostTempoAvailable = audioEngine.isExternalHostTempoAvailable();
}

void TransportController::registerTapTempo()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if (!tapTimesMs.isEmpty())
    {
        const double gapSinceLastTap = nowMs - tapTimesMs.getLast();

        if (gapSinceLastTap > 2000.0)
            tapTimesMs.clear();
    }

    tapTimesMs.add(nowMs);

    while (tapTimesMs.size() > 6)
        tapTimesMs.remove(0);

    if (tapTimesMs.size() < 2)
        return;

    juce::Array<double> validIntervalsMs;

    for (int i = 1; i < tapTimesMs.size(); ++i)
    {
        const double intervalMs = tapTimesMs[i] - tapTimesMs[i - 1];

        if (intervalMs >= 200.0 && intervalMs <= 3000.0)
            validIntervalsMs.add(intervalMs);
    }

    if (validIntervalsMs.isEmpty())
        return;

    double totalMs = 0.0;

    for (auto interval : validIntervalsMs)
        totalMs += interval;

    const double averageIntervalMs = totalMs / (double) validIntervalsMs.size();
    const double bpm = 60000.0 / averageIntervalMs;

    setHostTempoBpm(bpm);
}

double TransportController::getDisplayedTempoBpm() const
{
    return state.hostSyncEnabled ? audioEngine.getCurrentTempoBpm()
                                 : state.hostTempoBpm;
}

bool TransportController::isTempoEditorInteractive() const
{
    return !state.hostSyncEnabled;
}

bool TransportController::isExternalHostTempoAvailable() const
{
    return audioEngine.isExternalHostTempoAvailable();
}

bool TransportController::refreshFromEngineIfNeeded()
{
    if (!state.hostSyncEnabled)
        return false;

    const double currentTempo = audioEngine.getCurrentTempoBpm();
    const bool hostTempoAvailable = audioEngine.isExternalHostTempoAvailable();

    const bool tempoChanged = std::abs(currentTempo - lastDisplayedSyncedTempoBpm) >= 0.05;
    const bool availabilityChanged = hostTempoAvailable != lastDisplayedHostTempoAvailable;

    if (tempoChanged || availabilityChanged)
    {
        lastDisplayedSyncedTempoBpm = currentTempo;
        lastDisplayedHostTempoAvailable = hostTempoAvailable;
        return true;
    }

    return false;
}