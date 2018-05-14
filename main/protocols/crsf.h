#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "io/io.h"

#include "util/macros.h"

/* CRSF protocol characteristics: uninverted / 8 bits / 1 stop / no parity
 * XXX: Note that all data in the wire is big endian, as opposed to other protocols
 * Protocol can work both in half duplex and full duplex mode.
 *
 * Each CRSF frame has the following the structure:
 * <crsf_header_t><payload><crc>
 *
 * CRC uses crc8_dvb_s2 and includes the type field in the header plus the
 * whole payload.
 */

#define CRSF_RX_BAUDRATE 420000
#define CRSF_OPENTX_BAUDRATE 400000
#define CRSF_OPENTX_SLOW_BAUDRATE 115200 // Used for Q X7 not supporting 400kbps
#define CRSF_NUM_CHANNELS 16
#define CRSF_CHANNEL_VALUE_MIN 172
#define CRSF_CHANNEL_VALUE_MID 992
#define CRSF_CHANNEL_VALUE_MAX 1811

#define CRSF_PAYLOAD_SIZE_MAX 62
#define CRSF_FRAME_NOT_COUNTED_BYTES 2
#define CRSF_FRAME_SIZE(payload_size) ((payload_size) + 2) // See crsf_header_t.frame_size
#define CRSF_EXT_FRAME_SIZE(payload_size) (CRSF_FRAME_SIZE(payload_size) + 2)
#define CRSF_FRAME_SIZE_MAX (CRSF_PAYLOAD_SIZE_MAX + CRSF_FRAME_NOT_COUNTED_BYTES)

// Macros for big-endian (assume little endian host for now)
#define CRSF_U16(x) ((((uint16_t)x) << 8 & 0xFF) | ((uint16_t)x) >> 8)
#define CRSF_I16(x) ((int16_t)CRSF_U16(x))

#define CRSF_MSP_REQ_PAYLOAD_SIZE 8
#define CRSF_MSP_RESP_PAYLOAD_SIZE 58
#define CRSF_MSP_MAX_PAYLOAD_SIZE (CRSF_MSP_REQ_PAYLOAD_SIZE > CRSF_MSP_RESP_PAYLOAD_SIZE ? CRSF_MSP_REQ_PAYLOAD_SIZE : CRSF_MSP_RESP_PAYLOAD_SIZE)

typedef enum
{
    CRSF_FRAMETYPE_GPS = 0x02,
    CRSF_FRAMETYPE_BATTERY_SENSOR = 0x08,
    CRSF_FRAMETYPE_LINK_STATISTICS = 0x14,
    CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16,
    CRSF_FRAMETYPE_ATTITUDE = 0x1E,
    CRSF_FRAMETYPE_FLIGHT_MODE = 0x21,
    // Extended Header Frames, range: 0x28 to 0x96
    CRSF_FRAMETYPE_DEVICE_PING = 0x28,
    CRSF_FRAMETYPE_DEVICE_INFO = 0x29,
    CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B,
    CRSF_FRAMETYPE_PARAMETER_READ = 0x2C,
    CRSF_FRAMETYPE_PARAMETER_WRITE = 0x2D,
    CRSF_FRAMETYPE_COMMAND = 0x32,
    // MSP commands
    CRSF_FRAMETYPE_MSP_REQ = 0x7A,   // response request using msp sequence as command
    CRSF_FRAMETYPE_MSP_RESP = 0x7B,  // reply with 58 byte chunked binary
    CRSF_FRAMETYPE_MSP_WRITE = 0x7C, // write with 8 byte chunked binary (OpenTX outbound telemetry buffer limit)
} crsf_frame_type_e;

typedef enum
{
    CRSF_ADDRESS_BROADCAST = 0x00,
    CRSF_ADDRESS_USB = 0x10,
    CRSF_ADDRESS_TBS_CORE_PNP_PRO = 0x80,
    CRSF_ADDRESS_RESERVED1 = 0x8A,
    CRSF_ADDRESS_CURRENT_SENSOR = 0xC0,
    CRSF_ADDRESS_GPS = 0xC2,
    CRSF_ADDRESS_TBS_BLACKBOX = 0xC4,
    CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8,
    CRSF_ADDRESS_RESERVED2 = 0xCA,
    CRSF_ADDRESS_RACE_TAG = 0xCC,
    CRSF_ADDRESS_RADIO_TRANSMITTER = 0xEA,
    CRSF_ADDRESS_CRSF_RECEIVER = 0xEC,
    CRSF_ADDRESS_CRSF_TRANSMITTER = 0xEE,
} crsf_addr_e;

typedef enum
{
    CRSF_UINT8 = 0,
    CRSF_INT8 = 1,
    CRSF_UINT16 = 2,
    CRSF_INT16 = 3,
    CRSF_UINT32 = 4,
    CRSF_INT32 = 5,
    CRSF_UINT64 = 6,
    CRSF_INT64 = 7,
    CRSF_FLOAT = 8,
    CRSF_TEXT_SELECTION = 9,
    CRSF_STRING = 10,
    CRSF_FOLDER = 11,
    CRSF_INFO = 12,
    CRSF_COMMAND = 13,
    CRSF_VTX = 15,
    CRSF_OUT_OF_RANGE = 127,
} crsf_value_type_e;

typedef struct crsf_header_s
{
    uint8_t device_addr; // from crsf_addr_e
    uint8_t frame_size;  // counts size after this byte, so it must be the payload size + 2 (type and crc)
    uint8_t type;        // from crsf_frame_type_e
} PACKED crsf_header_t;

// Used by extended header frames (type in range 0x28 to 0x96)
typedef struct crsf_ext_header_s
{
    // Common header fields, see crsf_header_t
    uint8_t device_addr;
    uint8_t frame_size;
    uint8_t type;
    // Extended fields
    uint8_t dest_addr;
    uint8_t orig_addr;
} PACKED crsf_ext_header_t;

typedef struct crsf_channels_s
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
} PACKED crsf_channels_t;

_Static_assert(sizeof(crsf_channels_t) == 22, "invalid crsf_channels_t size");

typedef enum
{
    CRSF_ACTIVE_ANTENNA1 = 0,
    CRSF_ACTIVE_ANTENNA2 = 1
} crsf_active_antenna_e;

typedef enum
{
    CRSF_RF_FREQ_4_HZ = 0,
    CRSF_RF_FREQ_50_HZ = 1,
    CRSF_RF_FREQ_150_HZ = 2
} crsf_rf_freq_e;

typedef enum
{
    CRSF_RF_POWER_0_mW = 0,
    CRSF_RF_POWER_10_mW = 1,
    CRSF_RF_POWER_25_mW = 2,
    CRSF_RF_POWER_100_mW = 3,
    CRSF_RF_POWER_500_mW = 4,
    CRSF_RF_POWER_1000_mW = 5,
    CRSF_RF_POWER_2000_mW = 6,
} crsf_rf_power_e;

typedef struct crsf_link_stats_s
{
    int8_t uplink_rssi_ant1; // rssi of antenna 1 in RX (dB) [-128, 127]
    int8_t uplink_rssi_ant2; // rssi of antenna 2 in RX (dB) [-128, 127]
    int8_t uplink_lq;        // link quality as seen by the RX (%, but openTX accepts anything) [-128, 127]
    int8_t uplink_snr;       // SNR as seen by the RX (dB) [-128, 127]
    int8_t active_antenna;   // active antenna in RX. From crsf_active_antenna_e, but allows any value in [-128, 127]
    int8_t rf_mode;          // from crsf_rf_freq_e. Allows any value in [-128, 127]
    uint8_t uplink_tx_power; // from crsf_rf_power_e. Values not in the enum will show as 0mw in OpenTX.
    int8_t downlink_rssi;    // rssi in TX, it has a single antenna (dB) [-128, 127]
    int8_t downlink_lq;      // link quality as seen by the TX (%, but openTX accepts anything) [-128, 127]
    int8_t downlink_snr;     // SNR as seen by the TX (dB) [-128, 127]
} PACKED crsf_link_stats_t;

_Static_assert(sizeof(crsf_link_stats_t) == 10, "invalid crsf_link_stats_t size");

typedef struct crsf_gps_s
{
    int32_t lat;           // deg / 10_000_000
    int32_t lon;           // same as lat
    uint16_t ground_speed; // ( km/h / 10 )
    uint16_t heading;      // ( degree / 100 ) [0, 360]
    uint16_t altitude;     // ( meter ­1000m offset )
    uint8_t sats;          // number of satellites
} PACKED crsf_gps_t;

typedef struct crsf_battery_sensor_s
{
    uint16_t voltage;             // 0.1V
    uint16_t current;             // 0.1A
    uint32_t mah_drawn : 24;      // first 24 bits are drawn mah
    uint8_t percentage_remaining; // last 8 are remaining bat percentage
} PACKED crsf_battery_sensor_t;

typedef struct crsf_attitude_s
{
    int16_t pitch; // (rad / 10000)
    int16_t roll;
    int16_t yaw;
} PACKED crsf_attitude_t;

// Must be followed by CRC
typedef struct crsf_frame_s
{
    crsf_header_t header;
    union {
        crsf_channels_t channels;
        crsf_link_stats_t stats;
        crsf_gps_t gps;
        crsf_battery_sensor_t battery_sensor;
        crsf_attitude_t attitude;

        // Raw payload
        uint8_t payload[CRSF_PAYLOAD_SIZE_MAX];
    };
    // Used to guarantee we have space to write the CRC in crsf_port_write
    uint8_t crc_pad;
} PACKED crsf_frame_t;

typedef struct crsf_device_info_tail_s
{
    // All zeroes
    uint32_t null1;
    uint32_t null2;
    uint32_t null3;
    uint8_t parameter_count;
    uint8_t info_version;
} PACKED crsf_device_info_tail_t;

typedef struct crsf_device_info_s
{
    // This frame contains a null terminated string
    // followed by crsf_device_info_tail_t. Use crsf_device_info_get_tail()
    // to retrieve a pointer to the latter.
    char name[CRSF_MSP_MAX_PAYLOAD_SIZE - sizeof(crsf_ext_header_t) - sizeof(crsf_device_info_tail_t)];
} PACKED crsf_device_info_t;

typedef struct crsf_parameter_read_s
{
    uint8_t param_index; // XXX: First index is 1, not zero
    uint8_t chunks;      // Number of remaining chunks
} PACKED crsf_parameter_read_t;

#define CRSF_MAX_SETTINGS_ENTRY_PAYLOAD_SIZE (CRSF_FRAME_SIZE_MAX - sizeof(crsf_ext_header_t) - 4)
#define CRSF_SETTINGS_ENTRY_FRAME_SIZE(payload_size) CRSF_EXT_FRAME_SIZE(4 + payload_size)

typedef struct crsf_settings_entry_s
{
    uint8_t param_index;
    uint8_t chunk;
    uint8_t parent_folder_param_index;
    uint8_t type; // from crsf_value_type_e
    // The payload has variable length fields, so it can't be described
    // by a struct. See input_crsf.c for the implementation.
    uint8_t payload[CRSF_MAX_SETTINGS_ENTRY_PAYLOAD_SIZE];
} PACKED crsf_settings_entry_t;

#define CRSF_MAX_PARAMETER_WRITE_PAYLOAD_SIZE (CRSF_FRAME_SIZE_MAX - sizeof(crsf_ext_header_t) - 1)

typedef struct crsf_parameter_write_s
{
    uint8_t param_index; // As retrieved from a crsf_settings_entry_t
    // Note that all fields will be BE and might need to be converted
    // to LE
    union {
        uint8_t u8;
        uint16_t u16;
        char s[CRSF_MAX_PARAMETER_WRITE_PAYLOAD_SIZE]; // Null terminated
    } payload;
} PACKED crsf_parameter_write_t;

typedef struct crsf_ext_frame_s
{
    crsf_ext_header_t header;
    union {
        struct
        {
            uint8_t payload[CRSF_MSP_MAX_PAYLOAD_SIZE];
        } msp;
        crsf_device_info_t device_info;
        crsf_parameter_read_t parameter_read;
        crsf_settings_entry_t settings_entry;
        crsf_parameter_write_t parameter_write;
    };
    // Used to guarantee we have space to write the CRC in crsf_port_write
    uint8_t crc_pad;
} PACKED crsf_ext_frame_t;

const char *crsf_frame_str(crsf_frame_t *frame);
void crsf_frame_put_str(crsf_frame_t *frame, const char *s);
uint8_t crsf_frame_payload_size(crsf_frame_t *frame);
uint8_t crsf_frame_total_size(crsf_frame_t *frame);
uint8_t crsf_ext_frame_payload_size(crsf_ext_frame_t *frame);
inline crsf_ext_frame_t *crsf_frame_to_ext(crsf_frame_t *frame) { return (crsf_ext_frame_t *)frame; }
inline crsf_frame_t *crsf_ext_frame_to_frame(crsf_ext_frame_t *frame) { return (crsf_frame_t *)frame; }
inline crsf_device_info_tail_t *crsf_device_info_get_tail(crsf_device_info_t *info)
{
    for (int ii = 0; ii < sizeof(info->name); ii++)
    {
        if (info->name[ii] == '\0')
        {
            return (crsf_device_info_tail_t *)&info->name[ii + 1];
        }
    }
    return NULL;
}

inline unsigned channel_from_crsf_value(unsigned crsf_val) { return crsf_val; }
inline unsigned channel_to_crsf_value(unsigned val) { return val; }

typedef void (*crsf_frame_f)(void *data, crsf_frame_t *frame);

typedef struct crsf_port_s
{
    io_t io;
    crsf_frame_f frame_callback;
    void *callback_data;
    uint8_t buf[CRSF_FRAME_SIZE_MAX];
    unsigned buf_pos;
} crsf_port_t;

void crsf_port_init(crsf_port_t *port, io_t *io, crsf_frame_f frame_callback, void *callback_data);
int crsf_port_write(crsf_port_t *port, crsf_frame_t *frame);
bool crsf_port_read(crsf_port_t *port);

inline bool crsf_port_push(crsf_port_t *port, uint8_t c)
{
    if (port->buf_pos < sizeof(port->buf))
    {
        port->buf[port->buf_pos++] = c;
        return true;
    }
    return false;
}

bool crsf_port_decode(crsf_port_t *port);

inline bool crsf_port_has_buffered_data(crsf_port_t *port)
{
    return port->buf_pos > 0 && port->buf_pos < sizeof(port->buf);
}

inline void crsf_port_reset(crsf_port_t *port)
{
    port->buf_pos = 0;
}