#include "MeshPackets.hpp"

#include <algorithm>
#include <esp_log.h>

namespace wifi
{
    static auto TAG = "MeshPackets";

    MeshPacketType getMeshPacketType(const uint8_t* data)
    {
        return static_cast<MeshPacketType>(data[0]);
    }

    // Parent search packet
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


    // Parent search response packet
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

    // Parent request packet
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

    // Parent request response packet
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


    // subrouting table child added packet
    void SubroutingTableChangedPacket::operator>>(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(packetType);
        data[1] = addedChildrenCount;
        data[2] = removedChildrenCount;

        for (int j = 3; j < 3 + addedChildrenCount; j++)
        {
            for (int i = 0; i < 6; i++)
                data[j + 1 + i] = addedChildren[j].macAddress[i];
            for (int i = 0; i < 6; i++)
                data[j + 7 + i] = addedChildrenParents[j].macAddress[i];
        }

        for (int j = addedChildrenCount * 12 + 3; j < addedChildrenCount * 12 + 3 + removedChildrenCount; j++)
        {
            for (int i = 0; i < 6; i++)
                data[j + 1 + i] = removedChildren[j].macAddress[i];
        }
    }
    void SubroutingTableChangedPacket::operator<<(const uint8_t* data)
    {
        addedChildrenCount = data[1];
        removedChildrenCount = data[2];

        addedChildren = new MacAddress[addedChildrenCount];
        addedChildrenParents = new MacAddress[addedChildrenCount];
        removedChildren = new MacAddress[removedChildrenCount];

        for (int j = 3; j < 3 + addedChildrenCount; j++)
        {
            for (int i = 0; i < 6; i++)
                addedChildren[j].macAddress[i] = data[j + 1 + i];
            for (int i = 0; i < 6; i++)
                addedChildrenParents[j].macAddress[i] = data[j + 7 + i];
        }

        for (int j = addedChildrenCount * 12 + 3; j < addedChildrenCount * 12 + 3 + removedChildrenCount; j++)
        {
            for (int i = 0; i < 6; i++)
                removedChildren[j].macAddress[i]= data[j + 1 + i];
        }
    }

    void ConnectionCheckPacket::operator>>(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(packetType);
    }
    void ConnectionCheckPacket::operator<<(const uint8_t* data)
    {
        if (static_cast<MeshPacketType>(data[0]) != MeshPacketType::ConnectionCheck)
        {
            ESP_LOGE(TAG, "Wrong Packet type %u being deserialized into ConnectionCheckPacket!", static_cast<MeshPacketType>(data[0]));
        }
    }

    void ConnectionCheckResponsePacket::operator>>(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(packetType);
    }
    void ConnectionCheckResponsePacket::operator<<(const uint8_t* data)
    {
        if (static_cast<MeshPacketType>(data[0]) != MeshPacketType::ConnectionCheckResponse)
        {
            ESP_LOGE(TAG, "Wrong Packet type %u being deserialized into ConnectionCheckResponsePacket!", static_cast<MeshPacketType>(data[0]));
        }
    }
}
