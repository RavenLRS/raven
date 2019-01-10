#pragma once

#include <assert.h>
#include <stdbool.h>

#include "io/serial.h"

#include "msp/msp_io.h"

#include "rc/failsafe.h"
#include "rc/rc_data.h"
#include "rc/telemetry.h"

#include "util/time.h"

typedef enum
{
    OUTPUT_FLAG_LOCAL = 0,           // Connected by cable to the FC/servos
    OUTPUT_FLAG_REMOTE = 1 << 0,     // Sends data over the air
    OUTPUT_FLAG_SENDS_RSSI = 1 << 1, // Output sends RSSI to the FC on its own (e.g. FPort)
} output_flags_e;

typedef struct output_vtable_s
{
    bool (*open)(void *output, void *config);
    // update_control indicates if the output should send the control data again
    // Returns wheter the RC data update was sent to the FC
    bool (*update)(void *output, rc_data_t *data, bool update_control, time_micros_t now);
    void (*close)(void *output, void *config);
} output_vtable_t;

#define OUTPUT_TELEMETRY_UPDATE(output, id, v) ((output_t *)output)->telemetry_updated(output, id, v)
#define OUTPUT_TELEMETRY_CALCULATE(output, id) ((output_t *)output)->telemetry_calculate(output, id)

#define OUTPUT_TELEMETRY_UPDATE_U8(output, id, val)      \
    do                                                   \
    {                                                    \
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT8); \
        telemetry_val_t v = {                            \
            .u8 = val,                                   \
        };                                               \
        OUTPUT_TELEMETRY_UPDATE(output, id, &v);         \
    } while (0)

#define OUTPUT_TELEMETRY_UPDATE_I16(output, id, val)     \
    do                                                   \
    {                                                    \
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT16); \
        telemetry_val_t v = {                            \
            .i16 = val,                                  \
        };                                               \
        OUTPUT_TELEMETRY_UPDATE(output, id, &v);         \
    } while (0)

#define OUTPUT_TELEMETRY_UPDATE_U16(output, id, val)      \
    do                                                    \
    {                                                     \
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_UINT16); \
        telemetry_val_t v = {                             \
            .u16 = val,                                   \
        };                                                \
        OUTPUT_TELEMETRY_UPDATE(output, id, &v);          \
    } while (0)

#define OUTPUT_TELEMETRY_UPDATE_I32(output, id, val)     \
    do                                                   \
    {                                                    \
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_INT32); \
        telemetry_val_t v = {                            \
            .i32 = val,                                  \
        };                                               \
        OUTPUT_TELEMETRY_UPDATE(output, id, &v);         \
    } while (0)

#define OUTPUT_TELEMETRY_UPDATE_STRING(output, id, str, size) \
    do                                                        \
    {                                                         \
        TELEMETRY_ASSERT_TYPE(id, TELEMETRY_TYPE_STRING);     \
        telemetry_val_t v;                                    \
        ssize_t sz = size;                                    \
        if (sz < 0)                                           \
        {                                                     \
            sz = str ? strlen(str) : 0;                       \
        }                                                     \
        if (sz > 0)                                           \
        {                                                     \
            memcpy(v.s, str, sz);                             \
        }                                                     \
        v.s[sz] = '\0';                                       \
        OUTPUT_TELEMETRY_UPDATE(output, id, &v);              \
    } while (0)

#define OUTPUT_SET_MSP_TRANSPORT(output_impl, tr) msp_io_set_transport(&output_impl->output.msp, tr)
#define OUTPUT_MSP_CONN_GET(output) (&((output_t *)output)->msp.conn)
#define OUTPUT_MSP_SEND_REQ(output, req, callback) msp_conn_send(OUTPUT_MSP_CONN_GET(output), req, NULL, 0, callback, output)

#define OUTPUT_HAS_FLAG(output, flag) ((output->flags & flag) == flag)

// Used for polling MSP-capable endpoints for e.g. configuration settings
// which might affect other telemetry or stuff that's only available via
// MSP (like the craft name).
typedef struct output_msp_poll_s
{
    uint16_t cmd;
    time_micros_t interval;
    time_micros_t next_poll;
} output_msp_poll_t;

#define OUTPUT_FC_MAX_NUM_POLLS 10

typedef struct output_fc_s
{
    char fw_variant[4]; // 4-char code
    bool fw_version_is_pending;
    uint8_t fw_version[3];
    time_micros_t next_fw_update;
    struct
    {
        bool channel_auto;
        int8_t channel;
    } rssi;
    output_msp_poll_t polls[OUTPUT_FC_MAX_NUM_POLLS];
} output_fc_t;

typedef struct setting_s setting_t;

// Each output type should embed this struct
typedef struct output_s
{
    serial_port_t *serial_port;
    bool is_open;
    failsafe_t failsafe;
    rc_data_t *rc_data;
    // output driver should call this function when it wants to
    // update a telemetry value, usually via the OUTPUT_TELEMETRY_UPDATE_*
    // macros.
    telemetry_downlink_val_f telemetry_updated;
    // output driver should call this function when it wants some
    // telemetry value calculated from others. Only a few telemetry ids
    // are supported. See output.c for the available ones.
    telemetry_downlink_f telemetry_calculate;
    msp_io_t msp;
    output_fc_t fc;
    time_micros_t min_rc_update_interval;
    time_micros_t max_rc_update_interval;
    time_micros_t next_rc_update_no_earlier_than;
    time_micros_t next_rc_update_no_later_than;
    const setting_t *craft_name_setting;
    output_flags_e flags;
    output_vtable_t vtable;
} output_t;

bool output_open(rc_data_t *data, output_t *output, void *config);
// input_was_updated will be true iff the input returned new data
// during this cycle.
bool output_update(output_t *output, bool input_was_updated, time_micros_t now);
void output_close(output_t *output, void *config);
