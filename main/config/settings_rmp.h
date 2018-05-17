#pragma once

#include <stddef.h>
#include <stdint.h>

#include "air/air.h"

#include "config/settings.h"

#include "util/macros.h"

// TODO: Make this smaller, we don't prepend 32 characters to the device name
#define SETTINGS_RMP_DEVICE_NAME_LENGTH (AIR_MAX_NAME_LENGTH * 2)

typedef struct rmp_s rmp_t;
typedef struct rmp_msg_s rmp_msg_t;

typedef enum
{
    SETTINGS_RMP_HELO = 0,
    SETTINGS_RMP_EHLO,
    SETTINGS_RMP_READ_REQ,
    SETTINGS_RMP_READ,
    SETTINGS_RMP_WRITE_REQ,
    SETTINGS_RMP_WRITE,
} settings_rmp_code_e;

typedef struct settings_rmp_view_s
{
    uint8_t id;
    uint16_t folder_id;
    uint8_t recursive;
} PACKED settings_rmp_view_t;

typedef struct settings_rmp_helo_s
{
    settings_rmp_view_t view;
} PACKED settings_rmp_helo_t;

typedef struct settings_rmp_ehlo_s
{
    settings_rmp_view_t view;
    uint8_t is_visible; // Wether the requested root folder is visible
    uint16_t settings_count;
    char name[SETTINGS_RMP_DEVICE_NAME_LENGTH + 1];
} PACKED settings_rmp_ehlo_t;

typedef struct settings_rmp_read_req_s
{
    settings_rmp_view_t view;
    uint16_t setting_index; // zero indexed
} PACKED settings_rmp_read_req_t;

// XXX: 104 is the biggest payload we need to send the PWM
// output names for 16 channels.
#define SETTING_RMP_SETTING_MAX_PAYLOAD_SIZE 104

typedef struct settings_rmp_write_req_s
{
    settings_rmp_view_t view;
    uint16_t setting_index; // zero indexed

    // Value depending on type. String must be null terminated.
    uint8_t payload[SETTING_RMP_SETTING_MAX_PAYLOAD_SIZE];
} PACKED settings_rmp_write_req_t;

typedef struct settings_rmp_setting_s
{
    settings_rmp_view_t view;
    uint16_t setting_index; // zero indexed
    int32_t parent_index;   // -1 if no parent
    uint8_t type;           // from setting_type_e
    uint8_t flags;          // from setting_flag_e
    // Value, min and max depending on type follow (little endian) (numeric)
    // OR folder_id (u16-le) for folders
    // OR Null terminated value for strings followed by max length (u8)

    // Then, the null terminated name

    // If the setting is a map, the names follow separated by null and ended by two consecutive nulls
    uint8_t payload[SETTING_RMP_SETTING_MAX_PAYLOAD_SIZE];
} PACKED settings_rmp_setting_t;

typedef struct settings_rmp_msg_s
{
    uint8_t code; // from settings_rmp_code_e
    union {
        settings_rmp_helo_t helo;
        settings_rmp_ehlo_t ehlo;
        settings_rmp_read_req_t read_req;
        settings_rmp_write_req_t write_req;
        settings_rmp_setting_t setting;
    };
} PACKED settings_rmp_msg_t;

void settings_rmp_init(rmp_t *rmp);
bool settings_rmp_msg_is_valid(const rmp_msg_t *msg);
size_t settings_rmp_msg_size(const settings_rmp_msg_t *msg);

int32_t settings_rmp_setting_get_value(const settings_rmp_setting_t *s);
bool settings_rmp_setting_set_value(settings_rmp_setting_t *s, int32_t val);
int32_t settings_rmp_setting_get_min(const settings_rmp_setting_t *s);
int32_t settings_rmp_setting_get_max(const settings_rmp_setting_t *s);
const char *settings_rmp_setting_get_str_value(const settings_rmp_setting_t *s);
bool settings_rmp_setting_set_str_value(settings_rmp_setting_t *s, const char *val);
unsigned settings_rmp_setting_get_str_max_length(const settings_rmp_setting_t *s);
const char *settings_rmp_setting_get_name(const settings_rmp_setting_t *s);
unsigned settings_rmp_setting_get_mapped_names_count(const settings_rmp_setting_t *s);
const char *settings_rmp_setting_get_mapped_name(const settings_rmp_setting_t *s, unsigned idx);

setting_cmd_flag_e settings_rmp_setting_cmd_get_flags(const settings_rmp_setting_t *s);
bool settings_rmp_setting_cmd_set_exec(settings_rmp_setting_t *s);

bool settings_rmp_setting_increment(settings_rmp_setting_t *s);
bool settings_rmp_setting_decrement(settings_rmp_setting_t *s);
void settings_rmp_setting_prepare_write(const settings_rmp_setting_t *s, settings_rmp_msg_t *msg);