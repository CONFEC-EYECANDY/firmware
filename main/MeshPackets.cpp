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
        size_t i = 0;
        data[i++] = static_cast<uint8_t>(packetType);
        data[i++] = addedChildren.size();
        data[i++] = removedChildren.size();

        for (size_t j = 0; j < addedChildren.size(); j++)
        {
            for (size_t k = 0; k < 6; k++)
                data[i++] = addedChildren[j].address[k];
            for (size_t k = 0; k < 6; k++)
                data[i++] = addedChildrenParents[j].address[k];
        }

        for (auto j : removedChildren)
        {
            for (size_t k = 0; k < 6; k++)
                data[i++] = j.address[k];
        }
    }
    void SubroutingTableChangedPacket::operator<<(const uint8_t* data)
    {
        size_t i = 1;
        addedChildren.clear();
        addedChildrenParents.clear();
        removedChildren.clear();

        const int addedChildrenCount = data[i++];
        const int removedChildrenCount = data[i++];

        addedChildren.reserve(addedChildrenCount);
        addedChildrenParents.reserve(addedChildrenCount);
        removedChildren.reserve(removedChildrenCount);

        for (int j = 0; j < addedChildrenCount; j++)
        {
            addedChildren.emplace_back(&data[i]);
            i += 6;
            addedChildrenParents.emplace_back(&data[i]);
            i += 6;
        }

        for (int j = 0; j < removedChildrenCount; j++)
        {
            removedChildren.emplace_back(&data[i]);
            i += 6;
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
