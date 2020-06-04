#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>

#include <hal/rand.h>

static void adc_setup(void)
{
    rcc_periph_clock_enable(RCC_ADC1);

    /* Make sure the ADC doesn't run during config. */
    adc_power_off(ADC1);

    /* We configure everything for one single conversion. */
    adc_disable_scan_mode(ADC1);
    adc_set_single_conversion_mode(ADC1);
    adc_disable_external_trigger_regular(ADC1);
    adc_set_right_aligned(ADC1);
    /* We want to read the temperature sensor, so we have to enable it. */
    adc_enable_temperature_sensor();
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_1DOT5CYC);

    adc_power_on(ADC1);

    /* Wait for ADC starting up. */
    for (int i = 0; i < 800000; i++)
    {
        __asm__("nop");
    }

    adc_reset_calibration(ADC1);
    adc_calibrate(ADC1);
}

static uint16_t adc_read(void)
{
    // TODO: Set a channel array
    adc_start_conversion_direct(ADC1);

    /* Wait for end of conversion. */
    while (!(adc_eoc(ADC1)))
        ;

    return adc_read_regular(ADC1);
}

uint32_t hal_rand_u32(void)
{
    static bool adc_started = false;
    if (!adc_started)
    {
        adc_setup();
        adc_started = true;
    }
    uint32_t val = adc_read() << 16 | adc_read();
    return val;
}