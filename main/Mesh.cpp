#include "Mesh.hpp"

#include <algorithm>
#include <sys/types.h>

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "MeshPackets.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static auto TAG = SET_ROOT_NODE ? "ROOT MESH " NODE_NAME: "MESH " NODE_NAME;

namespace wifi
{
    Mesh& Mesh::getInstance()
    {
        static Mesh instance;
        return instance;
    }

    esp_err_t Mesh::init()
    {
        esp_err_t error = esp_now_init();                       if (error != ESP_OK) return error;

        addEspNowPeer(MacAddress{broadcastMac}, false);

        error = esp_now_register_recv_cb(onSignalReceived);     if (error != ESP_OK) return error;
        error = esp_now_register_send_cb(onFinishedSending);    if (error != ESP_OK) return error;

        eventQueue_ = xQueueCreate(MAX_MESH_PENDING_EVENTS, sizeof(MeshEvent));

        if (xTaskCreate(runEntry, "MeshTask", 2048, nullptr, MESH_TASK_PRIORITY, &taskHandle_)
            != pdPASS) return ESP_FAIL;

        constexpr WiFiEvent event{.type = WiFiEventType::StartRadio, .data = nullptr};
        WiFiManager::getInstance().sendEvent(&event);

        esp_read_mac(macAddress.macAddress, ESP_MAC_WIFI_STA);
        routingSubtable.setMacAddress(macAddress);

        if (isRootNode) routingSubtable.setDepth(0);

        return ESP_OK;
    }

    bool Mesh::sendEvent(const MeshEvent* event)
    {
        return xQueueSend(Mesh::getInstance().eventQueue_, event, portMAX_DELAY) == pdTRUE;
    }

    void Mesh::runEntry(void* arg)
    {
        getInstance().run();
    }
    void Mesh::run()
    {
        while (true)
        {
            const TickType_t t = nextFsmStateEventIn();
            ESP_LOGI(TAG, "Waiting on queue for %" PRIu32" ticks", t);
            processQueue(t);
            if (nextFsmStateEventIn() == 0) processFsmEventTask();
        }
    }

    void Mesh::processQueue(const TickType_t processFor)
    {
        MeshEvent currentEvent(MeshEventType::Null);
        if (xQueueReceive(eventQueue_, &currentEvent, processFor) == pdFALSE) return;

        ESP_LOGI(TAG, "Queue received task of type %d", currentEvent.type);

        switch (currentEvent.type)
        {
        case MeshEventType::RadioUp:
            radioUpMeshEvent(&currentEvent);
            break;
        case MeshEventType::RadioDown:
            radioDownMeshEvent(&currentEvent);
            break;
        case MeshEventType::ReceivedData:
            receivedDataMeshEvent(&currentEvent);
            break;
        case MeshEventType::TransmitData:
            transmitDataMeshEvent(&currentEvent);
            break;
        case MeshEventType::TransmissionCallback:
            transmissionCallbackEvent(&currentEvent);
        default:
            break;
        }
    }

    // process mesh events
    void Mesh::radioUpMeshEvent(const MeshEvent* event)
    {
        if (isRootNode)
        {
            switchState(MeshState::Online);
            return;
        }
        if (!belongsToMesh) switchState(MeshState::LookingForParent);
    }

    void Mesh::radioDownMeshEvent(const MeshEvent* event)
    {
    }

    void Mesh::transmitDataMeshEvent(const MeshEvent* event)
    {
        const TransmitData transmitData = event->data.transmitData;

        esp_now_send(transmitData.destinationMac.macAddress, transmitData.data, transmitData.length);

        delete[] event->data.transmitData.data;
    }

    void Mesh::receivedDataMeshEvent(const MeshEvent* event)
    {
        auto& receivedData = event->data.receivedData;

        switch (getMeshPacketType(receivedData.data)) {
        case MeshPacketType::ParentSearch:
            respondToParentSearchBroadcast(receivedData);
        break;
        case MeshPacketType::ParentSearchResponse:
            receivedParentSearchResponse(receivedData);
        break;
        case MeshPacketType::ParentRequest:
            respondToParentRequest(receivedData);
        break;
        case MeshPacketType::ParentRequestResponse:
            receivedParentRequestResponse(receivedData);
            break;
        case MeshPacketType::SubRoutingTableChildAdded:
            receivedSubroutingTableChildAdded(receivedData);
            break;
        default: ;
        }

        free(receivedData.data);
    }

    void Mesh::transmissionCallbackEvent(const MeshEvent* event)
    {
        if (ephemeralPeers.contains(event->data.transmissionCallbackData.destinationMac))
        {
            esp_now_del_peer(event->data.transmissionCallbackData.destinationMac.macAddress);
            ephemeralPeers.erase(event->data.transmissionCallbackData.destinationMac);
        }
    }

    void Mesh::respondToParentSearchBroadcast(const ReceivedData& receivedData) const
    {
        if (!((belongsToMesh || isRootNode) && routingSubtable.getChildrenCount() < MAX_CHILDREN_PER_NODE)) return;

        addEspNowPeer(receivedData.senderMac, true);

        uint8_t data[3]{};
        ParentSearchResponsePacket(routingSubtable.getChildrenCount(), routingSubtable.getDepth()) >> data;
        esp_now_send(receivedData.senderMac.macAddress, data, sizeof(data));
        ESP_LOGI(TAG, "Responded to Parent Search Broadcast");
    }

    void Mesh::receivedParentSearchResponse(const ReceivedData& receivedData)
    {
        ParentSearchResponsePacket parentSearchResponsePacket{};
        parentSearchResponsePacket << receivedData.data;
        ESP_LOGI(TAG, "Received Parent Search Response Packet");
        const CandidateParent candidateParent
        {
            .macAddress = receivedData.senderMac,
            .rssi = receivedData.rssi,
            .depth = parentSearchResponsePacket.depth,
            .childrenCount = parentSearchResponsePacket.childrenCount,
        };

        if (std::ranges::any_of(candidateParents,
                                [candidateParent](const CandidateParent& p) { return p.macAddress == candidateParent.macAddress; })) return;

        ESP_LOGI(TAG, "Added Candidate Parent");
        candidateParents.push_back(candidateParent);
        hasAddedNewCandidateParentThisCycle = true;
    }

    void Mesh::respondToParentRequest(const ReceivedData& receivedData)
    {
        if (routingSubtable.getChildrenCount() + 1 > MAX_CHILDREN_PER_NODE)
        {
            ESP_LOGI(TAG, "Too many children rejecting request");
            addEspNowPeer(receivedData.senderMac, true);

            uint8_t data[3];
            ParentRequestResponsePacket(false, routingSubtable.getDepth()) >> data;
            esp_now_send(receivedData.senderMac.macAddress, data, sizeof(data));
        }
        ESP_LOGI(TAG, "Accepting child request");

        // add child
        const Node newChild
        {
            .macAddress = receivedData.senderMac,
            .parentMacAddress = macAddress,
            .nextHopMacAddress = receivedData.senderMac,
        };
        routingSubtable.addNode(newChild, true);
        addEspNowPeer(receivedData.senderMac, false);
        uint8_t data[3];
        ParentRequestResponsePacket(true, routingSubtable.getDepth()) >> data;
        esp_now_send(receivedData.senderMac.macAddress, data, sizeof(data));

        // update parent
        auto* subRoutingTableUpdateData = new uint8_t[13];
        SubRoutingTableChildAddedPacket(newChild) >> subRoutingTableUpdateData;

        MeshEvent event(MeshEventType::TransmitData);
        event.data.transmitData.destinationMac = routingSubtable.getParentMac();
        event.data.transmitData.data = subRoutingTableUpdateData;
        event.data.transmitData.length = sizeof(subRoutingTableUpdateData);

        sendEvent(&event);
    }

    void Mesh::receivedParentRequestResponse(const ReceivedData& receivedData)
    {
        ParentRequestResponsePacket parentRequestResponsePacket{};
        parentRequestResponsePacket << receivedData.data;
        if (!parentRequestResponsePacket.accepted)
        {
            ESP_LOGI(TAG, "Candidate parent rejected request");
            return;
        }

        routingSubtable.setParentMac(receivedData.senderMac);
        routingSubtable.setDepth(parentRequestResponsePacket.depth + 1);

        ESP_LOGI(TAG, "Successfully connected to Parent with depth %d", routingSubtable.getDepth());

        isWaitingForParentResponse = false;
        hasAddedNewCandidateParentThisCycle = false;
        candidateParents.clear();

        switchState(MeshState::Online);
    }

    void Mesh::receivedSubroutingTableChildAdded(const ReceivedData& receivedData)
    {
        ESP_LOGI(TAG, "Subrouting Table Child Added");

        SubRoutingTableChildAddedPacket packet;
        packet << receivedData.data;


        Node newChild
        {
            .macAddress = packet.childMacAddress,
            .parentMacAddress = packet.childParentMacAddress,
            .nextHopMacAddress = receivedData.senderMac,
        };

        routingSubtable.addNode(newChild, true);

        if (isRootNode) return;

        // forward update to parent
        MeshEvent event(MeshEventType::TransmitData);
        event.data.transmitData.destinationMac = routingSubtable.getParentMac();
        event.data.transmitData.data = receivedData.data;
        event.data.transmitData.length = sizeof(receivedData.data);

        sendEvent(&event);
    }

    // fsm
    void Mesh::switchState(const MeshState newState)
    {
        meshState_ = newState;
        fsmEventTimerStartedAt = xTaskGetTickCount();
        ESP_LOGI(TAG, "State switched, new state %d", newState);
    }

    TickType_t Mesh::getFsmTaskDelay() const
    {
        switch (meshState_)
        {
            case MeshState::Offline:
                return portMAX_DELAY;
            case MeshState::LookingForParent:
                return isWaitingForParentResponse ? pdMS_TO_TICKS(PARENT_SEARCH_BROADCAST_INTERVAL_MS) : pdMS_TO_TICKS(PARENT_RESPONSE_WAIT_TIME);
            case MeshState::ContactingMissingChildren:
                return pdMS_TO_TICKS(MISSING_CHILDREN_CONTACT_PERIOD_MS);
            case MeshState::WaitingForChildrenAlive:
                return pdMS_TO_TICKS(CHILDREN_ALIVE_TIMEOUT_MS);
            case MeshState::Online:
                return portMAX_DELAY;
            default:
                return portMAX_DELAY;
        }
    }

    TickType_t Mesh::nextFsmStateEventIn() const
    {
        const TickType_t delay = getFsmTaskDelay();
        if (delay == portMAX_DELAY) return portMAX_DELAY;

        const TickType_t elapsed = xTaskGetTickCount() - fsmEventTimerStartedAt;

        // since TickType_t is unsigned we need to manually clamp to 0
        return elapsed >= delay ? 0 : delay - elapsed;
    }

    // fsm event tasks
    void Mesh::processFsmEventTask()
    {
        switch (meshState_)
        {
            case MeshState::Offline:
            break;
            case MeshState::LookingForParent:
                parentBroadcastFsmEvent();
            break;
            case MeshState::ContactingMissingChildren:
                // contactMissingChildren();
            break;
            case MeshState::WaitingForChildrenAlive:
                // childrenAliveTaskDone();
            break;
            case MeshState::Online:
            break;
            default:
            break;
        }

        fsmEventTimerStartedAt = xTaskGetTickCount();
    }

    void Mesh::parentBroadcastFsmEvent()
    {
        if ((!hasAddedNewCandidateParentThisCycle || isWaitingForParentResponse) && !candidateParents.empty())
        {
            ESP_LOGI(TAG, "Processing candidate parents.");
            // all possible parents have replied
            CandidateParent candidateParent;
            int parentIndex;
            if (pickCandidateParent(&candidateParent, &parentIndex))
            {
                addEspNowPeer(candidateParent.macAddress, false);

                uint8_t data[1];
                ParentRequestPacket{} >> data;
                esp_now_send(candidateParent.macAddress.macAddress, data, sizeof(data));
                ESP_LOGI(TAG, "Parent request sent");

                candidateParents.erase(candidateParents.begin() + parentIndex);
                isWaitingForParentResponse = true;
                hasAddedNewCandidateParentThisCycle = false;
                return;
            }
        }

        // this line runs if we were waiting for parent response and ran out of candidate parents cuz they stopped replying
        isWaitingForParentResponse = false;

        uint8_t data[1]{};
        ParentSearchPacket{} >> data;

        ESP_LOGI(TAG, "Sending parent broadcast");
        if (const esp_err_t err = esp_now_send(broadcastMacArr, data, sizeof(data)); err != ESP_OK)
        {
            ESP_LOGE(TAG, "ESP-NOW broadcast failed: %s", esp_err_to_name(err));
        }

        hasAddedNewCandidateParentThisCycle = false;
    }

    bool Mesh::pickCandidateParent(CandidateParent* parent, int* parentIndex) const
    {
        bool didPickParent = false;

        uint8_t minDepth{127};
        uint8_t minChildrenCount{127};
        signed maxRssi{-128};

        for (const CandidateParent candidateParent : candidateParents)
        {
            if (candidateParent.rssi < CANDIDATE_PARENT_RSSI_THRESHOLD) continue;
            minDepth = minDepth > candidateParent.depth ? candidateParent.depth : minDepth;
        }

        for (const CandidateParent candidateParent : candidateParents)
        {
            if (candidateParent.rssi < CANDIDATE_PARENT_RSSI_THRESHOLD) continue;
            if (candidateParent.depth > minDepth) continue;
            minChildrenCount = minChildrenCount > candidateParent.childrenCount ? candidateParent.childrenCount : minChildrenCount;
        }

        for (int i = 0; i < candidateParents.size(); i++)
        {
            const CandidateParent candidateParent = candidateParents[i];
            if (candidateParent.rssi < CANDIDATE_PARENT_RSSI_THRESHOLD) continue;
            if (candidateParent.depth > minDepth) continue;
            if (candidateParent.childrenCount > minChildrenCount) continue;
            if (maxRssi < candidateParent.rssi)
            {
                maxRssi = candidateParent.rssi;
                *parent = candidateParent;
                *parentIndex = i;
                didPickParent = true;
            }
        }

        return didPickParent;
    }

    void Mesh::addEspNowPeer(const MacAddress& macAddress, const bool isEphemeral = false)
    {
        if (esp_now_is_peer_exist(macAddress.macAddress)) return;

        esp_now_peer_info_t peer = {};

        memcpy(peer.peer_addr, macAddress.macAddress, 6);
        peer.channel = 0;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;

        if (const esp_err_t err = esp_now_add_peer(&peer); err != ESP_OK)
            ESP_LOGE("Mesh", "Failed to add peer: %s",esp_err_to_name(err));
    }

    void Mesh::removeEspNowPeer(const MacAddress& macAddress)
    {
        if (esp_now_is_peer_exist(macAddress.macAddress))
            esp_now_del_peer(macAddress.macAddress);
    }

    // esp now callbacks
    void Mesh::onSignalReceived(const esp_now_recv_info_t* info, const uint8_t* data, const int length)
    {
        if (!info || !info->src_addr || !data || length <= 0) return;

        MeshEvent event(MeshEventType::ReceivedData);
        event.type = MeshEventType::ReceivedData;

        auto& receivedData = event.data.receivedData;

        receivedData.senderMac = MacAddress{info->src_addr};
        receivedData.rssi = info->rx_ctrl->rssi;
        receivedData.length = length;
        receivedData.data = static_cast<uint8_t*>(malloc(length));
        memcpy(receivedData.data, data, length);

        if (!sendEvent(&event))
        {
            free(receivedData.data);
        }
    }

    void Mesh::onFinishedSending(const wifi_tx_info_t* info, const esp_now_send_status_t status)
    {
        MeshEvent event(MeshEventType::TransmissionCallback);
        event.data.transmissionCallbackData.destinationMac = MacAddress{info->des_addr};
        sendEvent(&event);
    }

}
