#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <driver/rmt.h>

#include <hal/ws2812.h>

#define DIVIDER 4     /* Above 4, timings start to deviate*/
#define DURATION 12.5 /* minimum time of a single RMT duration \
                         in nanoseconds based on clock */

#define PULSE_T0H (350 / (DURATION * DIVIDER))
#define PULSE_T1H (900 / (DURATION * DIVIDER))
#define PULSE_T0L (900 / (DURATION * DIVIDER))
#define PULSE_T1L (350 / (DURATION * DIVIDER))
#define PULSE_TRS (50000 / (DURATION * DIVIDER))

#define MAX_PULSES 32

#define RMTCHANNEL 0

typedef union rmt_pulse_u {
    struct
    {
        uint32_t duration0 : 15;
        uint32_t level0 : 1;
        uint32_t duration1 : 15;
        uint32_t level1 : 1;
    } __attribute__((packed));
    uint32_t val;
} rmt_pulse_t;

static rmt_pulse_t ws2812_pulses[2];
static const uint8_t *ws2812_buffer = NULL;
static unsigned int ws2812_pos, ws2812_len, ws2812_half;
static xSemaphoreHandle ws2812_sem = NULL;
static intr_handle_t rmt_intr_handle = NULL;

static void ws2812_copy_buffer(void)
{
    unsigned int i, j, offset, len, bit;

    offset = ws2812_half * MAX_PULSES;
    ws2812_half = !ws2812_half;

    len = ws2812_len - ws2812_pos;
    if (len > (MAX_PULSES / 8))
        len = (MAX_PULSES / 8);

    if (!len)
    {
        for (i = 0; i < MAX_PULSES; i++)
        {
            RMTMEM.chan[RMTCHANNEL].data32[i + offset].val = 0;
        }
        return;
    }

    for (i = 0; i < len; i++)
    {
        bit = ws2812_buffer[i + ws2812_pos];
        for (j = 0; j < 8; j++, bit <<= 1)
        {
            RMTMEM.chan[RMTCHANNEL].data32[j + i * 8 + offset].val =
                ws2812_pulses[(bit >> 7) & 0x01].val;
        }
        if (i + ws2812_pos == ws2812_len - 1)
            RMTMEM.chan[RMTCHANNEL].data32[7 + i * 8 + offset].duration1 = PULSE_TRS;
    }

    for (i *= 8; i < MAX_PULSES; i++)
        RMTMEM.chan[RMTCHANNEL].data32[i + offset].val = 0;

    ws2812_pos += len;
    return;
}

static void ws2812_intr_handler(void *arg)
{
    portBASE_TYPE taskAwoken = 0;

    if (RMT.int_st.ch0_tx_thr_event)
    {
        ws2812_copy_buffer();
        RMT.int_clr.ch0_tx_thr_event = 1;
    }
    else if (RMT.int_st.ch0_tx_end && ws2812_sem)
    {
        xSemaphoreGiveFromISR(ws2812_sem, &taskAwoken);
        RMT.int_clr.ch0_tx_end = 1;
    }
}

esp_err_t ws2812_init_rmt(unsigned channel)
{
    RMT.apb_conf.fifo_mask = 1;      //enable memory access, instead of FIFO mode.
    RMT.apb_conf.mem_tx_wrap_en = 1; //wrap around when hitting end of buffer
    RMT.conf_ch[channel].conf0.div_cnt = DIVIDER;
    RMT.conf_ch[channel].conf0.mem_size = 1;
    RMT.conf_ch[channel].conf0.carrier_en = 0;
    RMT.conf_ch[channel].conf0.carrier_out_lv = 1;
    RMT.conf_ch[channel].conf0.mem_pd = 0;

    RMT.conf_ch[channel].conf1.rx_en = 0;
    RMT.conf_ch[channel].conf1.mem_owner = 0;
    RMT.conf_ch[channel].conf1.tx_conti_mode = 0; //loop back mode.
    RMT.conf_ch[channel].conf1.ref_always_on = 1; // use apb clock: 80M
    RMT.conf_ch[channel].conf1.idle_out_en = 1;
    RMT.conf_ch[channel].conf1.idle_out_lv = 0;

    return ESP_OK;
}

hal_err_t hal_ws2812_open(hal_gpio_t gpio)
{
    esp_err_t err;

    if (rmt_intr_handle != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);

    if ((err = rmt_set_pin((rmt_channel_t)RMTCHANNEL, RMT_MODE_TX, (gpio_num_t)gpio)) != ESP_OK)
    {
        return err;
    }

    if ((err = ws2812_init_rmt(RMTCHANNEL)) != ESP_OK)
    {
        return err;
    }

    RMT.tx_lim_ch[RMTCHANNEL].limit = MAX_PULSES;
    RMT.int_ena.ch0_tx_thr_event = 1;
    RMT.int_ena.ch0_tx_end = 1;

    ws2812_pulses[0].level0 = 1;
    ws2812_pulses[0].level1 = 0;
    ws2812_pulses[0].duration0 = PULSE_T0H;
    ws2812_pulses[0].duration1 = PULSE_T0L;
    ws2812_pulses[1].level0 = 1;
    ws2812_pulses[1].level1 = 0;
    ws2812_pulses[1].duration0 = PULSE_T1H;
    ws2812_pulses[1].duration1 = PULSE_T1L;

    esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, ws2812_intr_handler, NULL, &rmt_intr_handle);

    return ESP_OK;
}

hal_err_t hal_ws2812_close(hal_gpio_t gpio)
{
    assert(rmt_intr_handle);
    esp_intr_free(rmt_intr_handle);
    rmt_intr_handle = NULL;
    return ESP_OK;
}

hal_err_t hal_ws2812_set_colors(hal_gpio_t gpio, const hal_ws2812_color_t *colors, size_t count)
{
    ws2812_len = sizeof(hal_ws2812_color_t) * count;
    ws2812_buffer = (const uint8_t *)colors;

    ws2812_pos = 0;
    ws2812_half = 0;

    ws2812_copy_buffer();

    if (ws2812_pos < ws2812_len)
        ws2812_copy_buffer();

    ws2812_sem = xSemaphoreCreateBinary();

    RMT.conf_ch[RMTCHANNEL].conf1.mem_rd_rst = 1;
    RMT.conf_ch[RMTCHANNEL].conf1.tx_start = 1;

    xSemaphoreTake(ws2812_sem, portMAX_DELAY);
    vSemaphoreDelete(ws2812_sem);
    ws2812_sem = NULL;

    return ESP_OK;
}