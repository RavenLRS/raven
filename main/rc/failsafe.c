#include "util/macros.h"

#include "failsafe.h"

#include <hal/log.h>

#include "config/config.h"

#include "rc/rc_data.h"

// Number of succesful resets to clear the active state
#define FAILSAFE_REQUIRED_RESETS_TO_CLEAR 5

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
static const char *TAG = "Failsafe";

static uint16_t custom_channels_values[RC_CHANNELS_NUM];
#endif

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

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
    fs->custom_channels_values_valid = config_get_fs_channels(custom_channels_values, sizeof(custom_channels_values));
    if (fs->custom_channels_values_valid)
    {
        fs->custom_channels_values = custom_channels_values;
        LOG_I(TAG, "Custom F/S channels values loaded");
    }
#endif
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
    ASSERT(fs->max_reset_interval > 0);
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