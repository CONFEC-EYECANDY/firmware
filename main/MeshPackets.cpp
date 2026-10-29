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

    void ParentRequestResponsePacket::operator>>(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(packetType);
        data[1] = depth;
        data[2] = accepted;
    }

    void ParentRequestResponsePacket::operator<<(const uint8_t* data)
    {
        if (static_cast<MeshPacketType>(data[0]) != MeshPacketType::ParentRequestResponse)
        {
            ESP_LOGE(TAG, "Wrong Packet type %u being deserialized into ParentRequestResponsePacket!", static_cast<MeshPacketType>(data[0]));
        }
        depth = data[1];
        accepted = data[2];
    }

    void SubRoutingTableChildAddedPacket::operator>>(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(packetType);
        for (int i = 0; i < 6; i++)
            data[1 + i] = childMacAddress.macAddress[i];
        for (int i = 0; i < 6; i++)
            data[7 + i] = childParentMacAddress.macAddress[i];
    }

    void SubRoutingTableChildAddedPacket::operator<<(const uint8_t* data)
    {
        for (int i = 0; i < 6; i++)
            childMacAddress.macAddress[i] = data[1 + i];
        for (int i = 0; i < 6; i++)
            childParentMacAddress.macAddress[i] = data[7 + i];
    }
}
