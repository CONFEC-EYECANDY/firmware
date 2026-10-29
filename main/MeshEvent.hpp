#ifndef FIRMWARE_MESHEVENT_HPP
#define FIRMWARE_MESHEVENT_HPP
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

    union MeshEventData
    {
        ReceivedData receivedData;
        TransmissionCallbackData transmissionCallbackData;
    };

    struct MeshEvent
    {
        explicit MeshEvent(const MeshEventType eventType) : type(eventType) {}

        MeshEventType type;
        MeshEventData data{};
    };
}
#endif
