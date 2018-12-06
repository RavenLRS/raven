
#include <hal/log.h>

#include "target.h"

#include "config/settings.h"

#include "platform/system.h"

#include "rc/rc.h"

#include "ui/led.h"
#include "ui/menu.h"
#include "ui/screen.h"

#include "ui/ui.h"

static const char *TAG = "UI";

#ifdef USE_SCREEN

// Returns true iff the event was consumed
static bool ui_handle_screen_wake(ui_t *ui)
{
    if (ui_screen_is_available(ui) && ui->internal.screen_is_off)
    {
        screen_power_on(&ui->internal.screen);
        screen_set_brightness(&ui->internal.screen, settings_get_key_u8(SETTING_KEY_SCREEN_BRIGHTNESS));
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
    bool handled = screen_handle_press(&ui->internal.screen, true);
    if (!handled)
    {
        handled |= menu_press();
    }
    if (!handled)
    {
        handled |= screen_handle_press(&ui->internal.screen, false);
    }
    if (handled)
    {
        beeper_beep(&ui->internal.beeper);
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
    bool handled = menu_long_press();
    if (handled)
    {
        beeper_beep(&ui->internal.beeper);
    }
}

static void ui_handle_screen_button_really_long_press(void *user_data)
{
    ui_t *ui = user_data;
    ui_reset_screen_autooff(ui);
    if (ui_handle_screen_wake(ui))
    {
        return;
    }
    bool handled = menu_really_long_press();
    if (handled)
    {
        beeper_beep(&ui->internal.beeper);
    }
}
#endif

static void ui_handle_noscreen_button_press(void *user_data)
{
    ui_t *ui = user_data;
    if (rc_has_pending_bind_request(ui->internal.rc, NULL))
    {
        rc_accept_bind(ui->internal.rc);
        beeper_beep(&ui->internal.beeper);
    }
}

static void ui_handle_noscreen_button_long_press(void *user_data)
{
}

static void ui_handle_noscreen_button_really_long_press(void *user_data)
{
    ui_t *ui = user_data;
    const setting_t *bind_setting = settings_get_key(SETTING_KEY_BIND);
    bool is_binding = setting_get_bool(bind_setting);
    if (time_micros_now() < SECS_TO_MICROS(15) && !is_binding)
    {
        setting_set_bool(bind_setting, true);
        beeper_beep(&ui->internal.beeper);
    }
    else if (is_binding)
    {
        setting_set_bool(bind_setting, false);
        beeper_beep(&ui->internal.beeper);
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

    if (SETTING_IS(setting, SETTING_KEY_DIAGNOSTICS_FREQUENCIES))
    {
        screen_enter_secondary_mode(&ui->internal.screen, SCREEN_SECONDARY_MODE_FREQUENCIES);
    }

    if (SETTING_IS(setting, SETTING_KEY_DIAGNOSTICS_DEBUG_INFO))
    {
        screen_enter_secondary_mode(&ui->internal.screen, SCREEN_SECONDARY_MODE_DEBUG_INFO);
    }
#endif
}

static void ui_update_beeper(ui_t *ui)
{
    if (rc_is_failsafe_active(ui->internal.rc, NULL))
    {
        beeper_set_mode(&ui->internal.beeper, BEEPER_MODE_FAILSAFE);
    }
    else if (rc_is_binding(ui->internal.rc))
    {
        beeper_set_mode(&ui->internal.beeper, BEEPER_MODE_BIND);
    }
    else if (ui->internal.beeper.mode != BEEPER_MODE_STARTUP)
    {
        beeper_set_mode(&ui->internal.beeper, BEEPER_MODE_NONE);
    }
    beeper_update(&ui->internal.beeper);
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
    ui->internal.button.gpio = cfg->button;
#if defined(USE_TOUCH_BUTTON)
    ui->internal.button.is_touch = cfg->button_is_touch;
#endif
    if (cfg->beeper != HAL_GPIO_NONE)
    {
        beeper_init(&ui->internal.beeper, cfg->beeper);
        beeper_set_mode(&ui->internal.beeper, BEEPER_MODE_STARTUP);
    }
    button_init(&ui->internal.button);
    system_add_flag(SYSTEM_FLAG_BUTTON);
#ifdef USE_SCREEN
    if (screen_is_available(&ui->internal.screen))
    {
        LOG_I(TAG, "Screen detected");
        system_add_flag(SYSTEM_FLAG_SCREEN);
        screen_set_orientation(&ui->internal.screen, settings_get_key_u8(SETTING_KEY_SCREEN_ORIENTATION));
        screen_set_brightness(&ui->internal.screen, settings_get_key_u8(SETTING_KEY_SCREEN_BRIGHTNESS));
        ui_set_screen_set_autooff(ui, settings_get_key_u8(SETTING_KEY_SCREEN_AUTO_OFF));
    }
    else
    {
        LOG_I(TAG, "No screen detected");
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
    if (ui->internal.cfg.beeper != HAL_GPIO_NONE)
    {
        ui_update_beeper(ui);
    }
    button_update(&ui->internal.button);
    led_mode_set(LED_MODE_FAILSAFE, rc_is_failsafe_active(ui->internal.rc, NULL));
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

void ui_yield(ui_t *ui)
{
    if (!ui_is_animating(ui))
    {
        time_ticks_t sleep = led_is_fading() ? 1 : MILLIS_TO_TICKS(10);
        vTaskDelay(sleep);
    }
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
