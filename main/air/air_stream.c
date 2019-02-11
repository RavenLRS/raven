#include <hal/log.h>

#include "air/air_cmd.h"

#include "rc/rc_data.h"

#include "util/macros.h"
#include "util/uvarint.h"

#include "air_stream.h"

static const char *TAG = "Air.Stream";

#define AIR_STREAM_TELEMETRY_MASK 0x80
// Commands have to MSB unset, then the next bit set.
#define AIR_STREAM_CMD_MASK 0x40
#define AIR_STREAM_FULL_CHANELL_MASK 0
#define AIR_STREAM_2_BIT_CHANEL_MASK (AIR_STREAM_TELEMETRY_MASK | AIR_STREAM_CMD_MASK)
#define AIR_STREAM_DATA_TYPE_MASK AIR_STREAM_2_BIT_CHANEL_MASK

static bool air_stream_sends_uplink(air_stream_t *s)
{
    // TX doesn't have a channel callback
    return s->channel == NULL;
}

static bool air_stream_sends_downlink(air_stream_t *s)
{
    return !air_stream_sends_uplink(s);
}

static bool air_stream_cmd_decode(air_cmd_e cmd, const void *data, size_t size, const void **cmd_data, size_t *cmd_data_size)
{
    const uint8_t *ptr = data;
    int data_size = air_cmd_size(cmd);
    size_t rem = size;
    if (data_size < 0)
    {
        // Variable size
        uint32_t explicit_size;
        int uvarint_size = uvarint_decode32(&explicit_size, data, size);
        if (uvarint_size <= 0)
        {
            // Invalid uvarint
            LOG_W(TAG, "Invalid uvarint in encoded variable size command");
            return false;
        }
        // XXX: This will break if we send a payload bigger than 2^31 bytes,
        // but if you're sending more than 2GB over the RC link you're fucked
        // anyway.
        data_size = (int)explicit_size;
        rem -= uvarint_size;
        ptr += uvarint_size;
    }
    if ((unsigned)data_size != rem)
    {
        // Invalid size
        LOG_W(TAG, "Invalid command size %d, expecting %d", (int)rem, (int)data_size);
        return false;
    }
    *cmd_data_size = (size_t)data_size;
    *cmd_data = data_size > 0 ? ptr : NULL;
    return true;
}

static void air_stream_decode(air_stream_t *s, time_micros_t now)
{
    uint8_t buf[AIR_STREAM_MAX_PAYLOAD_SIZE];
    unsigned p = 0;
    uint8_t c;
    ring_buffer_t *rb = &s->input_buf;
    while (ring_buffer_pop(rb, &c))
    {
        if (c == AIR_DATA_BYTE_STUFF)
        {
            if (!ring_buffer_pop(rb, &c))
            {
                // We missed a byte. Ignore this payload.
                ring_buffer_empty(rb);
                return;
            }
            c ^= AIR_DATA_XOR;
        }
        buf[p++] = c;
        if (p >= sizeof(buf))
        {
            // We got more data than expected. Maybe a newer protocol
            // which we don't understand yet?
            ring_buffer_empty(rb);
            return;
        }
    }
    // Now we got some decoding to do
    if (p > 0)
    {
        unsigned chn;
        switch (buf[0] & AIR_STREAM_DATA_TYPE_MASK)
        {
        case AIR_STREAM_2_BIT_CHANEL_MASK:
            // 2 bit encoded channel. 0 -> min, 1 -> center, 2 -> max, 3 -> invalid
            chn = ((buf[0] & ~AIR_STREAM_2_BIT_CHANEL_MASK) >> 2) + 4;
            if (chn < RC_CHANNELS_NUM)
            {
                unsigned value;
                switch (buf[0] & 3)
                {
                case 0:
                    value = RC_CHANNEL_MIN_VALUE;
                    break;
                case 1:
                    value = RC_CHANNEL_CENTER_VALUE;
                    break;
                case 2:
                    value = RC_CHANNEL_MAX_VALUE;
                    break;
                default:
                    return;
                }
                s->channel(s->user, chn, value, now);
            }
            break;
        case AIR_STREAM_TELEMETRY_MASK:
        {
            int telemetry_id = buf[0];
            // If the stream sends data uplink, it receives downlink telemetry
            if (air_stream_sends_uplink(s))
            {
                // Downlink telemetry IDs don't have the most
                // significant bit set.
                telemetry_id &= ~AIR_STREAM_TELEMETRY_MASK;
            }
            size_t telemetry_size = telemetry_get_data_size(telemetry_id);
            if (telemetry_size == 0)
            {
                // must zero terminated
                if (buf[p - 1] != 0)
                {
                    // invalid data
                    LOG_W(TAG, "Discarding variable sized telemetry data, not zero terminated");
                    break;
                }
                telemetry_size = p - 1;
            }
            else if (telemetry_size != p - 1)
            {
                // invalida data
                LOG_W(TAG, "Discarding fixed sized telemetry data (id = %d), expected %u != %d actual",
                      telemetry_id, telemetry_size, p - 1);
                LOG_BUFFER_W(TAG, &buf[1], p - 1);
                break;
            }
            s->telemetry(s->user, telemetry_id, &buf[1], telemetry_size, now);
            break;
        }
        case AIR_STREAM_CMD_MASK:
        {
            uint8_t cmd = buf[0] & ~AIR_STREAM_CMD_MASK;
            const void *cmd_data;
            size_t cmd_data_size;
            if (!air_stream_cmd_decode(cmd, &buf[1], p - 1, &cmd_data, &cmd_data_size))
            {
                LOG_W(TAG, "Discarding CMD %u", cmd);
                if (p > 1)
                {
                    LOG_BUFFER_W(TAG, &buf[1], p - 1);
                }
                break;
            }
            s->cmd(s->user, cmd, cmd_data, cmd_data_size, now);
            break;
        }
        case AIR_STREAM_FULL_CHANELL_MASK:
            if (p >= 2)
            {
                // It's a channel. First AIR_CHANNEL_BITS from the right are channel value,
                // then comes channel (number - 4)
                chn = (buf[0] >> (AIR_CHANNEL_BITS - 8)) + 4;
                if (chn < RC_CHANNELS_NUM)
                {
                    unsigned air_value = (buf[0] << 8 | buf[1]) & ((1 << AIR_CHANNEL_BITS) - 1);
                    unsigned value = RC_CHANNEL_DECODE_FROM_BITS(air_value, AIR_CHANNEL_BITS);
                    s->channel(s->user, chn, value, now);
                }
            }
            break;
        }
    }
}

void air_stream_init(air_stream_t *s, air_stream_channel_f channel, air_stream_telemetry_f telemetry, air_stream_cmd_f cmd, void *user)
{
    s->channel = channel;
    s->telemetry = telemetry;
    s->cmd = cmd;
    s->user = user;
    s->input_in_sync = false;
    s->input_seq = 0;
    RING_BUFFER_INIT(&s->input_buf, uint8_t, AIR_STREAM_INPUT_BUFFER_CAPACITY);
    RING_BUFFER_INIT(&s->output_buf, uint8_t, AIR_STREAM_OUTPUT_BUFFER_CAPACITY);
}

void air_stream_feed_input(air_stream_t *s, unsigned seq, const void *data, size_t size, time_micros_t now)
{
    if (++s->input_seq != seq)
    {
        LOG_D(TAG, "Resetting air stream sequency at %u", seq);
        s->input_in_sync = false;
        s->input_seq = seq;
        ring_buffer_empty(&s->input_buf);
    }

    const uint8_t *buf = data;
    for (size_t ii = 0; ii < size; ii++)
    {
        uint8_t c = buf[ii];
        if (!s->input_in_sync)
        {
            s->input_in_sync = c == AIR_DATA_START_STOP;
            continue;
        }
        if (c == AIR_DATA_START_STOP)
        {
            if (ring_buffer_count(&s->input_buf) > 0)
            {
                air_stream_decode(s, now);
            }
            continue;
        }
        ring_buffer_push(&s->input_buf, &c);
    }
}

static size_t air_stream_feed_output(air_stream_t *s, const void *data, size_t size)
{
    size_t n = 0;
    const uint8_t *p = data;
    for (unsigned ii = 0; ii < size; ii++, p++)
    {
        uint8_t c = *p;
        if (c == AIR_DATA_START_STOP || c == AIR_DATA_BYTE_STUFF)
        {
            n++;
            c ^= AIR_DATA_XOR;
            uint8_t bs = AIR_DATA_BYTE_STUFF;
            ring_buffer_push(&s->output_buf, &bs);
        }
        ring_buffer_push(&s->output_buf, &c);
        n++;
    }
    return n;
}

size_t air_stream_feed_output_channel(air_stream_t *s, unsigned chn, unsigned val)
{
    // Maximum channel number is now 20 since we can use
    // as much as 4 bits for 2-bit channel representation.
    ASSERT(chn < 20);

    uint8_t ss = AIR_DATA_START_STOP;
    ring_buffer_push(&s->output_buf, &ss);
    uint8_t buf[2];
    size_t bs;
    unsigned n = (chn - 4);
    unsigned air_value;
    switch (val)
    {
    case RC_CHANNEL_MIN_VALUE:
        buf[0] = AIR_STREAM_2_BIT_CHANEL_MASK | (n << 2);
        bs = 1;
        break;
    case RC_CHANNEL_CENTER_VALUE:
        buf[0] = AIR_STREAM_2_BIT_CHANEL_MASK | (n << 2) | 1;
        bs = 1;
        break;
    case RC_CHANNEL_MAX_VALUE:
        buf[0] = AIR_STREAM_2_BIT_CHANEL_MASK | (n << 2) | 2;
        bs = 1;
        break;
    default:
        air_value = RC_CHANNEL_ENCODE_TO_BITS(val, AIR_CHANNEL_BITS);
        buf[0] = (n << (AIR_CHANNEL_BITS - 8)) | air_value >> 8;
        buf[1] = air_value & 0xff;
        bs = 2;
        break;
    }
    return 1 + air_stream_feed_output(s, buf, bs);
}

static size_t air_stream_feed_output_telemetry(air_stream_t *s, telemetry_t *t, int id, uint8_t tid)
{
    size_t data_size = telemetry_get_data_size(id);
    if (data_size == 0)
    {
        ASSERT(telemetry_get_type(id) == TELEMETRY_TYPE_STRING);
        data_size = strlen(t->val.s) + 1;
    }
    uint8_t ss = AIR_DATA_START_STOP;
    ring_buffer_push(&s->output_buf, &ss);
    size_t n = air_stream_feed_output(s, &tid, sizeof(tid));
    return 1 + n + air_stream_feed_output(s, &t->val, data_size);
}

size_t air_stream_feed_output_uplink_telemetry(air_stream_t *s, telemetry_t *t, telemetry_uplink_id_e id)
{
    ASSERT(air_stream_sends_uplink(s));
    // Uplink telemetry IDs have MSB (0x80) set, so the ID sent over the air
    // is exactly its ID from the enum.
    return air_stream_feed_output_telemetry(s, t, id, id);
}

size_t air_stream_feed_output_downlink_telemetry(air_stream_t *s, telemetry_t *t, telemetry_downlink_id_e id)
{
    ASSERT(air_stream_sends_downlink(s));
    // Downlink telemetry has MSB (0x80) unset, so the ID sent over the air
    // has to be OR'ed with AIR_STREAM_TELEMETRY_MASK.
    return air_stream_feed_output_telemetry(s, t, id, id | AIR_STREAM_TELEMETRY_MASK);
}

size_t air_stream_feed_output_cmd(air_stream_t *s, uint8_t cmd, const void *data, size_t size)
{
    // We only have 6 bits for CMD encoding
    ASSERT(cmd < 64);
    uint8_t ss = AIR_DATA_START_STOP;
    ring_buffer_push(&s->output_buf, &ss);
    uint8_t cid = cmd | AIR_STREAM_CMD_MASK;
    size_t n = air_stream_feed_output(s, &cid, sizeof(cid));
    // Check if the command needs explicit size
    if (air_cmd_size(cmd) < 0)
    {
        uint8_t size_buf[9];
        int used = uvarint_encode32(size_buf, sizeof(size_buf), size);
        n += air_stream_feed_output(s, size_buf, used);
    }
    return 1 + n + air_stream_feed_output(s, data, size);
}

size_t air_stream_output_count(const air_stream_t *s)
{
    return ring_buffer_count(&s->output_buf);
}

void air_stream_reset_output(air_stream_t *s)
{
    ring_buffer_empty(&s->output_buf);
}

bool air_stream_pop_output(air_stream_t *s, uint8_t *c)
{
    return ring_buffer_pop(&s->output_buf, c);
}