#ifndef FIRMWARE_MESH_H
#define FIRMWARE_MESH_H

#include <esp_now.h>
#include <esp_wifi_types_generic.h>


#include "config.hpp"
#include "WiFiManager.hpp"
#include "MeshEvent.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace wifi
{
    static constexpr uint8_t broadcastMac[6] {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    class Mesh
    {
        enum class MeshPacketType : uint8_t
        {
            LookingForParent,
            TransferUpData,
        };
        struct MeshPacket
        {
            explicit MeshPacket(const MeshPacketType packetType, const uint8_t version)
            {
                MeshPacket::packetType = packetType;
                MeshPacket::version = version;
            }
            MeshPacketType packetType;
            uint8_t version;
        };

        // used when child is looking for parent
        struct ParentSearchPacket : MeshPacket
        {
            explicit ParentSearchPacket(const uint8_t childrenCount) : MeshPacket(MeshPacketType::LookingForParent, childrenCount)
            {
                ParentSearchPacket::childrenCount = childrenCount;
            };
            uint8_t childrenCount;
        };

        

        // used when node notices parent search broadcast

    public:
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        static Mesh& getInstance();

        // mesh properties
        uint8_t parentMAC[6]{};
        uint8_t childrenMACs[MAX_CHILDREN_PER_NODE][6] {};
        uint8_t childrenCount {};
        bool belongsToMesh {false};
        bool isRootNode {SET_ROOT_NODE};

        esp_err_t init();
        static void runEntry(void* arg);

        // interact with mesh task
        bool sendEvent(const MeshEvent* event) const;

    private:
        Mesh() = default;
        ~Mesh() = default;

        TaskHandle_t taskHandle_{};
        QueueHandle_t eventQueue_ = nullptr;
        MeshEvent currentEvent_{};

        // event responders
        void radioUp(const MeshEvent* event);
        void radioDown(const MeshEvent* event);
        void transmitData(const MeshEvent* event);
        void receivedData(const MeshEvent* event);

        // subroutines
        void searchForParent();
        void sendAlivePacket(const MeshEvent* event);

        // esp now callbacks
        static void onSignalReceived(const esp_now_recv_info_t* info, const uint8_t* data, int length);
        static void onFinishedSending(const wifi_tx_info_t* info, esp_now_send_status_t status);

        void run();
    };
}

#endif //FIRMWARE_MESH_H
