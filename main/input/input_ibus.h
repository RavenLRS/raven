#pragma once

#include <stdint.h>

#include "config/settings.h"

#include "io/gpio.h"
#include "io/serial.h"

#include "input/input.h"

#include "msp/msp_telemetry.h"

#include "protocols/ibus.h"

#include "util/time.h"

typedef struct rmp_port_s rmp_port_t;

typedef enum
{
    IBUS_INPUT_BPS_DETECT,
    IBUS_INPUT_BPS_400K,
    IBUS_INPUT_BPS_115K,
} ibus_input_bps_e;

typedef struct input_ibus_config_s
{
    // IBUS input from radio is single line half duplex
    hal_gpio_t gpio;
} input_ibus_config_t;

#define IBUS_INPUT_FRAME_QUEUE_SIZE 4
#define IBUS_INPUT_ADDR_LIST_SIZE 8

typedef struct input_ibus_s
{
    input_t input;
    serial_port_t *serial_port;
    ibus_input_bps_e bps;
    time_micros_t last_byte_at;
    time_micros_t last_frame_recv;
    time_micros_t next_resp_frame;
    time_micros_t enable_rx_deadline;
    time_micros_t bps_detect_switched;
    unsigned baud_rate;
    ibus_port_t ibus;
    hal_gpio_t gpio;
    unsigned telemetry_pos;
    const rmp_port_t *rmp_port;
    uint8_t ping_filter;
    RING_BUFFER_DECLARE(scheduled, ibus_frame_t, IBUS_INPUT_FRAME_QUEUE_SIZE);
    msp_telemetry_t msp_telemetry;
} input_ibus_t;

void input_ibus_init(input_ibus_t *input);