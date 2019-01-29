#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config/config.h"

#include "io/gpio.h"

#include "target.h"

#define SETTING_STRING_MAX_LENGTH 32
#define SETTING_STRING_BUFFER_SIZE (SETTING_STRING_MAX_LENGTH + 1)
#define SETTING_NAME_BUFFER_SIZE SETTING_STRING_BUFFER_SIZE
#define SETTING_STATIC_COUNT 15
#if defined(USE_TX_SUPPORT)
#if defined(USE_GPIO_REMAP)
#define SETTING_TX_FOLDER_COUNT 6
#else
#define SETTING_TX_FOLDER_COUNT 4
#endif
#define SETTING_TX_RECEIVERS_COUNT (1 + (5 * CONFIG_MAX_PAIRED_RX))
#else
#define SETTING_TX_RECEIVERS_COUNT 0
#define SETTING_TX_FOLDER_COUNT 0
#endif
#if defined(USE_RX_SUPPORT)
#if defined(USE_GPIO_REMAP)
#define SETTING_RX_FOLDER_COUNT 12
#else
#define SETTING_RX_FOLDER_COUNT 10
#endif
#else
#define SETTING_RX_FOLDER_COUNT 0
#endif
#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
// Use HAL_GPIO_USER_MAX to make sure we have a setting for every possible PWM output.
// +1 accounts for the folder to hold the output settings.
#define SETTING_PWM_COUNT (1 + HAL_GPIO_USER_MAX)
#else
#define SETTING_PWM_COUNT 0
#endif
#if defined(USE_SCREEN)
#if defined(SCREEN_FIXED_ORIENTATION)
#define SETTING_SCREEN_FOLDER_COUNT 3
#else
#define SETTING_SCREEN_FOLDER_COUNT 4
#endif
#else
#define SETTING_SCREEN_FOLDER_COUNT 0
#endif
#if defined(USE_DEVELOPER_MENU)
#define SETTING_DEVELOPER_FOLDER_COUNT 3
#else
#define SETTING_DEVELOPER_FOLDER_COUNT 0
#endif
#define SETTING_COUNT (SETTING_STATIC_COUNT + SETTING_TX_FOLDER_COUNT + SETTING_RX_FOLDER_COUNT + SETTING_TX_RECEIVERS_COUNT + SETTING_PWM_COUNT + SETTING_SCREEN_FOLDER_COUNT + SETTING_DEVELOPER_FOLDER_COUNT)

#define SETTING_KEY_RC_MODE "rc_mode"
#define SETTING_KEY_BIND "bind"
#define SETTING_KEY_AIR_BAND "air_band"
#define SETTING_KEY_RF_POWER_TEST "rf_power_test"

#define SETTING_KEY_TX "tx"
#define SETTING_KEY_TX_PREFIX SETTING_KEY_TX "."
#define SETTING_KEY_TX_RF_POWER SETTING_KEY_TX_PREFIX "rf_power"
#define SETTING_KEY_TX_INPUT SETTING_KEY_TX_PREFIX "input"
#define SETTING_KEY_TX_PILOT_NAME SETTING_KEY_TX_PREFIX "pilot_name"
#if defined(USE_GPIO_REMAP)
#define SETTING_KEY_TX_TX_GPIO SETTING_KEY_TX_PREFIX "tx"
#define SETTING_KEY_TX_RX_GPIO SETTING_KEY_TX_PREFIX "rx"
#endif

#define SETTING_KEY_RX "rx"
#define SETTING_KEY_RX_PREFIX SETTING_KEY_RX "."
#define SETTING_KEY_RX_SUPPORTED_MODES SETTING_KEY_RX_PREFIX "modes"
#define SETTING_KEY_RX_AUTO_CRAFT_NAME SETTING_KEY_RX_PREFIX "auto_c_name"
#define SETTING_KEY_RX_CRAFT_NAME SETTING_KEY_RX_PREFIX "craft_name"
#define SETTING_KEY_RX_OUTPUT SETTING_KEY_RX_PREFIX "output"
#if defined(USE_GPIO_REMAP)
#define SETTING_KEY_RX_TX_GPIO SETTING_KEY_RX_PREFIX "tx"
#define SETTING_KEY_RX_RX_GPIO SETTING_KEY_RX_PREFIX "rx"
#endif
#define SETTING_KEY_RX_RSSI_CHANNEL SETTING_KEY_RX_PREFIX "rssi_ch"
#define SETTING_KEY_RX_SBUS_INVERTED SETTING_KEY_RX_PREFIX "sbus_invert"
#define SETTING_KEY_RX_SPORT_INVERTED SETTING_KEY_RX_PREFIX "sport_invert"
#define SETTING_KEY_RX_MSP_BAUDRATE SETTING_KEY_RX_PREFIX "msp_baudrate"
#define SETTING_KEY_RX_FPORT_INVERTED SETTING_KEY_RX_PREFIX "fport_invert"

#define SETTING_KEY_RX_CHANNEL_OUTPUTS "rx-chs"
#define SETTING_KEY_RX_CHANNEL_OUTPUTS_PREFIX SETTING_KEY_RX_CHANNEL_OUTPUTS "."

#if defined(USE_SCREEN)
#define SETTING_KEY_SCREEN "scr" // Using screen here makes esp32 NVS return "key-too-long"
#define SETTING_KEY_SCREEN_PREFIX SETTING_KEY_SCREEN "."
#if !defined(SCREEN_FIXED_ORIENTATION)
#define SETTING_KEY_SCREEN_ORIENTATION SETTING_KEY_SCREEN_PREFIX "orientation"
#endif
#define SETTING_KEY_SCREEN_BRIGHTNESS SETTING_KEY_SCREEN_PREFIX "brightness"
#define SETTING_KEY_SCREEN_AUTO_OFF SETTING_KEY_SCREEN_PREFIX "auto_off"
#endif

#define SETTING_KEY_RECEIVERS "rxs"
#define SETTING_KEY_RECEIVERS_PREFIX SETTING_KEY_RECEIVERS "."
#define SETTING_KEY_RECEIVERS_RX_PREFIX SETTING_KEY_RECEIVERS_PREFIX "rx-"
#define SETTING_KEY_RECEIVERS_RX_NAME_PREFIX SETTING_KEY_RECEIVERS_PREFIX "rx-name-"
#define SETTING_KEY_RECEIVERS_RX_ADDR_PREFIX SETTING_KEY_RECEIVERS_PREFIX "rx-addr-"
#define SETTING_KEY_RECEIVERS_RX_SELECT_PREFIX SETTING_KEY_RECEIVERS_PREFIX "rx-sel-"
#define SETTING_KEY_RECEIVERS_RX_DELETE_PREFIX SETTING_KEY_RECEIVERS_PREFIX "rx-del-"

#define SETTING_KEY_DEVICES "devices"

#define SETTING_KEY_POWER_OFF "poweroff"

#define SETTING_KEY_ABOUT "abt"
#define SETTING_KEY_ABOUT_PREFIX SETTING_KEY_ABOUT "."
#define SETTING_KEY_ABOUT_VERSION SETTING_KEY_ABOUT_PREFIX "v"
#define SETTING_KEY_ABOUT_BUILD_DATE SETTING_KEY_ABOUT_PREFIX "bd"
#define SETTING_KEY_ABOUT_BOARD SETTING_KEY_ABOUT_PREFIX "brd"
#define SETTING_KEY_ABOUT_ADDR SETTING_KEY_ABOUT_PREFIX "addr"

#define SETTING_KEY_DIAGNOSTICS "diag"
#define SETTING_KEY_DIAGNOSTICS_PREFIX SETTING_KEY_DIAGNOSTICS "."
#define SETTING_KEY_DIAGNOSTICS_FREQUENCIES SETTING_KEY_DIAGNOSTICS_PREFIX "freqs"
#define SETTING_KEY_DIAGNOSTICS_DEBUG_INFO SETTING_KEY_DIAGNOSTICS_PREFIX "dbg-i"

#define SETTING_KEY_DEVELOPER "dev"
#define SETTING_KEY_DEVELOPER_PREFIX SETTING_KEY_DEVELOPER "."
#define SETTING_KEY_DEVELOPER_REMOTE_DEBUGGING SETTING_KEY_DEVELOPER_PREFIX "dbg"
#define SETTING_KEY_DEVELOPER_REBOOT SETTING_KEY_DEVELOPER_PREFIX "rbt"

#define SETTING_IS(setting, k) STR_EQUAL(setting->key, k)

typedef enum
{
    FOLDER_ID_ROOT = 1,
    FOLDER_ID_TX,
    FOLDER_ID_RX,
    FOLDER_ID_RX_CHANNEL_OUTPUTS,
    FOLDER_ID_SCREEN,
    FOLDER_ID_RECEIVERS,
    FOLDER_ID_DEVICES,
    FOLDER_ID_ABOUT,
    FOLDER_ID_DIAGNOSTICS,
    FOLDER_ID_DEVELOPER,
} folder_id_e;

typedef enum
{
    SETTING_CMD_FLAG_WARNING,
    SETTING_CMD_FLAG_CONFIRM,
} setting_cmd_flag_e;

typedef enum
{
    SETTING_CMD_STATUS_NONE = 0,
    SETTING_CMD_STATUS_CHANGE = 1,
    SETTING_CMD_STATUS_SHOW_WARNING = 2,
    SETTING_CMD_STATUS_ASK_CONFIRM = 3,
    SETTING_CMD_STATUS_COMMIT = 4,
    SETTING_CMD_STATUS_DISCARD = 5,
    SETTING_CMD_STATUS_PING = 0xFF,
} setting_cmd_status_e;

typedef enum
{
    SETTING_TYPE_U8 = 0,

    /* Unused, reserved
    SETTING_TYPE_I8,
    SETTING_TYPE_U16,
    SETTING_TYPE_I16,
    SETTING_TYPE_U32,
    SETTING_TYPE_I32,
    */
    SETTING_TYPE_STRING = 6,
    SETTING_TYPE_FOLDER = 7,
} setting_type_e;

typedef enum
{
    SETTING_FLAG_NAME_MAP = 1 << 0,
    SETTING_FLAG_EPHEMERAL = 1 << 1,
    SETTING_FLAG_READONLY = 1 << 2,
    SETTING_FLAG_CMD = 1 << 3,
    SETTING_FLAG_DYNAMIC = 1 << 4,
} setting_flag_e;

typedef union {
    uint8_t u8;
// Unused for now, let's save some memory
#if 0
    int8_t i8;
    uint16_t u16;
    int16_t i16;
    uint32_t u32;
    int32_t i32;
#endif
} setting_value_t;

typedef struct setting_s
{
    const char *key;
    const char *name;
    const setting_type_e type;
    const setting_flag_e flags;
    const char **val_names;
    const char *unit;
    const folder_id_e folder;
    const setting_value_t min;
    const setting_value_t max;
    const setting_value_t def_val;
    const void *data;
} setting_t;

typedef enum
{
    SETTINGS_VIEW_CRSF_INPUT,
    SETTINGS_VIEW_MENU,   // On-device menu
    SETTINGS_VIEW_REMOTE, // Remote settings (other device)
} settings_view_e;

typedef struct settings_view_s
{
    uint8_t indexes[SETTING_COUNT];
    int count;
} settings_view_t;

void settings_init(void);

typedef void (*setting_changed_f)(const setting_t *setting, void *user_data);

void settings_add_listener(setting_changed_f callback, void *user_data);
void settings_remove_listener(setting_changed_f callback, void *user_data);

int settings_get_count(void);

const setting_t *settings_get_setting_at(int idx);
const setting_t *settings_get_key(const char *key);
uint8_t settings_get_key_u8(const char *key);
hal_gpio_t settings_get_key_gpio(const char *key);
bool settings_get_key_bool(const char *key);
const char *settings_get_key_string(const char *key);
const setting_t *settings_get_key_idx(const char *key, int *idx);
const setting_t *settings_get_folder(folder_id_e folder);
bool settings_is_folder_visible(settings_view_e view_id, folder_id_e folder);
bool setting_is_visible(settings_view_e view_id, const char *key);

folder_id_e setting_get_folder_id(const setting_t *setting);
folder_id_e setting_get_parent_folder_id(const setting_t *setting);
int32_t setting_get_min(const setting_t *setting);
int32_t setting_get_max(const setting_t *setting);
int32_t setting_get_default(const setting_t *setting);
uint8_t setting_get_u8(const setting_t *setting);
void setting_set_u8(const setting_t *setting, uint8_t v);
hal_gpio_t setting_get_gpio(const setting_t *setting);
bool setting_get_bool(const setting_t *setting);
void setting_set_bool(const setting_t *setting, bool v);
const char *setting_get_string(const setting_t *setting);
void setting_set_string(const setting_t *setting, const char *s);

const char *setting_map_name(const setting_t *setting, uint8_t val);
void setting_format_name(char *buf, size_t size, const setting_t *setting);
void setting_format(char *buf, size_t size, const setting_t *setting);
void setting_format_value(char *buf, size_t size, const setting_t *setting);

// These functions are only valid for settings with the SETTING_FLAG_CMD flag
setting_cmd_flag_e setting_cmd_get_flags(const setting_t *setting);
bool setting_cmd_exec(const setting_t *setting);

int setting_rx_channel_output_get_pos(const setting_t *setting);
int setting_receiver_get_rx_num(const setting_t *setting);

void setting_increment(const setting_t *setting);
void setting_decrement(const setting_t *setting);

bool settings_view_get(settings_view_t *view, settings_view_e view_id, folder_id_e folder);
bool settings_view_get_folder_view(settings_view_t *view, settings_view_e view_id, folder_id_e folder, bool recursive);
const setting_t *settings_view_get_setting_at(settings_view_t *view, int idx);
int settings_view_get_parent_index(settings_view_t *view, const setting_t *setting);
