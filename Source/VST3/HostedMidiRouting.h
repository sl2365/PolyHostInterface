#pragma once

namespace phi_midi_routing
{
    inline bool shouldUsePreservedInputForDownstream(
        bool sourceIsSynth) noexcept
    {
        return sourceIsSynth;
    }

    inline bool isMidiOnlyEffect(bool isEffect,
                                 bool isBypassed,
                                 bool isQuarantined,
                                 bool producesMidi,
                                 int audioInputChannels,
                                 int audioOutputChannels) noexcept
    {
        return isEffect
            && ! isBypassed
            && ! isQuarantined
            && producesMidi
            && audioInputChannels == 0
            && audioOutputChannels == 0;
    }

    template <typename IsRouteReady>
    int findLatestReadySourceBefore(int tabIndex,
                                    IsRouteReady&& isRouteReady) noexcept
    {
        for (int sourceIndex = tabIndex - 1;
             sourceIndex >= 0;
             --sourceIndex)
        {
            if (isRouteReady(sourceIndex))
                return sourceIndex;
        }

        return -1;
    }
}
