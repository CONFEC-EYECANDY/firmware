#include "WiFiManager.hpp"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_wifi_types_generic.h>
#include <nvs_flash.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "Mesh.hpp"

static auto TAG = "WIFI";

namespace wifi
{
    esp_err_t WiFiManager::init()
    {
        if (initialized_) return ESP_OK;

        esp_err_t error = nvs_flash_init();                 if (error != ESP_OK) return error;
        ESP_LOGI(TAG, "NVS Flash Initialized...");

        // Initialize networking
        error = esp_netif_init();                           if (error != ESP_OK) return error;
        error = esp_event_loop_create_default();            if (error != ESP_OK) return error;
        ESP_LOGI(TAG, "Networking Initialized...");

        // WiFi configuration
        const wifi_init_config_t wifiConfig = WIFI_INIT_CONFIG_DEFAULT();

        error = esp_wifi_init(&wifiConfig);                             if (error != ESP_OK) return error;
        error = esp_wifi_set_storage(WIFI_STORAGE_RAM);                 if (error != ESP_OK) return error;
        error = esp_wifi_set_mode(WIFI_MODE_STA);                       if (error != ESP_OK) return error;
        error = esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);  if (error != ESP_OK) return error;
        ESP_LOGI(TAG, "WiFi Initialized!");

        if (xTaskCreate(runEntry, "WiFiManagerTask", 2048, nullptr, WIFI_TASK_PRIORITY, &taskHandle_) != pdFAIL) return ESP_FAIL;

        initialized_ = true;
        return ESP_OK;
    }
    esp_err_t WiFiManager::start()
    {
        if (!initialized_) return ESP_ERR_INVALID_STATE;
        if (running_) return ESP_OK;

        ESP_LOGI(TAG, "Starting WiFi.");

        if (const esp_err_t error = esp_wifi_start(); error != ESP_OK)
        {
            esp_wifi_stop();
            return error;
        }

        running_ = true;
        return ESP_OK;
    }
    esp_err_t WiFiManager::stop()
    {
        if (!initialized_) return ESP_ERR_INVALID_STATE;
        if (!running_) return ESP_OK;

        ESP_LOGI(TAG, "Stopping WiFi.");

        const esp_err_t error = esp_wifi_stop();

        if (error == ESP_OK)
            running_ = false;

        return error;
    }

    void WiFiManager::runEntry(void* arg)
    {
        while (true)
            getInstance().run();
    }
    void WiFiManager::run()
    {
        xQueueReceive(queueHandle_, currentEvent_, portMAX_DELAY);

        switch (currentEvent_->type)
        {
        case WiFiEventType::StartRadio:
            {
                start();

                constexpr MeshEvent event
                {
                    .type = MeshEventType::RadioUp,
                    .data = MeshEventData {},
                };
                if (!Mesh::getInstance().sendEvent(&event)) ESP_LOGE(TAG, "Failed to inform Mesh about radio start");

                break;
            }
        case WiFiEventType::StopRadio:
            {
                stop();

                constexpr MeshEvent event
                {
                    .type = MeshEventType::RadioDown,
                    .data = MeshEventData {},
                };
                if (!Mesh::getInstance().sendEvent(&event)) ESP_LOGE(TAG, "Failed to inform Mesh about radio stop");

                break;
            }
        }
    }

    bool WiFiManager::isWiFiRunning() const
    {
        return running_;
    }
    WiFiManager& WiFiManager::getInstance()
    {
        static WiFiManager instance;
        return instance;
    }
    void WiFiManager::setChannel(const uint8_t channel)
    {
        channel_ = channel;
    }
}
