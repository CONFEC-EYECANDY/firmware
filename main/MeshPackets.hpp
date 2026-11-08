#ifndef FIRMWARE_MESHPACKETS_HPP
#define FIRMWARE_MESHPACKETS_HPP

#include <utility>

#include "RoutingTable.hpp"

namespace wifi
{
    enum class MeshPacketType : uint8_t
    {
        ParentSearch,
        ParentSearchResponse,

        ParentRequest,
        ParentRequestResponse,

        ConnectionCheck,
        ConnectionCheckResponse,

        RoutingSubtableChanged,
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

    struct SubroutingTableChangedPacket : MeshPacket
    {
        SubroutingTableChangedPacket() : MeshPacket(MeshPacketType::RoutingSubtableChanged)
        {
        }

        explicit SubroutingTableChangedPacket(std::vector<MacAddress> addedChildren, std::vector<MacAddress> addedChildrenParents, std::vector<MacAddress> removedChildren) :
            MeshPacket(MeshPacketType::RoutingSubtableChanged),
            addedChildren(std::move(addedChildren)),
            addedChildrenParents(std::move(addedChildrenParents)),
            removedChildren(std::move(removedChildren)
        ) {}

        explicit SubroutingTableChangedPacket(MacAddress addedChild, MacAddress addedChildParent) :
            SubroutingTableChangedPacket({addedChild}, {addedChildParent}, {}) {}
        explicit SubroutingTableChangedPacket(std::vector<MacAddress> removedChildren) :
            SubroutingTableChangedPacket({}, {}, std::move(removedChildren)) {};

        std::vector<MacAddress> addedChildren{};
        std::vector<MacAddress> addedChildrenParents{};
        std::vector<MacAddress> removedChildren{};

        static constexpr size_t getSize(const uint8_t addedChildrenCount, const uint8_t removedChildrenCount)
        {
            return 3 + addedChildrenCount * 12 + removedChildrenCount * 6;
        }

        void operator>>(uint8_t* data) const override;
        void operator<<(const uint8_t* data) override;
    };
}

#endif
