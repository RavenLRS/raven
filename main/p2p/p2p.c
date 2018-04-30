
#include <string.h>

#include <hal/log.h>

#include "rmp/rmp.h"

#include "p2p.h"

static const char *TAG = "p2p";

typedef struct p2p_rmp_hdr_s
{
    air_addr_t src;
    uint8_t src_port;
    air_addr_t dst;
    uint8_t dst_port;
    uint8_t payload_size;
    uint8_t has_signature;
} PACKED p2p_rmp_hdr_t;

typedef struct p2p_rmp_msg_s
{
    p2p_rmp_hdr_t hdr;
    uint8_t payload[256];
} PACKED p2p_rmp_msg_t;

static bool p2p_decode_rmp(rmp_msg_t *msg, const void *data, size_t size)
{
    if (size >= sizeof(p2p_rmp_hdr_t))
    {
        const p2p_rmp_hdr_t *hdr = data;
        size_t expected_size = hdr->payload_size + sizeof(*hdr);
        if (hdr->has_signature)
        {
            expected_size += RMP_SIGNATURE_SIZE;
        }
        if (size != expected_size)
        {
            return false;
        }
        msg->src = hdr->src;
        msg->src_port = hdr->src_port;
        msg->dst = hdr->dst;
        msg->dst_port = hdr->dst_port;
        msg->payload_size = hdr->payload_size;
        msg->has_signature = hdr->has_signature ? true : false;
        const uint8_t *ptr = ((const uint8_t *)data) + sizeof(*hdr);
        if (hdr->payload_size > 0)
        {
            msg->payload = ptr;
            ptr += hdr->payload_size;
        }
        else
        {
            msg->payload = NULL;
        }
        if (msg->has_signature)
        {
            memcpy(msg->signature, ptr, RMP_SIGNATURE_SIZE);
            ptr += RMP_SIGNATURE_SIZE;
        }
        return true;
    }
    return false;
}

static int p2p_encode_rmp(rmp_msg_t *msg, void *data, size_t size)
{
    size_t encoded_size = sizeof(p2p_rmp_hdr_t);
    if (size < encoded_size)
    {
        LOG_E(TAG, "Could not encode p2p message of %d bytes in buffer of size %d", (int)encoded_size, (int)size);
        return -1;
    }
    p2p_rmp_hdr_t *hdr = data;
    hdr->src = msg->src;
    hdr->src_port = msg->src_port;
    hdr->dst = msg->dst;
    hdr->dst_port = msg->dst_port;
    hdr->payload_size = msg->payload_size;
    hdr->has_signature = msg->has_signature ? 1 : 0;
    uint8_t *ptr = data;
    ptr += sizeof(*hdr);
    if (msg->payload)
    {
        memcpy(ptr, msg->payload, msg->payload_size);
        ptr += msg->payload_size;
        encoded_size += msg->payload_size;
    }
    if (hdr->has_signature)
    {
        memcpy(ptr, msg->signature, RMP_SIGNATURE_SIZE);
        ptr += RMP_SIGNATURE_SIZE;
        encoded_size += RMP_SIGNATURE_SIZE;
    }
    return encoded_size;
}

static void p2p_hal_callback(p2p_hal_t *p2p_hal, const void *data, size_t size, void *user_data)
{
    rmp_msg_t msg;
    if (!p2p_decode_rmp(&msg, data, size))
    {
        LOG_W(TAG, "Error decoding p2p RMP payload of size %u", size);
        LOG_BUFFER_W(TAG, data, size);
        return;
    }
    p2p_t *p2p = user_data;
    rmp_process_message(p2p->internal.rmp, &msg, RMP_TRANSPORT_P2P);
}

static bool p2p_rmp_send(rmp_t *rmp, rmp_msg_t *msg, void *user_data)
{
    p2p_rmp_msg_t p2p_msg;
    p2p_t *p2p = user_data;

    if (!p2p->internal.started)
    {
        return false;
    }

    int p2p_msg_size = p2p_encode_rmp(msg, &p2p_msg, sizeof(p2p_msg));
    if (p2p_msg_size < 0)
    {
        return false;
    }
    p2p_hal_broadcast(&p2p->internal.hal, &p2p_msg, p2p_msg_size);
    return true;
}

void p2p_init(p2p_t *p2p, rmp_t *rmp)
{
    memset(p2p, 0, sizeof(*p2p));
    p2p->internal.rmp = rmp;
    p2p_hal_init(&p2p->internal.hal, p2p_hal_callback, p2p);
    rmp_set_transport(rmp, RMP_TRANSPORT_P2P, p2p_rmp_send, p2p);
}

void p2p_start(p2p_t *p2p)
{
    if (!p2p->internal.started)
    {
        p2p_hal_start(&p2p->internal.hal);
        p2p->internal.started = true;
    }
}

void p2p_stop(p2p_t *p2p)
{
    if (p2p->internal.started)
    {
        p2p_hal_stop(&p2p->internal.hal);
        p2p->internal.started = false;
    }
}

void p2p_update(p2p_t *p2p)
{
    // Nothing to do here for now, the current HW is totally async
}