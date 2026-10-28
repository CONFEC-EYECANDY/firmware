#ifndef FIRMWARE_MESH_H
#define FIRMWARE_MESH_H

#include <esp_now.h>
#include <esp_wifi_types_generic.h>
#include <unordered_set>

#include "config.hpp"
#include "WiFiManager.hpp"
#include "MeshEvent.hpp"
#include "RoutingSubtable.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace wifi
{
    static uint8_t broadcastMacArr[6] {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static const MacAddress broadcastMac{broadcastMacArr};

    class Mesh
    {
        enum class MeshState : uint8_t
        {
            Offline,
            Online,
            WaitingForChildrenAlive,
            ContactingMissingChildren,
            LookingForParent,
        };
        struct CandidateParent
        {
            MacAddress macAddress {};
            int8_t rssi {};
            uint8_t depth {};
            uint8_t childrenCount {};
        };

    public:
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        static Mesh& getInstance();

        // mesh properties
        RoutingSubtable routingSubtable {};
        TickType_t stateEventTimerStartedAt {};
        bool belongsToMesh {false};
        bool isRootNode {SET_ROOT_NODE};

        esp_err_t init();
        static void runEntry(void* arg);

        // interact with mesh task
        static bool sendEvent(const MeshEvent* event);

    private:
        Mesh() = default;
        ~Mesh() = default;
        void run();

        TaskHandle_t taskHandle_{};
        QueueHandle_t eventQueue_ = nullptr;
        QueueHandle_t transmitQueue_ = nullptr;
        MeshState meshState_ {MeshState::Offline};
        std::unordered_set<MacAddress, MacHash> ephemeralPeers {};

        // event system
        void processQueue(TickType_t processFor);

        void radioUpMeshEvent(const MeshEvent* event);
        void radioDownMeshEvent(const MeshEvent* event);
        void transmitDataMeshEvent(const MeshEvent* event);
        void receivedDataMeshEvent(const MeshEvent* event);
        void transmissionCallbackEvent(const MeshEvent* event);

        // subroutines
        void respondToParentSearchBroadcast(const ReceivedData& receivedData) const;
        void receivedParentSearchResponsePacket(const ReceivedData& receivedData);
        void addNewChild(const ReceivedData& receivedData);
        CandidateParent* pickCandidateParent() const;

        // finite state machine
        TickType_t getFsmTaskDelay() const;
        TickType_t nextFsmStateEventIn() const;
        void switchState(MeshState newState);
        void processFsmEventTask();

        std::vector<CandidateParent> candidateParents {};
        bool hasAddedCandidateParents {false};
        void parentBroadcastFsmEvent();

        // esp now helpers and callbacks
        static void addEspNowPeer(const MacAddress& macAddress, bool isEphemeral);
        static void removeEspNowPeer(const MacAddress& macAddress);
        static void onSignalReceived(const esp_now_recv_info_t* info, const uint8_t* data, int length);
        static void onFinishedSending(const wifi_tx_info_t* info, esp_now_send_status_t status);
    };
}

#endif //FIRMWARE_MESH_H
