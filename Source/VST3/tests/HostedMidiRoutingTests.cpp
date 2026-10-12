#include "../HostedMidiRouting.h"
#include "../SeqwencerBridgeProtocol.h"
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
    using phi_midi_routing::isMidiOnlyProcessor;
    using phi_midi_routing::shouldUsePreservedInputForDownstream;

    expect(shouldUsePreservedInputForDownstream(true, false, false),
           "a synth routes its preserved input MIDI downstream");
    expect(! shouldUsePreservedInputForDownstream(true, true, false),
           "an instrument-classified MIDI processor routes its processed output downstream");
    expect(! shouldUsePreservedInputForDownstream(true, false, true),
           "an audio-capable instrument that generated MIDI routes its processed output downstream");
    expect(! shouldUsePreservedInputForDownstream(false, false, false),
           "an FX-classified MIDI processor routes its processed output downstream");

    expect(isMidiOnlyProcessor(false, false, true, 0, 0),
           "a zero-audio MIDI generator is preprocessed regardless of plugin category");
    expect(! isMidiOnlyProcessor(false, false, true, 2, 2),
           "an audio FX is never double-processed");
    expect(! isMidiOnlyProcessor(true, false, true, 0, 0),
           "a bypassed arp does not control a synth");
    expect(! isMidiOnlyProcessor(false, true, true, 0, 0),
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

    const auto lanePacket = seqwencer_bridge::encodeLaneValue (
        seqwencer_bridge::sequencerHLane, true, true, 0.25f, 0x0a);
    auto sourceLane = -1;
    auto bipolar = false;
    auto active = false;
    auto value = 0.0f;
    auto serialPairMask = 0;
    expect(seqwencer_bridge::decodeLaneValue (
               lanePacket.data(), lanePacket.size(), sourceLane, bipolar,
               active, value, serialPairMask),
           "the eight-lane Seqwencer bridge packet decodes");
    expect(sourceLane == seqwencer_bridge::sequencerHLane,
           "the Seqwencer bridge preserves lane H");
    expect(bipolar && active && serialPairMask == 0x0a,
           "the Seqwencer bridge preserves lane and pair state");

    auto legacySerialPacket = lanePacket;
    legacySerialPacket[4] = seqwencer_bridge::legacyProtocolVersion;
    legacySerialPacket[6] = 2;
    legacySerialPacket[7] = 2;
    expect(seqwencer_bridge::decodeLaneValue (
               legacySerialPacket.data(), legacySerialPacket.size(),
               sourceLane, bipolar, active, value, serialPairMask)
               && sourceLane == seqwencer_bridge::serialLane
               && serialPairMask == 1,
           "the original A/B SERIAL bridge remains compatible");

    const auto targetRequest =
        seqwencer_bridge::encodeTargetBrowserRequest (0x0d);
    auto requestPairMask = 0;
    expect(seqwencer_bridge::decodeTargetBrowserRequest (
               targetRequest.data(), targetRequest.size(), requestPairMask)
               && requestPairMask == 0x0d,
           "the target browser preserves all four SERIAL pair states");

    std::cout << "All PHI hosted MIDI routing tests passed\n";
    return 0;
}
