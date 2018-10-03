#include "io/gpio.h"

#include "target.h"

hal_gpio_t hal_gpio_user_at(unsigned idx)
{
    int c = 0;
    for (int ii = 0; ii < HAL_GPIO_MAX; ii++)
    {
        if (HAL_GPIO_IS_USER(ii))
        {
            if (c == idx)
            {
                return ii;
            }
            c++;
        }
    }
    return HAL_GPIO_NONE;
}