#ifndef FIRMWARE_MESHPACKETS_HPP
#define FIRMWARE_MESHPACKETS_HPP

#include <cstdint>

#include "RoutingSubtable.hpp"

namespace wifi
{
    enum class MeshPacketType : uint8_t
    {
        ParentSearch,
        ParentSearchResponse,
        ParentRequest,
        ParentRequestResponse,
        SubRoutingTableChildAdded,
    };

    struct MeshPacket
    {
        explicit MeshPacket(const MeshPacketType type) : packetType(type) {};
        virtual ~MeshPacket() = default;

        MeshPacketType packetType;

        virtual void operator>>(uint8_t* data) const = 0;
        virtual void operator<<(const uint8_t* data) = 0;
    };

    MeshPacketType getMeshPacketType(const uint8_t* data);

    struct ParentSearchPacket : MeshPacket
    {
        explicit ParentSearchPacket() : MeshPacket(MeshPacketType::ParentSearch) {}

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };
    struct ParentRequestPacket : MeshPacket
    {
        explicit ParentRequestPacket() : MeshPacket(MeshPacketType::ParentRequest) {}

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };

    struct ParentSearchResponsePacket : MeshPacket
    {
        ParentSearchResponsePacket() : MeshPacket(MeshPacketType::ParentSearchResponse), childrenCount(0), depth(0) {}
        explicit ParentSearchResponsePacket(const uint8_t count, const uint8_t depth)
        : MeshPacket(MeshPacketType::ParentSearchResponse), childrenCount(count) , depth(depth) {}

        uint8_t childrenCount;
        uint8_t depth;

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };

    struct ParentRequestResponsePacket : MeshPacket
    {
        ParentRequestResponsePacket() : MeshPacket(MeshPacketType::ParentRequestResponse), accepted(false), depth(0) {}
        explicit ParentRequestResponsePacket(const bool accepted, const uint8_t depth)
        : MeshPacket(MeshPacketType::ParentRequestResponse), accepted(accepted), depth(depth) {}

        bool accepted;
        uint8_t depth;

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };

    struct SubRoutingTableChildAddedPacket : MeshPacket
    {
        SubRoutingTableChildAddedPacket() : MeshPacket(MeshPacketType::SubRoutingTableChildAdded), childMacAddress(),
                                            childParentMacAddress()
        {}

        explicit SubRoutingTableChildAddedPacket(const Node& child)
        : MeshPacket(MeshPacketType::SubRoutingTableChildAdded), childMacAddress(child.macAddress), childParentMacAddress(child.parentMacAddress) {}

        MacAddress childMacAddress;
        MacAddress childParentMacAddress;

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };
}

#endif