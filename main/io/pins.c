#include "util/macros.h"

#include "pins.h"

int pin_usable_at(int idx)
{
    int c = 0;
    for (int ii = 0; ii < PIN_MAX; ii++)
    {
        if (PIN_IS_USABLE(ii))
        {
            if (c == idx)
            {
                return ii;
            }
            c++;
        }
    }
    return -1;
}