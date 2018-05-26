#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "time.h"

#include "util/data_state.h"
#include "util/macros.h"

/* We have two types of telemetry, uplink and downlink. Uplink
 * goes TX->RX->FC while downlink follows the opposite direction
 * FC->RX->TX.
 */

#define TELEMETRY_STRING_MAX_SIZE 32

#define TELEMETRY_SNR_MULTIPLIER 4.0f

typedef enum
{
    TELEMETRY_TYPE_UINT8 = 1,
    TELEMETRY_TYPE_INT8,
    TELEMETRY_TYPE_UINT16,
    TELEMETRY_TYPE_INT16,
    TELEMETRY_TYPE_UINT32,
    TELEMETRY_TYPE_INT32,
    TELEMETRY_TYPE_STRING, // null terminated
} telemetry_type_e;

#define TELEMETRY_UPLINK_MASK (0x80)
#define TELEMETRY_UPLINK_ID(idx) (idx | TELEMETRY_UPLINK_MASK)
#define TELEMETRY_DOWNLINK_ID(idx) (idx)
#define TELEMETRY_UPLINK_GET_IDX(id) (id & ~TELEMETRY_UPLINK_MASK)
#define TELEMETRY_DOWNLINK_GET_IDX(id) (id)
#define TELEMETRY_IS_UPLINK(id) (id & TELEMETRY_UPLINK_MASK)
#define TELEMETRY_IS_DOWNLINK(id) (!TELEMETRY_IS_UPLINK(id))

#define TELEMETRY_ASSERT_TYPE(id, typ) assert(telemetry_get_type(id) == typ)

typedef enum
{
    TELEMETRY_ID_PILOT_NAME = TELEMETRY_UPLINK_MASK, // string
    TELEMETRY_ID_TX_RSSI_ANT1,                       // int8_t: RSSI of the antenna 1 in the TX (dB)
    TELEMETRY_ID_TX_LINK_QUALITY,                    // int8_t: Link quality as seen in the TX (%)
    TELEMETRY_ID_TX_SNR,                             // int8_t: SNR as seen by the TX (dB*TELEMETRY_SNR_MULTIPLIER)
    TELEMETRY_ID_TX_RF_POWER,                        // int8_t: TX power used by the TX (dBm)
} telemetry_uplink_id_e;

#define TELEMETRY_UPLINK_COUNT 5

typedef enum
{
    TELEMETRY_GPS_FIX_NONE = 0,
    TELEMETRY_GPS_FIX_2D,
    TELEMETRY_GPS_FIX_3D,
} telemetry_gps_fix_type_e;

typedef enum
{
    TELEMETRY_ID_CRAFT_NAME = 0,   // string
    TELEMETRY_ID_FLIGHT_MODE_NAME, // string
    TELEMETRY_ID_BAT_VOLTAGE,      // uint16_t: 0.01V
    TELEMETRY_ID_AVG_CELL_VOLTAGE, // uint16_t: 0.01V
    TELEMETRY_ID_CURRENT,          // int16_t: 0.01A
    TELEMETRY_ID_CURRENT_DRAWN,    // int32_t: Total current drawn in mah
    TELEMETRY_ID_BAT_CAPACITY,     // uin16_t: Battery capacity in mah
    TELEMETRY_ID_BAT_REMAINING_P,  // uint8_t: Battery remaining percentage
    TELEMETRY_ID_ALTITUDE,         // int32_t: altitude (positive or negative) in cm (might come from baro)
    TELEMETRY_ID_VERTICAL_SPEED,   // int16_t: Vertical speed in cm/s
    TELEMETRY_ID_HEADING,          // uint16_t: Heading in 0.01deg [0, 360)
    TELEMETRY_ID_ACC_X,            // int32_t: Acceleration on X axis 0.01G
    TELEMETRY_ID_ACC_Y,            // int32_t: Acceleration on Y axis 0.01G
    TELEMETRY_ID_ACC_Z,            // int32_t: Acceleration on Z axis 0.01G
    TELEMETRY_ID_ATTITUDE_X,       // int16_t: Attitude on X (pitch) 0.01 degree. Range (-180, 180).
    TELEMETRY_ID_ATTITUDE_Y,       // int16_t: Attitude on Y (roll) 0.01 degree. Range (-180, 180).
    TELEMETRY_ID_ATTITUDE_Z,       // int16_t: Attitude on Z (yaw) 0.01 degree. Range (-180, 180).
    TELEMETRY_ID_GPS_FIX,          // uint8_t: telemetry_gps_fix_type_e
    TELEMETRY_ID_GPS_NUM_SATS,     // uint8_t: num of sats
    TELEMETRY_ID_GPS_LAT,          // int32_t: degree / 10`000`000
    TELEMETRY_ID_GPS_LON,          // int32_t: degree / 10`000`000
    TELEMETRY_ID_GPS_ALT,          // int32_t: GPS altitude (positive or negative) in cm
    TELEMETRY_ID_GPS_SPEED,        // uint16_t: ground speed in cm/s
    TELEMETRY_ID_GPS_HEADING,      // uint16_t: degree / 100 [0, 360]
    TELEMETRY_ID_GPS_HDOP,         // uint16_t: HDOP in 0.01 increments
    TELEMETRY_ID_RX_RSSI_ANT1,     // int8_t: RSSI of the antenna 1 in the RX (dB)
    TELEMETRY_ID_RX_RSSI_ANT2,     // int8_t: RSSI of the antenna 2 (if any) in the RX (dB)
    TELEMETRY_ID_RX_LINK_QUALITY,  // int8_t: Link quality as seen in the RX (%)
    TELEMETRY_ID_RX_SNR,           // int8_t: SNR as seen by the RX (dB*TELEMETRY_SNR_MULTIPLIER)
    TELEMETRY_ID_RX_ACTIVE_ANT,    // uint8_t: Active antenna in the RX, starting at zero
    TELEMETRY_ID_RX_RF_POWER,      // int8_t: TX power used by the RX (dBm)
} telemetry_downlink_id_e;

#define TELEMETRY_DOWNLINK_COUNT 31

typedef union telemetry_u {
    uint8_t u8;
    int8_t i8;
    uint16_t u16;
    int16_t i16;
    uint32_t u32;
    int32_t i32;
    char s[TELEMETRY_STRING_MAX_SIZE + 1];
} PACKED telemetry_val_t;

#define TELEMETRY_MAX_SIZE (sizeof(telemetry_val_t))
#define TELEMETRY_COUNT (TELEMETRY_UPLINK_COUNT + TELEMETRY_DOWNLINK_COUNT)

typedef void (*telemetry_uplink_val_f)(void *data, telemetry_downlink_id_e id, telemetry_val_t *val);
typedef void (*telemetry_uplink_f)(void *data, telemetry_downlink_id_e id);

typedef void (*telemetry_downlink_val_f)(void *data, telemetry_downlink_id_e id, telemetry_val_t *val);
typedef void (*telemetry_downlink_f)(void *data, telemetry_downlink_id_e id);

typedef struct telemetry_s
{
    telemetry_val_t val;
    data_state_t data_state;
} telemetry_t;

int telemetry_get_id_count(void);
int telemetry_get_id_at(int idx);
telemetry_type_e telemetry_get_type(int id);
// Returns 0 for variable sized types (e.g. string)
size_t telemetry_get_data_size(int id);
const char *telemetry_get_name(int id);
const char *telemetry_format(const telemetry_t *val, int id, char *buf, size_t buf_size);
bool telemetry_has_value(const telemetry_t *val);

bool telemetry_value_is_equal(const telemetry_t *val, int id, const telemetry_val_t *new_val);

inline uint8_t telemetry_get_u8(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT8);
    return val->val.u8;
}

inline int8_t telemetry_get_i8(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT8);
    return val->val.i8;
}

inline uint16_t telemetry_get_u16(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16);
    return val->val.u16;
}

inline int16_t telemetry_get_i16(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT16);
    return val->val.i16;
}

inline uint32_t telemetry_get_u32(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT32);
    return val->val.u32;
}

inline int32_t telemetry_get_i32(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
    return val->val.i32;
}

inline const char *telemetry_get_str(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_STRING);
    return val->val.s;
}

inline bool telemetry_set_u8(telemetry_t *val, int id, uint8_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT8);
    bool changed = v != val->val.u8;
    val->val.u8 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

inline bool telemetry_set_i8(telemetry_t *val, int id, int8_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT8);
    bool changed = v != val->val.i8;
    val->val.i8 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

inline bool telemetry_set_u16(telemetry_t *val, int id, uint16_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16);
    bool changed = v != val->val.u16;
    val->val.u16 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

inline bool telemetry_set_i16(telemetry_t *val, int id, int16_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT16);
    bool changed = v != val->val.i16;
    val->val.i16 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

inline bool telemetry_set_u32(telemetry_t *val, int id, uint32_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT32);
    bool changed = v != val->val.u32;
    val->val.u32 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

inline bool telemetry_set_i32(telemetry_t *val, int id, int32_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
    bool changed = v != val->val.i32;
    val->val.i32 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

inline bool telemetry_set_str(telemetry_t *val, int id, const char *str, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_STRING);
    const char empty = '\0';
    bool changed = false;
    if (!str)
    {
        str = &empty;
    }
    if (strcmp(str, val->val.s) != 0)
    {
        strncpy(val->val.s, str, sizeof(val->val.s));
        val->val.s[sizeof(val->val.s) - 1] = '\0';
        changed = true;
    }
    data_state_update(&val->data_state, changed, now);
    return changed;
}

inline bool telemetry_set_bytes(telemetry_t *val, const void *data, size_t size, time_micros_t now)
{
    bool changed = false;
    if (memcmp(&val->val, data, size) != 0)
    {
        memcpy(&val->val, data, size);
        changed = true;
    }
    data_state_update(&val->data_state, changed, now);
    return changed;
}
