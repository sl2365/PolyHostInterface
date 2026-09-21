#include "../HostedMidiRouting.h"
#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
    void expect(bool condition, const char* message)
    {
        if (! condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }

        std::cout << "PASS: " << message << '\n';
    }
}

int main()
{
    using phi_midi_routing::findLatestReadySourceBefore;
    using phi_midi_routing::isMidiOnlyEffect;
    using phi_midi_routing::selectDownstreamMidiBuffer;

    const int consumedSynthOutput = 0;
    const int preservedSynthInput = 2;
    expect(selectDownstreamMidiBuffer(true,
                                      consumedSynthOutput,
                                      preservedSynthInput) == 2,
           "a synth that consumes MIDI still routes its preserved input downstream");

    const int processedMidiEffectOutput = 3;
    const int midiEffectInput = 1;
    expect(selectDownstreamMidiBuffer(false,
                                      processedMidiEffectOutput,
                                      midiEffectInput) == 3,
           "a MIDI processor routes its processed output downstream");

    expect(isMidiOnlyEffect(true, false, false, true, 0, 0),
           "a zero-audio MIDI-generating FX is preprocessed as an arp");
    expect(! isMidiOnlyEffect(true, false, false, true, 2, 2),
           "an audio FX is never double-processed");
    expect(! isMidiOnlyEffect(true, true, false, true, 0, 0),
           "a bypassed arp does not control a synth");
    expect(! isMidiOnlyEffect(true, false, true, true, 0, 0),
           "a quarantined arp is not called again");

    const std::array<bool, 5> routeReady { false, true, false, true, false };
    const auto sourceForThirdTab = findLatestReadySourceBefore(
        2,
        [&routeReady] (int index) { return routeReady[(std::size_t) index]; });
    expect(sourceForThirdTab == 1,
           "a synth receives MIDI from the nearest arp above it");

    const auto sourceForFirstTab = findLatestReadySourceBefore(
        1,
        [&routeReady] (int index) { return routeReady[(std::size_t) index]; });
    expect(sourceForFirstTab == -1,
           "an arp below a synth cannot feed backwards");

    const auto sourceForFifthTab = findLatestReadySourceBefore(
        4,
        [&routeReady] (int index) { return routeReady[(std::size_t) index]; });
    expect(sourceForFifthTab == 3,
           "chained MIDI processors use the latest upstream output");

    std::cout << "All PHI hosted MIDI routing tests passed\n";
    return 0;
}
