#include <string.h>

#include <hal/log.h>

#include "io/io.h"

#include "msp/msp.h"

#include "util/crc.h"
#include "util/macros.h"

#include "msp_serial.h"

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

static int msp_serial_v1_pack(msp_direction_e direction, uint16_t code, const void *data, size_t size, void *buf, size_t bufsize)
{
    if (bufsize < size + MSP_V1_PROTOCOL_BYTES)
    {
        return -1;
    }
    uint8_t *start = buf;
    uint8_t *ptr = start;
    // Preamble
    *ptr++ = '$';
    *ptr++ = 'M';
    switch (direction)
    {
    case MSP_DIRECTION_TO_MWC:
        *ptr++ = '<';
        break;
    case MSP_DIRECTION_FROM_MWC:
        *ptr++ = '>';
        break;
    default:
        assert(0 && "unreachable");
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

static int msp_serial_read(void *transport, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size)
{
    msp_serial_t *serial = transport;
    LOG_D(TAG, "Update begin");
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
    while (end - start >= 2)
    {
        if (serial->buf[start] == '$' && serial->buf[start + 1] == 'M')
        {
            // Got packet start
            break;
        }
        // Ignore one byte
        start++;
    }

    if (end - start >= MSP_V1_PROTOCOL_BYTES)
    {
        // Size is after $M>
        uint8_t payload_size = serial->buf[start + 3];
        int packet_size = MSP_V1_PROTOCOL_BYTES + payload_size;
        LOG_D(TAG, "Expecting packet of size %d, got %d", (int)packet_size, (int)end - start);
        if (end - start < packet_size)
        {
            // Incomplete packet, must wait until next update
            return MSP_EOF;
        }
        switch (serial->buf[start + 2])
        {
        case '<':
            if (direction)
            {
                *direction = MSP_DIRECTION_TO_MWC;
            }
            break;
        case '>':
            if (direction)
            {
                *direction = MSP_DIRECTION_FROM_MWC;
            }
            break;
            // TODO: ! as direction for error signaling
        }
        // We got a full packet
        uint16_t packet_code = serial->buf[start + 4];
        uint8_t crc = serial->buf[start + packet_size - 1];
        uint8_t ccrc = crc_xor_bytes(&serial->buf[start + 3], packet_size - 3 - 1);
        LOG_D(TAG, "Got serial code %d (payload size %d)", (int)packet_code, (int)payload_size);
        uint8_t *packet_data = NULL;
        if (payload_size > 0)
        {
            packet_data = &serial->buf[start + 5];
        }
        if (cmd)
        {
            *cmd = packet_code;
        }
        if (packet_data && payload)
        {
            memcpy(payload, packet_data, MIN(payload_size, size));
        }

        bool invalid_crc = crc != ccrc;
        if (invalid_crc)
        {
            LOG_W(TAG, "Invalid CRC 0x%02x, expecting 0x%02x", crc, ccrc);
            LOG_BUFFER_W(TAG, &serial->buf[start], packet_size);
        }

        // Packet was consumed
        start += packet_size;

        // start contains the number of bytes consumed
        if (start > 0)
        {
            LOG_D(TAG, "Consumed %d bytes", start);
            if (start != serial->buf_pos)
            {
                // Got some data at the end that we need to copy
                memmove(serial->buf, &serial->buf[start], end - start);
            }
            serial->buf_pos -= start;
        }
        // Data has been consumed, we can now return
        if (invalid_crc)
        {
            return MSP_INVALID_CHECKSUM;
        }
        if (size < payload_size)
        {
            // Return an error, so the caller knows the buffer was not big enough
            return MSP_BUF_TOO_SMALL;
        }
        return payload_size;
    }
    return MSP_EOF;
}

static int msp_serial_write(void *transport, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size)
{
    uint8_t buf[MSP_MAX_PAYLOAD_SIZE];
    int packet_size = msp_serial_v1_pack(direction, cmd, payload, size, buf, sizeof(buf));
    if (packet_size < 0)
    {
        return packet_size;
    }
    msp_serial_t *serial = transport;
    return io_write(&serial->io, buf, packet_size);
}

void msp_serial_init(msp_serial_t *tr, io_t *io)
{
    tr->transport.vtable.read = msp_serial_read;
    tr->transport.vtable.write = msp_serial_write;
    tr->io = *io;
    tr->buf_pos = 0;
}
