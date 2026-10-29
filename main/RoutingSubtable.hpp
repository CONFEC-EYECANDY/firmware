//
// Created by falingunit on 10/27/26.
//

#ifndef FIRMWARE_ROUTINGSUBTABLE_HPP
#define FIRMWARE_ROUTINGSUBTABLE_HPP
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <sys/types.h>

namespace wifi
{
    struct MacAddress
    {
        MacAddress() = default;
        explicit MacAddress(uint8_t macAddress[6]);

        uint8_t macAddress[6];

        bool operator==(const MacAddress &other) const
        {
            for (int i = 0; i < 6; i++)
            {
                if (macAddress[i] != other.macAddress[i])
                    return false;
            }
            return true;
        }
    };
    struct MacHash
    {
        std::size_t operator()(const MacAddress& mac) const
        {
            std::size_t hash = 0;

            for (const uint8_t byte : mac.macAddress)
            {
                hash = hash * 31 + byte;
            }

            return hash;
        }
    };

    struct Node
    {
        MacAddress macAddress;
        MacAddress parentMacAddress;
        MacAddress nextHopMacAddress;
    };

    class RoutingSubtable
    {
    private:
        MacAddress parentMacAddress_{};
        MacAddress macAddress_{};
        uint8_t depth_{};
        std::unordered_map<MacAddress, Node, MacHash> nodeMap_{};

    public:
        RoutingSubtable() = default;
        ~RoutingSubtable() = default;

        void addNode(const Node& node, bool containsNextHop);
        void removeNode(const Node& node);
        uint8_t getChildrenCount() const;
        MacAddress getNextHopTo(MacAddress destinationMacAddress);

        void setParentMac(MacAddress parentMacAddress);
        MacAddress getParentMac() const;
        void setMacAddress(MacAddress macAddress);
        MacAddress getMacAddress() const;
        void setDepth(uint8_t depth);
        uint8_t getDepth() const;
    };
}

#endif //FIRMWARE_ROUTINGSUBTABLE_HPP
