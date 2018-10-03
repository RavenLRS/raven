#include <driver/ledc.h>

#include "config/config.h"
#include "config/settings.h"

#include "io/gpio.h"

#include "util/macros.h"

#include "pwm.h"

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)

#define PWM_TIMER_FREQ_HZ 50
#define PWM_RESOLUTION LEDC_TIMER_15_BIT
#define PWM_DUTY_MAX_VALUE ((1 << PWM_RESOLUTION) - 1)
#define PWM_RC_MIN_VALUE (PWM_DUTY_MAX_VALUE * 0.05f)
#define PWM_RC_MAX_VALUE (PWM_DUTY_MAX_VALUE * 0.10f)

#define RC_CHANNEL_RANGE (RC_CHANNEL_MAX_VALUE - RC_CHANNEL_MIN_VALUE)

const char *pwm_channel_names[] = {
    "None",
    "CH 1",
    "CH 2",
    "CH 3",
    "CH 4",
    "CH 5",
    "CH 6",
    "CH 7",
    "CH 8",
    "CH 8",
    "CH 10",
    "CH 11",
    "CH 12",
#if RC_CHANNELS_NUM > 12
    "CH 13",
    "CH 14",
    "CH 15",
    "CH 16",
#if RC_CHANNELS_NUM > 16
    "CH 17",
    "CH 18",
#if RC_CHANNELS_NUM > 18
    "CH 19",
    "CH 20",
#endif
#endif
#endif
};

ARRAY_ASSERT_COUNT(pwm_channel_names, PWM_CHANNEL_COUNT, "invalid pwm_channel_names[] size");

typedef struct pwm_output_s
{
    hal_gpio_t gpio;
    int rc_channel;
    uint32_t duty;
    ledc_timer_t timer;
    ledc_mode_t timer_mode;
    ledc_channel_t timer_channel;
} pwm_output_t;

static bool pwm_output_is_enabled(const pwm_output_t *output)
{
    return output->gpio != HAL_GPIO_NONE && output->rc_channel >= 0;
}

static pwm_output_t pwm_outputs[HAL_GPIO_USER_MAX];

void pwm_init(void)
{
    // We need 2 timers, since each timer supports up to 8 channels
    ledc_timer_config_t ledc_timer1 = {
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_TIMER_FREQ_HZ,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer1));

    ledc_timer_config_t ledc_timer2 = {
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_TIMER_FREQ_HZ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer2));

    // Initialize all outputs as not ledc-enabled
    for (int ii = 0; ii < ARRAY_COUNT(pwm_outputs); ii++)
    {
        pwm_outputs[ii].gpio = HAL_GPIO_NONE;
    }
    pwm_update_config();
}

void pwm_update_config(void)
{
    int end = ARRAY_COUNT(pwm_outputs);
    // First, disable all enabled outputs
    for (int ii = 0; ii < end; ii++)
    {
        pwm_output_t *output = &pwm_outputs[ii];
        if (!pwm_output_is_enabled(output))
        {
            break;
        }
        ESP_ERROR_CHECK(ledc_stop(output->timer_mode, output->timer_channel, 0));
        output->gpio = HAL_GPIO_NONE;
    }
    int p = 0;
    ledc_timer_t timer = LEDC_TIMER_0;
    ledc_mode_t timer_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel_t timer_channel = LEDC_CHANNEL_0;
    const setting_t *setting = settings_get_key(SETTING_KEY_RX_CHANNEL_OUTPUTS) + 1;
    for (int ii = 0; ii < end; ii++, setting++)
    {
        pwm_channel_e pwm_ch = setting_get_u8(setting);
        if (pwm_ch == PWM_CHANNEL_NONE || pwm_ch >= PWM_CHANNEL_COUNT)
        {
            continue;
        }
        int pos = setting_rx_channel_output_get_pos(setting);
        ASSERT(pos >= 0);
        int gpio = hal_gpio_user_at(pos);
        if (!pwm_output_can_use_gpio(gpio))
        {
            continue;
        }
        pwm_output_t *output = &pwm_outputs[p++];
        ledc_channel_config_t ledc_channel = {
            .channel = timer_channel,
            .duty = 0,
            .gpio_num = gpio,
            .speed_mode = timer_mode,
            .timer_sel = timer,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
        output->gpio = gpio;
        output->rc_channel = pwm_ch - 1;
        output->duty = 0;
        output->timer = timer;
        output->timer_mode = timer_mode;
        output->timer_channel = timer_channel;
        if (++timer_channel >= LEDC_CHANNEL_MAX)
        {
            // Switch to next timer
            timer = LEDC_TIMER_1;
            timer_mode = LEDC_LOW_SPEED_MODE;
            timer_channel = LEDC_CHANNEL_0;
        }
    }
    if (p < end)
    {
        // Mark the end
        pwm_outputs[p].gpio = HAL_GPIO_NONE;
    }
}

void pwm_update(const rc_data_t *rc_data)
{
    // TODO: This takes up to 40us per PWM output, so
    // it might cause problems at 100hz with 16 PWM outputs.
    // Move it to a task which runs on core 0.
    for (int ii = 0; ii < ARRAY_COUNT(pwm_outputs); ii++)
    {
        pwm_output_t *output = &pwm_outputs[ii];
        if (!pwm_output_is_enabled(output))
        {
            // No more mapped PWM outputs
            break;
        }
        const control_channel_t *ch = &rc_data->channels[output->rc_channel];
        // If we don't have a value, we set a zero so the signal stops
        uint32_t duty = 0;
        if (data_state_has_value(&ch->data_state))
        {
            // Map to the duty cycle
            uint16_t chv = ch->value;
            uint32_t v = (PWM_RC_MAX_VALUE - PWM_RC_MIN_VALUE) * (chv - RC_CHANNEL_MIN_VALUE);
            duty = (v / RC_CHANNEL_RANGE) + PWM_RC_MIN_VALUE;
        }
        if (duty != output->duty)
        {
            ESP_ERROR_CHECK(ledc_set_duty(output->timer_mode, output->timer_channel, duty));
            ESP_ERROR_CHECK(ledc_update_duty(output->timer_mode, output->timer_channel));
            output->duty = duty;
        }
    }
}

bool pwm_output_can_use_gpio(hal_gpio_t gpio)
{
    if ((HAL_GPIO_USER_MASK & HAL_GPIO_M(gpio)) && config_get_rc_mode() == RC_MODE_RX)
    {
        // Check if the pin is being used as serial IO
        if (config_get_output_type() != RX_OUTPUT_NONE)
        {
            int tx_gpio = settings_get_key_gpio(SETTING_KEY_RX_TX_GPIO);
            int rx_gpio = settings_get_key_gpio(SETTING_KEY_RX_RX_GPIO);
            if (gpio == tx_gpio || gpio == rx_gpio)
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

#endif // CONFIG_RAVEN_USE_PWM_OUTPUTS
