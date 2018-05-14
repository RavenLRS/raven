#pragma once

#include "util/time.h"

typedef enum
{
    FAILSAFE_REASON_TX_LOST = 1, // RX doesn't see TX
    FAILSAFE_REASON_RX_LOST,     // TX doesn't see RX
    FAILSAFE_REASON_RADIO_LOST,  // TX doesn't see signal from radio
    FAILSAFE_REASON_FC_LOST,     // RX saw FC and now doesn't see it
} failsafe_reason_e;

const char *failsafe_reason_get_name(failsafe_reason_e reason);

typedef struct failsafe_s
{
    time_micros_t max_reset_interval;
    time_micros_t enable_at;
    time_micros_t active_since;
    unsigned resets_since_active; // Require a minimum number of succesful resets to clear
} failsafe_t;

void failsafe_init(failsafe_t *fs);
void failsafe_set_max_interval(failsafe_t *fs, time_micros_t interval);
void failsafe_reset_interval(failsafe_t *fs, time_micros_t now);
void failsafe_update(failsafe_t *fs, time_micros_t now);

inline bool failsafe_is_active(const failsafe_t *fs)
{
    return fs && fs->active_since > 0;
}
