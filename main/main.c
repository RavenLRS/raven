#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_log.h>
#include <esp_task_wdt.h>
#include "tcpip_adapter.h"
#include "nvs_flash.h"
#include <soc/timer_group_struct.h>
#include <soc/timer_group_reg.h>

#include "platform.h"

#include "bluetooth/bluetooth.h"

#include "config/config.h"
#include "config/settings.h"
#include "config/settings_rmp.h"

#include "io/lora.h"

#include "p2p/p2p.h"

#include "platform/system.h"

#include "rc/rc.h"
#include "rc/rc_data.h"

#include "rmp/rmp.h"

#include "ui/ui.h"

#include "util/time.h"

static lora_t lora = {
    .mosi = LORA_MOSI,
    .miso = LORA_MISO,
    .sck = LORA_SCK,
    .cs = LORA_CS,
    .rst = LORA_RST,
    .dio0 = LORA_DIO0,
    .output_type = LORA_OUTPUT_TYPE,
};

static rc_t rc;
static rmp_t rmp;
static p2p_t p2p;
static ui_t ui;

static void shutdown(void)
{
    lora_shutdown(&lora);
    ui_shutdown(&ui);
    system_shutdown();
}

static void setting_changed(const setting_t *setting, void *user_data)
{
    if (SETTING_IS(setting, SETTING_KEY_POWER_OFF))
    {
        shutdown();
    }
}

void task_ui(void *arg)
{
    ui_config_t cfg = {
        .button = BUTTON_1,
#ifdef USE_SCREEN
        .screen.sda = SCREEN_SDA,
        .screen.scl = SCREEN_SCL,
        .screen.rst = SCREEN_RST,
        .screen.addr = SCREEN_I2C_ADDR,
#endif
    };

    ui_init(&ui, &cfg, &rc);

    if (ui_screen_is_available(&ui))
    {
        ui_screen_splash(&ui);
    }

    for (;;)
    {
        ui_update(&ui);
        if (!ui_is_animating(&ui))
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}

void task_rmp(void *arg)
{
    p2p_start(&p2p);
    for (;;)
    {
        rmp_update(&rmp);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void task_rc_update(void *arg)
{
    // Initialize LoRa here so its interrupts
    // are fired in the same CPU as this task.
    lora_init(&lora);
    // Enable the WDT for this task
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    for (;;)
    {
        rc_update(&rc);
        // Feed the WTD using the registers directly. Otherwise
        // we take too long here.
        TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
        TIMERG0.wdt_feed = 1;
        TIMERG0.wdt_wprotect = 0;
    }
}

void app_main()
{
    config_init();
    settings_add_listener(setting_changed, NULL);

    tcpip_adapter_init();

    air_addr_t addr = config_get_addr();
    rmp_init(&rmp, &addr);

    settings_rmp_init(&rmp);

    p2p_init(&p2p, &rmp);

    rc_init(&rc, &lora, &rmp);

    xTaskCreatePinnedToCore(task_rc_update, "RC", 4096, NULL, 1, NULL, 1);

    xTaskCreatePinnedToCore(task_bluetooh, "BLUETOOTH", 4096, &rc, 2, NULL, 0);
    xTaskCreatePinnedToCore(task_rmp, "RMP", 4096, NULL, 2, NULL, 0);
    // Initialize UI the last, since it might query the others
    xTaskCreatePinnedToCore(task_ui, "UI", 4096, NULL, 1, NULL, 0);
}
