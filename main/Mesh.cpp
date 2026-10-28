#include "Mesh.hpp"

#include <algorithm>
#include <sys/types.h>

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_log.h"

#include "MeshPackets.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static auto TAG = "MESH";

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
            ESP_LOGI(TAG, "processing queue for %" PRIu32"", t);
            processQueue(t);
            if (nextFsmStateEventIn() <= 0) processFsmEventTask();
        }
    }

    void Mesh::processQueue(const TickType_t processFor)
    {
        MeshEvent currentEvent(MeshEventType::Null);
        if (xQueueReceive(eventQueue_, &currentEvent, processFor) == pdFALSE) return;

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
    }

    void Mesh::receivedDataMeshEvent(const MeshEvent* event)
    {
        auto& receivedData = event->data.receivedData;

        switch (getMeshPacketType(receivedData.data)) {
        case MeshPacketType::ParentSearch:
            respondToParentSearchBroadcast(receivedData);
        break;
        case MeshPacketType::ParentSearchResponse:
            receivedParentSearchResponsePacket(receivedData);
        break;
        case MeshPacketType::ParentRequest:
            addNewChild(receivedData);
        break;
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
        if (!belongsToMesh || !isRootNode || routingSubtable.getChildrenCount() >= MAX_CHILDREN_PER_NODE) return;

        addEspNowPeer(receivedData.senderMac, true);

        uint8_t data[3]{};
        ParentSearchResponsePacket(routingSubtable.getChildrenCount(), routingSubtable.getDepth()) >> data;
        esp_now_send(receivedData.senderMac.macAddress, data, sizeof(data));
        ESP_LOGI(TAG, "Responded to Parent Search Broadcast");
    }

    void Mesh::receivedParentSearchResponsePacket(const ReceivedData& receivedData)
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

        candidateParents.push_back(candidateParent);
        hasAddedNewCandidateParentThisCycle = true;
    }

    void Mesh::addNewChild(const ReceivedData& receivedData)
    {
        ESP_LOGI(TAG, "add child called but guess what it aint happening >:)");
        // TODO: Finish child adding logic
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
                return pdMS_TO_TICKS(PARENT_SEARCH_BROADCAST_INTERVAL_MS);
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
        return getFsmTaskDelay() - (xTaskGetTickCount() - fsmEventTimerStartedAt);
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
        if (!hasAddedNewCandidateParentThisCycle && !candidateParents.empty())
        {
            ESP_LOGI(TAG, "Processing candidate parents.");
            // all possible parents have replied
            if (const CandidateParent* candidateParent {pickCandidateParent()}; candidateParent != nullptr)
            {
                addEspNowPeer(candidateParent->macAddress, false);

                uint8_t data[1];
                ParentRequestPacket{} >> data;
                esp_now_send(candidateParent->macAddress.macAddress, data, sizeof(data));

                routingSubtable.setParentMac(candidateParent->macAddress);

                candidateParents.clear();
                hasAddedNewCandidateParentThisCycle = false;
                switchState(MeshState::Online);
                return;
            }
        }

        uint8_t data[1]{};
        ParentSearchPacket{} >> data;

        ESP_LOGI(TAG, "Sending parent broadcast");
        if (const esp_err_t err = esp_now_send(broadcastMacArr, data, sizeof(data)); err != ESP_OK)
        {
            ESP_LOGE(TAG, "ESP-NOW broadcast failed: %s", esp_err_to_name(err));
        }

        hasAddedNewCandidateParentThisCycle = false;
    }

    Mesh::CandidateParent* Mesh::pickCandidateParent() const
    {
        CandidateParent* parent = nullptr;
        uint8_t minDepth{0};
        uint8_t minChildrenCount{0};
        uint8_t minRssi{0};

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

        for (CandidateParent candidateParent : candidateParents)
        {
            if (candidateParent.rssi < CANDIDATE_PARENT_RSSI_THRESHOLD) continue;
            if (candidateParent.depth > minDepth) continue;
            if (candidateParent.childrenCount > minChildrenCount) continue;
            if (minRssi > candidateParent.rssi)
            {
                minRssi = candidateParent.rssi;
                parent = &candidateParent;
            }
        }

        return parent;
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
        ESP_LOGI(TAG, "%s", status == ESP_NOW_SEND_SUCCESS ? "Sent message." : "Unable to send message.");
    }

}
