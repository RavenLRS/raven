#pragma once

#include <stdbool.h>

#include "air/air.h"

#include "util/time.h"

#ifndef RMP_MAX_PEERS
#define RMP_MAX_PEERS 64
#endif
#ifndef RMP_MAX_PORTS
#define RMP_MAX_PORTS 8
#endif

#define RMP_SIGNATURE_SIZE 4

enum
{
    RMP_PORT_DEVICE = 0x22,
    RMP_PORT_MSP = 0x21,
    RMP_PORT_SETTINGS = 0x42,
    RMP_PORT_RC = 0x43,
};

typedef struct rmp_s rmp_t;
typedef struct rmp_msg_s rmp_msg_t;

typedef enum
{
    RMP_SEND_FLAG_NONE = 0,
    RMP_SEND_FLAG_BROADCAST_SELF = 1 << 0,
    RMP_SEND_FLAG_BROADCAST_RC = 1 << 1,
} rmp_send_flags_e;

typedef enum
{
    RMP_TRANSPORT_P2P = 0,
    RMP_TRANSPORT_RC,
    RMP_TRANSPORT_COUNT,
} rmp_transport_type_e;

typedef bool (*rmp_transport_send_f)(rmp_t *rmp, rmp_msg_t *msg, void *user_data);

typedef enum
{
    RMP_PEER_FLAG_CAN_AUTHENTICATE = 1 << 0, // We have some means to authenticate this peer
} rmp_peer_flag_e;

typedef struct rmp_peer_s
{
    air_addr_t addr;                    // The addr for the peer we've seen
    char name[AIR_MAX_NAME_LENGTH + 1]; // Peer name
    air_role_e role;                    // Its role
    air_addr_t pair_addr;               // The addr of the TX/RX/GS paired with this peer
    rmp_peer_flag_e flags;              // See rmp_peer_flag_e
    time_ticks_t last_seen;             // Last time we've seen this peer via p2p
    time_ticks_t last_info_update;      // Last time we got the device info for this peer
    time_ticks_t last_info_req;         // Last time we requested device info from this peer
} rmp_peer_t;

typedef struct rmp_msg_s
{
    air_addr_t src;
    uint8_t src_port;
    air_addr_t dst;
    uint8_t dst_port;
    const void *payload;
    size_t payload_size;
    bool has_signature;
    uint8_t signature[RMP_SIGNATURE_SIZE];
} rmp_msg_t;

typedef struct rmp_req_s
{
    bool is_authenticated; // True iff request is loopback or signed
    rmp_msg_t *msg;
    void (*resp)(const void *resp_data, const void *payload, size_t size);
    const void *resp_data;
} rmp_req_t;

typedef void (*rmp_port_f)(rmp_t *rmp, rmp_req_t *req, void *user_data);

typedef struct rmp_port_s
{
    uint8_t port;
    rmp_port_f handler;
    void *user_data;
} rmp_port_t;

typedef struct rmp_transport_s
{
    rmp_transport_send_f send;
    void *user_data;
} rmp_transport_t;

typedef struct rmp_s
{
    struct
    {
        air_addr_t addr;
        const char *name;
        air_role_e role;
        air_pairing_t pairing;
        time_ticks_t next_p2p_ping;
        time_ticks_t next_device_info;
        const rmp_port_t *device_port;
        rmp_peer_t peers[RMP_MAX_PEERS];
        rmp_port_t ports[RMP_MAX_PORTS];
        rmp_transport_t transports[RMP_TRANSPORT_COUNT];
    } internal;
} rmp_t;

void rmp_init(rmp_t *rmp, air_addr_t *addr);
void rmp_update(rmp_t *rmp);
// The data won't be copied, its the responsability of the caller to keep
// name alive (this is used to grab the up-to-date data from the telemetry)
void rmp_set_name(rmp_t *rmp, const char *name);
int rmp_get_name(rmp_t *rmp, char *name, size_t size);
void rmp_set_role(rmp_t *rmp, air_role_e role);
void rmp_set_pairing(rmp_t *rmp, air_pairing_t *pairing);
bool rmp_can_authenticate_peer(rmp_t *rmp, const air_addr_t *addr);
bool rmp_has_p2p_peer(rmp_t *rmp, const air_addr_t *addr);
void rmp_get_p2p_counts(rmp_t *rmp, int *tx_count, int *rx_count, bool *has_pairing_as_peer);

// Open/close ports and send
const rmp_port_t *rmp_open_port(rmp_t *rmp, uint8_t number, rmp_port_f handler, void *user_data);
void rmp_close_port(rmp_t *rmp, const rmp_port_t *port);
bool rmp_send(rmp_t *rmp, const rmp_port_t *port, const air_addr_t *dst, int dst_port, const void *payload, size_t size);
bool rmp_send_flags(rmp_t *rmp, const rmp_port_t *port, const air_addr_t *dst, int dst_port, const void *payload, size_t size, rmp_send_flags_e flags);
bool rmp_send_loopback(rmp_t *rmp, const rmp_port_t *port, int dst_port, const void *payload, size_t size);

// Transports
void rmp_set_transport(rmp_t *rmp, rmp_transport_type_e type, rmp_transport_send_f send, void *user_data);
void rmp_process_message(rmp_t *rmp, rmp_msg_t *msg, rmp_transport_type_e source);
