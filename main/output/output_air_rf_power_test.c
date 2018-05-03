#include <hal/log.h>

#include "air/air.h"
#include "air/air_rf_power.h"

#include "io/lora.h"

#include "ui/led.h"

#include "output_air_rf_power_test.h"

#define LED_BLINK_MS 300

#define RF_TEST_TX_MS 5000   // Transmit for 5 seconds
#define RF_TEST_WAIT_MS 5000 // Wait for 5s between power levels

#define RF_TEST_PACKET_SIZE AIR_MAX_PACKET_SIZE

#define RF_TEST_LED LED_ID_1

typedef enum {
    RF_POWER_TEST_STATE_IDLE,
    RF_POWER_TEST_STATE_BLINK,
    RF_POWER_TEST_STATE_TX,
} rf_power_test_state_e;

static const char *TAG = "Output.Air.RFPowerTest";

static void output_air_rf_power_test_send(output_air_rf_power_test_t *output)
{
    uint8_t packet[RF_TEST_PACKET_SIZE];
    lora_send(output->lora, packet, sizeof(packet));
}

static bool output_air_rf_power_test_open(void *data, void *config)
{
    LOG_I(TAG, "Open");
    output_air_rf_power_test_t *output = data;
    output->state = RF_POWER_TEST_STATE_IDLE;
    output->power = AIR_RF_POWER_FIRST;
    output->next_switch = 0;
    return true;
}

static bool output_air_rf_power_test_update(void *data, rc_data_t *rc_data, time_micros_t now)
{
    output_air_rf_power_test_t *output = data;
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
            led_set_blink_period(RF_TEST_LED, MILLIS_TO_TICKS(LED_BLINK_MS));
            output->state = RF_POWER_TEST_STATE_BLINK;
            output->next_switch = now + MILLIS_TO_MICROS(LED_BLINK_MS * 2 * output->power);
            break;
        case RF_POWER_TEST_STATE_BLINK:
        {
            LOG_I(TAG, "TX starting at power %d (%d dBm / %f MHz)", output->power,
                  air_rf_power_to_dbm(output->power), air_lora_band_frequency(output->band) / 1e6);
            // Start transmitting. Turn the led on during transmission.
            led_set_blink_mode(RF_TEST_LED, LED_BLINK_MODE_NONE);
            led_on(RF_TEST_LED);
            lora_sleep(output->lora);
            lora_set_tx_power(output->lora, air_rf_power_to_dbm(output->power));
            lora_set_frequency(output->lora, air_lora_band_frequency(output->band));
            air_lora_set_parameters(output->lora, AIR_LORA_MODE_LONGEST);
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
            led_off(RF_TEST_LED);
            lora_sleep(output->lora);
            output->power++;
            break;
        }
    }
    if (output->state == RF_POWER_TEST_STATE_TX && lora_is_tx_done(output->lora))
    {
        output_air_rf_power_test_send(output);
    }
    return true;
}

static void output_air_rf_power_test_close(void *data, void *config)
{
    output_air_rf_power_test_t *output = data;
    lora_sleep(output->lora);
    led_set_blink_mode(RF_TEST_LED, LED_BLINK_MODE_NONE);
    LOG_I(TAG, "Close");
}

void output_air_rf_power_test_init(output_air_rf_power_test_t *output, lora_t *lora, air_lora_band_e band)
{
    output->lora = lora;
    output->band = band;
    output->output.min_update_interval = 0;
    output->output.vtable = (output_vtable_t){
        .open = output_air_rf_power_test_open,
        .update = output_air_rf_power_test_update,
        .close = output_air_rf_power_test_close,
    };
}