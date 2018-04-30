#pragma once

#include "time.h"

typedef struct lpf_s
{
    float value;
    float RC;
    time_micros_t last_update;
} lpf_t;

void lpf_init(lpf_t *lpf, float cutoff);
float lpf_update(lpf_t *lpf, float value, time_micros_t now);
float lpf_reset(lpf_t *lpf, float value);
inline float lpf_value(const lpf_t *lpf) { return lpf->value; }