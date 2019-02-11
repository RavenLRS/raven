#include <stdio.h>

#include <os/os.h>

#include <libopencm3/cm3/systick.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include "FreeRTOSConfig.h"

#define SYSTICK_RELOAD_VAL ((configCPU_CLOCK_HZ / 1) / configTICK_RATE_HZ)

#define STDOUT_USART USART2
#define STDOUT_USART_GPIO_RCC RCC_GPIOA
#define STDOUT_USART_GPIO_BANK GPIOA
#define STDOUT_USART_RCC RCC_USART2
#define STDOUT_USART_GPIO_TX GPIO_USART2_TX

/*
#define STDOUT_USART USART1
#define STDOUT_USART_GPIO_RCC RCC_GPIOA
#define STDOUT_USART_GPIO_BANK GPIOA
#define STDOUT_USART_RCC RCC_USART1
#define STDOUT_USART_GPIO_TX GPIO_USART1_TX
*/

#if defined(STDOUT_USART)

static void setup_stdout_usart(void)
{
    rcc_periph_clock_enable(STDOUT_USART_GPIO_RCC);
    rcc_periph_clock_enable(STDOUT_USART_RCC);

    gpio_set_mode(STDOUT_USART_GPIO_BANK, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, STDOUT_USART_GPIO_TX);

    usart_set_baudrate(STDOUT_USART, 115200);
    usart_set_databits(STDOUT_USART, 8);
    usart_set_stopbits(STDOUT_USART, USART_STOPBITS_1);
    usart_set_mode(STDOUT_USART, USART_MODE_TX);
    usart_set_parity(STDOUT_USART, USART_PARITY_NONE);
    usart_set_flow_control(STDOUT_USART, USART_FLOWCONTROL_NONE);

    usart_enable(STDOUT_USART);
}

int _write(int fd, char *ptr, int len)
{
    for (int ii = 0; ii < len && *ptr; ii++, ptr++)
    {
        usart_send_blocking(STDOUT_USART, *ptr);
        if (*ptr == '\n')
        {
            usart_send_blocking(STDOUT_USART, '\r');
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

#if defined(STDOUT_USART)
    setup_stdout_usart();
#else
    // For printf() via semihosting
    initialise_monitor_handles();
#endif

    app_main();
    vTaskStartScheduler();
    return 0;
}