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
        : MeshPacket(MeshPacketType::ParentSearch), childrenCount(count) , depth(depth) {}

        uint8_t childrenCount;
        uint8_t depth;

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };
}

#endif