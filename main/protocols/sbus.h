#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SBUS_BAUDRATE 100000
#define SBUS_EXPECTED_TRANSMISSION_TIME_US ((1000000 * (sizeof(sbus_payload_t) * (8 + 1 + 2))) / SBUS_BAUDRATE)
#define SBUS_START_BYTE 0x0F
#define SBUS_END_BYTE 0x00
#define SBUS_NUM_CHANNELS 16

// Looks like FrSky hardware never sends values outside this range,
// but we should test this with hardware from other manufacturers.
#define SBUS_CHANNEL_VALUE_MIN 172
#define SBUS_CHANNEL_VALUE_MAX 1811

#define SBUS_FLAG_CHANNEL_16 (1 << 0)
#define SBUS_FLAG_CHANNEL_17 (1 << 1)
#define SBUS_FLAG_PACKET_LOST (1 << 2)
#define SBUS_FLAG_FAILSAFE_ACTIVE (1 << 3)

typedef struct rc_data_s rc_data_t;

typedef struct sbus_data_s
{
    unsigned ch0 : 11;
    unsigned ch1 : 11;
    unsigned ch2 : 11;
    unsigned ch3 : 11;
    unsigned ch4 : 11;
    unsigned ch5 : 11;
    unsigned ch6 : 11;
    unsigned ch7 : 11;
    unsigned ch8 : 11;
    unsigned ch9 : 11;
    unsigned ch10 : 11;
    unsigned ch11 : 11;
    unsigned ch12 : 11;
    unsigned ch13 : 11;
    unsigned ch14 : 11;
    unsigned ch15 : 11;
    uint8_t flags : 8;
} __attribute__((packed)) sbus_data_t;

typedef struct sbus_payload_s
{
    uint8_t start_byte : 8;
    sbus_data_t data;
    uint8_t end_byte : 8;
} __attribute__((packed)) sbus_payload_t;

_Static_assert(sizeof(sbus_payload_t) == 25, "sbus_payload_t size != 25");

typedef union {
    uint8_t bytes[sizeof(sbus_payload_t)];
    sbus_payload_t payload;
} sbus_frame_t;

// SBUS uses the same channel stepping and numbering as our internal representation,
// just with +1 offset.
inline unsigned channel_from_sbus_value(unsigned sbus_val) { return sbus_val - 1; }
inline unsigned channel_to_sbus_value(unsigned val) { return val + 1; }

void sbus_encode_data(sbus_data_t *data, const rc_data_t *rc_data, bool failsafe);
