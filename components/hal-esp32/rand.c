#include <esp_system.h>

#include <hal/rand.h>

uint32_t hal_rand_u32(void)
{
    // Note that it might not be totally secure RNG value
    // if wifi and BT and both off at the time esp_random() is called.
    return esp_random();
}