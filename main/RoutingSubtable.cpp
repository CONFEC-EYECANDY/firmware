#include "RoutingSubtable.hpp"

#include <algorithm>
#include <esp_log.h>
#include <vector>

namespace wifi
{
    static auto TAG = "RoutingSubtable";

    void RoutingSubtable::addNode(const MacAddress& nodeMac, const MacAddress& nodeParentMac)
    {
        if (nodeMap.contains(nodeMac)) return;

        const Node node(nodeMac, nodeParentMac);
        nodeMap[node.macAddress] = node;
        nodeMap[node.macAddress].nextHopMacAddress = getNextHopMac(node.macAddress);

        if (node.parentMacAddress == macAddress)
            children.push_back(node.macAddress);
    }

    void RoutingSubtable::addNode(const MacAddress& nodeMac, const MacAddress& nodeParentMac, const MacAddress& nextHopMac)
    {
        if (nodeMap.contains(nodeMac)) return;

        const Node node(nodeMac, nodeParentMac, nextHopMac);
        nodeMap[node.macAddress] = node;

        if (node.parentMacAddress == macAddress)
            children.push_back(node.macAddress);
    }

    void RoutingSubtable::removeNode(const MacAddress& nodeMacAddress)
    {
        if (nodeMap[nodeMacAddress].parentMacAddress == macAddress)
            children.erase(std::ranges::find(children.begin(), children.end(), nodeMacAddress));

        nodeMap.erase(nodeMacAddress);
    }

    MacAddress RoutingSubtable::getNextHopTo(const MacAddress& destinationMacAddress)
    {
        if (destinationMacAddress == macAddress)
        {
            ESP_LOGE(TAG, "Routing requested to self!");
            return macAddress;
        }

        if (const auto it = nodeMap.find(destinationMacAddress); it != nodeMap.end())
            return it->second.nextHopMacAddress;

        return parentMacAddress;
    }

    MacAddress RoutingSubtable::getNextHopMac(const MacAddress& nodeMacAddress) const
    {
        auto searchResult = nodeMap.find(nodeMacAddress);
        while (true)
        {
            if (searchResult->second.parentMacAddress == macAddress)
            {
                return searchResult->first;
            }

            searchResult = nodeMap.find(searchResult->second.parentMacAddress);
        }
    }
}
