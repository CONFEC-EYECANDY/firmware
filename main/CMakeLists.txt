idf_component_register(
        SRCS
            "main.cpp"
            "Mesh.cpp"
            "WiFiManager.cpp"

        PRIV_REQUIRES
            nvs_flash
            esp_netif
            esp_event
            esp_wifi
        INCLUDE_DIRS "."
)