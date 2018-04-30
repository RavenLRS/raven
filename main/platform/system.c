#include <driver/rtc_io.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_sleep.h>

#include "platform.h"

#include "util/time.h"

#include "system.h"

static system_flag_e system_flags = 0;

system_flag_e system_get_flags(void)
{
    return system_flags;
}

system_flag_e system_add_flag(system_flag_e flag)
{
    system_flag_e old_flags = system_flags;
    system_flags |= flag;
    return old_flags;
}

system_flag_e system_remove_flag(system_flag_e flag)
{
    system_flag_e old_flags = system_flags;
    system_flags &= ~flag;
    return old_flags;
}

bool system_has_flag(system_flag_e flag)
{
    return system_flags & flag;
}

bool system_awake_from_deep_sleep(void)
{
    return esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
}

void system_shutdown(void)
{
    // Wait until the button is released if it's now low
    while (gpio_get_level(BUTTON_1) == 0)
    {
    };

    // Wake up if the button is pressed
    // TODO: This might increase sleep current consumption
    // by a lot, measure it.
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON));
    ESP_ERROR_CHECK(gpio_pulldown_dis(BUTTON_1));
    ESP_ERROR_CHECK(gpio_pullup_en(BUTTON_1));
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(1 << BUTTON_1, ESP_EXT1_WAKEUP_ALL_LOW));

    // TODO: Wake up if the 5V power is connected.
    // If battery power doesn't enable the 5V line
    // we can use a voltage divider from there to
    // a GPIO that supports wakeup by going high.
    esp_deep_sleep_start();
}