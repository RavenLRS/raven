#include <hal/log.h>
#include <hal/md5.h>

#include "config/config.h"

#include "util/time.h"

#include "rmp.h"

static const char *TAG = "RMP";

#define RMP_P2P_PING_INTERVAL MILLIS_TO_TICKS(500)
#define RMP_DEVICE_INFO_INTERVAL SECS_TO_TICKS(30)
#define RMP_P2P_PEER_EXPIRATION_INTERVAL MILLIS_TO_TICKS(3000)

#define RMP_TRANSPORT_LOOPBACK 0xFF

typedef enum
{
    RMP_DEVICE_CODE_REQ_INFO = 1,
    RMP_DEVICE_CODE_INFO,
} rmp_device_code_e;

typedef struct rmp_device_info_s
{
    air_role_e role;
    air_addr_t pair_addr;
    char name[AIR_MAX_NAME_LENGTH + 1];
} PACKED rmp_device_info_t;

typedef struct rmp_device_frame_s
{
    uint8_t code; // from rmp_device_code_e
    union {
        rmp_device_info_t device_info;
    };
} PACKED rmp_device_frame_t;

typedef struct rmp_resp_data_s
{
    rmp_t *rmp;
    const rmp_port_t *src_port;
    air_addr_t dst;
    uint8_t dst_port;
} rmp_resp_data_t;

static rmp_peer_t *rmp_get_peer(rmp_t *rmp, const air_addr_t *addr)
{
    for (int ii = 0; ii < RMP_MAX_PEERS; ii++)
    {
        rmp_peer_t *peer = &rmp->internal.peers[ii];
        if (air_addr_equals(&peer->addr, addr))
        {
            return peer;
        }
    }
    return NULL;
}

static void rmp_update_peer_authentication(rmp_t *rmp, rmp_peer_t *peer)
{
    bool can_authenticate = config_get_pairing(NULL, &peer->addr);
    if (can_authenticate)
    {
        peer->flags |= RMP_PEER_FLAG_CAN_AUTHENTICATE;
    }
    else
    {
        peer->flags &= ~RMP_PEER_FLAG_CAN_AUTHENTICATE;
    }
}

static rmp_peer_t *rmp_add_peer(rmp_t *rmp, air_addr_t *addr)
{
    for (int ii = 0; ii < RMP_MAX_PEERS; ii++)
    {
        rmp_peer_t *peer = &rmp->internal.peers[ii];
        if (!air_addr_is_valid(&peer->addr))
        {
            // Empty slot
            air_addr_cpy(&peer->addr, addr);
            rmp_update_peer_authentication(rmp, peer);
            LOG_I(TAG, "Added p2p peer (can authenticate: %c)", (peer->flags & RMP_PEER_FLAG_CAN_AUTHENTICATE) ? 'Y' : 'N');
            return peer;
        }
    }
    return NULL;
}

static bool rmp_get_peer_key(rmp_t *rmp, air_key_t *key, const air_addr_t *addr)
{
    air_pairing_t pairing;
    if (config_get_pairing(&pairing, addr))
    {
        *key = pairing.key;
        return true;
    }
    return false;
}

static void rmp_get_message_signature(rmp_t *rmp, uint8_t *signature, rmp_msg_t *msg, air_key_t *key)
{
    hal_md5_ctx_t ctx;
    uint8_t md5_output[HAL_MD5_OUTPUT_SIZE];

    hal_md5_init(&ctx);
    hal_md5_update(&ctx, (unsigned char *)key, sizeof(*key));
    hal_md5_update(&ctx, (unsigned char *)&msg->src, sizeof(msg->src));
    hal_md5_update(&ctx, (unsigned char *)&msg->src_port, sizeof(msg->src_port));
    hal_md5_update(&ctx, (unsigned char *)&msg->dst, sizeof(msg->dst));
    hal_md5_update(&ctx, (unsigned char *)&msg->dst_port, sizeof(msg->dst_port));
    if (msg->payload && msg->payload_size > 0)
    {
        hal_md5_update(&ctx, (unsigned char *)msg->payload, msg->payload_size);
    }
    hal_md5_digest(&ctx, md5_output);
    hal_md5_destroy(&ctx);

    memcpy(signature, &md5_output[sizeof(md5_output) - RMP_SIGNATURE_SIZE], RMP_SIGNATURE_SIZE);
}

static void rmp_sign_message(rmp_t *rmp, rmp_msg_t *msg, air_key_t *key)
{
    msg->has_signature = true;
    rmp_get_message_signature(rmp, msg->signature, msg, key);
}

static bool rmp_port_number_is_free(rmp_t *rmp, uint8_t n)
{
    for (int ii = 0; ii < RMP_MAX_PORTS; ii++)
    {
        if (rmp->internal.ports[ii].port == n)
        {
            return false;
        }
    }
    return true;
}

static void rmp_send_response(const void *data, const void *payload, size_t size)
{
    const rmp_resp_data_t *resp_data = data;
    rmp_send(resp_data->rmp, resp_data->src_port, &resp_data->dst, resp_data->dst_port, payload, size);
}

static void rmp_remove_stale_peers(rmp_t *rmp, time_ticks_t now)
{
    if (now < RMP_P2P_PEER_EXPIRATION_INTERVAL)
    {
        return;
    }
    time_ticks_t threshold = now - RMP_P2P_PEER_EXPIRATION_INTERVAL;
    for (int ii = 0; ii < RMP_MAX_PEERS; ii++)
    {
        rmp_peer_t *peer = &rmp->internal.peers[ii];
        if (peer->last_seen > 0 && peer->last_seen < threshold)
        {
            LOG_I(TAG, "Removing p2p peer");
            memset(peer, 0, sizeof(*peer));
        }
    }
}

static void rmp_update_peers_info(rmp_t *rmp, time_ticks_t now)
{
    time_ticks_t interval = RMP_DEVICE_INFO_INTERVAL * 1.5f;
    if (now < interval)
    {
        return;
    }
    time_ticks_t threshold = now - interval;
    uint8_t code = RMP_DEVICE_CODE_REQ_INFO;
    for (int ii = 0; ii < RMP_MAX_PEERS; ii++)
    {
        rmp_peer_t *peer = &rmp->internal.peers[ii];
        if (peer->last_seen > 0 && peer->last_info_update < threshold && peer->last_info_req < now - SECS_TO_TICKS(10))
        {
            rmp_send(rmp, NULL, &peer->addr, RMP_PORT_DEVICE, &code, sizeof(code));
            peer->last_info_req = now;
        }
    }
}

static void rmp_update_peers(rmp_t *rmp, time_ticks_t now)
{
    rmp_remove_stale_peers(rmp, now);
    rmp_update_peers_info(rmp, now);
}

static void rmp_send_device_info(rmp_t *rmp, const air_addr_t *dst)
{
    rmp_device_frame_t frame = {
        .code = RMP_DEVICE_CODE_INFO,
        .device_info.role = rmp->internal.role,
        .device_info.pair_addr = rmp->internal.pairing.addr,
    };
    memset(frame.device_info.name, 0, sizeof(frame.device_info.name));
    if (rmp->internal.name)
    {
        strlcpy(frame.device_info.name, rmp->internal.name, sizeof(frame.device_info.name));
    }
    size_t frame_size = 1 + sizeof(frame.device_info) - sizeof(frame.device_info.name) + strlen(frame.device_info.name) + 1;
    rmp_send(rmp, NULL, dst, RMP_PORT_DEVICE, &frame, frame_size);
}

static void rmp_broadcast_device_info(rmp_t *rmp, time_ticks_t now)
{
    rmp_send_device_info(rmp, AIR_ADDR_BROADCAST);
    rmp->internal.next_device_info = now + RMP_DEVICE_INFO_INTERVAL;
}

static void rmp_device_handler(rmp_t *rmp, rmp_req_t *req, void *user_data)
{
    rmp_peer_t *peer = rmp_get_peer(rmp, &req->msg->src);
    if (!peer)
    {
        return;
    }
    const rmp_device_frame_t *frame = req->msg->payload;
    switch ((rmp_device_code_e)frame->code)
    {
    case RMP_DEVICE_CODE_REQ_INFO:
        rmp_send_device_info(rmp, &req->msg->src);
        break;
    case RMP_DEVICE_CODE_INFO:
        peer->role = frame->device_info.role;
        peer->pair_addr = frame->device_info.pair_addr;
        // Make sure name is null terminated
        for (int ii = 0; ii < sizeof(frame->device_info.name); ii++)
        {
            if (peer->name[ii] == '\0')
            {
                // Null terminated, we can proceed
                if (strlen(frame->device_info.name) > 0)
                {
                    strlcpy(peer->name, frame->device_info.name, sizeof(peer->name));
                }
                else
                {
                    air_addr_format(&peer->addr, peer->name, sizeof(peer->name));
                }
                break;
            }
        }
        peer->last_info_update = time_ticks_now();
        peer->last_info_req = 0;
        rmp_update_peer_authentication(rmp, peer);
        break;
    }
}

static bool rmp_send_p2p(rmp_t *rmp, rmp_msg_t *msg, time_ticks_t now)
{
    rmp_transport_t transport = rmp->internal.transports[RMP_TRANSPORT_P2P];
    if (transport.send)
    {
        bool ok = transport.send(rmp, msg, transport.user_data);
        if (ok && air_addr_is_broadcast(&msg->dst))
        {
            // Sending a broadcast resets the PING timer
            rmp->internal.next_p2p_ping = now + RMP_P2P_PING_INTERVAL;
        }
        return ok;
    }
    return false;
}

static bool rmp_send_rc(rmp_t *rmp, rmp_msg_t *msg, time_ticks_t now)
{
    rmp_transport_t transport = rmp->internal.transports[RMP_TRANSPORT_RC];
    if (transport.send)
    {
        return transport.send(rmp, msg, transport.user_data);
    }
    return false;
}

static void rmp_send_p2p_ping(rmp_t *rmp, time_ticks_t now)
{
    LOG_D(TAG, "Sending p2p ping");
    rmp_send(rmp, NULL, AIR_ADDR_BROADCAST, 0, NULL, 0);
}

void rmp_init(rmp_t *rmp, air_addr_t *addr)
{
    memset(rmp, 0, sizeof(*rmp));
    air_addr_cpy(&rmp->internal.addr, addr);
    rmp->internal.device_port = rmp_open_port(rmp, RMP_PORT_DEVICE, rmp_device_handler, rmp);
}

void rmp_update(rmp_t *rmp)
{
    time_ticks_t now = time_ticks_now();

    if (rmp->internal.next_device_info < now)
    {
        rmp_broadcast_device_info(rmp, now);
    }
    else if (rmp->internal.next_p2p_ping < now)
    {
        rmp_send_p2p_ping(rmp, now);
    }
    rmp_update_peers(rmp, now);
}

void rmp_set_name(rmp_t *rmp, const char *name)
{
    rmp->internal.name = name;
}

int rmp_get_name(rmp_t *rmp, char *name, size_t size)
{
    const char *suffix = NULL;
    switch (rmp->internal.role)
    {
    case AIR_ROLE_TX:
        suffix = "TX";
        break;
    case AIR_ROLE_RX:
    case AIR_ROLE_RX_AWAITING_CONFIRMATION:
        suffix = "RX";
        break;
    }
    if (rmp->internal.name && *rmp->internal.name)
    {
        return snprintf(name, size, "Raven %s: %s", suffix, rmp->internal.name);
    }
    char addr[AIR_ADDR_STRING_BUFFER_SIZE];
    air_addr_format(&rmp->internal.addr, name, size);
    return snprintf(name, size, "Raven %s: %s", suffix, addr);
}

void rmp_set_role(rmp_t *rmp, air_role_e role)
{
    rmp->internal.role = role;
}

void rmp_set_pairing(rmp_t *rmp, air_pairing_t *pairing)
{
    if (pairing)
    {
        rmp->internal.pairing = *pairing;
    }
    else
    {
        memset(&rmp->internal.pairing, 0, sizeof(rmp->internal.pairing));
    }
}

bool rmp_can_authenticate_peer(rmp_t *rmp, const air_addr_t *addr)
{
    if (air_addr_equals(&rmp->internal.addr, addr))
    {
        return true;
    }
    rmp_peer_t *peer = rmp_get_peer(rmp, addr);
    if (peer && (peer->flags & RMP_PEER_FLAG_CAN_AUTHENTICATE))
    {
        return true;
    }
    return false;
}

bool rmp_has_p2p_peer(rmp_t *rmp, const air_addr_t *addr)
{
    rmp_peer_t *peer = rmp_get_peer(rmp, addr);
    return peer && peer->last_seen > 0; // RC peers have last_seen == 0
}

void rmp_get_p2p_counts(rmp_t *rmp, int *tx_count, int *rx_count, bool *has_pairing_as_peer)
{
    *tx_count = 0;
    *rx_count = 0;
    *has_pairing_as_peer = false;
    for (int ii = 0; ii < RMP_MAX_PEERS; ii++)
    {
        rmp_peer_t *peer = &rmp->internal.peers[ii];
        if (air_addr_is_valid(&peer->addr))
        {
            switch (peer->role)
            {
            case AIR_ROLE_TX:
                (*tx_count)++;
                break;
            case AIR_ROLE_RX:
            case AIR_ROLE_RX_AWAITING_CONFIRMATION:
                (*rx_count)++;
                break;
            }
            if (air_addr_equals(&rmp->internal.pairing.addr, &peer->addr))
            {
                *has_pairing_as_peer = true;
            }
        }
    }
}

const rmp_port_t *rmp_open_port(rmp_t *rmp, uint8_t number, rmp_port_f handler, void *user_data)
{
    if (number == 0)
    {
        for (int ii = 1; ii < 128; ii++)
        {
            if (rmp_port_number_is_free(rmp, ii))
            {
                number = ii;
                break;
            }
        }
        if (number == 0)
        {
            return NULL;
        }
    }
    // Check if the port number is free
    if (rmp_port_number_is_free(rmp, number))
    {
        for (int ii = 0; ii < RMP_MAX_PORTS; ii++)
        {
            if (rmp->internal.ports[ii].port == 0)
            {
                // Free port
                rmp->internal.ports[ii].port = number;
                rmp->internal.ports[ii].handler = handler;
                rmp->internal.ports[ii].user_data = user_data;
                return &rmp->internal.ports[ii];
            }
        }
    }
    return NULL;
}

void rmp_close_port(rmp_t *rmp, const rmp_port_t *port)
{
    // Make sure we own this port
    for (int ii = 0; ii < RMP_MAX_PORTS; ii++)
    {
        if (port == &rmp->internal.ports[ii])
        {
            memset(&rmp->internal.ports[ii], 0, sizeof(rmp->internal.ports[ii]));
            break;
        }
    }
}

bool rmp_send(rmp_t *rmp, const rmp_port_t *port, const air_addr_t *dst, int dst_port, const void *payload, size_t size)
{
    return rmp_send_flags(rmp, port, dst, dst_port, payload, size, RMP_SEND_FLAG_NONE);
}

bool rmp_send_flags(rmp_t *rmp, const rmp_port_t *port, const air_addr_t *dst, int dst_port, const void *payload, size_t size, rmp_send_flags_e flags)
{
    if (!dst)
    {
        dst = AIR_ADDR_BROADCAST;
    }
    rmp_msg_t msg = {
        .src = rmp->internal.addr,
        .src_port = port ? port->port : 0,
        .dst = *dst,
        .dst_port = dst_port,
        .payload = payload,
        .payload_size = size,
        .has_signature = false,
    };
    // Check if it's a loopback message
    if (air_addr_equals(&rmp->internal.addr, dst))
    {
        rmp_process_message(rmp, &msg, RMP_TRANSPORT_LOOPBACK);
        return true;
    }
    time_ticks_t now = time_ticks_now();
    bool is_broadcast = air_addr_is_broadcast(dst);
    if (is_broadcast)
    {
        if (flags & RMP_SEND_FLAG_BROADCAST_SELF)
        {
            // Send via loopback too
            rmp_process_message(rmp, &msg, RMP_TRANSPORT_LOOPBACK);
        }
        if (flags & RMP_SEND_FLAG_BROADCAST_RC)
        {
            rmp_send_rc(rmp, &msg, now);
        }

        rmp_send_p2p(rmp, &msg, now);
        return true;
    }
    // Not a broadcast message. Check if we should sign it.
    air_key_t key;
    if (rmp_get_peer_key(rmp, &key, dst))
    {
        rmp_sign_message(rmp, &msg, &key);
    }
    if (rmp_has_p2p_peer(rmp, dst) && rmp_send_p2p(rmp, &msg, now))
    {
        return true;
    }
    return rmp_send_rc(rmp, &msg, now);
}

bool rmp_send_loopback(rmp_t *rmp, const rmp_port_t *port, int dst_port, const void *payload, size_t size)
{
    return rmp_send(rmp, port, &rmp->internal.addr, dst_port, payload, size);
}

void rmp_set_transport(rmp_t *rmp, rmp_transport_type_e type, rmp_transport_send_f send, void *user_data)
{
    rmp->internal.transports[type].send = send;
    rmp->internal.transports[type].user_data = user_data;
}

void rmp_process_message(rmp_t *rmp, rmp_msg_t *msg, rmp_transport_type_e source)
{
    char addr_buf[AIR_ADDR_STRING_BUFFER_SIZE];
    air_addr_format(&msg->src, addr_buf, sizeof(addr_buf));

    // If the src address is invalid or broadcast, reject this message
    if (!air_addr_is_valid(&msg->src) || air_addr_is_broadcast(&msg->src))
    {
        LOG_I(TAG, "Invalid message source %s", addr_buf);
        return;
    }

    // Check if it's a spoofed loopback message
    bool is_loopback = air_addr_equals(&rmp->internal.addr, &msg->src);
    if (is_loopback && source != RMP_TRANSPORT_LOOPBACK)
    {
        LOG_I(TAG, "Spoofed loopback message");
        return;
    }

    // Check if the message is addressed to us
    bool is_broadcast = air_addr_is_broadcast(&msg->dst);
    if (!is_broadcast && !air_addr_equals(&msg->dst, &rmp->internal.addr))
    {
        LOG_D(TAG, "Message from %s not for me", addr_buf);
        return;
    }

    rmp_peer_t *peer = rmp_get_peer(rmp, &msg->src);
    if (!peer)
    {
        // Check if we can add it
        peer = rmp_add_peer(rmp, &msg->src);
        if (!peer)
        {
            LOG_W(TAG, "Can't handle message from %s, no space for more peers", addr_buf);
            return;
        }
    }
    if (msg->has_signature)
    {
        // Check the signature for validity
        air_key_t key;
        if (!rmp_get_peer_key(rmp, &key, &msg->src))
        {
            LOG_W(TAG, "Dropping signed message, no key found");
            return;
        }
        uint8_t signature[RMP_SIGNATURE_SIZE];
        rmp_get_message_signature(rmp, signature, msg, &key);
        if (memcmp(signature, msg->signature, RMP_SIGNATURE_SIZE) != 0)
        {
            LOG_W(TAG, "Dropping signed message, invalid signature");
            return;
        }
    }
    if (source == RMP_TRANSPORT_P2P)
    {
        // Update last seen time
        peer->last_seen = time_ticks_now();
    }
    LOG_D(TAG, "Got message from port %u to port %u (signed: %c)", msg->src_port, msg->dst_port, msg->has_signature ? 'Y' : 'N');
    if (msg->dst_port == 0)
    {
        // Nothing else to do
        return;
    }
    // Match it to a port
    for (int ii = 0; ii < RMP_MAX_PORTS; ii++)
    {
        if (rmp->internal.ports[ii].port == msg->dst_port)
        {
            rmp_resp_data_t resp_data = {
                .rmp = rmp,
                .src_port = &rmp->internal.ports[ii],
                .dst = msg->src,
                .dst_port = msg->src_port,
            };
            rmp_req_t req = {
                // Signature has been previously verified
                .is_authenticated = is_loopback || msg->has_signature,
                .msg = msg,
                .resp = rmp_send_response,
                .resp_data = &resp_data,
            };
            rmp->internal.ports[ii].handler(rmp, &req, rmp->internal.ports[ii].user_data);
            break;
        }
    }
}
