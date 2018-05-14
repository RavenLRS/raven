#include <hal/log.h>

#include "air/air_stream.h"

#include "rmp/rmp.h"

#include "rmp_air.h"

static const char *TAG = "RMP.Air";

typedef enum
{
    RMP_AIR_MSG_SADDR = 1 << 0,
    RMP_AIR_MSG_SPORT = 1 << 1,
    RMP_AIR_MSG_DADDR = 1 << 2,
    RMP_AIR_MSG_DPORT = 1 << 3,
    RMP_AIR_MSG_SIGNED = 1 << 4,
    RMP_AIR_MSG_BROADCAST = 1 << 5,
} rmp_air_msg_flags_e;

void rmp_air_init(rmp_air_t *rmp_air, rmp_t *rmp, air_addr_t *addr, air_stream_t *stream)
{
    rmp_air->rmp = rmp;
    rmp_air->stream = stream;
    air_addr_cpy(&rmp_air->addr, addr);
    rmp_air_set_bound_addr(rmp_air, NULL);
}

void rmp_air_set_bound_addr(rmp_air_t *rmp_air, air_addr_t *bound_addr)
{
    if (bound_addr)
    {
        air_addr_cpy(&rmp_air->bound_addr, bound_addr);
    }
    else
    {
        rmp_air->bound_addr = AIR_ADDR_INVALID;
    }
}

bool rmp_air_encode(rmp_air_t *rmp_air, rmp_msg_t *msg)
{
    // We only send messages that are either intended for the bound
    // pair or broadcast for now, since there's no relay support in
    // RMP yet.
    if (air_addr_is_broadcast(&msg->dst) || air_addr_equals(&msg->dst, &rmp_air->bound_addr))
    {
        uint8_t buf[512];
        int pos = 1;
        uint8_t flags = 0;
        if (!air_addr_equals(&rmp_air->addr, &msg->src))
        {
            flags |= RMP_AIR_MSG_SADDR;
            memcpy(&buf[pos], &msg->src, sizeof(msg->src));
            pos += sizeof(msg->src);
        }
        if (msg->src_port != 0)
        {
            flags |= RMP_AIR_MSG_SPORT;
            buf[pos++] = msg->src_port;
        }
        if (!air_addr_equals(&rmp_air->bound_addr, &msg->dst))
        {
            flags |= RMP_AIR_MSG_DADDR;
            if (air_addr_is_broadcast(&msg->dst))
            {
                flags |= RMP_AIR_MSG_BROADCAST;
            }
            else
            {
                memcpy(&buf[pos], &msg->dst, sizeof(msg->dst));
                pos += sizeof(msg->dst);
            }
        }
        if (msg->dst_port != 0)
        {
            flags |= RMP_AIR_MSG_DPORT;
            buf[pos++] = msg->dst_port;
        }
        if (msg->has_signature)
        {
            flags |= RMP_AIR_MSG_SIGNED;
            memcpy(&buf[pos], msg->signature, RMP_SIGNATURE_SIZE);
            pos += RMP_SIGNATURE_SIZE;
        }

        if (msg->payload && msg->payload_size > 0)
        {
            // Check remaining space
            if (sizeof(buf) - pos < msg->payload_size)
            {
                LOG_W(TAG, "Can't send payload of size %u, %u bytes remaining in buf", msg->payload_size, sizeof(buf) - pos);
                return false;
            }

            memcpy(&buf[pos], msg->payload, msg->payload_size);
            pos += msg->payload_size;
        }

        buf[0] = flags;
        air_stream_feed_output_cmd(rmp_air->stream, AIR_CMD_RMP, buf, pos);
        return true;
    }

    return false;
}

void rmp_air_decode(rmp_air_t *rmp_air, const void *data, size_t size)
{
    rmp_msg_t msg;
    if (size < 1)
    {
        LOG_W(TAG, "Data size must be at least 1");
        return;
    }
    const uint8_t *start = data;
    const uint8_t *ptr = start;
    uint8_t flags = *ptr++;

#define remaining_bytes() (size - (ptr - start))
#define ENSURE_REMAINING_BYTES(n, flag)                                                                                       \
    do                                                                                                                        \
    {                                                                                                                         \
        if (remaining_bytes() < n)                                                                                            \
        {                                                                                                                     \
            LOG_W(TAG, "No remaining bytes for parsing flag %s: required %u, but %u remaining", #flag, n, remaining_bytes()); \
            return;                                                                                                           \
        }                                                                                                                     \
    } while (0)

    if (flags & RMP_AIR_MSG_SADDR)
    {
        ENSURE_REMAINING_BYTES(sizeof(msg.src), RMP_AIR_MSG_SADDR);
        memcpy(&msg.src, ptr, sizeof(msg.src));
        ptr += sizeof(msg.src);
    }
    else
    {
        memcpy(&msg.src, &rmp_air->bound_addr, sizeof(msg.src));
    }
    if (flags & RMP_AIR_MSG_SPORT)
    {
        ENSURE_REMAINING_BYTES(1, RMP_AIR_MSG_SPORT);
        msg.src_port = *ptr++;
    }
    else
    {
        msg.src_port = 0;
    }
    if (flags & RMP_AIR_MSG_DADDR)
    {
        if (flags & RMP_AIR_MSG_BROADCAST)
        {
            msg.dst = AIR_ADDR_BROADCAST;
        }
        else
        {
            ENSURE_REMAINING_BYTES(sizeof(msg.dst), RMP_AIR_MSG_DADDR);
            memcpy(&msg.dst, ptr, sizeof(msg.dst));
            ptr += sizeof(msg.dst);
        }
    }
    else
    {
        memcpy(&msg.dst, &rmp_air->addr, sizeof(msg.dst));
    }
    if (flags & RMP_AIR_MSG_DPORT)
    {
        ENSURE_REMAINING_BYTES(1, RMP_AIR_MSG_DPORT);
        msg.dst_port = *ptr++;
    }
    else
    {
        msg.dst_port = 0;
    }
    if (flags & RMP_AIR_MSG_SIGNED)
    {
        ENSURE_REMAINING_BYTES(RMP_SIGNATURE_SIZE, RMP_AIR_MSG_SIGNED);
        msg.has_signature = true;
        memcpy(msg.signature, ptr, RMP_SIGNATURE_SIZE);
        ptr += RMP_SIGNATURE_SIZE;
    }
    else
    {
        msg.has_signature = false;
    }

    msg.payload_size = remaining_bytes();
    msg.payload = msg.payload_size > 0 ? ptr : NULL;
    rmp_process_message(rmp_air->rmp, &msg, RMP_TRANSPORT_RC);
}