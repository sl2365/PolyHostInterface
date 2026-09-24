#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace seqwencer_bridge
{
constexpr std::uint8_t manufacturerID = 0x7d;
constexpr std::uint8_t legacyProtocolVersion = 1;
constexpr std::uint8_t protocolVersion = 2;
constexpr std::uint8_t modulationValueMessage = 1;
constexpr std::uint8_t targetBrowserRequestMessage = 2;
constexpr std::uint8_t laneValueMessage = 3;
constexpr std::uint8_t phiPresenceMessage = 4;
constexpr std::size_t packetSize = 10;

constexpr int sequencerLaneCount = 8;
constexpr int sequencerPairCount = 4;
constexpr int sequencerALane = 0;
constexpr int sequencerBLane = 1;
constexpr int sequencerCLane = 2;
constexpr int sequencerDLane = 3;
constexpr int sequencerELane = 4;
constexpr int sequencerFLane = 5;
constexpr int sequencerGLane = 6;
constexpr int sequencerHLane = 7;
// Version-one packets used lane 2 to mean the shared A/B SERIAL stream.
// Decode it to this out-of-range sentinel so lane C remains unambiguous.
constexpr int serialLane = sequencerLaneCount;
constexpr int targetMaskA = 1 << sequencerALane;
constexpr int targetMaskB = 1 << sequencerBLane;
constexpr int targetMaskC = 1 << sequencerCLane;
constexpr int targetMaskD = 1 << sequencerDLane;
constexpr int targetMaskE = 1 << sequencerELane;
constexpr int targetMaskF = 1 << sequencerFLane;
constexpr int targetMaskG = 1 << sequencerGLane;
constexpr int targetMaskH = 1 << sequencerHLane;
constexpr int allTargetMasks = (1 << sequencerLaneCount) - 1;
constexpr int allSerialPairMasks = (1 << sequencerPairCount) - 1;

using Packet = std::array<std::uint8_t, packetSize>;

inline int updateTargetMask (int currentMask,
                             int lane,
                             bool assigned,
                             bool serialMode) noexcept
{
    currentMask = std::clamp (currentMask, 0, allTargetMasks);
    lane = std::clamp (lane, sequencerALane, sequencerHLane);
    const auto bit = 1 << (serialMode ? lane - (lane & 1) : lane);
    return assigned ? currentMask | bit : currentMask & ~bit;
}

inline Packet encodeModulationValue (int sourceLane,
                                     int macroIndex,
                                     float normalizedValue) noexcept
{
    sourceLane = std::clamp (sourceLane, sequencerALane, sequencerHLane);
    macroIndex = std::clamp (macroIndex, 0, 127);
    normalizedValue = std::clamp (normalizedValue, 0.0f, 1.0f);
    const auto quantized = std::clamp (
        static_cast<int> (std::lround (normalizedValue * 16383.0f)),
        0, 16383);

    return { manufacturerID,
             static_cast<std::uint8_t> ('S'),
             static_cast<std::uint8_t> ('Q'),
             static_cast<std::uint8_t> ('W'),
             protocolVersion,
             modulationValueMessage,
             static_cast<std::uint8_t> (sourceLane),
             static_cast<std::uint8_t> (macroIndex),
             static_cast<std::uint8_t> (quantized & 0x7f),
             static_cast<std::uint8_t> ((quantized >> 7) & 0x7f) };
}

inline bool decodeModulationValue (const std::uint8_t* data,
                                   std::size_t dataSize,
                                   int& sourceLane,
                                   int& macroIndex,
                                   float& normalizedValue) noexcept
{
    if (data == nullptr || dataSize != packetSize
        || data[0] != manufacturerID
        || data[1] != static_cast<std::uint8_t> ('S')
        || data[2] != static_cast<std::uint8_t> ('Q')
        || data[3] != static_cast<std::uint8_t> ('W')
        || (data[4] != legacyProtocolVersion && data[4] != protocolVersion)
        || data[5] != modulationValueMessage
        || data[6] > (data[4] == legacyProtocolVersion
                          ? sequencerBLane : sequencerHLane)
        || data[7] > 127
        || data[8] > 127 || data[9] > 127)
    {
        return false;
    }

    sourceLane = static_cast<int> (data[6]);
    macroIndex = static_cast<int> (data[7]);
    const auto quantized = static_cast<int> (data[8])
                         | (static_cast<int> (data[9]) << 7);
    normalizedValue = static_cast<float> (quantized) / 16383.0f;
    return true;
}

inline Packet encodeLaneValue (int sourceLane,
                               bool bipolar,
                               bool active,
                               float normalizedValue,
                               int serialPairMask = 0) noexcept
{
    sourceLane = std::clamp (sourceLane, sequencerALane, sequencerHLane);
    serialPairMask = std::clamp (
        serialPairMask, 0, allSerialPairMasks);
    normalizedValue = std::clamp (normalizedValue, 0.0f, 1.0f);
    const auto quantized = std::clamp (
        static_cast<int> (std::lround (normalizedValue * 16383.0f)),
        0, 16383);
    const auto flags = static_cast<std::uint8_t> ((bipolar ? 1 : 0)
                                                | (active ? 2 : 0)
                                                | (serialPairMask << 2));

    return { manufacturerID,
             static_cast<std::uint8_t> ('S'),
             static_cast<std::uint8_t> ('Q'),
             static_cast<std::uint8_t> ('W'),
             protocolVersion,
             laneValueMessage,
             static_cast<std::uint8_t> (sourceLane),
             flags,
             static_cast<std::uint8_t> (quantized & 0x7f),
             static_cast<std::uint8_t> ((quantized >> 7) & 0x7f) };
}

inline bool decodeLaneValue (const std::uint8_t* data,
                             std::size_t dataSize,
                             int& sourceLane,
                             bool& bipolar,
                             bool& active,
                             float& normalizedValue,
                             int& serialPairMask) noexcept
{
    const auto version = data != nullptr && dataSize == packetSize
        ? data[4] : 0;
    const auto legacy = version == legacyProtocolVersion;
    if (data == nullptr || dataSize != packetSize
        || data[0] != manufacturerID
        || data[1] != static_cast<std::uint8_t> ('S')
        || data[2] != static_cast<std::uint8_t> ('Q')
        || data[3] != static_cast<std::uint8_t> ('W')
        || (! legacy && version != protocolVersion)
        || data[5] != laneValueMessage
        || data[6] > (legacy ? 2 : sequencerHLane)
        || data[7] > (legacy ? 3 : 63)
        || data[8] > 127 || data[9] > 127)
    {
        return false;
    }

    sourceLane = legacy && data[6] == 2
        ? serialLane : static_cast<int> (data[6]);
    bipolar = (data[7] & 1) != 0;
    active = (data[7] & 2) != 0;
    serialPairMask = legacy
        ? (sourceLane == serialLane ? 1 : 0)
        : (static_cast<int> (data[7]) >> 2) & allSerialPairMasks;
    const auto quantized = static_cast<int> (data[8])
                         | (static_cast<int> (data[9]) << 7);
    normalizedValue = static_cast<float> (quantized) / 16383.0f;
    return true;
}

inline bool decodeLaneValue (const std::uint8_t* data,
                             std::size_t dataSize,
                             int& sourceLane,
                             bool& bipolar,
                             bool& active,
                             float& normalizedValue) noexcept
{
    auto serialPairMask = 0;
    return decodeLaneValue (data, dataSize, sourceLane, bipolar, active,
                            normalizedValue, serialPairMask);
}

inline float distanceFromZero (float normalizedValue,
                               bool bipolar) noexcept
{
    normalizedValue = std::clamp (normalizedValue, 0.0f, 1.0f);
    return std::abs (bipolar ? 2.0f * normalizedValue - 1.0f
                             : normalizedValue);
}

inline float selectFurthestFromZero (float valueA,
                                    bool bipolarA,
                                    float valueB,
                                    bool bipolarB) noexcept
{
    valueA = std::clamp (valueA, 0.0f, 1.0f);
    valueB = std::clamp (valueB, 0.0f, 1.0f);
    return distanceFromZero (valueA, bipolarA)
               >= distanceFromZero (valueB, bipolarB)
        ? valueA : valueB;
}

inline Packet encodeTargetBrowserRequest (int serialPairMask = 0) noexcept
{
    serialPairMask = std::clamp (
        serialPairMask, 0, allSerialPairMasks);
    return { manufacturerID,
             static_cast<std::uint8_t> ('S'),
             static_cast<std::uint8_t> ('Q'),
             static_cast<std::uint8_t> ('W'),
             protocolVersion,
             targetBrowserRequestMessage,
             static_cast<std::uint8_t> (serialPairMask),
             0, 0, 0 };
}

inline bool decodeTargetBrowserRequest (const std::uint8_t* data,
                                        std::size_t dataSize,
                                        int& serialPairMask) noexcept
{
    const auto version = data != nullptr && dataSize == packetSize
        ? data[4] : 0;
    const auto valid = data != nullptr && dataSize == packetSize
        && data[0] == manufacturerID
        && data[1] == static_cast<std::uint8_t> ('S')
        && data[2] == static_cast<std::uint8_t> ('Q')
        && data[3] == static_cast<std::uint8_t> ('W')
        && (version == legacyProtocolVersion || version == protocolVersion)
        && data[5] == targetBrowserRequestMessage
        && data[6] <= (version == legacyProtocolVersion
                          ? 1 : allSerialPairMasks)
        && data[7] == 0
        && data[8] == 0 && data[9] == 0;

    if (valid)
        serialPairMask = static_cast<int> (data[6]);
    return valid;
}

inline bool decodeTargetBrowserRequest (const std::uint8_t* data,
                                        std::size_t dataSize,
                                        bool& serialMode) noexcept
{
    auto serialPairMask = 0;
    const auto valid = decodeTargetBrowserRequest (
        data, dataSize, serialPairMask);
    if (valid)
        serialMode = (serialPairMask & 1) != 0;
    return valid;
}

inline bool decodeTargetBrowserRequest (const std::uint8_t* data,
                                        std::size_t dataSize) noexcept
{
    auto serialPairMask = 0;
    return decodeTargetBrowserRequest (data, dataSize, serialPairMask);
}

inline Packet encodePhiPresence() noexcept
{
    return { manufacturerID,
             static_cast<std::uint8_t> ('S'),
             static_cast<std::uint8_t> ('Q'),
             static_cast<std::uint8_t> ('W'),
             legacyProtocolVersion,
             phiPresenceMessage,
             0, 0, 0, 0 };
}

inline bool decodePhiPresence (const std::uint8_t* data,
                               std::size_t dataSize) noexcept
{
    return data != nullptr && dataSize == packetSize
        && data[0] == manufacturerID
        && data[1] == static_cast<std::uint8_t> ('S')
        && data[2] == static_cast<std::uint8_t> ('Q')
        && data[3] == static_cast<std::uint8_t> ('W')
        && (data[4] == legacyProtocolVersion || data[4] == protocolVersion)
        && data[5] == phiPresenceMessage
        && data[6] == 0 && data[7] == 0
        && data[8] == 0 && data[9] == 0;
}
}
