#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config/config.h"

#include "io/gpio.h"

#include "target.h"

typedef uint16_t setting_key_t;

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
#if defined(USE_GPIO_REMAP) && defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
#define SETTING_RX_FOLDER_COUNT 14
#elif defined(USE_GPIO_REMAP)
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

// We leave 6 bits for the folder_id, so we can
// have up to 64 folders. Note that the upper
// CONFIG_MAX_PAIRED_RX folders are taken by
// the receivers.
#define _SK_FOLDER_BITS 6
#define _SK_FOLDER_MAX (1 << _SK_FOLDER_BITS)
#define _SK_FOLDER_SHIFT ((sizeof(setting_key_t) * 8) - _SK_FOLDER_BITS)
#define _SKE(folder, s) (((setting_key_t)folder << _SK_FOLDER_SHIFT) | s)
#define _SK_GET_FOLDER(sk) (sk >> _SK_FOLDER_SHIFT)
#define _SK_FOLDER(folder) _SKE(folder, 0)

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

#define SETTING_KEY_ROOT _SK_FOLDER(FOLDER_ID_ROOT)
#define SETTING_KEY_RC_MODE _SKE(FOLDER_ID_ROOT, 1)
#define SETTING_KEY_BIND _SKE(FOLDER_ID_ROOT, 2)
#define SETTING_KEY_AIR_BAND _SKE(FOLDER_ID_ROOT, 3)
#define SETTING_KEY_RF_POWER_TEST _SKE(FOLDER_ID_ROOT, 4)
#define SETTING_KEY_POWER_OFF _SKE(FOLDER_ID_ROOT, 5)

#define SETTING_KEY_TX _SK_FOLDER(FOLDER_ID_TX)
#define SETTING_KEY_TX_RF_POWER _SKE(FOLDER_ID_TX, 2)
#define SETTING_KEY_TX_INPUT _SKE(FOLDER_ID_TX, 3)
#define SETTING_KEY_TX_PILOT_NAME _SKE(FOLDER_ID_TX, 4)
#if defined(USE_GPIO_REMAP)
#define SETTING_KEY_TX_TX_GPIO _SKE(FOLDER_ID_TX, 5)
#define SETTING_KEY_TX_RX_GPIO _SKE(FOLDER_ID_TX, 6)
#endif

#define SETTING_KEY_RX _SK_FOLDER(FOLDER_ID_RX)
#define SETTING_KEY_RX_SUPPORTED_MODES _SKE(FOLDER_ID_RX, 1)
#define SETTING_KEY_RX_AUTO_CRAFT_NAME _SKE(FOLDER_ID_RX, 2)
#define SETTING_KEY_RX_CRAFT_NAME _SKE(FOLDER_ID_RX, 3)
#define SETTING_KEY_RX_OUTPUT _SKE(FOLDER_ID_RX, 4)
#if defined(USE_GPIO_REMAP)
#define SETTING_KEY_RX_TX_GPIO _SKE(FOLDER_ID_RX, 5)
#define SETTING_KEY_RX_RX_GPIO _SKE(FOLDER_ID_RX, 6)
#endif
#define SETTING_KEY_RX_RSSI_CHANNEL _SKE(FOLDER_ID_RX, 7)
#define SETTING_KEY_RX_SBUS_INVERTED _SKE(FOLDER_ID_RX, 8)
#define SETTING_KEY_RX_SPORT_INVERTED _SKE(FOLDER_ID_RX, 9)
#define SETTING_KEY_RX_MSP_BAUDRATE _SKE(FOLDER_ID_RX, 10)
#define SETTING_KEY_RX_FPORT_INVERTED _SKE(FOLDER_ID_RX, 11)

#define SETTING_KEY_RX_CHANNEL_OUTPUTS _SK_FOLDER(FOLDER_ID_RX_CHANNEL_OUTPUTS)

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
#define SETTING_KEY_RX_FS_MODE SETTING_KEY_RX_PREFIX "fs_mode"
#define SETTING_KEY_RX_FS_SET_CUSTOM SETTING_KEY_RX_PREFIX "fs_set_cust"
#endif

#if defined(USE_SCREEN)
#define SETTING_KEY_SCREEN _SK_FOLDER(FOLDER_ID_SCREEN)
#if !defined(SCREEN_FIXED_ORIENTATION)
#define SETTING_KEY_SCREEN_ORIENTATION _SKE(FOLDER_ID_SCREEN, 1)
#endif
#define SETTING_KEY_SCREEN_BRIGHTNESS _SKE(FOLDER_ID_SCREEN, 2)
#define SETTING_KEY_SCREEN_AUTO_OFF _SKE(FOLDER_ID_SCREEN, 3)
#endif

#define SETTING_KEY_RECEIVERS _SK_FOLDER(FOLDER_ID_RECEIVERS)
#define SETTING_KEY_RECEIVERS_RX_PREFIX 1
#define SETTING_KEY_RECEIVERS_RX_NAME_PREFIX 2
#define SETTING_KEY_RECEIVERS_RX_ADDR_PREFIX 3
#define SETTING_KEY_RECEIVERS_RX_SELECT_PREFIX 4
#define SETTING_KEY_RECEIVERS_RX_DELETE_PREFIX 5
#define SETTING_KEY_RXNUM_SHIFT 6
#if CONFIG_MAX_PAIRED_RX > (1 << SETTING_KEY_RXNUM_SHIFT)
#error CONFIG_MAX_PAIRED_RX > 64
#endif
#define SETTING_KEY_RECEIVER_ENCODE(prefix, rx_num) (prefix << SETTING_KEY_RXNUM_SHIFT | rx_num)
#if defined(USE_TX_SUPPORT)
#define SETTING_IS_FROM_RECEIVER_FOLDER(setting) (_SK_GET_FOLDER(setting->key) >= _SK_FOLDER_MAX - CONFIG_MAX_PAIRED_RX)
#define SETTING_HAS_RECEIVERS_PREFIX(setting, p) (SETTING_IS_FROM_RECEIVER_FOLDER(setting) && ((setting->key & 0xFF) >> SETTING_KEY_RXNUM_SHIFT) == p)
#else
#define SETTING_IS_FROM_RECEIVER_FOLDER(setting) false
#define SETTING_HAS_RECEIVERS_PREFIX(setting, p) false
#endif

#define SETTING_KEY_DEVICES _SK_FOLDER(FOLDER_ID_DEVICES)

#define SETTING_KEY_ABOUT _SK_FOLDER(FOLDER_ID_ABOUT)
#define SETTING_KEY_ABOUT_VERSION _SKE(FOLDER_ID_ABOUT, 1)
#define SETTING_KEY_ABOUT_BUILD_DATE _SKE(FOLDER_ID_ABOUT, 2)
#define SETTING_KEY_ABOUT_BOARD _SKE(FOLDER_ID_ABOUT, 3)
#define SETTING_KEY_ABOUT_ADDR _SKE(FOLDER_ID_ABOUT, 4)

#define SETTING_KEY_DIAGNOSTICS _SK_FOLDER(FOLDER_ID_DIAGNOSTICS)
#define SETTING_KEY_DIAGNOSTICS_FREQUENCIES _SKE(FOLDER_ID_DIAGNOSTICS, 1)
#define SETTING_KEY_DIAGNOSTICS_DEBUG_INFO _SKE(FOLDER_ID_DIAGNOSTICS, 2)

#define SETTING_KEY_DEVELOPER _SK_FOLDER(FOLDER_ID_DEVELOPER)
#define SETTING_KEY_DEVELOPER_REMOTE_DEBUGGING _SKE(FOLDER_ID_DEVELOPER, 1)
#define SETTING_KEY_DEVELOPER_REBOOT _SKE(FOLDER_ID_DEVELOPER, 2)

#define SETTING_IS(setting, k) (setting->key == k)
#define SETTING_IS_FROM_FOLDER(setting, d) (_SK_GET_FOLDER(setting->key) == d)

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
    const setting_key_t key;
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
const setting_t *settings_get_key(setting_key_t key);
uint8_t settings_get_key_u8(setting_key_t key);
hal_gpio_t settings_get_key_gpio(setting_key_t key);
bool settings_get_key_bool(setting_key_t key);
const char *settings_get_key_string(setting_key_t key);
const setting_t *settings_get_key_idx(setting_key_t key, int *idx);
const setting_t *settings_get_folder(folder_id_e folder);
bool settings_is_folder_visible(settings_view_e view_id, folder_id_e folder);
bool setting_is_visible(settings_view_e view_id, setting_key_t key);

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
