#ifndef MESH__CONFIG__HPP__
#define MESH__CONFIG__HPP__

// debug name
#define NODE_NAME "A"

// esp-now can have 20 max unencrypted peers and 7 (default) to 17 encrypted peers, so 1 parent and 16 children is as max
#define MAX_CHILDREN_PER_NODE 16

// set this to true for destined root node
#define SET_ROOT_NODE true

// task priorities
#define MESH_TASK_PRIORITY 1
#define WIFI_TASK_PRIORITY 1

// mesh config
#define MAX_MESH_PENDING_EVENTS 10
#define PARENT_SEARCH_BROADCAST_INTERVAL_MS 5000
#define PARENT_RESPONSE_WAIT_TIME 500
#define MISSING_CHILDREN_CONTACT_PERIOD_MS 100
#define CHILDREN_ALIVE_TIMEOUT_MS 100
#define CANDIDATE_PARENT_RSSI_THRESHOLD (-70)
#define CONNECTION_CHECK_INTERVAL_MS 5000

// wifi config
#define MAX_WIFI_PENDING_EVENTS 10

#define BROADCAST_MAC {0xFFF, 0xFFF, 0xFFF, 0xFFFF, 0xFFF, 0xFFFF}

#endif