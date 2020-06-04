#include <stdio.h>

#include <os/os.h>

#include <libopencm3/cm3/systick.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include "io/serial.h"

#include "FreeRTOSConfig.h"

#define SYSTICK_RELOAD_VAL ((configCPU_CLOCK_HZ / 1) / configTICK_RATE_HZ)

#define STDOUT_TX HAL_GPIO_PA(2) // USART2_TX

#if defined(STDOUT_TX)

static serial_port_t *stdout_port;

static void setup_stdout_usart(void)
{
    serial_port_config_t config = {
        .baud_rate = 115200,
        .tx = STDOUT_TX,
        .rx = SERIAL_UNUSED_GPIO,
        .parity = SERIAL_PARITY_DISABLE,
        .stop_bits = SERIAL_STOP_BITS_1,
    };

    stdout_port = serial_port_open(&config);
}

int _write(int fd, char *ptr, int len)
{
    char r = '\r';
    for (int ii = 0; ii < len && *ptr; ii++, ptr++)
    {
        serial_port_write(stdout_port, ptr, 1);
        if (*ptr == '\n')
        {
            serial_port_write(stdout_port, &r, 1);
        }
    }
    return len;
}

#endif

uint64_t stm32_time_micros_now(void)
{
    volatile uint64_t val = xTaskGetTickCount() * 1000;
    val += ((SYSTICK_RELOAD_VAL - systick_get_value()) / (SYSTICK_RELOAD_VAL / 1000));
    return val;
}

void vPortSetupTimerInterrupt(void)
{
    systick_counter_disable();
    systick_interrupt_disable();

    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload(SYSTICK_RELOAD_VAL - 1);
    systick_interrupt_enable();
    systick_counter_enable();
}

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
    _Static_assert(configCPU_CLOCK_HZ == 72000000, "Invalid CPU frequency");
    rcc_clock_setup_in_hse_8mhz_out_72mhz();

#if defined(STDOUT_TX)
    setup_stdout_usart();
#else
    // For printf() via semihosting
    initialise_monitor_handles();
#endif

    app_main();
    vTaskStartScheduler();
    return 0;
}