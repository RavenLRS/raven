#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <driver/ledc.h>

#include <hal/pwm.h>

// ESP32 has a total of 8 LEDC timers, divided by 4 high speed and 4 low speed.
// A given timer is defined by 2 parameters: ledc_mode_t [0-1] and
// ledc_timer_t [0-3].
// We arrange them in an 8 element array, so the first four timers are
// high speed ones and the last 4 are slow speed.
//
// A total of 16 channels can be connected to a GPIO for output. The channels
// are also defined by 2 parameters: ledc_mode_t [0-1] and ledc_channel_t [0-7].
// We arrange them in memory in the same way as the timers (first 8 high speed,
// last 8 slow speed)

#define LEDC_SPEED_COUNT LEDC_SPEED_MODE_MAX
#define TIMER_NUM_COUNT 4 // No constant defined by esp-idf

#define TIMER_COUNT (LEDC_SPEED_COUNT * TIMER_NUM_COUNT)
#define TIMER_INDEX(speed, num) ((speed * TIMER_NUM_COUNT) + num)
#define TIMER_SPEED(index) (index / TIMER_NUM_COUNT)
#define TIMER_NUM(index) (index % TIMER_NUM_COUNT)

typedef struct ledc_timer_state_s
{
    uint32_t freq_hz; // Timer frequency Hz
    uint8_t ref;      // Number of channels referencing this timer
    uint8_t duty_resolution_bits;
} ledc_timer_state_t;

typedef struct ledc_channel_state_s
{
    uint8_t timer;   // Reference the timer for this output by its index
    hal_gpio_t gpio; // GPIO outputting this PWM signal. HAL_GPIO_NONE means it's unused.
} ledc_channel_state_t;

#define LEDC_CHANNEL_NUM_COUNT 8
#define LEDC_CHANNEL_COUNT (LEDC_SPEED_COUNT * LEDC_CHANNEL_NUM_COUNT)
#define LEDC_CHANNEL_INDEX(speed, num) ((speed * LEDC_CHANNEL_NUM_COUNT) + num)
#define LEDC_CHANNEL_SPEED(index) (index / LEDC_CHANNEL_NUM_COUNT)
#define LEDC_CHANNEL_NUM(index) (index % LEDC_CHANNEL_NUM_COUNT)

static ledc_timer_state_t timers[TIMER_COUNT];
static ledc_channel_state_t channels[LEDC_CHANNEL_COUNT];

static hal_err_t hal_pwm_configure_timer(int index, uint32_t freq_hz, unsigned duty_resolution_bits)
{
    // TODO: esp-idf 3.2 allows 1-20 bits
    if (duty_resolution_bits < LEDC_TIMER_10_BIT || duty_resolution_bits > LEDC_TIMER_15_BIT)
    {
        return ESP_ERR_INVALID_ARG;
    }
    ledc_timer_config_t config = {
        .speed_mode = TIMER_SPEED(index),
        .duty_resolution = duty_resolution_bits,
        .timer_num = TIMER_NUM(index),
        .freq_hz = freq_hz,
    };
    esp_err_t err;
    if ((err = ledc_timer_config(&config)) != ESP_OK)
    {
        return err;
    }
    timers[index].freq_hz = freq_hz;
    timers[index].duty_resolution_bits = duty_resolution_bits;
    return ESP_OK;
}

hal_err_t hal_pwm_init(void)
{
    static bool initialized = false;
    if (!initialized)
    {
        esp_err_t err;

        memset(timers, 0, sizeof(timers));
        memset(channels, 0, sizeof(channels));
        for (int ii = 0; ii < LEDC_CHANNEL_COUNT; ii++)
        {
            channels[ii].gpio = HAL_GPIO_NONE;
        }
        if ((err = ledc_fade_func_install(0)) != ESP_OK)
        {
            return err;
        }

        initialized = true;
    }
    return HAL_ERR_NONE;
}

hal_err_t hal_pwm_open(hal_gpio_t gpio, unsigned freq_hz, unsigned duty_resolution_bits)
{
    esp_err_t err;
    // Check if this gpio is already open
    for (int ii = 0; ii < LEDC_CHANNEL_COUNT; ii++)
    {
        if (channels[ii].gpio == gpio)
        {
            return ESP_ERR_INVALID_STATE;
        }
    }
    // Try to find a free channel slot
    for (int ii = 0; ii < LEDC_CHANNEL_COUNT; ii++)
    {
        if (channels[ii].gpio == HAL_GPIO_NONE)
        {
            ledc_mode_t speed = LEDC_CHANNEL_SPEED(ii);
            ledc_channel_t channel_num = LEDC_CHANNEL_NUM(ii);
            // XXX: Since we have 8 channels and 4 timers per speed
            // we could have a free channel on this speed but no
            // free timer for the given freq/duty. We're currently
            // not handling this case, since we don't have that many
            // frequencies to handle, but it should be fixed if we
            // support several different ones.
            int timer_index = -1;
            int timer_start = TIMER_INDEX(speed, 0);
            int timer_end = TIMER_INDEX(speed, TIMER_NUM_COUNT);
            // Search for a timer with the given frequency
            for (int jj = timer_start; jj < timer_end; jj++)
            {
                if (timers[jj].freq_hz == freq_hz && timers[jj].duty_resolution_bits == duty_resolution_bits)
                {
                    timer_index = jj;
                    break;
                }
            }
            if (timer_index < 0)
            {
                // No timer with the given frequency
                for (int jj = timer_start; jj < timer_end; jj++)
                {
                    if (timers[jj].ref == 0)
                    {
                        // Free timer. Configure it.
                        if ((err = hal_pwm_configure_timer(jj, freq_hz, duty_resolution_bits) != HAL_ERR_NONE))
                        {
                            return err;
                        }
                        timer_index = jj;
                        break;
                    }
                }
                if (timer_index < 0)
                {
                    // Could not find a timer
                    return ESP_ERR_NOT_SUPPORTED;
                }
            }
            timers[timer_index].ref++;
            ledc_channel_config_t config = {
                .gpio_num = gpio,
                .speed_mode = speed,
                .channel = channel_num,
                .intr_type = LEDC_INTR_DISABLE,
                .timer_sel = TIMER_NUM(timer_index),
                .duty = 0,
            };
            if ((err = ledc_channel_config(&config)) != ESP_OK)
            {
                return err;
            }
            channels[ii].gpio = gpio;
            channels[ii].timer = timer_index;
            return ESP_OK;
        }
    }
    // Could not find a free channel slot
    return ESP_ERR_NOT_SUPPORTED;
}

hal_err_t hal_pwm_close(hal_gpio_t gpio)
{
    esp_err_t err;
    for (int ii = 0; ii < LEDC_CHANNEL_COUNT; ii++)
    {
        if (channels[ii].gpio == gpio)
        {
            ledc_mode_t speed = LEDC_CHANNEL_SPEED(ii);
            ledc_channel_t num = LEDC_CHANNEL_NUM(ii);
            if ((err = ledc_stop(speed, num, 0)) != ESP_OK)
            {
                return err;
            }
            assert(timers[channels[ii].timer].ref > 0);
            if (--timers[channels[ii].timer].ref == 0)
            {
                ledc_timer_t timer_num = TIMER_NUM(channels[ii].timer);
                if ((err = ledc_timer_rst(speed, timer_num)) != ESP_OK)
                {
                    return err;
                }
            }
            // Looks like there's no way to stop a timer, so we
            // don't need to take any action when ref reaches 0
            channels[ii].gpio = HAL_GPIO_NONE;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

hal_err_t hal_pwm_set_duty(hal_gpio_t gpio, uint32_t duty)
{
    return hal_pwm_set_duty_fading(gpio, duty, 0);
}

hal_err_t hal_pwm_set_duty_fading(hal_gpio_t gpio, uint32_t duty, unsigned ms)
{
    esp_err_t err;
    for (int ii = 0; ii < LEDC_CHANNEL_COUNT; ii++)
    {
        if (channels[ii].gpio == gpio)
        {
            ledc_mode_t mode = LEDC_CHANNEL_SPEED(ii);
            ledc_channel_t num = LEDC_CHANNEL_NUM(ii);
            if (ms > 0)
            {
                if ((err = ledc_set_fade_with_time(mode, num, duty, ms)) != ESP_OK)
                {
                    return err;
                }
                if ((err = ledc_fade_start(mode, num, LEDC_FADE_NO_WAIT)) != ESP_OK)
                {
                    return err;
                }
            }
            else
            {
                if ((err = ledc_set_duty(mode, num, duty)) != ESP_OK)
                {
                    return err;
                }
                if ((err = ledc_update_duty(mode, num)) != ESP_OK)
                {
                    return err;
                }
            }
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}
