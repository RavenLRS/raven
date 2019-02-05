#pragma once

#include <stddef.h>

#include "io/io.h"

#include "util/time.h"

#define SERIAL_IO(port) IO_MAKE(serial_port_read, serial_port_write, serial_port_io_flags, port)

#define SERIAL_UNUSED_GPIO -1

typedef struct serial_port_s serial_port_t;

typedef enum
{
    SERIAL_PARITY_DISABLE,
    SERIAL_PARITY_EVEN,
    SERIAL_PARITY_ODD,
} serial_parity_e;

typedef enum
{
    SERIAL_STOP_BITS_1,
    SERIAL_STOP_BITS_2,
} serial_stop_bits_e;

typedef enum
{
    SERIAL_HALF_DUPLEX_MODE_NONE,
    SERIAL_HALF_DUPLEX_MODE_RX,
    SERIAL_HALF_DUPLEX_MODE_TX,
} serial_half_duplex_mode_e;

typedef void (*serial_byte_callback_f)(const serial_port_t *port, uint8_t b, void *user_data);

typedef struct serial_port_config_s
{
    int baud_rate;
    // Set tx == rx for half duplex. -1 for unused.
    int tx_pin;
    int rx_pin;
    int tx_buffer_size;
    int rx_buffer_size;
    serial_parity_e parity;
    serial_stop_bits_e stop_bits;
    bool inverted;
    serial_byte_callback_f byte_callback;
    void *byte_callback_data;
} serial_port_config_t;

serial_port_t *serial_port_open(const serial_port_config_t *config);
int serial_port_read(serial_port_t *port, void *buf, size_t size, time_ticks_t timeout);
bool serial_port_begin_write(serial_port_t *port);
bool serial_port_end_write(serial_port_t *port);
int serial_port_write(serial_port_t *port, const void *buf, size_t size);
bool serial_port_set_baudrate(serial_port_t *port, uint32_t baudrate);
bool serial_port_set_inverted(serial_port_t *port, bool inverted);
void serial_port_close(serial_port_t *port);
bool serial_port_is_half_duplex(const serial_port_t *port);
serial_half_duplex_mode_e serial_port_half_duplex_mode(const serial_port_t *port);
void serial_port_set_half_duplex_mode(serial_port_t *port, serial_half_duplex_mode_e mode);
void serial_port_destroy(serial_port_t **port);

io_flags_t serial_port_io_flags(serial_port_t *port);
