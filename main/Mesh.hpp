#ifndef FIRMWARE_MESH_H
#define FIRMWARE_MESH_H

#include <esp_now.h>
#include <esp_wifi_types_generic.h>
#include <unordered_set>

#include "config.hpp"
#include "WiFiManager.hpp"
#include "MeshEvent.hpp"
#include "RoutingTable.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace wifi
{
    static constexpr uint8_t BROADCAST_MAC_ADDRESS_ARRAY[6] {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static MacAddress BROADCAST_MAC_ADDRESS{BROADCAST_MAC_ADDRESS_ARRAY};

    class Mesh
    {
        enum class MeshState : uint8_t
        {
            Offline,
            Online,
            FindingNemo,
            ContactingMissingChildren,
            LookingForParent,
        };
        struct CandidateParent
        {
            MacAddress macAddress {};
            signed rssi {};
            uint8_t depth {};
            uint8_t childrenCount {};
        };
        struct TransmitData
        {
            TransmitData() = default;
            ~TransmitData() = default;
            explicit TransmitData(const MacAddress destAddress, const uint8_t* data, const size_t length) : destinationMac(destAddress), data(data), length(length) {}

            MacAddress destinationMac;
            const uint8_t* data{};
            size_t length{};
        };

    public:
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        static Mesh& getInstance();

        // mesh properties
        RoutingTable routingTable {};
        TickType_t lastFsmEventAt {};
        bool belongsToMesh {false};
        bool isRootNode {SET_ROOT_NODE};

        esp_err_t init();
        static void runEntry(void* arg);

        // interact with mesh task
        static bool sendEvent(const MeshEvent& event);
        static bool transmitData(const MacAddress& destination, const uint8_t* data, size_t length);

    private:
        Mesh() = default;
        ~Mesh() = default;
        void run();

        TaskHandle_t taskHandle_{};
        QueueHandle_t eventQueue_ = nullptr;
        QueueHandle_t transmitQueue_ = nullptr;
        MeshState meshState_ {MeshState::Offline};
        std::unordered_map<MacAddress, bool, MacHash> peers {};

        // event system
        void processQueue(TickType_t waitOnQueueFor);

        void radioUpMeshEvent(const MeshEvent* event);
        void radioDownMeshEvent(const MeshEvent* event);
        void receivedDataMeshEvent(const MeshEvent* event);
        void transmissionCallbackEvent(const MeshEvent* event);

        // subroutines
        static void transmitData(const TransmitData& transmitData);
        void respondToParentSearchBroadcast(const ReceivedData& receivedData) const;
        void receivedParentSearchResponse(const ReceivedData& receivedData);
        void respondToParentRequest(const ReceivedData& receivedData);
        static void respondToConnectionCheck(const ReceivedData& receivedData);
        void receivedConnectionCheckResponse(const ReceivedData& receivedData);
        void receivedParentRequestResponse(const ReceivedData& receivedData);
        void receivedSubroutingTableChanged(const ReceivedData& receivedData);
        bool pickCandidateParent(CandidateParent* parent, int* parentIndex) const;

        void reInitEspNow();
        static void deInitEspNow();

        // finite state machine
        TickType_t getFsmEventInterval() const;
        TickType_t nextFsmEventIn() const;
        void switchState(MeshState newState);
        void processFsmEvent();

        std::vector<CandidateParent> candidateParents {};
        bool hasAddedNewCandidateParentThisCycle {false};
        bool isWaitingForParentResponse {false};
        void parentSearchFsmEvent();

        bool isWaitingForChildren {true};
        bool hasInformedParent {false};
        std::unordered_map<MacAddress, bool, MacHash> respondedChildren {};
        void findingNemoFsmEvent();

        // esp now helpers and callbacks
        static void addEspNowPeer(const MacAddress& macAddress, bool isEphemeral);
        static void removeEspNowPeer(const MacAddress& macAddress);
        static void onSignalReceived(const esp_now_recv_info_t* info, const uint8_t* data, int length);
        static void onFinishedSending(const wifi_tx_info_t* info, esp_now_send_status_t status);
    };
}

#endif //FIRMWARE_MESH_H
