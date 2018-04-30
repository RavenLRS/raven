#include <math.h>

#include "lpf.h"

void lpf_init(lpf_t *lpf, float cutoff)
{
    lpf->RC = 1.0 / (2.0 * M_PI * cutoff);
    lpf->last_update = 0;
}

float lpf_update(lpf_t *lpf, float value, time_micros_t now)
{
    if (lpf->last_update > 0)
    {
        float dt = (now - lpf->last_update) * 1e-6f;
        lpf->value = lpf->value + dt / (lpf->RC + dt) * (value - lpf->value);
    }
    else
    {
        lpf->value = value;
    }
    lpf->last_update = now;
    return lpf->value;
}

float lpf_reset(lpf_t *lpf, float value)
{
    lpf->value = value;
    lpf->last_update = 0;
    return value;
}