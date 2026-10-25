#ifndef FIRMWARE_MESHEVENT_HPP
#define FIRMWARE_MESHEVENT_HPP

namespace wifi
{
    enum class MeshEventType : uint8_t
    {
        RadioUp,
        RadioDown,
        TransmitData,
        ReceivedData,
    };

    struct MeshEventRadioUpData
    {

    };

    struct MeshEventRadioDownData
    {

    };

    struct MeshEventTransmitData
    {

    };

    union MeshEventData
    {
        MeshEventRadioUpData radioUpData;
        MeshEventRadioDownData radioDownData;
        MeshEventTransmitData transmitData;
    };

    struct MeshEvent
    {
        MeshEventType type;
        MeshEventData data;
    };
}
#endif
