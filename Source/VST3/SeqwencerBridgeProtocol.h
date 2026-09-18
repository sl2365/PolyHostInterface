#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace seqwencer_bridge
{
constexpr std::uint8_t manufacturerID = 0x7d;
constexpr std::uint8_t protocolVersion = 1;
constexpr std::uint8_t modulationValueMessage = 1;
constexpr std::uint8_t targetBrowserRequestMessage = 2;
constexpr std::uint8_t laneValueMessage = 3;
constexpr std::uint8_t phiPresenceMessage = 4;
constexpr std::size_t packetSize = 10;

constexpr int sequencerALane = 0;
constexpr int sequencerBLane = 1;
constexpr int serialLane = 2;
constexpr int targetMaskA = 1;
constexpr int targetMaskB = 2;

using Packet = std::array<std::uint8_t, packetSize>;

inline int updateTargetMask (int currentMask,
                             int lane,
                             bool assigned,
                             bool serialMode) noexcept
{
    currentMask = std::clamp (currentMask, 0, targetMaskA | targetMaskB);
    lane = std::clamp (lane, sequencerALane, sequencerBLane);
    const auto bit = serialMode ? targetMaskA : (1 << lane);
    return assigned ? currentMask | bit : currentMask & ~bit;
}

inline Packet encodeModulationValue (int sourceLane,
                                     int macroIndex,
                                     float normalizedValue) noexcept
{
    sourceLane = std::clamp (sourceLane, 0, 1);
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
        || data[4] != protocolVersion
        || data[5] != modulationValueMessage
        || data[6] > 1 || data[7] > 127
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
                               float normalizedValue) noexcept
{
    sourceLane = std::clamp (sourceLane, sequencerALane, serialLane);
    normalizedValue = std::clamp (normalizedValue, 0.0f, 1.0f);
    const auto quantized = std::clamp (
        static_cast<int> (std::lround (normalizedValue * 16383.0f)),
        0, 16383);
    const auto flags = static_cast<std::uint8_t> ((bipolar ? 1 : 0)
                                                | (active ? 2 : 0));

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
                             float& normalizedValue) noexcept
{
    if (data == nullptr || dataSize != packetSize
        || data[0] != manufacturerID
        || data[1] != static_cast<std::uint8_t> ('S')
        || data[2] != static_cast<std::uint8_t> ('Q')
        || data[3] != static_cast<std::uint8_t> ('W')
        || data[4] != protocolVersion
        || data[5] != laneValueMessage
        || data[6] > serialLane || data[7] > 3
        || data[8] > 127 || data[9] > 127)
    {
        return false;
    }

    sourceLane = static_cast<int> (data[6]);
    bipolar = (data[7] & 1) != 0;
    active = (data[7] & 2) != 0;
    const auto quantized = static_cast<int> (data[8])
                         | (static_cast<int> (data[9]) << 7);
    normalizedValue = static_cast<float> (quantized) / 16383.0f;
    return true;
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

inline Packet encodeTargetBrowserRequest (bool serialMode = false) noexcept
{
    return { manufacturerID,
             static_cast<std::uint8_t> ('S'),
             static_cast<std::uint8_t> ('Q'),
             static_cast<std::uint8_t> ('W'),
             protocolVersion,
             targetBrowserRequestMessage,
             static_cast<std::uint8_t> (serialMode ? 1 : 0),
             0, 0, 0 };
}

inline bool decodeTargetBrowserRequest (const std::uint8_t* data,
                                        std::size_t dataSize,
                                        bool& serialMode) noexcept
{
    const auto valid = data != nullptr && dataSize == packetSize
        && data[0] == manufacturerID
        && data[1] == static_cast<std::uint8_t> ('S')
        && data[2] == static_cast<std::uint8_t> ('Q')
        && data[3] == static_cast<std::uint8_t> ('W')
        && data[4] == protocolVersion
        && data[5] == targetBrowserRequestMessage
        && data[6] <= 1 && data[7] == 0
        && data[8] == 0 && data[9] == 0;

    if (valid)
        serialMode = data[6] != 0;
    return valid;
}

inline bool decodeTargetBrowserRequest (const std::uint8_t* data,
                                        std::size_t dataSize) noexcept
{
    auto serialMode = false;
    return decodeTargetBrowserRequest (data, dataSize, serialMode);
}

inline Packet encodePhiPresence() noexcept
{
    return { manufacturerID,
             static_cast<std::uint8_t> ('S'),
             static_cast<std::uint8_t> ('Q'),
             static_cast<std::uint8_t> ('W'),
             protocolVersion,
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
        && data[4] == protocolVersion
        && data[5] == phiPresenceMessage
        && data[6] == 0 && data[7] == 0
        && data[8] == 0 && data[9] == 0;
}
}
