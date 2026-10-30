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
        RoutingSubtableChanged,
        ConnectionCheck,
        ConnectionCheckResponse,
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

        static constexpr size_t size {1};

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

        static constexpr size_t size {3};

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };

    struct ParentRequestPacket : MeshPacket
    {
        explicit ParentRequestPacket() : MeshPacket(MeshPacketType::ParentRequest) {}

        static constexpr size_t size {1};

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

        static constexpr size_t size {3};

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };

    struct SubroutingTableChangedPacket : MeshPacket
    {
        SubroutingTableChangedPacket() : MeshPacket(MeshPacketType::RoutingSubtableChanged)
        {
        }

        explicit SubroutingTableChangedPacket(MacAddress* addedChildren, MacAddress* addedChildrenParents, MacAddress* removedChildren,
                                                const uint8_t addedChildrenCount, const uint8_t removedChildrenCount) :
            MeshPacket(MeshPacketType::RoutingSubtableChanged),
            addedChildren(addedChildren), addedChildrenParents(addedChildrenParents), addedChildrenCount(addedChildrenCount),
            removedChildren(removedChildren), removedChildrenCount(removedChildrenCount)
        {
        }

        explicit SubroutingTableChangedPacket(MacAddress* addedChild, MacAddress* addedChildParent) :
            SubroutingTableChangedPacket(addedChild, addedChildParent, nullptr,1, 0) {}
        explicit SubroutingTableChangedPacket(MacAddress* removedChildren, const uint8_t removedChildrenCount) :
            SubroutingTableChangedPacket(nullptr, nullptr, removedChildren, 0, removedChildrenCount) {};

        MacAddress* addedChildren{};
        MacAddress* addedChildrenParents{};
        uint8_t addedChildrenCount{};

        MacAddress* removedChildren{};
        uint8_t removedChildrenCount{};

        static constexpr size_t getSize(uint8_t addedChildrenCount, uint8_t removedChildrenCount)
        {
            return 2 + addedChildrenCount * 12 + removedChildrenCount * 6;
        }
        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };

    struct ConnectionCheckPacket : MeshPacket
    {
        explicit ConnectionCheckPacket() : MeshPacket(MeshPacketType::ConnectionCheck) {}

        static constexpr size_t size {1};

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };

    struct ConnectionCheckResponsePacket : MeshPacket
    {
        explicit ConnectionCheckResponsePacket() : MeshPacket(MeshPacketType::ConnectionCheckResponse) {}

        static constexpr size_t size {1};

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };
}

#endif