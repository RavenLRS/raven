#include <string.h>

#include <hal/log.h>

#include "io/io.h"

#include "msp/msp.h"

#include "util/crc.h"
#include "util/macros.h"

#include "msp_serial.h"

#define MSP_SERIAL_SYNC_BYTE '$'
#define MSP_V1_SERIAL_MARKER_BYTE 'M'
#define MSP_V2_SERIAL_MARKER_BYTE 'X'
#define MSP_HALF_DUPLEX_MIN_TIMEOUT_US 10000
#define MSP_HALF_DUPLEX_MAX_TIMEOUT_US MICROS_PER_SEC

static const char *TAG = "MSP.Transport.Serial";

int msp_serial_baudrate_get(msp_serial_baudrate_e br)
{
    switch (br)
    {
    case MSP_SERIAL_BAUDRATE_115200:
        return 115200;

    case MSP_SERIAL_BAUDRATE_COUNT:
        break;
    }

    return -1;
}

static unsigned msp_serial_expected_response_delay(msp_serial_t *serial, size_t response_size)
{
    if (serial->half_duplex.bytes_per_second == 0)
    {
        return MSP_HALF_DUPLEX_MAX_TIMEOUT_US;
    }

    unsigned delay = ((response_size + serial->half_duplex.last_write_size) * MICROS_PER_SEC / serial->half_duplex.bytes_per_second) * 1.2f;
    return MIN(MAX(delay, MSP_HALF_DUPLEX_MIN_TIMEOUT_US), MSP_HALF_DUPLEX_MAX_TIMEOUT_US);
}

static int msp_serial_v1_encode(msp_direction_e direction, uint16_t code, const void *data, size_t size, void *buf, size_t bufsize)
{
    if (bufsize < size + MSP_V1_PROTOCOL_BYTES)
    {
        return -1;
    }
    uint8_t *start = buf;
    uint8_t *ptr = start;
    // Preamble
    *ptr++ = MSP_SERIAL_SYNC_BYTE;
    *ptr++ = MSP_V1_SERIAL_MARKER_BYTE;
    switch (direction)
    {
    case MSP_DIRECTION_TO_MWC:
        *ptr++ = MSP_SERIAL_DIRECTION_TO_MWC_BYTE;
        break;
    case MSP_DIRECTION_FROM_MWC:
        *ptr++ = MSP_SERIAL_DIRECTION_FROM_MWC_BYTE;
        break;
    default:
        UNREACHABLE();
    }
    // Payload size
    *ptr++ = (uint8_t)size;
    // Command
    *ptr++ = (uint8_t)code;
    // Payload
    if (size > 0)
    {
        memcpy(ptr, data, size);
        ptr += size;
    }
    // CRC
    unsigned data_size = ptr - start;
    unsigned crc_size = data_size - 3; // Preamble is not used in CRC
    *ptr = crc_xor_bytes(start + 3, crc_size);
    return data_size + 1;
}

static int msp_serial_v2_encode(msp_direction_e direction, uint16_t code, const void *data, size_t size, void *buf, size_t bufsize)
{
    if (bufsize < size + MSP_V2_PROTOCOL_BYTES)
    {
        return -1;
    }
    uint8_t *start = buf;
    uint8_t *ptr = start;
    // Preamble
    *ptr++ = MSP_SERIAL_SYNC_BYTE;
    *ptr++ = MSP_V2_SERIAL_MARKER_BYTE;
    // Direction
    switch (direction)
    {
    case MSP_DIRECTION_TO_MWC:
        *ptr++ = MSP_SERIAL_DIRECTION_TO_MWC_BYTE;
        break;
    case MSP_DIRECTION_FROM_MWC:
        *ptr++ = MSP_SERIAL_DIRECTION_FROM_MWC_BYTE;
        break;
    default:
        UNREACHABLE();
    }
    // Flag
    *ptr++ = 0;
    // Command (uint16_t, LE)
    *ptr++ = (uint8_t)(code & 0xFF);
    *ptr++ = (uint8_t)((code >> 8) & 0xFF);
    // Payload size (uint16_t, LE)
    *ptr++ = (uint8_t)(size & 0xFF);
    *ptr++ = (uint8_t)((size >> 8) & 0xFF);
    // Payload
    if (size > 0)
    {
        memcpy(ptr, data, size);
        ptr += size;
    }
    // CRC
    unsigned data_size = ptr - start;
    unsigned crc_size = data_size - 3; // Preamble is not used in CRC
    *ptr = crc8_dvb_s2_bytes(start + 3, crc_size);
    return data_size + 1;
}

static bool msp_serial_decode_direction(uint8_t b, msp_direction_e *direction)
{
    switch (b)
    {
    case MSP_SERIAL_DIRECTION_TO_MWC_BYTE:
        *direction = MSP_DIRECTION_TO_MWC;
        break;
    case MSP_SERIAL_DIRECTION_FROM_MWC_BYTE:
        *direction = MSP_DIRECTION_FROM_MWC;
        break;
    case MSP_SERIAL_DIRECTION_ERROR_BYTE:
        *direction = MSP_DIRECTION_ERROR;
        break;
    default:
        LOG_W(TAG, "Invalid direction chracter 0x%02x (%c)", b, b);
        return false;
    }
    return true;
}

static int msp_serial_v1_decode(msp_serial_t *serial, int *start, int *end, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size)
{
    if (*end - *start < MSP_V1_PROTOCOL_BYTES)
    {
        return MSP_EOF;
    }

    // Size is after $M>
    uint8_t payload_size = serial->buf[*start + 3];
    size_t packet_size = MSP_V1_PROTOCOL_BYTES + payload_size;
    LOG_D(TAG, "Expecting MSPv1 packet of size %d, got %d", (int)packet_size, (int)*end - *start);
    if (*end - *start < packet_size)
    {
        // Incomplete packet, must wait until next update
        serial->half_duplex.expected_response_size = packet_size;
        return MSP_EOF;
    }
    if (!msp_serial_decode_direction(serial->buf[*start + 2], direction))
    {
        // Advance the pointer one byte, to avoid infinite loop
        *start += 1;
        return MSP_EOF;
    }
    // We got a full packet
    uint16_t packet_code = serial->buf[*start + 4];
    uint8_t crc = serial->buf[*start + packet_size - 1];
    uint8_t ccrc = crc_xor_bytes(&serial->buf[*start + 3], packet_size - 3 - 1);
    LOG_D(TAG, "Got MSPv1 serial code %d (payload size %d)", (int)packet_code, (int)payload_size);
    uint8_t *packet_data = NULL;
    if (payload_size > 0)
    {
        packet_data = &serial->buf[*start + 5];
    }
    if (cmd)
    {
        *cmd = packet_code;
    }
    if (packet_data && payload)
    {
        memcpy(payload, packet_data, MIN(payload_size, size));
    }

    // Packet was consumed
    *start += packet_size;

    if (crc != ccrc)
    {
        LOG_W(TAG, "Invalid CRC 0x%02x, expecting 0x%02x", crc, ccrc);
        LOG_BUFFER_W(TAG, &serial->buf[*start - packet_size], packet_size);
        return MSP_INVALID_CHECKSUM;
    }

    if (size < payload_size)
    {
        // Return an error, so the caller knows the buffer was not big enough
        return MSP_BUF_TOO_SMALL;
    }
    return payload_size;
}

static int msp_serial_v2_decode(msp_serial_t *serial, int *start, int *end, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size)
{
    if (*end - *start < MSP_V2_PROTOCOL_BYTES)
    {
        return MSP_EOF;
    }

    // Size is after $X>
    uint16_t payload_size = serial->buf[*start + 6];
    payload_size |= serial->buf[*start + 7] << 8;
    size_t packet_size = MSP_V2_PROTOCOL_BYTES + payload_size;
    LOG_D(TAG, "Expecting MSPv2 packet of size %d, got %d", (int)packet_size, (int)*end - *start);
    if (*end - *start < packet_size)
    {
        serial->half_duplex.expected_response_size = packet_size;
        // Incomplete packet, must wait until next update
        return MSP_EOF;
    }
    if (!msp_serial_decode_direction(serial->buf[*start + 2], direction))
    {
        // Advance the pointer one byte, to avoid infinite loop
        *start += 1;
        return MSP_EOF;
    }
    // We got a full packet
    uint16_t packet_code = serial->buf[*start + 4];
    packet_code |= serial->buf[*start + 5] << 8;
    uint8_t crc = serial->buf[*start + packet_size - 1];
    uint8_t ccrc = crc8_dvb_s2_bytes(&serial->buf[*start + 3], packet_size - 3 - 1);
    LOG_D(TAG, "Got MSPv2 serial code %d (payload size %d)", (int)packet_code, (int)payload_size);
    uint8_t *packet_data = NULL;
    if (payload_size > 0)
    {
        packet_data = &serial->buf[*start + 8];
    }
    if (cmd)
    {
        *cmd = packet_code;
    }
    if (packet_data && payload)
    {
        memcpy(payload, packet_data, MIN(payload_size, size));
    }

    // Packet was consumed
    *start += packet_size;

    if (crc != ccrc)
    {
        LOG_W(TAG, "Invalid CRC 0x%02x, expecting 0x%02x", crc, ccrc);
        LOG_BUFFER_W(TAG, &serial->buf[*start - packet_size], packet_size);
        return MSP_INVALID_CHECKSUM;
    }

    if (size < payload_size)
    {
        // Return an error, so the caller knows the buffer was not big enough
        return MSP_BUF_TOO_SMALL;
    }
    return payload_size;
}

static int msp_serial_read(void *transport, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size)
{
    msp_serial_t *serial = transport;
    int rem = sizeof(serial->buf) - serial->buf_pos;
    int n = io_read(&serial->io, &serial->buf[serial->buf_pos], rem, 0);
    if (n <= 0 && serial->buf_pos == 0)
    {
        return MSP_EOF;
    }
    if (n > 0)
    {
        serial->buf_pos += n;
        //LOG_I(TAG, "Read %d bytes, buf at %u", n, serial->buf_pos);
    }
    // Discard invalid data
    int start = 0;
    int end = serial->buf_pos;
    int ret = MSP_EOF;
    while (end - start >= 2)
    {
        if (serial->buf[start] == MSP_SERIAL_SYNC_BYTE)
        {
            // Got packet start
            switch (serial->buf[start + 1])
            {
            case MSP_V1_SERIAL_MARKER_BYTE:
                ret = msp_serial_v1_decode(serial, &start, &end, direction, cmd, payload, size);
                break;
            case MSP_V2_SERIAL_MARKER_BYTE:
                ret = msp_serial_v2_decode(serial, &start, &end, direction, cmd, payload, size);
                break;
            default:
                LOG_W(TAG, "Skipping unknown MSP sync byte %c (0x%02x)", serial->buf[start + 1], serial->buf[start + 1]);
                start++;
                continue;
            }
            break;
        }
        // Advance to the next byte
        start++;
    }

    // start contains the number of bytes consumed
    if (start > 0)
    {
        LOG_D(TAG, "Consumed %d bytes", start);
        LOG_BUFFER_D(TAG, &serial->buf[start], start);
        if (start != serial->buf_pos)
        {
            // Got some data at the end that we need to copy
            memmove(serial->buf, &serial->buf[start], end - start);
        }
        serial->buf_pos -= start;

        if (serial->half_duplex.active)
        {
            if (ret != MSP_EOF)
            {
                serial->half_duplex.response_pending_until = 0;
                if (ret > 0)
                {
                    time_micros_t now = time_micros_now();
                    uint32_t bytes_per_second = ((ret + serial->half_duplex.last_write_size) * MICROS_PER_SEC) / (now - serial->half_duplex.last_write);
                    serial->half_duplex.bytes_per_second = serial->half_duplex.bytes_per_second * 0.95f + bytes_per_second * 0.05f;
                }
            }
        }
    }

    if (serial->half_duplex.active && ret == MSP_EOF)
    {
        // Check if we already know the expected payload size. Extend the max delay if needed.
        if (serial->half_duplex.response_pending_is_estimate && serial->half_duplex.expected_response_size > 0)
        {
            unsigned delay = msp_serial_expected_response_delay(serial, serial->half_duplex.expected_response_size);
            serial->half_duplex.response_pending_until = MAX(serial->half_duplex.last_write + delay, serial->half_duplex.response_pending_until);
            serial->half_duplex.response_pending_is_estimate = false;
        }
    }

    return ret;
}

static int msp_serial_write(void *transport, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size)
{
    msp_serial_t *serial = transport;
    uint8_t buf[MSP_MAX_PAYLOAD_SIZE];
    int packet_size;

    if (serial->half_duplex.active)
    {
        if (serial->half_duplex.response_pending_until > time_micros_now())
        {
            return MSP_BUSY;
        }
    }

    if (cmd <= 254)
    {
        packet_size = msp_serial_v1_encode(direction, cmd, payload, size, buf, sizeof(buf));
    }
    else
    {
        packet_size = msp_serial_v2_encode(direction, cmd, payload, size, buf, sizeof(buf));
    }
    if (packet_size < 0)
    {
        return packet_size;
    }
    LOG_D(TAG, "Writing %d bytes", packet_size);
    if (serial->half_duplex.active)
    {
        time_micros_t now = time_micros_now();
        serial->half_duplex.last_write = now;
        serial->half_duplex.last_write_size = packet_size;
        // Initially, expect a reply of 32 bytes. We'll adjust it
        // when the packet header is received.
        unsigned expected_delay = msp_serial_expected_response_delay(serial, 32);
        serial->half_duplex.response_pending_until = now + expected_delay;
        serial->half_duplex.response_pending_is_estimate = true;
        serial->half_duplex.expected_response_size = 0;
    }
    return io_write(&serial->io, buf, packet_size);
}

void msp_serial_init(msp_serial_t *tr, io_t *io)
{
    tr->transport.vtable.read = msp_serial_read;
    tr->transport.vtable.write = msp_serial_write;
    tr->io = *io;
    memset(&tr->half_duplex, 0, sizeof(tr->half_duplex));
    tr->half_duplex.active = io_is_half_duplex(io);
    tr->buf_pos = 0;
}
