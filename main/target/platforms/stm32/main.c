#include <os/os.h>

extern void app_main(void);

#warning temp fix
void assert(int c)
{
}

int main(void)
{
    app_main();
    vTaskStartScheduler();
}