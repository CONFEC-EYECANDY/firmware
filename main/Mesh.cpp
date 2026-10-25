#include "Mesh.hpp"

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_log.h"

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

        error = esp_now_register_recv_cb(onSignalReceived);     if (error != ESP_OK) return error;
        error = esp_now_register_send_cb(onFinishedSending);    if (error != ESP_OK) return error;

        eventQueue_ = xQueueCreate(MAX_MESH_PENDING_EVENTS, sizeof(MeshEvent));

        if (xTaskCreate(runEntry, "MeshTask", 2048, nullptr, MESH_TASK_PRIORITY, &taskHandle_)
            != pdPASS) return ESP_FAIL;

        return ESP_OK;
    }

    bool Mesh::sendEvent(const MeshEvent* event) const
    {
        return xQueueSend(eventQueue_, event, portMAX_DELAY) == pdTRUE;
    }

    void Mesh::runEntry(void* arg)
    {
        while (true)
            getInstance().run();
    }
    void Mesh::run()
    {
        xQueueReceive(eventQueue_, &currentEvent_, portMAX_DELAY);

        switch (currentEvent_.type)
        {
        case MeshEventType::RadioUp:
            radioUp(&currentEvent_);
        break;
        case MeshEventType::RadioDown:
            radioDown(&currentEvent_);
        break;
        case MeshEventType::ReceivedData:
            receivedData(&currentEvent_);
        break;
        case MeshEventType::TransmitData:
            transmitData(&currentEvent_);
        break;
        default:
        break;
        }
    }

    void Mesh::radioUp(const MeshEvent* event)
    {
        if (!belongsToMesh) searchForParent();
    }

    void Mesh::radioDown(const MeshEvent* event)
    {
    }

    void Mesh::transmitData(const MeshEvent* event)
    {
    }

    void Mesh::receivedData(const MeshEvent* event)
    {

    }

    // subroutines
    void Mesh::searchForParent()
    {
        const ParentSearchPacket packet(childrenCount);

        constexpr TickType_t delay = pdMS_TO_TICKS(PARENT_SEARCH_BROADCAST_DELAY_MS);
        while (!belongsToMesh)
        {
            esp_now_send(broadcastMac, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet) );

            vTaskDelay(delay);
        }
    }

    void Mesh::sendAlivePacket(const MeshEvent* event)
    {
        if (!belongsToMesh) return;

    }

    void Mesh::onSignalReceived(const esp_now_recv_info_t* info, const uint8_t* data, int length)
    {
        if (length < sizeof(MeshPacket)) return;
    }

    void Mesh::onFinishedSending(const wifi_tx_info_t* info, const esp_now_send_status_t status)
    {
        ESP_LOGI(TAG, "%s", status == ESP_NOW_SEND_SUCCESS ? "Sent message." : "Unable to send message.");
    }
}
