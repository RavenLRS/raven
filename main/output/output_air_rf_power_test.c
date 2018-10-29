#include <hal/log.h>
#include <hal/ws2812.h>

#include "air/air.h"
#include "air/air_radio.h"
#include "air/air_rf_power.h"

#include "ui/led.h"

#include "output_air_rf_power_test.h"

#define RF_TEST_LED LED_ID_1
#define RF_TEST_LED_COLOR HAL_WS2812_RGB(75, 0, 130) // Indigo
#define LED_BLINK_MS 500

static const led_stage_t blink_stages[] = {
    LED_STAGE(LED_LEVEL_MAX, RF_TEST_LED_COLOR, LED_BLINK_MS, 0),
    LED_STAGE_OFF(LED_BLINK_MS, 0),
};
LED_PATTERN(blink_pattern, blink_stages, LED_REPEAT_FOREVER);

static const led_stage_t on_stages[] = {
    LED_STAGE(LED_LEVEL_MAX, RF_TEST_LED_COLOR, 10000, 150),
};
LED_PATTERN(on_pattern, on_stages, LED_REPEAT_FOREVER);

#define RF_TEST_TX_MS 5000   // Transmit for 5 seconds
#define RF_TEST_WAIT_MS 5000 // Wait for 5s between power levels

#define RF_TEST_PACKET_SIZE AIR_MAX_PACKET_SIZE

typedef enum
{
    RF_POWER_TEST_STATE_IDLE,
    RF_POWER_TEST_STATE_BLINK,
    RF_POWER_TEST_STATE_TX,
} rf_power_test_state_e;

static const char *TAG = "Output.Air.RFPowerTest";

static void output_air_rf_power_test_send(output_air_rf_power_test_t *output)
{
    uint8_t packet[RF_TEST_PACKET_SIZE];
    air_radio_send(output->air_config.radio, packet, sizeof(packet));
}

static bool output_air_rf_power_test_open(void *data, void *config)
{
    LOG_I(TAG, "Open");
    led_pause();
    output_air_rf_power_test_t *output = data;
    output->state = RF_POWER_TEST_STATE_IDLE;
    output->power = AIR_RF_POWER_FIRST;
    output->next_switch = 0;
    return true;
}

static bool output_air_rf_power_test_update(void *data, rc_data_t *rc_data, bool update_rc, time_micros_t now)
{
    output_air_rf_power_test_t *output = data;
    air_radio_t *radio = output->air_config.radio;
    if (now > output->next_switch)
    {
        switch ((rf_power_test_state_e)output->state)
        {
        case RF_POWER_TEST_STATE_IDLE:
            // Check if the level is valid, then start the blink
            if (output->power > AIR_RF_POWER_LAST)
            {
                output->power = AIR_RF_POWER_FIRST;
            }
            if (output->power == AIR_RF_POWER_AUTO)
            {
                output->power++;
            }
            led_start_pattern(RF_TEST_LED, &blink_pattern);
            output->state = RF_POWER_TEST_STATE_BLINK;
            output->next_switch = now + MILLIS_TO_MICROS(LED_BLINK_MS * 2 * (output->power - 1));
            break;
        case RF_POWER_TEST_STATE_BLINK:
        {
            LOG_I(TAG, "TX starting at power %d (%d dBm / %f MHz)", output->power,
                  air_rf_power_to_dbm(output->power), air_band_frequency(output->air_config.band) / 1e6);
            // Start transmitting. Turn the led on during transmission.
            led_start_pattern(RF_TEST_LED, &on_pattern);
            air_radio_sleep(radio);
            air_radio_set_tx_power(radio, air_rf_power_to_dbm(output->power));
            air_radio_set_frequency(radio, air_band_frequency(output->air_config.band), 0);
            air_radio_set_powertest_mode(radio);
            output_air_rf_power_test_send(output);
            output->state = RF_POWER_TEST_STATE_TX;
            output->next_switch = now + MILLIS_TO_MICROS(RF_TEST_TX_MS);
            break;
        }
        case RF_POWER_TEST_STATE_TX:
            // Finish transmission and wait
            LOG_I(TAG, "Test done at power %d", output->power);
            output->state = RF_POWER_TEST_STATE_IDLE;
            output->next_switch = now + MILLIS_TO_MICROS(RF_TEST_WAIT_MS);
            led_start_pattern(RF_TEST_LED, NULL);
            air_radio_sleep(radio);
            output->power++;
            break;
        }
    }
    if (output->state == RF_POWER_TEST_STATE_TX && air_radio_is_tx_done(output->air_config.radio))
    {
        output_air_rf_power_test_send(output);
    }
    return false;
}

static void output_air_rf_power_test_close(void *data, void *config)
{
    output_air_rf_power_test_t *output = data;
    air_radio_sleep(output->air_config.radio);
    led_resume();
    LOG_I(TAG, "Close");
}

void output_air_rf_power_test_init(output_air_rf_power_test_t *output, air_config_t *air_config)
{
    output->air_config = *air_config;
    output->output.vtable = (output_vtable_t){
        .open = output_air_rf_power_test_open,
        .update = output_air_rf_power_test_update,
        .close = output_air_rf_power_test_close,
    };
}