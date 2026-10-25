#ifndef FIRMWARE_WIFI_HPP
#define FIRMWARE_WIFI_HPP
#include <esp_err.h>
#include <functional>
#include <unordered_map>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace wifi
{
    enum class WiFiEventType : uint8_t
    {
        StartRadio,
        StopRadio,
    };

    union WiFiEventData
    {
        unsigned int dutyCycle;
    };

    struct WiFiEvent
    {
        WiFiEventType type;
        WiFiEventData* data;
    };

    struct RadioEventData
    {

    };

    class WiFiManager
    {
    public:
        WiFiManager(const WiFiManager&) = delete;
        WiFiManager& operator=(const WiFiManager&) = delete;

        static WiFiManager& getInstance();

        esp_err_t init();
        esp_err_t start();
        esp_err_t stop();

        static void runEntry(void* arg);

        void setChannel(uint8_t channel);
        bool isWiFiRunning() const;

    private:
        WiFiManager() = default;
        ~WiFiManager() = default;

        TaskHandle_t taskHandle_ = nullptr;
        QueueHandle_t queueHandle_ = nullptr;

        WiFiEvent* currentEvent_ = nullptr;

        uint8_t channel_ = 1;
        bool initialized_ = false;
        bool running_ = false;

        void run();
    };
}

#endif //FIRMWARE_WIFI_HPP
