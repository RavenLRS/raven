#pragma once

#include <stdint.h>

#include <hal/err.h>
#include <hal/gpio.h>

hal_err_t hal_pwm_init(void);
hal_err_t hal_pwm_open(hal_gpio_t gpio, uint32_t freq_hz, unsigned duty_resolution_bits);
hal_err_t hal_pwm_close(hal_gpio_t gpio);
hal_err_t hal_pwm_set_duty(hal_gpio_t gpio, uint32_t duty);
hal_err_t hal_pwm_set_duty_fading(hal_gpio_t gpio, uint32_t duty, unsigned ms);
