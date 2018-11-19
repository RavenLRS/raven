#include <stdio.h>

#include <os/os.h>

#include <libopencm3/stm32/rcc.h>

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void)pxTask;
    printf("Stack overflow in task %s\n", pcTaskName);
    for (;;)
        ;
}

void vApplicationMallocFailedHook(void)
{
    printf("malloc() failed\n");
    for (;;)
        ;
}

extern void app_main(void);

extern void initialise_monitor_handles(void);

int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

    initialise_monitor_handles();

    app_main();
    vTaskStartScheduler();
    return 0;
}