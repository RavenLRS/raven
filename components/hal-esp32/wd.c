#include <soc/timer_group_reg.h>
#include <soc/timer_group_struct.h>

#include <esp_task_wdt.h>

void hal_wd_add_task(void *task_handle)
{
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
}

void hal_wd_feed(void)
{
    // Feed the WTD using the registers directly. Otherwise
    // we take too long here.
    TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
    TIMERG0.wdt_feed = 1;
    TIMERG0.wdt_wprotect = 0;
}