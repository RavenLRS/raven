#include <os/os.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

extern void vPortSVCHandler(void) __attribute__((naked));
extern void xPortPendSVHandler(void) __attribute__((naked));
extern void xPortSysTickHandler(void);

void sv_call_handler(void)
{
    vPortSVCHandler();
}

void pend_sv_handler(void)
{
    xPortPendSVHandler();
}

void sys_tick_handler(void)
{
    xPortSysTickHandler();
}
