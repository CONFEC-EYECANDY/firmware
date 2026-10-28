#ifndef FIRMWARE_MESHEVENT_HPP
#define FIRMWARE_MESHEVENT_HPP
#include "RoutingSubtable.hpp"

namespace wifi
{
    enum class MeshEventType : uint8_t
    {
        Null = 0,
        RadioUp,
        RadioDown,
        TransmitData,
        TransmissionCallback,
        ReceivedData,
    };

    struct ReceivedData
    {
        MacAddress senderMac;
        int8_t rssi;
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
