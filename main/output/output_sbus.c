#include <stdio.h>
#include <string.h>

#include "protocols/sbus.h"
#include "protocols/smartport.h"

#include "rc/rc_data.h"

#include "util/time.h"

#include "output_sbus.h"

static bool output_sbus_open(void *output, void *config)
{
    output_sbus_t *output_sbus = output;
    output_sbus_config_t *cfg = config;

    serial_port_config_t sbus_port_config = {
        .baud_rate = SBUS_BAUDRATE,
        .tx_pin = cfg->sbus_pin_num,
        .rx_pin = -1,
        .tx_buffer_size = 0,
        .rx_buffer_size = 0,
        .parity = SERIAL_PARITY_EVEN,
        .stop_bits = SERIAL_STOP_BITS_2,
        .inverted = cfg->sbus_inverted,
    };

    output_sbus->output.serial_port = serial_port_open(&sbus_port_config);

    serial_port_config_t sport_port_config = {
        .baud_rate = SMARTPORT_BAUDRATE,
        .tx_pin = cfg->sport_pin_num,
        .rx_pin = cfg->sport_pin_num,
        .tx_buffer_size = 0,
        .rx_buffer_size = sizeof(smartport_payload_t) * 2,
        .parity = SERIAL_PARITY_DISABLE,
        .stop_bits = SERIAL_STOP_BITS_1,
        .inverted = cfg->sport_inverted,
        .byte_callback = NULL,
        .byte_callback_data = NULL,
    };

    output_sbus->sport_serial_port = serial_port_open(&sport_port_config);

    io_t smartport_io = SERIAL_IO(output_sbus->sport_serial_port);
    smartport_master_init(&output_sbus->sport_master, &smartport_io);
    OUTPUT_SET_MSP_TRANSPORT(output_sbus, MSP_TRANSPORT(&output_sbus->sport_master.msp_telemetry));
    output_sbus->sport_master.telemetry_found = output_sbus->output.telemetry_updated;
    output_sbus->sport_master.telemetry_data = output_sbus;
    return true;
}

static bool output_sbus_update_sbus(void *output, rc_data_t *data)
{
    output_sbus_t *output_sbus = output;
    sbus_payload_t payload = {
        .start_byte = SBUS_START_BYTE,
        .end_byte = SBUS_END_BYTE,
    };
    sbus_encode_data(&payload.data, data, failsafe_is_active(data->failsafe.input));
    int n = serial_port_write(output_sbus->output.serial_port, &payload, sizeof(payload));
    return n == sizeof(payload);
}

static bool output_sbus_update_sport(void *output, rc_data_t *data)
{
    output_sbus_t *output_sbus = output;
    smartport_master_update(&output_sbus->sport_master);
    return true;
}

static bool output_sbus_update(void *output, rc_data_t *data, bool update_rc, time_micros_t now)
{
    if (update_rc)
    {
        if (!output_sbus_update_sbus(output, data))
        {
            return false;
        }
    }
    output_sbus_update_sport(output, data);
    return true;
}

static void output_sbus_close(void *output, void *config)
{
    output_sbus_t *output_sbus = output;
    serial_port_destroy(&output_sbus->output.serial_port);
    serial_port_destroy(&output_sbus->sport_serial_port);
}

void output_sbus_init(output_sbus_t *output)
{
    output->output.flags = OUTPUT_FLAG_LOCAL;
    output->output.vtable = (output_vtable_t){
        .open = output_sbus_open,
        .update = output_sbus_update,
        .close = output_sbus_close,
    };
}
