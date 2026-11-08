//
// Created by falingunit on 10/27/26.
//

#ifndef FIRMWARE_ROUTINGSUBTABLE_HPP
#define FIRMWARE_ROUTINGSUBTABLE_HPP
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <sys/types.h>

#include "MacAddress.hpp"

namespace wifi
{

    struct Node
    {
        MacAddress macAddress {};
        MacAddress parentMacAddress {};
        MacAddress nextHopMacAddress {};
    };

    class RoutingTable
    {
    public:
        MacAddress parentMacAddress{};
        MacAddress macAddress{};
        uint8_t depth{};
        std::vector<MacAddress> children {};
        std::unordered_map<MacAddress, Node, MacHash> nodeMap{};

        RoutingTable() = default;
        ~RoutingTable() = default;

        void addNode(const MacAddress& nodeMac, const MacAddress& nodeParentMac);
        void addNode(const MacAddress& nodeMac, const MacAddress& nodeParentMac, const MacAddress& nextHopMac);
        void removeNode(const MacAddress& nodeMacAddress);
        MacAddress getNextHopTo(const MacAddress& destinationMacAddress);

    private:
        MacAddress getNextHopMac(const MacAddress& nodeMacAddress) const;
    };
}

#endif //FIRMWARE_ROUTINGSUBTABLE_HPP
