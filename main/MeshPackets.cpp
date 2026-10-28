#include "MeshPackets.hpp"

#include <esp_log.h>

namespace wifi
{
    static auto TAG = "MeshPackets";

    MeshPacketType getMeshPacketType(const uint8_t* data)
    {
        return static_cast<MeshPacketType>(data[0]);
    }

    void ParentSearchPacket::operator>>(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(packetType);
    }

    void ParentSearchPacket::operator<<(const uint8_t* data)
    {
        if (static_cast<MeshPacketType>(data[0]) != MeshPacketType::ParentSearch)
        {
            ESP_LOGE(TAG, "Wrong Packet type %u being deserialized into ParentSearchPacket!", static_cast<MeshPacketType>(data[0]));
        }
    }

    void ParentRequestPacket::operator>>(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(packetType);
    }

    void ParentRequestPacket::operator<<(const uint8_t* data)
    {
        if (static_cast<MeshPacketType>(data[0]) != MeshPacketType::ParentRequest)
        {
            ESP_LOGE(TAG, "Wrong Packet type %u being deserialized into ParentRequestPacket!", static_cast<MeshPacketType>(data[0]));
        }
    }

    void ParentSearchResponsePacket::operator>>(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(packetType);
        data[1] = childrenCount;
        data[2] = depth;
    }

    void ParentSearchResponsePacket::operator<<(const uint8_t* data)
    {
        if (static_cast<MeshPacketType>(data[0]) != MeshPacketType::ParentSearchResponse)
        {
            ESP_LOGE(TAG, "Wrong Packet type %u being deserialized into ParentSearchResponsePacket!", static_cast<MeshPacketType>(data[0]));
        }
        childrenCount = data[1];
        depth = data[2];
    }
}
