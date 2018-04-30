#pragma once

#include <stdint.h>

#include <driver/uart.h>

#include "config/settings.h"

#include "io/serial.h"

#include "input/input.h"

#include "msp/msp_telemetry.h"

#include "protocols/crsf.h"

#include "util/time.h"

typedef struct rmp_port_s rmp_port_t;

typedef enum {
    CRSF_INPUT_BPS_DETECT,
    CRSF_INPUT_BPS_400K,
    CRSF_INPUT_BPS_115K,
} crsf_input_bps_e;

typedef struct input_crsf_config_s
{
    // CRSF input from radio is always half duplex inverted
    int pin_num;
} input_crsf_config_t;

// Used to map air_addr_t to just one byte for CRSF and storing
// write requests until we resolve the setting.
typedef struct input_crsf_addr_s
{
    air_addr_t addr;
    time_micros_t last_seen;
    crsf_parameter_write_t pending_write;
} input_crsf_addr_t;

#define CRSF_INPUT_FRAME_QUEUE_SIZE 4
#define CRSF_INPUT_ADDR_LIST_SIZE 8

typedef struct input_crsf_s
{
    input_t input;
    serial_port_t *serial_port;
    crsf_input_bps_e bps;
    time_micros_t last_isr;
    time_micros_t last_frame_recv;
    time_micros_t next_resp_frame;
    time_micros_t enable_rx_deadline;
    time_micros_t bps_detect_switched;
    unsigned baud_rate;
    crsf_port_t crsf;
    uart_isr_handle_t isr_handle;
    int pin_num;
    unsigned telemetry_pos;
    const rmp_port_t *rmp_port;
    input_crsf_addr_t rmp_addresses[CRSF_INPUT_ADDR_LIST_SIZE];
    uint8_t ping_filter;
    RING_BUFFER_DECLARE(scheduled, crsf_frame_t, CRSF_INPUT_FRAME_QUEUE_SIZE);
    msp_telemetry_t msp_telemetry;
} input_crsf_t;

void input_crsf_init(input_crsf_t *input);