#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hal/log.h>

#include "air/air_rf_power.h"

#include "config/config.h"

#include "io/pwm.h"

#include "msp/msp_serial.h"

#include "platform/storage.h"
#include "platform/system.h"

#include "ui/screen.h"
#include "ui/ui.h"

#include "util/macros.h"
#include "util/version.h"

#include "settings.h"

static const char *TAG = "Settings";

#define MAX_SETTING_KEY_LENGTH 15

// clang-format off
#define NO_VALUE {0}
#define BOOL(v) {.u8 = (v ? 1 : 0)}
#define U8(v) {.u8 = v}
#define U16(v) {.u16 = v}
// clang-format on

static char settings_string_storage[2][SETTING_STRING_BUFFER_SIZE];

#define SETTING_SHOW_IF(c) ((c) ? SETTING_VISIBILITY_SHOW : SETTING_VISIBILITY_HIDE)
#define SETTING_SHOW_IF_SCREEN(view_id) SETTING_SHOW_IF(view_id == SETTINGS_VIEW_MENU && system_has_flag(SYSTEM_FLAG_SCREEN))

static const char *off_on_table[] = {"Off", "On"};
#if defined(USE_RX_SUPPORT)
static const char *no_yes_table[] = {"No", "Yes"};
#endif
static char gpio_name_storage[HAL_GPIO_USER_COUNT][HAL_GPIO_NAME_LENGTH];
static const char *gpio_names[HAL_GPIO_USER_COUNT];

#define FOLDER(k, n, id, p, fn) \
    (setting_t) { .key = k, .name = n, .type = SETTING_TYPE_FOLDER, .flags = SETTING_FLAG_READONLY | SETTING_FLAG_EPHEMERAL, .folder = p, .def_val = U8(id), .data = fn }
#define STRING_SETTING(k, n, p) \
    (setting_t) { .key = k, .name = n, .type = SETTING_TYPE_STRING, .folder = p }
#define FLAGS_STRING_SETTING(k, n, f, p, d) \
    (setting_t) { .key = k, .name = n, .type = SETTING_TYPE_STRING, .flags = f, .folder = p, .data = d }
#define RO_STRING_SETTING(k, n, p, v) \
    (setting_t) { .key = k, .name = n, .type = SETTING_TYPE_STRING, .flags = SETTING_FLAG_READONLY, .folder = p, .data = v }
#define U8_SETTING(k, n, f, p, mi, ma, def) \
    (setting_t) { .key = k, .name = n, .type = SETTING_TYPE_U8, .flags = f, .folder = p, .min = U8(mi), .max = U8(ma), .def_val = U8(def) }
#define U8_MAP_SETTING_UNIT(k, n, f, p, m, u, def) \
    (setting_t) { .key = k, .name = n, .type = SETTING_TYPE_U8, .flags = f | SETTING_FLAG_NAME_MAP, .val_names = m, .unit = u, .folder = p, .min = U8(0), .max = U8(ARRAY_COUNT(m) - 1), .def_val = U8(def) }
#define U8_MAP_SETTING(k, n, f, p, m, def) U8_MAP_SETTING_UNIT(k, n, f, p, m, NULL, def)
#define BOOL_SETTING(k, n, f, p, def) U8_MAP_SETTING(k, n, f, p, off_on_table, def ? 1 : 0)
#define BOOL_YN_SETTING(k, n, f, p, def) U8_MAP_SETTING(k, n, f, p, no_yes_table, def ? 1 : 0)

#define CMD_SETTING(k, n, p, f, c_fl) U8_SETTING(k, n, f | SETTING_FLAG_EPHEMERAL | SETTING_FLAG_CMD, p, 0, 0, c_fl)

#define GPIO_USER_SETTING(k, n, p, def) U8_MAP_SETTING(k, n, 0, p, gpio_names, def)

#define RX_CHANNEL_OUTPUT_GPIO_USER_SETTING_KEY(p) (SETTING_KEY_RX_CHANNEL_OUTPUTS_PREFIX #p)
#define RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(p) \
    (setting_t) { .key = RX_CHANNEL_OUTPUT_GPIO_USER_SETTING_KEY(p), .name = NULL, .type = SETTING_TYPE_U8, .flags = SETTING_FLAG_NAME_MAP | SETTING_FLAG_DYNAMIC, .val_names = pwm_channel_names, .unit = NULL, .folder = FOLDER_ID_RX_CHANNEL_OUTPUTS, .min = U8(0), .max = U8(PWM_CHANNEL_COUNT - 1), .def_val = U8(0), .data = setting_format_rx_channel_output }

#define RX_FOLDER_ID(n) (0xFF - n)

#define RX_KEY(k, n) (k #n)
#define RX_STRING_SETTING(k, n, p, f) FLAGS_STRING_SETTING(k, n, SETTING_FLAG_READONLY | SETTING_FLAG_DYNAMIC, p, f)
#define RX_CMD(k, n, p) CMD_SETTING(k, n, p, 0, SETTING_CMD_FLAG_CONFIRM)

#define RX_ENSURE_COUNT(n) _Static_assert(CONFIG_MAX_PAIRED_RX == n, "invalid CONFIG_MAX_PAIRED_RX vs settings")

#define RX_FOLDER(n)                                                                                                            \
    FOLDER(RX_KEY(SETTING_KEY_RECEIVERS_RX_PREFIX, n), "Receiver #" #n, RX_FOLDER_ID(n), FOLDER_ID_RECEIVERS, NULL),            \
        RX_STRING_SETTING(RX_KEY(SETTING_KEY_RECEIVERS_RX_NAME_PREFIX, n), "Name", RX_FOLDER_ID(n), setting_format_rx_name),    \
        RX_STRING_SETTING(RX_KEY(SETTING_KEY_RECEIVERS_RX_ADDR_PREFIX, n), "Address", RX_FOLDER_ID(n), setting_format_rx_addr), \
        RX_CMD(RX_KEY(SETTING_KEY_RECEIVERS_RX_SELECT_PREFIX, n), "Select", RX_FOLDER_ID(n)),                                   \
        RX_CMD(RX_KEY(SETTING_KEY_RECEIVERS_RX_DELETE_PREFIX, n), "Delete", RX_FOLDER_ID(n))

#define SETTINGS_STORAGE_KEY "settings"

typedef enum
{
    SETTING_VISIBILITY_SHOW,
    SETTING_VISIBILITY_HIDE,
    SETTING_VISIBILITY_MOVE_CONTENTS_TO_PARENT,
} setting_visibility_e;

typedef enum
{
    SETTING_DYNAMIC_FORMAT_NAME,
    SETTING_DYNAMIC_FORMAT_VALUE,
} setting_dynamic_format_e;

typedef setting_visibility_e (*setting_visibility_f)(folder_id_e folder, settings_view_e view_id, const setting_t *setting);
typedef int (*setting_dynamic_format_f)(char *buf, size_t size, const setting_t *setting, setting_dynamic_format_e fmt);

static int setting_format_own_addr(char *buf, size_t size, const setting_t *setting, setting_dynamic_format_e fmt)
{
    if (fmt == SETTING_DYNAMIC_FORMAT_VALUE)
    {
        air_addr_t addr = config_get_addr();
        air_addr_format(&addr, buf, size);
        return strlen(buf) + 1;
    }
    return 0;
}

static setting_visibility_e setting_visibility_root(folder_id_e folder, settings_view_e view_id, const setting_t *setting)
{
    if (SETTING_IS(setting, SETTING_KEY_RC_MODE))
    {
#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
        return SETTING_SHOW_IF(view_id != SETTINGS_VIEW_CRSF_INPUT);
#else
        return SETTING_VISIBILITY_HIDE;
#endif
    }
    if (SETTING_IS(setting, SETTING_KEY_AIR_BAND))
    {
        // Air band is only selectable in the TX, the RX
        // switches to whatever air band the TX is using.
        return SETTING_SHOW_IF(config_get_rc_mode() == RC_MODE_TX);
    }
    if (SETTING_IS(setting, SETTING_KEY_TX))
    {
        return SETTING_SHOW_IF(config_get_rc_mode() == RC_MODE_TX);
    }
    if (SETTING_IS(setting, SETTING_KEY_RX))
    {
        return SETTING_SHOW_IF(config_get_rc_mode() == RC_MODE_RX);
    }
#if defined(USE_SCREEN)
    if (SETTING_IS(setting, SETTING_KEY_SCREEN))
    {
        return SETTING_SHOW_IF_SCREEN(view_id);
    }
#endif
    if (SETTING_IS(setting, SETTING_KEY_RECEIVERS))
    {
        return SETTING_SHOW_IF(config_get_rc_mode() == RC_MODE_TX);
    }
    if (SETTING_IS(setting, SETTING_KEY_DEVICES))
    {
        return SETTING_SHOW_IF(view_id == SETTINGS_VIEW_MENU && config_get_rc_mode() == RC_MODE_TX);
    }
    if (SETTING_IS(setting, SETTING_KEY_POWER_OFF))
    {
        return SETTING_SHOW_IF(config_get_rc_mode() == RC_MODE_RX);
    }
    if (SETTING_IS(setting, SETTING_KEY_DIAGNOSTICS))
    {
        return SETTING_SHOW_IF_SCREEN(view_id);
    }
    return SETTING_VISIBILITY_SHOW;
}

#if defined(USE_TX_SUPPORT)
static setting_visibility_e setting_visibility_tx(folder_id_e folder, settings_view_e view_id, const setting_t *setting)
{
    if (SETTING_IS(setting, SETTING_KEY_TX_INPUT))
    {
        return SETTING_SHOW_IF(view_id != SETTINGS_VIEW_CRSF_INPUT);
    }
    if (SETTING_IS(setting, SETTING_KEY_TX_TX_GPIO))
    {
        // Don't allow changing the TX pin from the CRSF/IBUS configuration scripts
        // since it will break communication.
        return SETTING_SHOW_IF(config_get_input_type() != TX_INPUT_FAKE && view_id != SETTINGS_VIEW_CRSF_INPUT);
    }
    if (SETTING_IS(setting, SETTING_KEY_TX_RX_GPIO))
    {
        return SETTING_VISIBILITY_HIDE;
    }
    return SETTING_VISIBILITY_SHOW;
}
#endif

#if defined(USE_RX_SUPPORT)
static setting_visibility_e setting_visibility_rx(folder_id_e folder, settings_view_e view_id, const setting_t *setting)
{
    if (SETTING_IS(setting, SETTING_KEY_RX_TX_GPIO))
    {
        return SETTING_SHOW_IF(config_get_output_type() != RX_OUTPUT_NONE);
    }

    if (SETTING_IS(setting, SETTING_KEY_RX_RX_GPIO))
    {
        return SETTING_SHOW_IF(config_get_output_type() != RX_OUTPUT_NONE);
    }

    if (SETTING_IS(setting, SETTING_KEY_RX_RSSI_CHANNEL))
    {
        return SETTING_SHOW_IF(config_get_output_type() != RX_OUTPUT_NONE);
    }

    if (SETTING_IS(setting, SETTING_KEY_RX_SBUS_INVERTED) || SETTING_IS(setting, SETTING_KEY_RX_SPORT_INVERTED))
    {
        return SETTING_SHOW_IF(config_get_output_type() == RX_OUTPUT_SBUS_SPORT);
    }

    if (SETTING_IS(setting, SETTING_KEY_RX_MSP_BAUDRATE))
    {
        return SETTING_SHOW_IF(config_get_output_type() == RX_OUTPUT_MSP);
    }

    if (SETTING_IS(setting, SETTING_KEY_RX_FPORT_INVERTED))
    {
        return SETTING_SHOW_IF(config_get_output_type() == RX_OUTPUT_FPORT);
    }

    return SETTING_VISIBILITY_SHOW;
}
#endif

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
static setting_visibility_e setting_visibility_rx_channel_outputs(folder_id_e folder, settings_view_e view_id, const setting_t *setting)
{
    int index = setting_rx_channel_output_get_pos(setting);
    if (index >= 0)
    {
        int gpio = hal_gpio_user_at(index);
        if (gpio != HAL_GPIO_NONE && pwm_output_can_use_gpio(gpio))
        {
            return SETTING_VISIBILITY_SHOW;
        }
    }
    return SETTING_VISIBILITY_HIDE;
}

static int setting_format_rx_channel_output(char *buf, size_t size, const setting_t *setting, setting_dynamic_format_e fmt)
{
    int index = setting_rx_channel_output_get_pos(setting);
    char gpio_name[HAL_GPIO_NAME_LENGTH];
    if (index >= 0)
    {
        int gpio = hal_gpio_user_at(index);
        switch (fmt)
        {
        case SETTING_DYNAMIC_FORMAT_NAME:
            hal_gpio_toa(gpio, gpio_name, sizeof(gpio_name));
            return snprintf(buf, size, "Pin %s", gpio_name);
        case SETTING_DYNAMIC_FORMAT_VALUE:
            break;
        }
    }
    return 0;
}

#endif

#if defined(USE_TX_SUPPORT)
static setting_visibility_e setting_visibility_receivers(folder_id_e folder, settings_view_e view_id, const setting_t *setting)
{
    int rx_num = setting_receiver_get_rx_num(setting);
    return SETTING_SHOW_IF(config_get_paired_rx_at(NULL, rx_num));
}

static int setting_format_rx_name(char *buf, size_t size, const setting_t *setting, setting_dynamic_format_e fmt)
{
    if (fmt == SETTING_DYNAMIC_FORMAT_VALUE)
    {
        int rx_num = setting_receiver_get_rx_num(setting);
        air_pairing_t pairing;
        if (!config_get_paired_rx_at(&pairing, rx_num) || !config_get_air_name(buf, size, &pairing.addr))
        {
            strncpy(buf, "None", size);
        }
        return strlen(buf) + 1;
    }
    return 0;
}

static int setting_format_rx_addr(char *buf, size_t size, const setting_t *setting, setting_dynamic_format_e fmt)
{
    if (fmt == SETTING_DYNAMIC_FORMAT_VALUE)
    {
        int rx_num = setting_receiver_get_rx_num(setting);
        air_pairing_t pairing;
        if (config_get_paired_rx_at(&pairing, rx_num))
        {
            air_addr_format(&pairing.addr, buf, size);
        }
        else
        {
            strncpy(buf, "None", size);
        }
        return strlen(buf) + 1;
    }
    return 0;
}
#endif

#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
static const char *mode_table[] = {"TX", "RX"};
#endif

// Keep in sync with config_air_band_e
static const char *air_band_table[] = {
#if defined(USE_AIR_BAND_147)
    "147MHz",
#endif
#if defined(USE_AIR_BAND_169)
    "169MHz",
#endif
#if defined(USE_AIR_BAND_315)
    "315MHz",
#endif
#if defined(USE_AIR_BAND_433)
    "433MHz",
#endif
#if defined(USE_AIR_BAND_470)
    "470MHz",
#endif
#if defined(USE_AIR_BAND_868)
    "868MHz",
#endif
#if defined(USE_AIR_BAND_915)
    "915MHz",
#endif
};
_Static_assert(ARRAY_COUNT(air_band_table) == CONFIG_AIR_BAND_COUNT, "Invalid air band names table");
#if defined(USE_TX_SUPPORT)
static const char *tx_input_table[] = {"CRSF", "PPM", "IBUS", "Test"};
#endif
static const char *air_rf_power_table[] = {"Auto", "1mw", "10mw", "25mw", "50mw", "100mw"};
_Static_assert(ARRAY_COUNT(air_rf_power_table) == AIR_RF_POWER_LAST - AIR_RF_POWER_FIRST + 1, "air_rf_power_table invalid");
// Keep in sync with config_air_mode_e
static const char *config_air_modes_table[] = {
    "1-5 (9-150Hz)",
    "2-5 (9-50Hz)",
    "1 (150Hz)",
    "2 (50Hz)",
    "3 (30Hz)",
    "4 (15Hz)",
    "5 (9Hz)",
};
_Static_assert(ARRAY_COUNT(config_air_modes_table) == CONFIG_AIR_MODES_COUNT, "CONFIG_AIR_MODES_COUNT is invalid");
#if defined(USE_RX_SUPPORT)
static const char *rx_output_table[] = {"MSP", "CRSF", "FPort", "SBUS/Smartport", "Channels"};
static const char *msp_baudrate_table[] = {"115200"};
static const char *rssi_channel_table[] = {
    "Auto",
    "None",
    "CH 1",
    "CH 2",
    "CH 3",
    "CH 4",
    "CH 5",
    "CH 6",
    "CH 7",
    "CH 8",
    "CH 9",
    "CH 10",
    "CH 11",
    "CH 12",
#if RC_CHANNELS_NUM > 12
    "CH 13",
    "CH 14",
    "CH 15",
    "CH 16",
#if RC_CHANNELS_NUM > 16
    "CH 17",
    "CH 18",
#if RC_CHANNELS_NUM > 18
    "CH 19",
    "CH 20",
#endif
#endif
#endif
};
#endif
#if defined(USE_SCREEN)
#if !defined(SCREEN_FIXED_ORIENTATION)
static const char *screen_orientation_table[] = {"Horizontal", "Horizontal (buttons at the right)", "Vertical", "Vertical (buttons on top)"};
#endif
static const char *screen_brightness_table[] = {"Low", "Medium", "High"};
static const char *screen_autopoweroff_table[] = {"Disabled", "30 sec", "1 min", "5 min", "10 min"};
#endif

static const char *view_crsf_input_tx_settings[] = {
    "",
    SETTING_KEY_BIND,
    SETTING_KEY_TX,
    SETTING_KEY_TX_RF_POWER,
    SETTING_KEY_TX_PILOT_NAME,
    SETTING_KEY_ABOUT,
    SETTING_KEY_ABOUT_VERSION,
    SETTING_KEY_ABOUT_BUILD_DATE,
    SETTING_KEY_ABOUT_ADDR,
};

static setting_value_t setting_values[SETTING_COUNT];

#define TX_DEFAULT_GPIO_IDX HAL_GPIO_USER_GET_IDX(TX_DEFAULT_GPIO)
#define RX_DEFAULT_GPIO_IDX HAL_GPIO_USER_GET_IDX(RX_DEFAULT_GPIO)

static const setting_t settings[] = {
    FOLDER("", "Settings", FOLDER_ID_ROOT, 0, setting_visibility_root),
#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
    U8_MAP_SETTING(SETTING_KEY_RC_MODE, "RC Mode", 0, FOLDER_ID_ROOT, mode_table, RC_MODE_TX),
#elif defined(USE_TX_SUPPORT)
    U8_SETTING(SETTING_KEY_RC_MODE, NULL, SETTING_FLAG_READONLY, FOLDER_ID_ROOT, RC_MODE_TX, RC_MODE_TX, RC_MODE_TX),
#elif defined(USE_RX_SUPPORT)
    U8_SETTING(SETTING_KEY_RC_MODE, NULL, SETTING_FLAG_READONLY, FOLDER_ID_ROOT, RC_MODE_TX, RC_MODE_TX, RC_MODE_TX),
#endif
    BOOL_SETTING(SETTING_KEY_BIND, "Bind", SETTING_FLAG_EPHEMERAL, FOLDER_ID_ROOT, false),
    U8_MAP_SETTING(SETTING_KEY_AIR_BAND, "Band", 0, FOLDER_ID_ROOT, air_band_table, CONFIG_AIR_BAND_DEFAULT),

#if defined(USE_TX_SUPPORT)
    FOLDER(SETTING_KEY_TX, "TX", FOLDER_ID_TX, FOLDER_ID_ROOT, setting_visibility_tx),
    U8_MAP_SETTING(SETTING_KEY_TX_RF_POWER, "Power", 0, FOLDER_ID_TX, air_rf_power_table, AIR_RF_POWER_DEFAULT),
    STRING_SETTING(SETTING_KEY_TX_PILOT_NAME, "Pilot Name", FOLDER_ID_TX),
    U8_MAP_SETTING(SETTING_KEY_TX_INPUT, "Input", 0, FOLDER_ID_TX, tx_input_table, TX_INPUT_FIRST),
    GPIO_USER_SETTING(SETTING_KEY_TX_TX_GPIO, "TX Pin", FOLDER_ID_TX, TX_DEFAULT_GPIO_IDX),
    GPIO_USER_SETTING(SETTING_KEY_TX_RX_GPIO, "RX Pin", FOLDER_ID_TX, RX_DEFAULT_GPIO_IDX),
#endif

#if defined(USE_RX_SUPPORT)
    FOLDER(SETTING_KEY_RX, "RX", FOLDER_ID_RX, FOLDER_ID_ROOT, setting_visibility_rx),
    U8_MAP_SETTING(SETTING_KEY_RX_SUPPORTED_MODES, "Modes", 0, FOLDER_ID_RX, config_air_modes_table, CONFIG_AIR_MODES_2_5),
    BOOL_YN_SETTING(SETTING_KEY_RX_AUTO_CRAFT_NAME, "Auto Craft Name", 0, FOLDER_ID_RX, true),
    STRING_SETTING(SETTING_KEY_RX_CRAFT_NAME, "Craft Name", FOLDER_ID_RX),
    U8_MAP_SETTING(SETTING_KEY_RX_OUTPUT, "Output", 0, FOLDER_ID_RX, rx_output_table, RX_OUTPUT_MSP),
    GPIO_USER_SETTING(SETTING_KEY_RX_TX_GPIO, "TX Pin", FOLDER_ID_RX, TX_DEFAULT_GPIO_IDX),
    GPIO_USER_SETTING(SETTING_KEY_RX_RX_GPIO, "RX Pin", FOLDER_ID_RX, RX_DEFAULT_GPIO_IDX),
    U8_MAP_SETTING(SETTING_KEY_RX_RSSI_CHANNEL, "RSSI Channel", 0, FOLDER_ID_RX, rssi_channel_table, RX_RSSI_CHANNEL_AUTO),
    BOOL_YN_SETTING(SETTING_KEY_RX_SBUS_INVERTED, "SBUS Inverted", 0, FOLDER_ID_RX, true),
    BOOL_YN_SETTING(SETTING_KEY_RX_SPORT_INVERTED, "S.Port Inverted", 0, FOLDER_ID_RX, true),
    U8_MAP_SETTING(SETTING_KEY_RX_MSP_BAUDRATE, "MSP Baudrate", 0, FOLDER_ID_RX, msp_baudrate_table, MSP_SERIAL_BAUDRATE_FIRST),
    BOOL_YN_SETTING(SETTING_KEY_RX_FPORT_INVERTED, "FPort Inverted", 0, FOLDER_ID_RX, false),
#endif

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
    FOLDER(SETTING_KEY_RX_CHANNEL_OUTPUTS, "Channel Outputs", FOLDER_ID_RX_CHANNEL_OUTPUTS, FOLDER_ID_RX, setting_visibility_rx_channel_outputs),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(0),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(1),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(2),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(3),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(4),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(5),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(6),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(7),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(8),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(9),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(10),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(11),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(12),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(13),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(14),
    RX_CHANNEL_OUTPUT_GPIO_USER_SETTING(15),
#endif

#if defined(USE_SCREEN)
    FOLDER(SETTING_KEY_SCREEN, "Screen", FOLDER_ID_SCREEN, FOLDER_ID_ROOT, NULL),
#if !defined(SCREEN_FIXED_ORIENTATION)
    U8_MAP_SETTING(SETTING_KEY_SCREEN_ORIENTATION, "Orientation", 0, FOLDER_ID_SCREEN, screen_orientation_table, SCREEN_ORIENTATION_DEFAULT),
#endif
    U8_MAP_SETTING(SETTING_KEY_SCREEN_BRIGHTNESS, "Brightness", 0, FOLDER_ID_SCREEN, screen_brightness_table, SCREEN_BRIGHTNESS_DEFAULT),
    U8_MAP_SETTING(SETTING_KEY_SCREEN_AUTO_OFF, "Auto Off", 0, FOLDER_ID_SCREEN, screen_autopoweroff_table, UI_SCREEN_AUTOOFF_DEFAULT),
#endif

#if defined(USE_TX_SUPPORT)
    FOLDER(SETTING_KEY_RECEIVERS, "Receivers", FOLDER_ID_RECEIVERS, FOLDER_ID_ROOT, setting_visibility_receivers),

    RX_FOLDER(0),
    RX_FOLDER(1),
    RX_FOLDER(2),
    RX_FOLDER(3),
#if CONFIG_MAX_PAIRED_RX > 4
    RX_FOLDER(4),
    RX_FOLDER(5),
    RX_FOLDER(6),
    RX_FOLDER(7),
    RX_FOLDER(8),
    RX_FOLDER(9),
    RX_FOLDER(10),
    RX_FOLDER(11),
    RX_FOLDER(12),
    RX_FOLDER(13),
    RX_FOLDER(14),
    RX_FOLDER(15),
#if CONFIG_MAX_PAIRED_RX > 16
    RX_FOLDER(16),
    RX_FOLDER(17),
    RX_FOLDER(18),
    RX_FOLDER(19),
    RX_FOLDER(20),
    RX_FOLDER(21),
    RX_FOLDER(22),
    RX_FOLDER(23),
    RX_FOLDER(24),
    RX_FOLDER(25),
    RX_FOLDER(26),
    RX_FOLDER(27),
    RX_FOLDER(28),
    RX_FOLDER(29),
    RX_FOLDER(30),
    RX_FOLDER(31),
#endif
#endif
#endif

    FOLDER(SETTING_KEY_DEVICES, "Other Devices", FOLDER_ID_DEVICES, FOLDER_ID_ROOT, NULL),

    CMD_SETTING(SETTING_KEY_POWER_OFF, "Power Off", FOLDER_ID_ROOT, 0, SETTING_CMD_FLAG_CONFIRM),

    BOOL_SETTING(SETTING_KEY_RF_POWER_TEST, "RF Power Test", SETTING_FLAG_EPHEMERAL, FOLDER_ID_ROOT, false),

    FOLDER(SETTING_KEY_ABOUT, "About", FOLDER_ID_ABOUT, FOLDER_ID_ROOT, NULL),
    RO_STRING_SETTING(SETTING_KEY_ABOUT_VERSION, "Version", FOLDER_ID_ABOUT, SOFTWARE_VERSION),
    RO_STRING_SETTING(SETTING_KEY_ABOUT_BUILD_DATE, "Build Date", FOLDER_ID_ABOUT, __DATE__),
    RO_STRING_SETTING(SETTING_KEY_ABOUT_BUILD_DATE, "Board", FOLDER_ID_ABOUT, BOARD_NAME),
    RX_STRING_SETTING(SETTING_KEY_ABOUT_ADDR, "Address", FOLDER_ID_ABOUT, setting_format_own_addr),

    FOLDER(SETTING_KEY_DIAGNOSTICS, "Diagnostics", FOLDER_ID_DIAGNOSTICS, FOLDER_ID_ROOT, NULL),
    CMD_SETTING(SETTING_KEY_DIAGNOSTICS_FREQUENCIES, "Frequencies", FOLDER_ID_DIAGNOSTICS, 0, 0),
    CMD_SETTING(SETTING_KEY_DIAGNOSTICS_DEBUG_INFO, "Debug Info", FOLDER_ID_DIAGNOSTICS, 0, 0),

#if defined(USE_DEVELOPER_MENU)
    FOLDER(SETTING_KEY_DEVELOPER, "Developer Options", FOLDER_ID_DEVELOPER, FOLDER_ID_DIAGNOSTICS, NULL),
    BOOL_SETTING(SETTING_KEY_DEVELOPER_REMOTE_DEBUGGING, "Remote Debugging", 0, FOLDER_ID_DEVELOPER, false),
    CMD_SETTING(SETTING_KEY_DEVELOPER_REBOOT, "Reboot", FOLDER_ID_DEVELOPER, 0, 0),
#endif
};

#if CONFIG_MAX_PAIRED_RX != 4 && CONFIG_MAX_PAIRED_RX != 16 && CONFIG_MAX_PAIRED_RX != 32
#error Adjust RX_FOLDER(n) settings
#endif

_Static_assert(SETTING_COUNT == ARRAY_COUNT(settings), "SETTING_COUNT != ARRAY_COUNT(settings)");

typedef struct settings_listener_s
{
    setting_changed_f callback;
    void *user_data;
} settings_listener_t;

static settings_listener_t listeners[4];
static storage_t storage;

static void map_setting_keys(settings_view_t *view, const char *keys[], int size)
{
    view->count = 0;
    for (int ii = 0; ii < size; ii++)
    {
        int idx;
        ASSERT(settings_get_key_idx(keys[ii], &idx));
        view->indexes[view->count++] = (uint8_t)idx;
    }
}

static setting_value_t *setting_get_val_ptr(const setting_t *setting)
{
    int index = setting - settings;
    return &setting_values[index];
}

static char *setting_get_str_ptr(const setting_t *setting)
{
    if (setting->type == SETTING_TYPE_STRING)
    {
        if (setting->flags & SETTING_FLAG_READONLY)
        {
            // Stored in the data ptr in the setting
            return (char *)setting->data;
        }
        // The u8 stored the string index
        return settings_string_storage[setting_get_val_ptr(setting)->u8];
    }
    return NULL;
}

static void setting_save(const setting_t *setting)
{
    if (setting->flags & SETTING_FLAG_EPHEMERAL)
    {
        return;
    }
    switch (setting->type)
    {
    case SETTING_TYPE_U8:
        storage_set_u8(&storage, setting->key, setting_get_val_ptr(setting)->u8);
        break;
        /*
    case SETTING_TYPE_I8:
        storage_set_i8(&storage, setting->key, setting->val.i8);
        break;
    case SETTING_TYPE_U16:
        storage_set_u16(&storage, setting->key, setting->val.u16);
        break;
    case SETTING_TYPE_I16:
        storage_set_i16(&storage, setting->key, setting->val.i16);
        break;
    case SETTING_TYPE_U32:
        storage_set_u32(&storage, setting->key, setting->val.u32);
        break;
    case SETTING_TYPE_I32:
        storage_set_i32(&storage, setting->key, setting->val.i32);
        break;
        */
    case SETTING_TYPE_STRING:
        storage_set_str(&storage, setting->key, setting_get_str_ptr(setting));
        break;
    case SETTING_TYPE_FOLDER:
        break;
    }
    storage_commit(&storage);
}

static void setting_changed(const setting_t *setting)
{
    LOG_I(TAG, "Setting %s changed", setting->key);
    for (int ii = 0; ii < ARRAY_COUNT(listeners); ii++)
    {
        if (listeners[ii].callback)
        {
            listeners[ii].callback(setting, listeners[ii].user_data);
        }
    }
    setting_save(setting);
}

static void setting_move(const setting_t *setting, int delta)
{
    if (setting->flags & SETTING_FLAG_READONLY)
    {
        return;
    }
    switch (setting->type)
    {
    case SETTING_TYPE_U8:
    {
        uint8_t v;
        uint8_t ov = setting_get_val_ptr(setting)->u8;
        if (delta < 0 && ov == 0)
        {
            v = setting->max.u8;
        }
        else if (delta > 0 && ov == setting->max.u8)
        {
            v = 0;
        }
        else
        {
            v = ov + delta;
        }
        if (v != ov)
        {
            setting_get_val_ptr(setting)->u8 = v;
            setting_changed(setting);
        }
        break;
    }
    default:
        break;
    }
}

void settings_init(void)
{
    storage_init(&storage, SETTINGS_STORAGE_KEY);

    // Initialize GPIO names
    for (int ii = 0; ii < HAL_GPIO_USER_COUNT; ii++)
    {
        hal_gpio_t x = hal_gpio_user_at(ii);
        hal_gpio_toa(x, gpio_name_storage[ii], sizeof(gpio_name_storage[ii]));
        gpio_names[ii] = gpio_name_storage[ii];
    }

    unsigned string_storage_index = 0;

    for (int ii = 0; ii < ARRAY_COUNT(settings); ii++)
    {
        const setting_t *setting = &settings[ii];
        // Checking this at compile time is tricky, since most strings are
        // assembled via macros. Do it a runtime instead, impact should be
        // pretty minimal.
        if (strlen(setting->key) > MAX_SETTING_KEY_LENGTH)
        {
            LOG_E(TAG, "Setting key '%s' is too long (%d, max is %d)", setting->key,
                  strlen(setting->key), MAX_SETTING_KEY_LENGTH);
            abort();
        }
        if (setting->flags & SETTING_FLAG_READONLY)
        {
            continue;
        }
        bool found = true;
        size_t size;
        switch (setting->type)
        {
        case SETTING_TYPE_U8:
            found = storage_get_u8(&storage, setting->key, &setting_get_val_ptr(setting)->u8);
            break;
            /*
        case SETTING_TYPE_I8:
            found = storage_get_i8(&storage, setting->key, &setting->val.i8);
            break;
        case SETTING_TYPE_U16:
            found = storage_get_u16(&storage, setting->key, &setting->val.u16);
            break;
        case SETTING_TYPE_I16:
            found = storage_get_i16(&storage, setting->key, &setting->val.i16);
            break;
        case SETTING_TYPE_U32:
            found = storage_get_u32(&storage, setting->key, &setting->val.u32);
            break;
        case SETTING_TYPE_I32:
            found = storage_get_i32(&storage, setting->key, &setting->val.i32);
            break;
            */
        case SETTING_TYPE_STRING:
            assert(string_storage_index < ARRAY_COUNT(settings_string_storage));
            setting_get_val_ptr(setting)->u8 = string_storage_index++;
            size = sizeof(settings_string_storage[0]);
            if (!storage_get_str(&storage, setting->key, setting_get_str_ptr(setting), &size))
            {
                memset(setting_get_str_ptr(setting), 0, SETTING_STRING_BUFFER_SIZE);
            }
            // We can't copy the default_value over to string settings, otherwise
            // the pointer becomes NULL.
            found = true;
            break;
        case SETTING_TYPE_FOLDER:
            break;
        }
        if (!found && !(setting->flags & SETTING_FLAG_CMD))
        {
            memcpy(setting_get_val_ptr(setting), &setting->def_val, sizeof(setting->def_val));
        }
    }
}

void settings_add_listener(setting_changed_f callback, void *user_data)
{
    for (int ii = 0; ii < ARRAY_COUNT(listeners); ii++)
    {
        if (!listeners[ii].callback)
        {
            listeners[ii].callback = callback;
            listeners[ii].user_data = user_data;
            return;
        }
    }
    // Must increase listeners size
    UNREACHABLE();
}

void settings_remove_listener(setting_changed_f callback, void *user_data)
{
    for (int ii = 0; ii < ARRAY_COUNT(listeners); ii++)
    {
        if (listeners[ii].callback == callback && listeners[ii].user_data == user_data)
        {
            listeners[ii].callback = NULL;
            listeners[ii].user_data = NULL;
            return;
        }
    }
    // Tried to remove an unexisting listener
    UNREACHABLE();
}

const setting_t *settings_get_setting_at(int idx)
{
    return &settings[idx];
}

const setting_t *settings_get_key(const char *key)
{
    return settings_get_key_idx(key, NULL);
}

uint8_t settings_get_key_u8(const char *key)
{
    return setting_get_u8(settings_get_key(key));
}

hal_gpio_t settings_get_key_gpio(const char *key)
{
    return setting_get_gpio(settings_get_key(key));
}

bool settings_get_key_bool(const char *key)
{
    return setting_get_bool(settings_get_key(key));
}

const char *settings_get_key_string(const char *key)
{
    return setting_get_string(settings_get_key(key));
}

const setting_t *settings_get_key_idx(const char *key, int *idx)
{
    for (int ii = 0; ii < ARRAY_COUNT(settings); ii++)
    {
        if (STR_EQUAL(key, settings[ii].key))
        {
            if (idx)
            {
                *idx = ii;
            }
            return &settings[ii];
        }
    }
    return NULL;
}

const setting_t *settings_get_folder(folder_id_e folder)
{
    for (int ii = 0; ii < ARRAY_COUNT(settings); ii++)
    {
        if (setting_get_folder_id(&settings[ii]) == folder)
        {
            return &settings[ii];
        }
    }
    return NULL;
}

bool settings_is_folder_visible(settings_view_e view_id, folder_id_e folder)
{
    const setting_t *setting = settings_get_folder(folder);
    return setting && setting_is_visible(view_id, setting->key);
}

bool setting_is_visible(settings_view_e view_id, const char *key)
{
    const setting_t *setting = settings_get_key(key);
    if (setting)
    {
        setting_visibility_e visibility = SETTING_VISIBILITY_SHOW;
        if (setting->folder)
        {
            const setting_t *folder = NULL;
            for (int ii = 0; ii < ARRAY_COUNT(settings); ii++)
            {
                if (setting_get_folder_id(&settings[ii]) == setting->folder)
                {
                    folder = &settings[ii];
                    break;
                }
            }
            if (folder && folder->data)
            {
                setting_visibility_f vf = folder->data;
                visibility = vf(setting_get_folder_id(folder), view_id, setting);
            }
        }
        return visibility != SETTING_VISIBILITY_HIDE;
    }
    return false;
}

folder_id_e setting_get_folder_id(const setting_t *setting)
{
    if (setting->type == SETTING_TYPE_FOLDER)
    {
        return setting->def_val.u8;
    }
    return 0;
}

folder_id_e setting_get_parent_folder_id(const setting_t *setting)
{
    return setting->folder;
}

int32_t setting_get_min(const setting_t *setting)
{
    switch (setting->type)
    {
    case SETTING_TYPE_U8:
        return setting->min.u8;
    case SETTING_TYPE_STRING:
        break;
    case SETTING_TYPE_FOLDER:
        break;
    }
    return 0;
}

int32_t setting_get_max(const setting_t *setting)
{
    switch (setting->type)
    {
    case SETTING_TYPE_U8:
        return setting->max.u8;
    case SETTING_TYPE_STRING:
        break;
    case SETTING_TYPE_FOLDER:
        break;
    }
    return 0;
}

int32_t setting_get_default(const setting_t *setting)
{
    switch (setting->type)
    {
    case SETTING_TYPE_U8:
        return setting->def_val.u8;
    case SETTING_TYPE_STRING:
        break;
    case SETTING_TYPE_FOLDER:
        break;
    }
    return 0;
}

uint8_t setting_get_u8(const setting_t *setting)
{
    assert(setting->type == SETTING_TYPE_U8);
    return setting_get_val_ptr(setting)->u8;
}

// Commands need special processing when used via setting values.
// This is only used when exposing the settings via CRSF.
static void setting_set_u8_cmd(const setting_t *setting, uint8_t v)
{
    setting_cmd_flag_e cmd_flags = setting_cmd_get_flags(setting);
    setting_value_t *val = setting_get_val_ptr(setting);
    switch ((setting_cmd_status_e)v)
    {
    case SETTING_CMD_STATUS_CHANGE:
        if (cmd_flags & SETTING_CMD_FLAG_CONFIRM)
        {
            // Setting needs a confirmation. Change its value to SETTING_CMD_STATUS_ASK_CONFIRM
            // So clients know they need to show the dialog.
            val->u8 = SETTING_CMD_STATUS_ASK_CONFIRM;
            break;
        }
        if (cmd_flags & SETTING_CMD_FLAG_WARNING)
        {
            val->u8 = SETTING_CMD_STATUS_SHOW_WARNING;
            break;
        }
        // TODO: Timeout if the client doesn't commit or discard after some time for
        // SETTING_CMD_FLAG_CONFIRM and SETTING_CMD_FLAG_WARNING

        // No flags. Just run it.
        setting_cmd_exec(setting);
        break;
    case SETTING_CMD_STATUS_COMMIT:
        setting_cmd_exec(setting);
        val->u8 = 0;
        break;
    case SETTING_CMD_STATUS_NONE:
    case SETTING_CMD_STATUS_DISCARD:
        // TODO: If the command shows a warning while it's active,
        // we need to generate a notification when it stops.
        val->u8 = 0;
        break;
    default:
        // TODO: Once we have a timeout, reset it here
        break;
    }
}

void setting_set_u8(const setting_t *setting, uint8_t v)
{
    assert(setting->type == SETTING_TYPE_U8);

    if (setting->flags & SETTING_FLAG_CMD)
    {
        setting_set_u8_cmd(setting, v);
        return;
    }

    v = MIN(v, setting->max.u8);
    v = MAX(v, setting->min.u8);
    if ((setting->flags & SETTING_FLAG_READONLY) == 0 && setting_get_val_ptr(setting)->u8 != v)
    {
        setting_get_val_ptr(setting)->u8 = v;
        setting_changed(setting);
    }
}

hal_gpio_t setting_get_gpio(const setting_t *setting)
{
    assert(setting->val_names == gpio_names);
    return hal_gpio_user_at(setting_get_val_ptr(setting)->u8);
}

bool setting_get_bool(const setting_t *setting)
{
    assert(setting->type == SETTING_TYPE_U8);
    return setting_get_val_ptr(setting)->u8 ? true : false;
}

void setting_set_bool(const setting_t *setting, bool v)
{
    assert(setting->type == SETTING_TYPE_U8);
    setting_set_u8(setting, v ? 1 : 0);
}

const char *setting_get_string(const setting_t *setting)
{
    assert(setting->type == SETTING_TYPE_STRING);
    assert((setting->flags & SETTING_FLAG_DYNAMIC) == 0);
    return setting_get_str_ptr(setting);
}

void setting_set_string(const setting_t *setting, const char *s)
{
    assert(setting->type == SETTING_TYPE_STRING);
    if (setting->flags & SETTING_FLAG_READONLY)
    {
        return;
    }
    char *v = setting_get_str_ptr(setting);
    if (!STR_EQUAL(v, s))
    {
        strlcpy(v, s, SETTING_STRING_BUFFER_SIZE);
        setting_changed(setting);
    }
}

int settings_get_count(void)
{
    return ARRAY_COUNT(settings);
}

const char *setting_map_name(const setting_t *setting, uint8_t val)
{
    if (setting->flags & SETTING_FLAG_NAME_MAP && setting->val_names)
    {
        if (val <= setting->max.u8)
        {
            return setting->val_names[val];
        }
    }
    return NULL;
}

void setting_format_name(char *buf, size_t size, const setting_t *setting)
{
    if ((setting->flags & SETTING_FLAG_DYNAMIC) && setting->data)
    {
        setting_dynamic_format_f format_f = setting->data;
        if (format_f(buf, size, setting, SETTING_DYNAMIC_FORMAT_NAME) > 0)
        {
            return;
        }
    }
    if (setting->name)
    {
        strncpy(buf, setting->name, size);
    }
    else
    {
        buf[0] = '\0';
    }
}

void setting_format(char *buf, size_t size, const setting_t *setting)
{
    char name[SETTING_NAME_BUFFER_SIZE];

    setting_format_name(name, sizeof(name), setting);

    if (setting->type == SETTING_TYPE_FOLDER)
    {
        snprintf(buf, size, "%s >>", name);
        return;
    }
    if (setting->flags & SETTING_FLAG_CMD)
    {
        // Commands don't show their values
        snprintf(buf, size, "%s \xAC", name);
        return;
    }
    char value[64];
    setting_format_value(value, sizeof(value), setting);
    snprintf(buf, size, "%s: %s", name, value);
}

void setting_format_value(char *buf, size_t size, const setting_t *setting)
{
    if (setting->flags & SETTING_FLAG_NAME_MAP)
    {
        const char *name = setting_map_name(setting, setting_get_u8(setting));
        snprintf(buf, size, "%s", name);
    }
    else
    {
        switch (setting->type)
        {
        case SETTING_TYPE_U8:
            snprintf(buf, size, "%u", setting_get_u8(setting));
            break;
            /*
        case SETTING_TYPE_I8:
            snprintf(buf, size, "%d", setting->val.i8);
            break;
        case SETTING_TYPE_U16:
            snprintf(buf, size, "%u", setting->val.u16);
            break;
        case SETTING_TYPE_I16:
            snprintf(buf, size, "%d", setting->val.i16);
            break;
        case SETTING_TYPE_U32:
            snprintf(buf, size, "%u", setting->val.u32);
            break;
        case SETTING_TYPE_I32:
            snprintf(buf, size, "%d", setting->val.i32);
            break;
            */
        case SETTING_TYPE_STRING:
        {
            const char *value = setting_get_str_ptr(setting);
            char value_buf[SETTING_STRING_BUFFER_SIZE];
            if (setting->flags & SETTING_FLAG_DYNAMIC)
            {
                if (setting->data)
                {
                    setting_dynamic_format_f format_f = setting->data;
                    if (format_f(value_buf, sizeof(value_buf), setting, SETTING_DYNAMIC_FORMAT_VALUE) > 0)
                    {
                        value = value_buf;
                    }
                }
            }
            snprintf(buf, size, "%s", value ?: "<null>");
            break;
        }
        case SETTING_TYPE_FOLDER:
            break;
        }
    }
    if (setting->unit)
    {
        strlcat(buf, setting->unit, size);
    }
}

setting_cmd_flag_e setting_cmd_get_flags(const setting_t *setting)
{
    if (setting->flags & SETTING_FLAG_CMD)
    {
        // CMD flags are stored in the default value
        return setting->def_val.u8;
    }
    return 0;
}

bool setting_cmd_exec(const setting_t *setting)
{
    setting_changed(setting);
    return true;
}

int setting_rx_channel_output_get_pos(const setting_t *setting)
{
    static const setting_t *folder = NULL;
    if (!folder)
    {
        folder = settings_get_key(SETTING_KEY_RX_CHANNEL_OUTPUTS);
    }
    if (setting > folder)
    {
        int index = (setting - folder) - 1;
        if (index <= HAL_GPIO_USER_COUNT)
        {
            return index;
        }
    }
    return -1;
}

int setting_receiver_get_rx_num(const setting_t *setting)
{
    if (strstr(setting->key, SETTING_KEY_RECEIVERS_PREFIX))
    {
        folder_id_e folder;
        if (setting->type == SETTING_TYPE_FOLDER)
        {
            folder = setting_get_folder_id(setting);
        }
        else
        {
            folder = setting_get_parent_folder_id(setting);
        }
        return RX_FOLDER_ID(folder);
    }
    return -1;
}

void setting_increment(const setting_t *setting)
{
    setting_move(setting, 1);
}

void setting_decrement(const setting_t *setting)
{
    setting_move(setting, -1);
}

static void settings_view_get_actual_folder_view(settings_view_t *view, settings_view_e view_id, folder_id_e folder, bool recursive)
{
    setting_visibility_f visibility_fn = NULL;
    for (int ii = 0; ii < SETTING_COUNT; ii++)
    {
        const setting_t *setting = &settings[ii];
        if (setting_get_folder_id(setting) == folder)
        {
            visibility_fn = settings[ii].data;
            continue;
        }
        if (setting_get_parent_folder_id(setting) == folder)
        {
            setting_visibility_e visibility = SETTING_VISIBILITY_SHOW;
            if (visibility_fn)
            {
                visibility = visibility_fn(folder, view_id, setting);
            }
            switch (visibility)
            {
            case SETTING_VISIBILITY_SHOW:
                view->indexes[view->count++] = ii;
                if (recursive && setting->type == SETTING_TYPE_FOLDER)
                {
                    // Include this dir
                    settings_view_get_actual_folder_view(view, view_id, setting_get_folder_id(setting), recursive);
                }
                break;
            case SETTING_VISIBILITY_HIDE:
                break;
            case SETTING_VISIBILITY_MOVE_CONTENTS_TO_PARENT:
                // Add the settings in this folder to its parent
                ASSERT(setting->type == SETTING_TYPE_FOLDER);
                settings_view_get_actual_folder_view(view, view_id, setting_get_folder_id(setting), recursive);
                break;
            }
        }
    }
}

bool settings_view_get_folder_view(settings_view_t *view, settings_view_e view_id, folder_id_e folder, bool recursive)
{
    view->count = 0;
    settings_view_get_actual_folder_view(view, view_id, folder, recursive);
    return view->count > 0;
}

bool settings_view_get(settings_view_t *view, settings_view_e view_id, folder_id_e folder)
{
    switch (view_id)
    {
    case SETTINGS_VIEW_CRSF_INPUT:
        map_setting_keys(view, view_crsf_input_tx_settings, ARRAY_COUNT(view_crsf_input_tx_settings));
        return true;
    case SETTINGS_VIEW_MENU:
        return settings_view_get_folder_view(view, view_id, folder, false);
    case SETTINGS_VIEW_REMOTE:
        view->count = 0;
        return settings_view_get_folder_view(view, view_id, folder, true);
    }
    return false;
}

const setting_t *settings_view_get_setting_at(settings_view_t *view, int idx)
{
    if (idx >= 0 && idx < view->count)
    {
        return &settings[view->indexes[idx]];
    }
    return NULL;
}

int settings_view_get_parent_index(settings_view_t *view, const setting_t *setting)
{
    for (int ii = 0; ii < view->count; ii++)
    {
        const setting_t *vs = settings_view_get_setting_at(view, ii);
        if (vs->type == SETTING_TYPE_FOLDER && setting_get_folder_id(vs) == setting->folder)
        {
            return ii;
        }
    }
    return -1;
}
