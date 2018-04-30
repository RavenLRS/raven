
#include "platform.h"

#include "config/settings.h"

#include "platform/system.h"

#include "rc/rc.h"

#include "ui/led.h"
#include "ui/menu.h"
#include "ui/screen.h"

#include "ui/ui.h"

#ifdef USE_SCREEN

// Returns true iff the event was consumed
static bool ui_handle_screen_wake(ui_t *ui)
{
    if (ui_screen_is_available(ui) && ui->internal.screen_is_off)
    {
        screen_power_on(&ui->internal.screen);
        ui->internal.screen_is_off = false;
        return true;
    }
    return false;
}

static void ui_reset_screen_autooff(ui_t *ui)
{
    if (ui->internal.screen_autooff_interval > 0)
    {
        ui->internal.screen_off_at = time_ticks_now() + ui->internal.screen_autooff_interval;
    }
}

static void ui_handle_screen_button_press(void *user_data)
{
    ui_t *ui = user_data;
    ui_reset_screen_autooff(ui);
    if (ui_handle_screen_wake(ui))
    {
        return;
    }
    if (!menu_press())
    {
        screen_handle_press(&ui->internal.screen);
    }
}

static void ui_handle_screen_button_long_press(void *user_data)
{
    ui_t *ui = user_data;
    ui_reset_screen_autooff(ui);
    if (ui_handle_screen_wake(ui))
    {
        return;
    }
    menu_long_press();
}

static void ui_handle_screen_button_really_long_press(void *user_data)
{
    ui_t *ui = user_data;
    ui_reset_screen_autooff(ui);
    if (ui_handle_screen_wake(ui))
    {
        return;
    }
    menu_really_long_press();
}
#endif

static void ui_handle_noscreen_button_press(void *user_data)
{
    ui_t *ui = user_data;
    if (rc_has_pending_bind_request(ui->internal.rc, NULL))
    {
        rc_accept_bind(ui->internal.rc);
    }
}

static void ui_handle_noscreen_button_long_press(void *user_data)
{
}

static void ui_handle_noscreen_button_really_long_press(void *user_data)
{
    setting_t *bind_setting = settings_get_key(SETTING_KEY_BIND);
    bool is_binding = setting_get_bool(bind_setting);
    if (time_micros_now() < SECS_TO_MICROS(15) && !is_binding)
    {
        setting_set_bool(bind_setting, true);
    }
    else if (is_binding)
    {
        setting_set_bool(bind_setting, false);
    }
}

static void ui_settings_handler(const setting_t *setting, void *user_data)
{

#ifdef USE_SCREEN
#define UPDATE_SCREEN_SETTING(k, f)                                              \
    do                                                                           \
    {                                                                            \
        if (SETTING_IS(setting, k) && screen_is_available(&ui->internal.screen)) \
        {                                                                        \
            f(&ui->internal.screen, setting_get_u8(setting));                    \
        }                                                                        \
    } while (0)

    ui_t *ui = user_data;
    UPDATE_SCREEN_SETTING(SETTING_KEY_SCREEN_ORIENTATION, screen_set_orientation);
    UPDATE_SCREEN_SETTING(SETTING_KEY_SCREEN_BRIGHTNESS, screen_set_brightness);

    if (SETTING_IS(setting, SETTING_KEY_SCREEN_AUTO_OFF) && screen_is_available(&ui->internal.screen))
    {
        ui_set_screen_set_autooff(ui, setting_get_u8(setting));
    }
#endif
}

void ui_init(ui_t *ui, ui_config_t *cfg, rc_t *rc)
{
    led_init();
#ifdef USE_SCREEN
    if (screen_init(&ui->internal.screen, &cfg->screen, rc))
    {
        ui->internal.button.press_callback = ui_handle_screen_button_press;
        ui->internal.button.long_press_callback = ui_handle_screen_button_long_press;
        ui->internal.button.really_long_press_callback = ui_handle_screen_button_really_long_press;
    }
    else
    {
#endif
        ui->internal.button.press_callback = ui_handle_noscreen_button_press;
        ui->internal.button.long_press_callback = ui_handle_noscreen_button_long_press;
        ui->internal.button.really_long_press_callback = ui_handle_noscreen_button_really_long_press;
#ifdef USE_SCREEN
    }
#endif
    ui->internal.rc = rc;
    ui->internal.button.user_data = ui;
    ui->internal.button.pin = cfg->button;
    button_init(&ui->internal.button);
    system_add_flag(SYSTEM_FLAG_BUTTON);
#ifdef USE_SCREEN
    if (screen_is_available(&ui->internal.screen))
    {
        system_add_flag(SYSTEM_FLAG_SCREEN);
        screen_set_orientation(&ui->internal.screen, settings_get_key_u8(SETTING_KEY_SCREEN_ORIENTATION));
        screen_set_brightness(&ui->internal.screen, settings_get_key_u8(SETTING_KEY_SCREEN_BRIGHTNESS));
        ui_set_screen_set_autooff(ui, settings_get_key_u8(SETTING_KEY_SCREEN_AUTO_OFF));
    }
    menu_init(rc);
#endif
    settings_add_listener(ui_settings_handler, ui);
}

bool ui_screen_is_available(const ui_t *ui)
{
#ifdef USE_SCREEN
    return screen_is_available(&ui->internal.screen);
#else
    return false;
#endif
}

void ui_screen_splash(ui_t *ui)
{
#ifdef USE_SCREEN
    screen_splash(&ui->internal.screen);
#endif
}

bool ui_is_animating(const ui_t *ui)
{
#ifdef USE_SCREEN
    return screen_is_animating(&ui->internal.screen);
#else
    return false;
#endif
}

void ui_update(ui_t *ui)
{
    button_update(&ui->internal.button);
    led_update();
#ifdef USE_SCREEN
    if (ui_screen_is_available(ui))
    {
        if (ui->internal.screen_off_at > 0 && ui->internal.screen_off_at < time_ticks_now())
        {
            ui->internal.screen_off_at = 0;
            ui->internal.screen_is_off = true;
            screen_shutdown(&ui->internal.screen);
        }
        menu_update();
        if (!ui->internal.screen_is_off)
        {
            screen_update(&ui->internal.screen);
        }
    }
#endif
}

void ui_shutdown(ui_t *ui)
{
#ifdef USE_SCREEN
    if (ui_screen_is_available(ui))
    {
        screen_shutdown(&ui->internal.screen);
    }
#endif
}

void ui_set_screen_set_autooff(ui_t *ui, ui_screen_autooff_e autooff)
{
#ifdef USE_SCREEN
    ui->internal.screen_autooff_interval = 0;
    switch (autooff)
    {
    case UI_SCREEN_AUTOOFF_DISABLED:
        break;
    case UI_SCREEN_AUTOOFF_30_SEC:
        ui->internal.screen_autooff_interval = SECS_TO_TICKS(30);
        break;
    case UI_SCREEN_AUTOOFF_1_MIN:
        ui->internal.screen_autooff_interval = SECS_TO_TICKS(60);
        break;
    case UI_SCREEN_AUTOOFF_5_MIN:
        ui->internal.screen_autooff_interval = SECS_TO_TICKS(60 * 5);
        break;
    case UI_SCREEN_AUTOOFF_10_MIN:
        ui->internal.screen_autooff_interval = SECS_TO_TICKS(60 * 10);
        break;
    }
    ui_reset_screen_autooff(ui);
#endif
}
