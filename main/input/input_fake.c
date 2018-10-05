
#include <hal/rand.h>

#include "util/time.h"

#include "input_fake.h"

//#define INPUT_FAKE_CONSTANT
//#define INPUT_FAKE_FORCE_PERIODIC_FAILSAFE

#ifdef INPUT_FAKE_FORCE_PERIODIC_FAILSAFE
// Enter FS for 5 seconds out of every 30
#define INPUT_FAKE_SHOULD_RESET_FS() ({                        \
    unsigned __p = ((unsigned)(time_micros_now() / 1e6)) % 30; \
    __p < 10 || __p >= 15;                                     \
})
#else
#define INPUT_FAKE_SHOULD_RESET_FS() (1)
#endif
#define FAKE_INPUT_MOVED_CHANNELS 8 // The rest are flipped

static bool input_fake_open(void *input, void *config)
{
    input_fake_t *input_fake = input;
    failsafe_set_max_interval(&input_fake->input.failsafe, 1000);
    time_micros_t now = time_micros_now();
    for (int ii = 0; ii < FAKE_INPUT_MOVED_CHANNELS; ii++)
    {
        input_fake->channels[ii] = hal_rand_u32() % 2 == 0;
        rc_data_update_channel(input_fake->input.rc_data, ii, RC_CHANNEL_CENTER_VALUE, now);
    }
    // Put all flipped channels to low, so they have a value
    for (int ii = FAKE_INPUT_MOVED_CHANNELS; ii < RC_CHANNELS_NUM; ii++)
    {
        rc_data_update_channel(input_fake->input.rc_data, ii, RC_CHANNEL_MIN_VALUE, now);
    }
    input_fake->next_flip = 0;
    input_fake->next_update = 0;
    INPUT_SET_MSP_TRANSPORT(input_fake, NULL);
    return true;
}

static void input_fake_update_channel(input_fake_t *input, rc_data_t *data, int idx, time_micros_t now)
{
    control_channel_t *ch = &data->channels[idx];
    unsigned value = ch->value;
#if !defined(INPUT_FAKE_CONSTANT)
    bool *up = &input->channels[idx];
    if (*up && ch->value == RC_CHANNEL_MAX_VALUE)
    {
        *up = false;
    }
    else if (!(*up) && ch->value == RC_CHANNEL_MIN_VALUE)
    {
        *up = true;
    }
    value += *up ? 1 : -1;
#endif
    rc_data_update_channel(data, idx, value, now);
}

static bool input_fake_update(void *input, rc_data_t *data, time_micros_t now)
{
    input_fake_t *input_fake = input;
    if (INPUT_FAKE_SHOULD_RESET_FS())
    {
        failsafe_reset_interval(&input_fake->input.failsafe, now);
    }
    if (failsafe_is_active(&input_fake->input.failsafe))
    {
        return false;
    }
    if (now > input_fake->next_update)
    {
        for (int ii = 0; ii < FAKE_INPUT_MOVED_CHANNELS; ii++)
        {
            input_fake_update_channel(input_fake, data, ii, now);
        }
        input_fake->next_update = now + input_fake->update_interval;

        // Once in a while, flip a random channel in the remaining channels
        if (now > input_fake->next_flip)
        {
            int ch = FAKE_INPUT_MOVED_CHANNELS + (hal_rand_u32() % (RC_CHANNELS_NUM - FAKE_INPUT_MOVED_CHANNELS));
            unsigned value = data->channels[ch].value;
#if !defined(INPUT_FAKE_CONSTANT)
            if (value > RC_CHANNEL_MAX_VALUE * 0.9)
            {
                // Channel high
                value = hal_rand_u32() % 2 == 0 ? RC_CHANNEL_MIN_VALUE : RC_CHANNEL_CENTER_VALUE;
            }
            else if (value < RC_CHANNEL_MAX_VALUE * 0.1)
            {
                // Channel low
                value = hal_rand_u32() % 2 == 0 ? RC_CHANNEL_CENTER_VALUE : RC_CHANNEL_MAX_VALUE;
            }
            else
            {
                // Channel mid
                value = hal_rand_u32() % 2 == 0 ? RC_CHANNEL_MIN_VALUE : RC_CHANNEL_MAX_VALUE;
            }
#endif
            rc_data_update_channel(data, ch, value, now);
            input_fake->next_flip = now + MILLIS_TO_MICROS(500);
        }
        return true;
    }
    return false;
}

static void input_fake_close(void *input, void *config)
{
}

void input_fake_init(input_fake_t *input)
{
    input->input.vtable = (input_vtable_t){
        .open = input_fake_open,
        .update = input_fake_update,
        .close = input_fake_close,
    };
    input->update_interval = FREQ_TO_MICROS(250);
}