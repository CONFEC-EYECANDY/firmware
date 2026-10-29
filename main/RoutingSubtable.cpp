#include "RoutingSubtable.hpp"

#include <esp_log.h>
#include <vector>

namespace wifi
{
    static auto TAG = "RoutingSubtable";

    void RoutingSubtable::setParentMac(const MacAddress parentMacAddress)
    {
        parentMacAddress_ = parentMacAddress;
    }
    MacAddress RoutingSubtable::getParentMac() const
    {
        return parentMacAddress_;
    }

    void RoutingSubtable::setMacAddress(const MacAddress macAddress)
    {
        macAddress_ = macAddress;
    }

    MacAddress RoutingSubtable::getMacAddress() const
    {
        return macAddress_;
    }

    uint8_t RoutingSubtable::getChildrenCount() const
    {
        return nodeMap_.size();
    }

    uint8_t RoutingSubtable::getDepth() const
    {
        return depth_;
    }

    void RoutingSubtable::setDepth(const uint8_t depth)
    {
        depth_ = depth;
    }

    MacAddress RoutingSubtable::getNextHopTo(const MacAddress destinationMacAddress)
    {
        if (destinationMacAddress == macAddress_)
        {
            ESP_LOGE(TAG, "Routing requested to self!");
            return macAddress_;
        }

        if (nodeMap_.contains(destinationMacAddress))
            return nodeMap_[destinationMacAddress].nextHopMacAddress;
        return parentMacAddress_;
    }

    MacAddress::MacAddress(uint8_t macAddress[6]) : macAddress{}
    {
        memcpy(MacAddress::macAddress, macAddress, 6);
    }

    void RoutingSubtable::addNode(const Node& node, const bool containsNextHop = false)
    {
        nodeMap_[node.macAddress] = node;
        if (containsNextHop) return;

        auto searchResult = nodeMap_.find(node.macAddress);
        while (true)
        {
            if (searchResult->second.parentMacAddress == macAddress_)
            {
                nodeMap_[node.macAddress].nextHopMacAddress = searchResult->first;
                break;
            }

            searchResult = nodeMap_.find(searchResult->second.parentMacAddress);
        }
    }

    void RoutingSubtable::removeNode(const Node& node)
    {
        nodeMap_.erase(node.macAddress);
    }
}
