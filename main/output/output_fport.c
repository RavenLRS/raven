
#include <hal/log.h>

#include "io/io.h"
#include "io/serial.h"

#include "protocols/sbus.h"

#include "util/macros.h"

#include "output_fport.h"

static const char *TAG = "Output.FPort";

#define FPORT_BAUDRATE 115200
#define FPORT_SERIAL_BUFFER_SIZE 256

#define FPORT_FRAME_MARKER 0x7E

#define FPORT_ESCAPE_CHAR 0x7D
#define FPORT_ESCAPE_MASK 0x20

#define FPORT_CRC_VALUE 0xFF

#define FPORT_TELEMETRY_RECV_BUFSIZE (sizeof(smartport_payload_t) + 3)

enum
{
    FPORT_FRAME_TYPE_CONTROL = 0x00,
    FPORT_FRAME_TYPE_TELEMETRY_REQUEST = 0x01,
    FPORT_FRAME_TYPE_TELEMETRY_RESPONSE = 0x81,
};

enum
{
    FPORT_FRAME_ID_NULL = 0x00,     // (master/slave)
    FPORT_FRAME_ID_DATA = 0x10,     // (master/slave)
    FPORT_FRAME_ID_READ = 0x30,     // (master)
    FPORT_FRAME_ID_WRITE = 0x31,    // (master)
    FPORT_FRAME_ID_RESPONSE = 0x32, // (slave)
};

typedef struct fport_control_data_s
{
    sbus_data_t sbus;
    uint8_t rssi;
} PACKED fport_control_data_t;

static void output_fport_serial_byte_callback(const serial_port_t *port, uint8_t c, void *user_data)
{
    output_fport_t *output_fport = user_data;
    output_fport->buf[output_fport->buf_pos++] = c;
}

static uint8_t fport_checksum_from_sum(uint16_t sum)
{
    return FPORT_CRC_VALUE - ((sum & 0xff) + (sum >> 8));
}

static uint8_t fport_checksum(const uint8_t *data, size_t size)
{
    uint16_t sum = 0;

    for (size_t ii = 0; ii < size; ii++)
    {
        sum += *(const uint8_t *)(data + ii);
    }

    return fport_checksum_from_sum(sum);
}

static int fport_write_byte(output_fport_t *output, uint8_t b, uint16_t *sum)
{
    if (sum)
    {
        *sum += b;
    }

    if (b == FPORT_FRAME_MARKER || b == FPORT_ESCAPE_CHAR)
    {
        uint8_t buf[] = {
            FPORT_ESCAPE_CHAR,
            b ^ FPORT_ESCAPE_MASK,
        };
        serial_port_write(output->output.serial_port, buf, sizeof(buf));
        return 2;
    }

    serial_port_write(output->output.serial_port, &b, 1);
    return 1;
}

static int fport_write_payload(output_fport_t *output, uint8_t type, const void *data, size_t size)
{
    int count = 0;
    uint16_t sum = 0;
    uint8_t marker = FPORT_FRAME_MARKER;

    serial_port_write(output->output.serial_port, &marker, 1);
    count++;

    count += fport_write_byte(output, size + 1, &sum);
    count += fport_write_byte(output, type, &sum);

    const uint8_t *ptr = data;
    for (int ii = 0; ii < size; ii++, ptr++)
    {
        count += fport_write_byte(output, *ptr, &sum);
    }

    count += fport_write_byte(output, fport_checksum_from_sum(sum), NULL);

    serial_port_write(output->output.serial_port, &marker, 1);
    count++;

    return count;
}

static bool output_fport_open(void *output, void *config)
{
    output_fport_t *output_fport = output;
    output_fport_config_t *cfg = config;

    serial_port_config_t port_config = {
        .baud_rate = FPORT_BAUDRATE,
        .tx_pin = cfg->tx,
        .rx_pin = cfg->rx,
        .tx_buffer_size = FPORT_SERIAL_BUFFER_SIZE,
        .rx_buffer_size = FPORT_SERIAL_BUFFER_SIZE,
        .parity = SERIAL_PARITY_DISABLE,
        .stop_bits = SERIAL_STOP_BITS_1,
        .inverted = cfg->inverted,
        .byte_callback = output_fport_serial_byte_callback,
        .byte_callback_data = output_fport,
    };

    output_fport->output.serial_port = serial_port_open(&port_config);
    output_fport->buf_pos = 0;

    io_t smartport_io = SERIAL_IO(output_fport->output.serial_port);
    smartport_master_init(&output_fport->sport_master, &smartport_io);
    OUTPUT_SET_MSP_TRANSPORT(output_fport, MSP_TRANSPORT(&output_fport->sport_master.msp_telemetry));
    output_fport->sport_master.telemetry_found = output_fport->output.telemetry_updated;
    output_fport->sport_master.telemetry_data = output_fport;

    LOG_I(TAG, "Open");

    return true;
}

static void output_fport_receive(output_fport_t *output_fport)
{
    // Read potential telemetry response
    int n = 0;
    if (output_fport->buf_pos == 0)
    {
        // We're in half duplex mode (or we're talking to a faulty end).
        // Read from the serial buffer, it won't hurt if we are in half
        // duplex mode since serial_port_read() will just return zero.
        output_fport->buf_pos = serial_port_read(output_fport->output.serial_port, output_fport->buf, sizeof(output_fport->buf), 0);
    }
    while (n < output_fport->buf_pos)
    {
        // First byte is size without counting length an CRC
        uint8_t frame_size = output_fport->buf[n] + 2;
        if (output_fport->buf_pos < frame_size)
        {
            // No more full frames in the buffer, discard
            break;
        }
        uint8_t crc = fport_checksum(&output_fport->buf[n], frame_size - 1);
        if (crc != output_fport->buf[n + frame_size - 1])
        {
            // Invalid checksum
            LOG_W(TAG, "Invalid checksum 0x%02x, expecting 0x%02x", output_fport->buf[n + frame_size - 1], crc);
            n += frame_size;
            continue;
        }
        if (output_fport->buf[n + 1] != FPORT_FRAME_TYPE_TELEMETRY_RESPONSE)
        {
            // We don't handle this type of data (yet)
            n += frame_size;
            continue;
        }
        const smartport_payload_t *sport_payload = (const smartport_payload_t *)&output_fport->buf[n + 2];
        if (sport_payload->frame_id == 0)
        {
            // Empty frame sent by the FC when it has no data to send
            n += frame_size;
            continue;
        }
        smartport_master_decode_payload(&output_fport->sport_master, sport_payload);
        n += frame_size;
    }
    // Once we're done with a receive cycle, clear the buffer
    output_fport->buf_pos = 0;
}

static bool output_fport_update(void *output, rc_data_t *data, bool update_rc, time_micros_t now)
{
    // TODO: The half duplex nature of FPort means that we won't update the telemetry
    // when the TX is powered off (and MSP received from BT over FPort won't work).
    // This could be worked around by scheduling telemetry requests in any case,
    // but since the output doesn't know when an update to the RC data will come,
    // it will need some intrusive changes.
    output_fport_t *output_fport = output;

    if (update_rc)
    {
        output_fport_receive(output_fport);
        serial_port_begin_write(output_fport->output.serial_port);

        // Write the control frame
        fport_control_data_t control = {
            // RSSI is directly used as a % value, so we can pass the LQ as is
            .rssi = MAX(TELEMETRY_GET_DOWNLINK_I8(data, TELEMETRY_ID_RX_LINK_QUALITY), 0),
        };
        sbus_encode_data(&control.sbus, data, failsafe_is_active(data->failsafe.input));
        fport_write_payload(output_fport, FPORT_FRAME_TYPE_CONTROL, &control, sizeof(control));

        // Request telemetry. Doesn't matter what we write here since the FC
        // just checks for FPORT_FRAME_TYPE_TELEMETRY_REQUEST
        smartport_payload_t telemetry = {0};
        fport_write_payload(output_fport, FPORT_FRAME_TYPE_TELEMETRY_REQUEST, &telemetry, sizeof(telemetry));

        serial_port_end_write(output_fport->output.serial_port);
    }

    return true;
}

static void output_fport_close(void *output, void *config)
{
    output_fport_t *output_fport = output;
    serial_port_destroy(&output_fport->output.serial_port);
}

void output_fport_init(output_fport_t *output)
{
    output->output.flags = OUTPUT_FLAG_LOCAL | OUTPUT_FLAG_SENDS_RSSI;
    output->output.vtable = (output_vtable_t){
        .open = output_fport_open,
        .update = output_fport_update,
        .close = output_fport_close,
    };
}
