#ifndef FIRMWARE_MESHEVENT_HPP
#define FIRMWARE_MESHEVENT_HPP
#include <cstring>

#include "MeshPackets.hpp"
#include "RoutingSubtable.hpp"

namespace wifi
{
    enum class MeshEventType : uint8_t
    {
        Null = 0,
        RadioUp = 1,
        RadioDown = 2,
        TransmitData = 3,
        TransmissionCallback = 4,
        ReceivedData = 5,
    };

    struct ReceivedData
    {
        MacAddress senderMac;
        signed rssi;
        uint8_t* data;
        size_t length;
    };

    struct TransmissionCallbackData
    {
        MacAddress destinationMac;
    };

    struct TransmitData
    {
        explicit TransmitData(const MacAddress destAddress, const uint8_t* data, const size_t length) : destinationMac(destAddress), data(data), length(length) {}

        MacAddress destinationMac;
        const uint8_t* data;
        size_t length;
    };

    union MeshEventData
    {
        ReceivedData receivedData;
        TransmissionCallbackData transmissionCallbackData;
        TransmitData transmitData;
    };

    struct MeshEvent
    {
        explicit MeshEvent(const MeshEventType eventType) : type(eventType) {}

        MeshEventType type;
        MeshEventData data{};
    };
}
#endif
