#include <math.h>
#include <stdio.h>
#include <string.h>

#include "util/macros.h"

#include "telemetry.h"

typedef struct telemetry_info_s
{
    telemetry_type_e type;
    const char *name;
    const char *(*format)(const telemetry_t *val, char *buf, size_t bufsize);
} telemetry_info_t;

static const char *telemetry_format_str(const telemetry_t *val, char *buf, size_t bufsize)
{
    return val->val.s;
}

static const char *telemetry_format_dbm(const telemetry_t *val, char *buf, size_t bufsize)
{
    int8_t dbm = val->val.i8;
    int mw = roundf(powf(10, dbm / 10.0));
    snprintf(buf, bufsize, "%dmW", mw);
    return buf;
}

static const char *telemetry_format_db(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%ddB", val->val.i8);
    return buf;
}

static const char *telemetry_format_snr(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%1.01fdB", val->val.i8 * 0.25f);
    return buf;
}

static const char *telemetry_format_voltage(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%.02fV", val->val.u16 / 100.0);
    return buf;
}

static const char *telemetry_format_current(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%.02fA", val->val.u16 / 100.0);
    return buf;
}

static const char *telemetry_format_mah_i32(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%dmAh", val->val.i32);
    return buf;
}

static const char *telemetry_format_mah_u16(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%umAh", val->val.u16);
    return buf;
}

static const char *telemetry_format_spercentage(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%d%%", val->val.u8);
    return buf;
}

static const char *telemetry_format_percentage(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%u%%", val->val.u8);
    return buf;
}

static const char *telemetry_format_altitude(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%.02fm", val->val.i32 / 100.0);
    return buf;
}

static const char *telemetry_format_vertical_speed(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%.02fm/s", val->val.i16 / 100.0);
    return buf;
}

static const char *telemetry_format_deg(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%ddeg", val->val.u16 / 100);
    return buf;
}

static const char *telemetry_format_acc(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%.02fG", val->val.i32 / 100.0);
    return buf;
}

static const char *telemetry_format_att(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%+.02fdeg", val->val.i16 / 100.0);
    return buf;
}

static const char *telemetry_format_gps_fix(const telemetry_t *val, char *buf, size_t bufsize)
{
    switch ((telemetry_gps_fix_type_e)val->val.u8)
    {
    case TELEMETRY_GPS_FIX_NONE:
        return "None";
    case TELEMETRY_GPS_FIX_2D:
        return "2D";
    case TELEMETRY_GPS_FIX_3D:
        return "3D";
    }
    return NULL;
}

static const char *telemetry_format_u8(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%u", val->val.u8);
    return buf;
}

static const char *telemetry_format_coordinate(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%.06f", val->val.i32 / 10000000.0);
    return buf;
}

static const char *telemetry_format_horizontal_speed(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%.02fkm/h", (val->val.u16 / 100.0) * 3.6);
    return buf;
}

static const char *telemetry_format_hdop(const telemetry_t *val, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "%.02f", val->val.u16 / 100.0);
    return buf;
}

static const telemetry_info_t uplink_info[] = {
    {TELEMETRY_TYPE_STRING, "Pilot", telemetry_format_str},       // TELEMETRY_ID_PILOT_NAME
    {TELEMETRY_TYPE_INT8, "TX RSSI", telemetry_format_db},        // TELEMETRY_ID_TX_RSSI_ANT1
    {TELEMETRY_TYPE_INT8, "TX LQ", telemetry_format_spercentage}, // TELEMETRY_ID_TX_LINK_QUALITY
    {TELEMETRY_TYPE_INT8, "TX SNR", telemetry_format_snr},        // TELEMETRY_ID_TX_SNR
    {TELEMETRY_TYPE_INT8, "TX Pwr.", telemetry_format_dbm},       // TELEMETRY_ID_TX_RF_POWER
};

ARRAY_ASSERT_COUNT(uplink_info, TELEMETRY_UPLINK_COUNT, "invalid uplink telemetry info count");

static const telemetry_info_t downlink_info[] = {
    {TELEMETRY_TYPE_STRING, "Craft", telemetry_format_str},                 // TELEMETRY_ID_CRAFT_NAME
    {TELEMETRY_TYPE_STRING, "Flight Mode", telemetry_format_str},           // TELEMETRY_ID_FLIGHT_MODE_NAME
    {TELEMETRY_TYPE_UINT16, "Batt. V.", telemetry_format_voltage},          // TELEMETRY_ID_BAT_VOLTAGE
    {TELEMETRY_TYPE_UINT16, "Avg Cell V.", telemetry_format_voltage},       // TELEMETRY_ID_AVG_CELL_VOLTAGE
    {TELEMETRY_TYPE_INT16, "Current", telemetry_format_current},            // TELEMETRY_ID_CURRENT
    {TELEMETRY_TYPE_INT32, "mAh Drawn", telemetry_format_mah_i32},          // TELEMETRY_ID_CURRENT_DRAWN
    {TELEMETRY_TYPE_UINT16, "Batt. Cap.", telemetry_format_mah_u16},        // TELEMETRY_ID_BAT_CAPACITY
    {TELEMETRY_TYPE_UINT8, "Batt. Rem.", telemetry_format_percentage},      // TELEMETRY_ID_BAT_REMAINING_P
    {TELEMETRY_TYPE_INT32, "Altitude", telemetry_format_altitude},          // TELEMETRY_ID_ALTITUDE
    {TELEMETRY_TYPE_INT16, "Vert. Speed", telemetry_format_vertical_speed}, // TELEMETRY_ID_VERTICAL_SPEED
    {TELEMETRY_TYPE_UINT16, "Heading", telemetry_format_deg},               // TELEMETRY_ID_HEADING
    {TELEMETRY_TYPE_INT32, "Accel X", telemetry_format_acc},                // TELEMETRY_ID_ACC_X
    {TELEMETRY_TYPE_INT32, "Accel Y", telemetry_format_acc},                // TELEMETRY_ID_ACC_Y
    {TELEMETRY_TYPE_INT32, "Accel Z", telemetry_format_acc},                // TELEMETRY_ID_ACC_Z
    {TELEMETRY_TYPE_INT16, "Attitude P", telemetry_format_att},             // TELEMETRY_ID_ATTITUDE_X
    {TELEMETRY_TYPE_INT16, "Attitude R", telemetry_format_att},             // TELEMETRY_ID_ATTITUDE_Y
    {TELEMETRY_TYPE_INT16, "Attitude Y", telemetry_format_att},             // TELEMETRY_ID_ATTITUDE_Z
    {TELEMETRY_TYPE_UINT8, "GPS Fix Type", telemetry_format_gps_fix},       // TELEMETRY_ID_GPS_FIX
    {TELEMETRY_TYPE_UINT8, "GPS Satellites", telemetry_format_u8},          // TELEMETRY_ID_GPS_NUM_SATS
    {TELEMETRY_TYPE_INT32, "Lat", telemetry_format_coordinate},             // TELEMETRY_ID_GPS_LAT
    {TELEMETRY_TYPE_INT32, "Long", telemetry_format_coordinate},            // TELEMETRY_ID_GPS_LON
    {TELEMETRY_TYPE_INT32, "GPS Alt.", telemetry_format_altitude},          // TELEMETRY_ID_GPS_ALT
    {TELEMETRY_TYPE_UINT16, "Speed", telemetry_format_horizontal_speed},    // TELEMETRY_ID_GPS_SPEED
    {TELEMETRY_TYPE_UINT16, "GPS Heading", telemetry_format_deg},           // TELEMETRY_ID_GPS_HEADING
    {TELEMETRY_TYPE_UINT16, "GPS HDOP", telemetry_format_hdop},             // TELEMETRY_ID_GPS_HDOP
    {TELEMETRY_TYPE_INT8, "RX RSSI A1", telemetry_format_db},               // TELEMETRY_ID_RX_RSSI_ANT1
    {TELEMETRY_TYPE_INT8, "RX RSSI A2", telemetry_format_db},               // TELEMETRY_ID_RX_RSSI_ANT2
    {TELEMETRY_TYPE_INT8, "RX LQ", telemetry_format_spercentage},           // TELEMETRY_ID_RX_LINK_QUALITY
    {TELEMETRY_TYPE_INT8, "RX SNR", telemetry_format_snr},                  // TELEMETRY_ID_RX_SNR
    {TELEMETRY_TYPE_UINT8, "RX Ant.", telemetry_format_u8},                 // TELEMETRY_ID_RX_ACTIVE_ANT
    {TELEMETRY_TYPE_INT8, "RX Pwr.", telemetry_format_dbm},                 // TELEMETRY_ID_RX_RF_POWER
};

ARRAY_ASSERT_COUNT(downlink_info, TELEMETRY_DOWNLINK_COUNT, "invalid downlink telemetry info count");

int telemetry_get_id_count(void)
{
    return TELEMETRY_UPLINK_COUNT + TELEMETRY_DOWNLINK_COUNT;
}

int telemetry_get_id_at(int idx)
{
    if (idx < TELEMETRY_UPLINK_COUNT)
    {
        return TELEMETRY_UPLINK_ID(idx);
    }
    if (idx < TELEMETRY_UPLINK_COUNT + TELEMETRY_DOWNLINK_COUNT)
    {
        return TELEMETRY_DOWNLINK_ID(idx - TELEMETRY_UPLINK_COUNT);
    }
    return -1;
}

static const telemetry_info_t *telemetry_get_info(int id)
{
    const telemetry_info_t *info = downlink_info;
    if (id & TELEMETRY_UPLINK_MASK)
    {
        id &= ~TELEMETRY_UPLINK_MASK;
        info = uplink_info;
    }
    return &info[id];
}

telemetry_type_e telemetry_get_type(int id)
{
    return telemetry_get_info(id)->type;
}

size_t telemetry_get_data_size(int id)
{
    switch (telemetry_get_type(id))
    {
    case TELEMETRY_TYPE_UINT8:
        // Same as size TELEMETRY_TYPE_INT8
    case TELEMETRY_TYPE_INT8:
        return 1;
    case TELEMETRY_TYPE_UINT16:
        // Same size as TELEMETRY_TYPE_INT16
    case TELEMETRY_TYPE_INT16:
        return 2;
    case TELEMETRY_TYPE_UINT32:
        // Same size as TELEMETRY_TYPE_INT32
    case TELEMETRY_TYPE_INT32:
        return 4;
    case TELEMETRY_TYPE_STRING:
        // Variable
        return 0;
    }
    return -1;
}

const char *telemetry_get_name(int id)
{
    return telemetry_get_info(id)->name;
}

const char *telemetry_format(const telemetry_t *val, int id, char *buf, size_t buf_size)
{
    return telemetry_get_info(id)->format(val, buf, buf_size);
}

bool telemetry_has_value(const telemetry_t *val)
{
    return data_state_get_last_update(&val->data_state) > 0;
}

bool telemetry_value_is_equal(const telemetry_t *val, int id, const telemetry_val_t *new_val)
{
    switch (telemetry_get_type(id))
    {
    case TELEMETRY_TYPE_UINT8:
        return val->val.u8 == new_val->u8;
    case TELEMETRY_TYPE_INT8:
        return val->val.i8 == new_val->i8;
    case TELEMETRY_TYPE_UINT16:
        return val->val.u16 == new_val->u16;
    case TELEMETRY_TYPE_INT16:
        return val->val.i16 == new_val->i16;
    case TELEMETRY_TYPE_UINT32:
        return val->val.u32 == new_val->u32;
    case TELEMETRY_TYPE_INT32:
        return val->val.i32 == new_val->i32;
    case TELEMETRY_TYPE_STRING:
        return strcmp(val->val.s, new_val->s) == 0;
    }
    return false;
}

uint8_t telemetry_get_u8(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT8);
    return val->val.u8;
}

int8_t telemetry_get_i8(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT8);
    return val->val.i8;
}

uint16_t telemetry_get_u16(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16);
    return val->val.u16;
}

int16_t telemetry_get_i16(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT16);
    return val->val.i16;
}

uint32_t telemetry_get_u32(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT32);
    return val->val.u32;
}

int32_t telemetry_get_i32(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
    return val->val.i32;
}

const char *telemetry_get_str(const telemetry_t *val, int id)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_STRING);
    return val->val.s;
}

bool telemetry_set_u8(telemetry_t *val, int id, uint8_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT8);
    bool changed = v != val->val.u8;
    val->val.u8 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

bool telemetry_set_i8(telemetry_t *val, int id, int8_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT8);
    bool changed = v != val->val.i8;
    val->val.i8 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

bool telemetry_set_u16(telemetry_t *val, int id, uint16_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16);
    bool changed = v != val->val.u16;
    val->val.u16 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

bool telemetry_set_i16(telemetry_t *val, int id, int16_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT16);
    bool changed = v != val->val.i16;
    val->val.i16 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

bool telemetry_set_u32(telemetry_t *val, int id, uint32_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT32);
    bool changed = v != val->val.u32;
    val->val.u32 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

bool telemetry_set_i32(telemetry_t *val, int id, int32_t v, time_micros_t now)
{
    TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32);
    bool changed = v != val->val.i32;
    val->val.i32 = v;
    data_state_update(&val->data_state, changed, now);
    return changed;
}

bool telemetry_set_str(telemetry_t *val, int id, const char *str, time_micros_t now)
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

bool telemetry_set_bytes(telemetry_t *val, const void *data, size_t size, time_micros_t now)
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
