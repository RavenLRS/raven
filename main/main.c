#include <stdio.h>

#include <hal/init.h>
#include <hal/log.h>
#include <hal/wd.h>

#include <os/os.h>

#include "target.h"

#include "air/air_radio.h"
#include "air/air_radio_sx127x.h"

#if defined(USE_BLUETOOTH)
#include "bluetooth/bluetooth.h"
#endif

#include "config/config.h"
#include "config/settings.h"
#include "config/settings_rmp.h"

#include "io/sx127x.h"

#if defined(USE_OTA)
#include "ota/ota.h"
#endif

#if defined(USE_P2P)
#include "p2p/p2p.h"
#endif

#include "platform/system.h"

#include "rc/rc.h"
#include "rc/rc_data.h"

#include "rmp/rmp.h"

#include "ui/led.h"
#include "ui/ui.h"

#include "util/macros.h"
#include "util/time.h"

static air_radio_t radio = {
    .sx127x.spi_bus = SX127X_SPI_BUS,
    .sx127x.mosi = SX127X_GPIO_MOSI,
    .sx127x.miso = SX127X_GPIO_MISO,
    .sx127x.sck = SX127X_GPIO_SCK,
    .sx127x.cs = SX127X_GPIO_CS,
    .sx127x.rst = SX127X_GPIO_RST,
    .sx127x.dio0 = SX127X_GPIO_DIO0,
    .sx127x.output_type = SX127X_OUTPUT_TYPE,
};

static rc_t rc;
static rmp_t rmp;
#if defined(USE_P2P)
static p2p_t p2p;
#endif
static ui_t ui;

static void shutdown(void)
{
    air_radio_shutdown(&radio);
    ui_shutdown(&ui);
    system_shutdown();
}

static void setting_changed(const setting_t *setting, void *user_data)
{
    UNUSED(user_data);

    if (SETTING_IS(setting, SETTING_KEY_POWER_OFF))
    {
        shutdown();
    }
}

void raven_ui_init(void)
{
    ui_config_t cfg = {
        .button = BUTTON_1_GPIO,
#if defined(USE_TOUCH_BUTTON)
#if defined(BUTTON_1_GPIO_IS_TOUCH)
        .button_is_touch = true,
#else
        .button_is_touch = false,
#endif
#endif
        .beeper = BEEPER_GPIO,
#ifdef USE_SCREEN
        .screen.i2c_bus = SCREEN_I2C_BUS,
        .screen.sda = SCREEN_GPIO_SDA,
        .screen.scl = SCREEN_GPIO_SCL,
        .screen.rst = SCREEN_GPIO_RST,
        .screen.addr = SCREEN_I2C_ADDR,
#endif
    };

    ui_init(&ui, &cfg, &rc);
    led_mode_add(LED_MODE_BOOT);
}

void task_ui(void *arg)
{
    UNUSED(arg);

    if (ui_screen_is_available(&ui))
    {
        ui_screen_splash(&ui);
    }

    for (;;)
    {
        ui_update(&ui);
        ui_yield(&ui);
    }
}

void task_rmp(void *arg)
{
    UNUSED(arg);

#if defined(USE_P2P)
    p2p_start(&p2p);
#endif
    for (;;)
    {
        rmp_update(&rmp);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void task_rc_update(void *arg)
{
    UNUSED(arg);

    // Initialize the radio here so its interrupts
    // are fired in the same CPU as this task.
    air_radio_init(&radio);
    // Enable the WDT for this task
    hal_wd_add_task(NULL);
    for (;;)
    {
        rc_update(&rc);
        hal_wd_feed();
    }
}

void app_main(void)
{
    hal_init();

#if defined(USE_OTA)
    ota_init();
#endif

    config_init();
    settings_add_listener(setting_changed, NULL);

    air_addr_t addr = config_get_addr();
    rmp_init(&rmp, &addr);

    settings_rmp_init(&rmp);

#if defined(USE_P2P)
    p2p_init(&p2p, &rmp);
#endif

    rc_init(&rc, &radio, &rmp);

    raven_ui_init();

    xTaskCreatePinnedToCore(task_rc_update, "RC", 4096, NULL, 1, NULL, 1);

#if defined(USE_BLUETOOTH)
    xTaskCreatePinnedToCore(task_bluetooh, "BLUETOOTH", 4096, &rc, 2, NULL, 0);
#endif
    xTaskCreatePinnedToCore(task_rmp, "RMP", 4096, NULL, 2, NULL, 0);

    // Start updating the UI after everything else is set up, since it queries other subsystems
    xTaskCreatePinnedToCore(task_ui, "UI", 4096, NULL, 1, NULL, 0);
}
