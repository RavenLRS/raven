#include "failsafe.h"

// Number of succesful resets to clear the active state
#define FAILSAFE_REQUIRED_RESETS_TO_CLEAR 5

const char *failsafe_reason_get_name(failsafe_reason_e reason)
{
    switch (reason)
    {
    case FAILSAFE_REASON_TX_LOST:
        return "TX LOST";
    case FAILSAFE_REASON_RX_LOST:
        return "TELEMETRY LOST";
    case FAILSAFE_REASON_FC_LOST:
        return "FC DISCONNECTED";
    case FAILSAFE_REASON_RADIO_LOST:
        return "RADIO DISCONNECTED";
    }
    return NULL;
}

void failsafe_init(failsafe_t *fs)
{
    failsafe_set_max_interval(fs, 0);
    fs->active_since = 0;
    fs->resets_since_active = 0;
    fs->enable_at = TIME_MICROS_MAX;
}

void failsafe_set_max_interval(failsafe_t *fs, time_micros_t interval)
{
    fs->max_reset_interval = interval;
    if (interval == 0)
    {
        fs->enable_at = TIME_MICROS_MAX;
    }
}

void failsafe_reset_interval(failsafe_t *fs, time_micros_t now)
{
    assert(fs->max_reset_interval > 0);
    fs->enable_at = now + fs->max_reset_interval;

    if (fs->active_since > 0)
    {
        // FS was active, try to reset it
        if (++fs->resets_since_active >= FAILSAFE_REQUIRED_RESETS_TO_CLEAR)
        {
            fs->active_since = 0;
        }
    }
}

void failsafe_update(failsafe_t *fs, time_micros_t now)
{
    if (now > fs->enable_at)
    {
        fs->active_since = now;
        fs->resets_since_active = 0;
    }
}

bool failsafe_is_active(const failsafe_t *fs)
{
    return fs && fs->active_since > 0;
}