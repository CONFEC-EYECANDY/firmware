#include "esp_netif.h"
#include "Mesh.hpp"
#include "WiFiManager.hpp"

extern "C" void app_main()
{
    wifi::WiFiManager& wifi = wifi::WiFiManager::getInstance();
    wifi::Mesh& mesh = wifi::Mesh::getInstance();

    ESP_ERROR_CHECK(wifi.init());
    ESP_ERROR_CHECK(mesh.init());

    // start wifi
    constexpr wifi::WiFiEvent event{.type = wifi::WiFiEventType::StartRadio, .data = nullptr};
    if (!wifi::WiFiManager::getInstance().sendEvent(&event)) return;
}
