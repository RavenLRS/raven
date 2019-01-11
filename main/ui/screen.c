#include "target.h"

#ifdef USE_SCREEN
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <u8g2.h>

#include "air/air.h"

#include "ota/ota.h"

#include "platform/system.h"

#include "rc/rc.h"

#include "rmp/rmp.h"

#include "ui/animation/animation.h"
#include "ui/menu.h"
#include "ui/screen_i2c.h"

#include "util/time.h"
#include "util/version.h"

#include "screen.h"

#define SCREEN_DRAW_BUF_SIZE 128
#define ANIMATION_FRAME_DURATION_MS 66
#define ANIMATION_TOTAL_DURATION_MS (ANIMATION_REPEAT * ANIMATION_COUNT * ANIMATION_FRAME_DURATION_MS)

typedef enum
{
    SCREEN_DIRECTION_HORIZONTAL,
    SCREEN_DIRECTION_VERTICAL,
} screen_direction_e;

#define SCREEN_DIRECTION(s) ((screen_direction_e)s->internal.direction)
#define SCREEN_W(s) (s->internal.w)
#define SCREEN_H(s) (s->internal.h)
#define SCREEN_BUF(s) (s->internal.buf)

typedef enum
{
    SCREEN_MULTILINEOPT_NONE = 0,
    SCREEN_MULTILINEOPT_BORDER = 1,
    SCREEN_MULTILINEOPT_BOX = 2,
} screen_multiline_opt_e;

static u8g2_t u8g2;

bool screen_init(screen_t *screen, screen_i2c_config_t *cfg, rc_t *rc)
{
    memset(screen, 0, sizeof(*screen));
    screen->internal.available = screen_i2c_init(cfg, &u8g2);
    screen->internal.cfg = *cfg;
    screen->internal.rc = rc;
    return screen->internal.available;
}

bool screen_is_available(const screen_t *screen)
{
    return screen->internal.available;
}

void screen_shutdown(screen_t *screen)
{
    screen_i2c_shutdown(&screen->internal.cfg, &u8g2);
}

void screen_power_on(screen_t *screen)
{
    screen_i2c_power_on(&screen->internal.cfg, &u8g2);
}

void screen_enter_secondary_mode(screen_t *screen, screen_secondary_mode_e mode)
{
    screen->internal.secondary_mode = mode;
}

static void screen_splash_task(void *arg)
{
    screen_t *screen = arg;
    screen->internal.splashing = true;
    uint16_t w = u8g2_GetDisplayWidth(&u8g2);
    uint16_t h = u8g2_GetDisplayHeight(&u8g2);

    uint16_t anim_x = (w - ANIMATION_WIDTH) / 2;
    uint16_t anim_y = (h - ANIMATION_HEIGHT) / 2;

#define SPLASH_TOP FIRMWARE_NAME
#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
#define SPLASH_TOP_SUBTITLE "TX/RX"
#elif defined(USE_TX_SUPPORT)
#define SPLASH_TOP_SUBTITLE "TX"
#elif defined(USE_RX_SUPPORT)
#define SPLASH_TOP_SUBTITLE "RX"
#endif
#define SPLASH_VERSION_LABEL "Version:"
#define SPLASH_BOTTOM_HORIZONTAL SOFTWARE_VERSION

    for (int ii = 0; ii < MAX(ANIMATION_REPEAT, 1); ii++)
    {
        for (int jj = 0; jj < ANIMATION_COUNT; jj++)
        {
            u8g2_ClearBuffer(&u8g2);

            u8g2_SetFontPosTop(&u8g2);
            u8g2_SetFont(&u8g2, u8g2_font_profont22_tf);
            uint16_t tw = u8g2_GetStrWidth(&u8g2, SPLASH_TOP);
            u8g2_DrawStr(&u8g2, (w - tw) / 2, 0, SPLASH_TOP);

            u8g2_SetFontPosBottom(&u8g2);
            u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);

            uint16_t bw;
            if (w > h)
            {
                // Horizontal
                bw = u8g2_GetStrWidth(&u8g2, SPLASH_TOP_SUBTITLE);
                u8g2_DrawStr(&u8g2, 0, 15, SPLASH_TOP_SUBTITLE);

                bw = u8g2_GetStrWidth(&u8g2, SPLASH_BOTTOM_HORIZONTAL);
                u8g2_DrawStr(&u8g2, (w - bw) / 2, h, SPLASH_BOTTOM_HORIZONTAL);
            }
            else
            {
                // Vertical
                bw = u8g2_GetStrWidth(&u8g2, SPLASH_TOP_SUBTITLE);
                u8g2_DrawStr(&u8g2, (w - bw) / 2, 34, SPLASH_TOP_SUBTITLE);

#if defined(VERSION) && defined(GIT_REVISION)
#define SUB_VERSION "(" GIT_REVISION ")"
                bw = u8g2_GetStrWidth(&u8g2, SPLASH_VERSION_LABEL);
                u8g2_DrawStr(&u8g2, (w - bw) / 2, h - 30, SPLASH_VERSION_LABEL);

                bw = u8g2_GetStrWidth(&u8g2, VERSION);
                u8g2_DrawStr(&u8g2, (w - bw) / 2, h - 15, VERSION);

                bw = u8g2_GetStrWidth(&u8g2, SUB_VERSION);
                u8g2_DrawStr(&u8g2, (w - bw) / 2, h - 0, SUB_VERSION);
#else
                bw = u8g2_GetStrWidth(&u8g2, SPLASH_VERSION_LABEL);
                u8g2_DrawStr(&u8g2, (w - bw) / 2, h - 15, SPLASH_VERSION_LABEL);

                bw = u8g2_GetStrWidth(&u8g2, SOFTWARE_VERSION);
                u8g2_DrawStr(&u8g2, (w - bw) / 2, h - 0, SOFTWARE_VERSION);
#endif
            }

            u8g2_DrawXBM(&u8g2, anim_x, anim_y, ANIMATION_WIDTH, ANIMATION_HEIGHT, (uint8_t *)animation_images[jj]);

            u8g2_SendBuffer(&u8g2);
            time_millis_delay(ANIMATION_FRAME_DURATION_MS);
        }
    }
    screen->internal.splashing = false;
    vTaskDelete(NULL);
}

void screen_splash(screen_t *screen)
{
    xTaskCreatePinnedToCore(screen_splash_task, "SCREEN-SPLASH", 4096, screen, 2, NULL, xPortGetCoreID());
}

void screen_set_orientation(screen_t *screen, screen_orientation_e orientation)
{
    const u8g2_cb_t *rotation = U8G2_R0;
    switch (orientation)
    {
    case SCREEN_ORIENTATION_HORIZONTAL_LEFT:
        // Default rotation, buttons at the left
        break;
    case SCREEN_ORIENTATION_HORIZONTAL_RIGHT:
        // Buttons at the right
        rotation = U8G2_R2;
        break;
    case SCREEN_ORIENTATION_VERTICAL:
        // Buttons on botttom
        rotation = U8G2_R1;
        break;
    case SCREEN_ORIENTATION_VERTICAL_UPSIDE_DOWN:
        rotation = U8G2_R3;
        break;
    }
    u8g2_SetDisplayRotation(&u8g2, rotation);
}

void screen_set_brightness(screen_t *screen, screen_brightness_e brightness)
{
    int contrast = -1;
    switch (brightness)
    {
    case SCREEN_BRIGHTNESS_LOW:
        /* XXX: Don't use 0 contrast here, since some screens will turn
         * off completely with zero. Using 1 has no noticeable difference
         * and let's us easily make the same code work in all supported
         * boards. See #1 for discussion.
         */
        contrast = 1;
        break;
    case SCREEN_BRIGHTNESS_MEDIUM:
        contrast = 128;
        break;
    case SCREEN_BRIGHTNESS_HIGH:
        contrast = 255;
        break;
    }
    if (contrast >= 0)
    {
        u8g2_SetContrast(&u8g2, contrast);
    }
}

bool screen_is_animating(const screen_t *screen)
{
    return false;
}

bool screen_handle_button_event(screen_t *screen, bool before_menu, const button_event_t *ev)
{
    if (screen->internal.secondary_mode != SCREEN_SECONDARY_MODE_NONE)
    {
        screen->internal.secondary_mode = SCREEN_SECONDARY_MODE_NONE;
        return true;
    }

    if (before_menu)
    {
        return false;
    }

    if (ev->type != BUTTON_EVENT_TYPE_SHORT_PRESS)
    {
        return false;
    }

#if defined(USE_BUTTON_5WAY)
    button_id_e bid = button_event_id(ev);
    int direction = bid == BUTTON_ID_LEFT ? -1 : bid == BUTTON_ID_RIGHT ? 1 : 0;
    if (direction == 0)
    {
        return false;
    }
#else
    int direction = 1;
#endif

    if (screen->internal.main_mode == SCREEN_MODE_TELEMETRY)
    {
        screen->internal.telemetry.page += direction;
        if (screen->internal.telemetry.page < 0)
        {
            screen->internal.telemetry.page = 0;
            screen->internal.main_mode = SCREEN_MODE_CHANNELS;
        }
        else if (screen->internal.telemetry.page >= screen->internal.telemetry.count)
        {
            screen->internal.telemetry.page = screen->internal.telemetry.count - 1;
            screen->internal.main_mode = SCREEN_MODE_MAIN;
        }
    }
    else
    {
        screen->internal.main_mode += direction;
        if (screen->internal.main_mode < 0)
        {
            screen->internal.main_mode = SCREEN_MODE_TELEMETRY;
            screen->internal.telemetry.page = screen->internal.telemetry.count - 1;
        }
        else if (screen->internal.main_mode == SCREEN_MODE_TELEMETRY)
        {
            if (direction > 0)
            {
                screen->internal.telemetry.page = 0;
            }
            else
            {
                screen->internal.telemetry.page = screen->internal.telemetry.count - 1;
            }
        }
        // It's not possible to go over SCREEN_MODE_TELEMETRY when moving
        // to the right, because it's handled separately in the previous if block
    }
    return true;
}

static bool screen_format_binding(screen_t *screen, char *buf)
{
    if (rc_is_binding(screen->internal.rc))
    {
        strcpy(buf, "Binding");
        int end = ((time_micros_now() / 1000) / 500) % 4;
        for (int ii = 0; ii < end; ii++)
        {
            size_t len = strlen(buf);
            buf[len] = '.';
            buf[len + 1] = '\0';
        }
        return true;
    }
    return false;
}

static uint16_t screen_animation_offset(uint16_t width, uint16_t max_width, uint16_t *actual_width)
{
    if (width > max_width)
    {
        // Value is too wide, gotta animate
        uint16_t extra = width - max_width;
        // Move 1 pixel every 200ms, stopping for 3 cycles at each end
        time_ticks_t step_duration = MILLIS_TO_TICKS(200);
        uint16_t stop = 3;
        uint16_t offset = (time_ticks_now() / step_duration) % (extra + stop * 2);
        if (offset < stop * 2)
        {
            offset = 0;
        }
        else
        {
            offset -= stop;
            if (offset > extra - 1)
            {
                offset = extra - 1;
            }
        }
        if (actual_width)
        {
            *actual_width = max_width;
        }
        return offset;
    }
    if (actual_width)
    {
        *actual_width = width;
    }
    return 0;
}

static int screen_autosplit_lines(char *buf, uint16_t max_width)
{
    uint16_t line_width;
    size_t len = strlen(buf);
    const char *p = buf;
    int lines = 1;
    int sep = -1;
    for (int ii = 0; ii <= len; ii++)
    {
        if (buf[ii] == ' ' || buf[ii] == '\0')
        {
            buf[ii] = '\0';
            line_width = u8g2_GetStrWidth(&u8g2, p);
            if (line_width <= max_width)
            {
                // Still fits in the line. Store the separator and continue.
                sep = ii;
                buf[ii] = ' ';
            }
            else
            {
                // We need a new line. Check if we had a previous separator,
                // otherwise break here
                if (sep >= 0)
                {
                    buf[ii] = ' ';
                    buf[sep] = '\n';
                    p = &buf[sep + 1];
                    sep = ii;
                }
                else
                {
                    if (ii == len)
                    {
                        // String ends exactly here, the last line scrolls
                        break;
                    }
                    buf[ii] = '\n';
                    p = &buf[ii + 1];
                    sep = -1;
                }
                lines++;
            }
        }
    }
    buf[len] = '\0';
    return lines;
}

static int screen_draw_multiline(char *buf, uint16_t y, screen_multiline_opt_e opt)
{
    const uint16_t display_width = u8g2_GetDisplayWidth(&u8g2);
    uint16_t max_width = display_width;
    int border = 0;
    int x = 0;
    if (opt == SCREEN_MULTILINEOPT_BORDER)
    {
        max_width -= 2;
        border = 2;
    }
    int lines = screen_autosplit_lines(buf, max_width);
    int line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;
    int text_height = lines * line_height;
    int yy = y;

    u8g2_SetFontPosTop(&u8g2);

    switch (opt)
    {
    case SCREEN_MULTILINEOPT_NONE:
        u8g2_SetDrawColor(&u8g2, 1);
        break;
    case SCREEN_MULTILINEOPT_BORDER:
        u8g2_SetDrawColor(&u8g2, 1);
        // Draw the frame later in case the text needs animation
        yy += 1;
        x += 1;
        break;
    case SCREEN_MULTILINEOPT_BOX:
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawBox(&u8g2, 0, y, display_width, text_height);
        u8g2_SetDrawColor(&u8g2, 0);
        break;
    }
    for (int ii = 0; ii < lines; ii++)
    {
        const char *s = u8x8_GetStringLineStart(ii, buf);
        uint16_t width = u8g2_GetStrWidth(&u8g2, s);
        if (width <= max_width)
        {
            u8g2_DrawStr(&u8g2, x + (max_width - width) / 2, yy, s);
        }
        else
        {
            // Animate
            int offset = screen_animation_offset(width, max_width - 2, NULL);
            u8g2_DrawStr(&u8g2, x - offset, yy, s);
        }
        yy += line_height;
    }

    if (opt == SCREEN_MULTILINEOPT_BORDER)
    {
        u8g2_DrawFrame(&u8g2, 0, y, display_width, text_height);
    }

    return text_height + border;
}

static void screen_draw_label_value(screen_t *screen, const char *label, const char *val, uint16_t w, uint16_t y, uint16_t sep)
{
    uint16_t label_width = (label ? u8g2_GetStrWidth(&u8g2, label) : 0) + sep;
    uint16_t max_value_width = w - label_width - 1;
    uint16_t val_width = u8g2_GetStrWidth(&u8g2, val);
    uint16_t val_offset = screen_animation_offset(val_width, max_value_width, &val_width);
    u8g2_DrawStr(&u8g2, label_width - val_offset, y, val);
    u8g2_SetDrawColor(&u8g2, 0);
    uint16_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2);
    u8g2_DrawBox(&u8g2, 0, y - line_height, label_width, line_height);
    u8g2_SetDrawColor(&u8g2, 1);
    if (label)
    {
        u8g2_DrawStr(&u8g2, 0, y, label);
    }
}

static void screen_draw_main_failsafe(screen_t *s, int16_t x, int16_t y, failsafe_reason_e reason)
{
    const char *fs_marker = "* * * * * *";
    bool draw_fs_marker = TIME_CYCLE_EVERY_MS(200, 2) == 0;
    uint16_t offset_y = 0;
    switch (SCREEN_DIRECTION(s))
    {
    case SCREEN_DIRECTION_HORIZONTAL:
        u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);
        offset_y = 15;
        break;
    case SCREEN_DIRECTION_VERTICAL:
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        offset_y = 30;
        break;
    }
    char *buf = SCREEN_BUF(s);
    if (TIME_CYCLE_EVERY_MS(800, 2) == 0)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "!!FAILSAFE!!");
    }
    else
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, failsafe_reason_get_name(reason));
    }
    uint16_t fw;
    if (SCREEN_DIRECTION(s) == SCREEN_DIRECTION_VERTICAL)
    {
        if (draw_fs_marker)
        {
            fw = u8g2_GetStrWidth(&u8g2, fs_marker);
            u8g2_DrawStr(&u8g2, (SCREEN_W(s) - fw) / 2, y + offset_y - 10, fs_marker);
        }
        size_t len = strlen(buf);
        const char *p = buf;
        uint16_t cur_y = y + offset_y;
        for (int ii = 0; ii < len; ii++)
        {
            if (buf[ii] == ' ')
            {
                buf[ii] = '\0';
                fw = u8g2_GetStrWidth(&u8g2, p);
                u8g2_DrawStr(&u8g2, (SCREEN_W(s) - fw) / 2, cur_y, p);
                p = &buf[ii + 1];
                cur_y += 15;
            }
        }
        // Print remaining
        fw = u8g2_GetStrWidth(&u8g2, p);
        u8g2_DrawStr(&u8g2, (SCREEN_W(s) - fw) / 2, cur_y, p);

        if (draw_fs_marker)
        {
            fw = u8g2_GetStrWidth(&u8g2, fs_marker);
            u8g2_DrawStr(&u8g2, (SCREEN_W(s) - fw) / 2, cur_y + 15, fs_marker);
        }
    }
    else
    {
        fw = u8g2_GetStrWidth(&u8g2, buf);
        u8g2_DrawStr(&u8g2, (SCREEN_W(s) - fw) / 2, y + offset_y, buf);

        if (draw_fs_marker)
        {
            u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
            fw = u8g2_GetStrWidth(&u8g2, fs_marker);
            u8g2_DrawStr(&u8g2, (SCREEN_W(s) - fw) / 2, y + offset_y - 11, fs_marker);
            u8g2_DrawStr(&u8g2, (SCREEN_W(s) - fw) / 2, y + offset_y + 9, fs_marker);
        }
    }
}

static void screen_draw_main_rssi(screen_t *s, int16_t x, int16_t y)
{
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    u8g2_DrawStr(&u8g2, x, y + 10, "LQ");

    rc_t *rc = s->internal.rc;
    int rssi_percentage = rc_get_rssi_percentage(rc);
    uint16_t bar_x = 0;
    uint16_t bar_y = 0;

    switch (SCREEN_DIRECTION(s))
    {
    case SCREEN_DIRECTION_HORIZONTAL:
        bar_x = x + 13;
        bar_y = 2;
        break;
    case SCREEN_DIRECTION_VERTICAL:
        bar_x = x;
        bar_y = 14;
        break;
    }

    char *buf = SCREEN_BUF(s);

    float snr = rc_get_snr(rc);
    if (SCREEN_DIRECTION(s) == SCREEN_DIRECTION_HORIZONTAL)
    {
        // Show the units for the first 3 seconds after animation, then
        // for 2 seconds out of every 30.
        long m = (time_micros_now() / 1000);
        if (m < (ANIMATION_TOTAL_DURATION_MS + 3000) || (m / 1000) % 30 <= 1)
        {
            snprintf(buf, SCREEN_DRAW_BUF_SIZE, "R: dBm|F: Hz| S: dB");
        }
        else
        {
            char snr_str[8];
            if (snr >= 10 || snr <= -10)
            {
                snprintf(snr_str, sizeof(snr_str), "%+d", (int)snr);
            }
            else
            {
                snprintf(snr_str, sizeof(snr_str), "%+0.1f", snr);
            }
            snprintf(buf, SCREEN_DRAW_BUF_SIZE, "R:%+3d |F:%3u| S:%s",
                     rc_get_rssi_db(rc), rc_get_update_frequency(rc), snr_str);
        }
        u8g2_DrawStr(&u8g2, x, y + bar_y + 20, buf);
    }
    else
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%+ddBm", rc_get_rssi_db(rc));
        screen_draw_label_value(s, "R:", buf, SCREEN_W(s), y + bar_y + 26, 3);
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%uHz", rc_get_update_frequency(rc));
        screen_draw_label_value(s, "F:", buf, SCREEN_W(s), y + bar_y + 38, 3);
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%+.2fdB", snr);
        screen_draw_label_value(s, "S:", buf, SCREEN_W(s), y + bar_y + 50, 3);
    }

    uint16_t rssi_width = SCREEN_W(s) - bar_x;
    uint16_t rssi_box_width = (rssi_width * rssi_percentage) / 100;
    u8g2_DrawFrame(&u8g2, bar_x, y + bar_y, rssi_width, 8);
    u8g2_DrawBox(&u8g2, bar_x, y + bar_y, rssi_box_width, 8);

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%3d%%", rssi_percentage);
    if (u8g2_GetStrWidth(&u8g2, buf) + 1 < rssi_box_width)
    {
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawStr(&u8g2, bar_x + 1, y + bar_y + 7, buf);
        u8g2_SetDrawColor(&u8g2, 1);
    }
}

static void screen_draw_main_signal(screen_t *s)
{
    int16_t y = 32;
    int16_t x = 0;

    failsafe_reason_e reason = 0;
    if (rc_is_failsafe_active(s->internal.rc, &reason))
    {
        screen_draw_main_failsafe(s, x, y, reason);
    }
    else
    {
        screen_draw_main_rssi(s, x, y);
    }
}

static void screen_draw_main(screen_t *s)
{
    const char *pilot_name = rc_get_pilot_name(s->internal.rc);
    if (!pilot_name)
    {
        pilot_name = "---";
    }
    const char *craft_name = rc_get_craft_name(s->internal.rc);
    if (!craft_name)
    {
        craft_name = "---";
    }

    const char *top_prefix = NULL;
    const char *top_suffix = NULL;
    const char *bottom_prefix = NULL;
    const char *bottom_suffix = NULL;
    switch (rc_get_mode(s->internal.rc))
    {
    case RC_MODE_TX:
        top_prefix = "TX:";
        top_suffix = pilot_name;
        bottom_prefix = "RX:";
        bottom_suffix = craft_name;
        break;
    case RC_MODE_RX:
        top_prefix = "RX:";
        top_suffix = craft_name;
        bottom_prefix = "TX:";
        bottom_suffix = pilot_name;
        break;
    }

    if (SCREEN_DIRECTION(s) == SCREEN_DIRECTION_VERTICAL)
    {
        top_prefix = "";
    }

    u8g2_SetFontPosBaseline(&u8g2);
    // Write Mode
    u8g2_SetFont(&u8g2, u8g2_font_profont22_tf);
    screen_draw_label_value(s, top_prefix, top_suffix, SCREEN_W(s), 14, 2);

    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    char *buf = SCREEN_BUF(s);
    if (screen_format_binding(s, buf))
    {
        u8g2_DrawStr(&u8g2, 0, 25, buf);
    }
    else
    {
        screen_draw_label_value(s, bottom_prefix, bottom_suffix, SCREEN_W(s), 25, 1);
    }

    screen_draw_main_signal(s);

    u8g2_SetFont(&u8g2, u8g2_font_profont11_tf);
    int tx_count;
    int rx_count;
    bool has_pairing_as_peer;
    rmp_get_p2p_counts(s->internal.rc->rmp, &tx_count, &rx_count, &has_pairing_as_peer);
    uint16_t offset = 0;
    u8g2_SetFontPosBottom(&u8g2);
    uint16_t dot_x;
    if (SCREEN_DIRECTION(s) == SCREEN_DIRECTION_HORIZONTAL)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "P2P: %dT/%dR", tx_count, rx_count);
        uint16_t pw = u8g2_GetStrWidth(&u8g2, buf);
        offset = SCREEN_W(s) - pw - offset;
        if (has_pairing_as_peer)
        {
            offset -= 8;
        }
        dot_x = SCREEN_W(s) - 6;
    }
    else
    {
        u8g2_DrawStr(&u8g2, 0, SCREEN_H(s) - 10, "P2P:");
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%dT/%dR", tx_count, rx_count);
        dot_x = u8g2_GetStrWidth(&u8g2, buf) + 3;
    }
    u8g2_DrawStr(&u8g2, offset, SCREEN_H(s) + 2, buf);
    if (has_pairing_as_peer)
    {
        // ASCII bullet (149, 0x95)
        u8g2_DrawStr(&u8g2, dot_x, SCREEN_H(s) + 3, "\x95");
    }
}

static void screen_draw_channels(screen_t *s)
{
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_micro_tr);
    u8g2_SetFontPosTop(&u8g2);

    uint16_t ch_height = 0;
    uint16_t ch_width = 0;
    uint16_t x = 0;
    uint16_t y = 0;

    char *buf = SCREEN_BUF(s);

    switch (SCREEN_DIRECTION(s))
    {
    case SCREEN_DIRECTION_HORIZONTAL:
        ch_height = 2 * SCREEN_H(s) / RC_CHANNELS_NUM;
        ch_width = SCREEN_W(s) / 2;
        y = SCREEN_H(s) - (ch_height * (RC_CHANNELS_NUM / 2));
        break;
    case SCREEN_DIRECTION_VERTICAL:
        ch_height = SCREEN_H(s) / RC_CHANNELS_NUM;
        ch_width = SCREEN_W(s);
        y = SCREEN_H(s) - (ch_height * RC_CHANNELS_NUM);
        break;
    }

    uint16_t sy = y;

    uint16_t ch_mw = 0;
    for (int ii = 0; ii < RC_CHANNELS_NUM; ii++)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "CH%02d", ii + 1);
        uint16_t cw = u8g2_GetStrWidth(&u8g2, buf);
        if (cw > ch_mw)
        {
            ch_mw = cw;
        }
    }

    ch_mw += 3;
    uint16_t bar_width = ch_width - ch_mw - 3;
    uint16_t bar_height = ch_height - 2;

    for (int ii = 0; ii < RC_CHANNELS_NUM; ii++)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "CH%02d", ii + 1);
        u8g2_DrawStr(&u8g2, x, y, buf);
        u8g2_DrawFrame(&u8g2, x + ch_mw, y + 1, bar_width, bar_height);
        float div = rc_data_get_channel_percentage(&s->internal.rc->data, ii) / 100.f;
        u8g2_DrawBox(&u8g2, x + ch_mw, y + 1, bar_width * div, bar_height);
        y += ch_height;
        if (ii == (RC_CHANNELS_NUM / 2) - 1 && ch_width < SCREEN_W(s))
        {
            x += ch_width;
            y = sy;
        }
    }
}

#define TELEMETRY_LINE_HEIGHT 12

static bool screen_display_telemetry(const telemetry_t *val, int id)
{
    return telemetry_has_value(val);
}

static unsigned screen_draw_telemetry_val(screen_t *s, const telemetry_t *val, int id, uint16_t y)
{
    char *buf = SCREEN_BUF(s);
    const char *name = telemetry_get_name(id);
    const char *value = telemetry_format(val, id, buf, SCREEN_DRAW_BUF_SIZE);
    uint16_t name_width = u8g2_GetStrWidth(&u8g2, name);
    uint16_t max_value_width = 0;
    uint16_t value_y_offset = 0;
    uint16_t value_indentation_vert = 0;
    uint16_t update_indicator_width = 10;
    unsigned total_height = 0;

    switch (SCREEN_DIRECTION(s))
    {
    case SCREEN_DIRECTION_HORIZONTAL:
        max_value_width = SCREEN_W(s) - name_width - update_indicator_width - 5;
        value_y_offset = 0;
        total_height = TELEMETRY_LINE_HEIGHT;
        break;
    case SCREEN_DIRECTION_VERTICAL:
        max_value_width = SCREEN_W(s) - value_indentation_vert;
        value_y_offset = TELEMETRY_LINE_HEIGHT;
        total_height = TELEMETRY_LINE_HEIGHT * 2 + 3;
        break;
    }

    uint16_t value_width = u8g2_GetStrWidth(&u8g2, value);
    int value_offset = screen_animation_offset(value_width, max_value_width, &value_width);
    uint16_t value_x = SCREEN_DIRECTION(s) == SCREEN_DIRECTION_HORIZONTAL ? SCREEN_W(s) - value_width - 1 : value_indentation_vert;
    u8g2_DrawStr(&u8g2, value_x - value_offset, y + value_y_offset, value);

    // Erase at the left of the value if needed
    if (value_offset > 0)
    {
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawBox(&u8g2, value_x - value_offset, y + value_y_offset, value_offset, TELEMETRY_LINE_HEIGHT);
        u8g2_SetDrawColor(&u8g2, 1);
    }

    u8g2_DrawStr(&u8g2, 0, y, name);
    // If the value was updated recently, draw an * next to
    // its value.
    if (data_state_get_last_update(&val->data_state) + MILLIS_TO_MICROS(200) > time_micros_now())
    {
        switch (SCREEN_DIRECTION(s))
        {
        case SCREEN_DIRECTION_HORIZONTAL:
            // Draw at the left of the value
            u8g2_DrawStr(&u8g2, value_x - update_indicator_width, y + 2, "*");
            break;
        case SCREEN_DIRECTION_VERTICAL:
            // Draw at right of the label, if possible. Otherwise draw at the
            // right of the value.
            {
                uint16_t indicator_x = SCREEN_W(s) - update_indicator_width;
                uint16_t offset = 0;
                if (name_width >= indicator_x)
                {
                    offset = value_y_offset;
                }
                u8g2_DrawStr(&u8g2, indicator_x, y + offset + 2, "*");
                break;
            }
        }
    }
    // Draw a separator if we're in vertical mode
    if (SCREEN_DIRECTION(s) == SCREEN_DIRECTION_VERTICAL)
    {
        uint16_t line_y = y + total_height - 2;
        u8g2_DrawLine(&u8g2, 15, line_y, SCREEN_W(s) - 30, line_y);
    }
    return total_height;
}

#define TELEMETRY_ITEMS_PER_PAGE 4

// Returns the number of telemetry items
static int telemetry_update_count(screen_t *s)
{
    int telemetry_count = telemetry_get_id_count();
    int displayed = 0;
    for (int ii = 0; ii < telemetry_count; ii++)
    {
        int id = telemetry_get_id_at(ii);
        telemetry_t *val = rc_data_get_telemetry(&s->internal.rc->data, id);
        if (screen_display_telemetry(val, id))
        {
            displayed++;
        }
    }
    s->internal.telemetry.count = (displayed + (TELEMETRY_ITEMS_PER_PAGE - 1)) / TELEMETRY_ITEMS_PER_PAGE;
    if (s->internal.telemetry.count < 1)
    {
        s->internal.telemetry.count = 1;
    }
    return telemetry_count;
}

static void screen_draw_telemetry(screen_t *s)
{
#define TELEMETRY_TITLE "Telemetry"
    int telemetry_title_height = 0;

    switch (SCREEN_DIRECTION(s))
    {
    case SCREEN_DIRECTION_HORIZONTAL:
        telemetry_title_height = 12;
        break;
    case SCREEN_DIRECTION_VERTICAL:
        telemetry_title_height = 24;
        break;
    }

    int telemetry_count = telemetry_update_count(s);

    // In case some telemetry items go away while looking at the telemetry
    // Also, if s->interal.telemetry.page is < 0, it means some place that tried
    // to initialize it to the last page didn't update the count first. Instead of
    // updating the count from several places that might not need to, do it here.
    if (s->internal.telemetry.page >= s->internal.telemetry.count - 1 || s->internal.telemetry.page < 0)
    {
        s->internal.telemetry.page = s->internal.telemetry.count - 1;
    }

    u8g2_SetFontPosTop(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    u8g2_DrawBox(&u8g2, 0, 0, SCREEN_W(s), telemetry_title_height + 1);
    uint16_t tw = u8g2_GetStrWidth(&u8g2, TELEMETRY_TITLE);
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawStr(&u8g2, (SCREEN_W(s) - tw) / 2, 1, TELEMETRY_TITLE);
    snprintf(SCREEN_BUF(s), SCREEN_DRAW_BUF_SIZE, "%d/%d",
             s->internal.telemetry.page + 1,
             s->internal.telemetry.count);
    uint16_t pw = u8g2_GetStrWidth(&u8g2, SCREEN_BUF(s));
    if (SCREEN_DIRECTION(s) == SCREEN_DIRECTION_HORIZONTAL)
    {
        // Right aligned
        u8g2_DrawStr(&u8g2, SCREEN_W(s) - pw - 2, 1, SCREEN_BUF(s));
    }
    else
    {
        // Centered
        u8g2_DrawStr(&u8g2, (SCREEN_W(s) - pw) / 2, 1 + telemetry_title_height / 2, SCREEN_BUF(s));
    }
    u8g2_SetDrawColor(&u8g2, 1);

    int start = s->internal.telemetry.page * TELEMETRY_ITEMS_PER_PAGE;
    int end = start + TELEMETRY_ITEMS_PER_PAGE;

    uint16_t y = telemetry_title_height + 1;

    for (int ii = start; ii < end; ii++)
    {
        int valid = 0;
        for (int jj = 0; jj < telemetry_count; jj++)
        {
            int id = telemetry_get_id_at(jj);
            telemetry_t *val = rc_data_get_telemetry(&s->internal.rc->data, id);
            if (screen_display_telemetry(val, id))
            {
                if (valid == ii)
                {
                    y += screen_draw_telemetry_val(s, val, id, y);
                    break;
                }
                valid++;
            }
        }
    }
}

static void screen_draw_menu(screen_t *s, menu_t *menu, uint16_t y)
{
#define MENU_LINE_HEIGHT 12
    int entries = menu_get_num_entries(menu);
    u8g2_SetFontPosTop(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    const char *prompt = menu_get_prompt(menu);
    if (prompt)
    {
        uint16_t pw = u8g2_GetStrWidth(&u8g2, prompt);
        uint16_t mpw = SCREEN_W(s) - 4;
        uint16_t px;
        if (pw <= mpw)
        {
            px = (SCREEN_W(s) - pw) / 2;
        }
        else
        {
            uint16_t po = screen_animation_offset(pw, mpw, &pw);
            px = 2 - po;
        }
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawStr(&u8g2, px, y + 2, prompt);
        u8g2_DrawFrame(&u8g2, 0, y, SCREEN_W(s), MENU_LINE_HEIGHT + 1);
        y += MENU_LINE_HEIGHT + 2;
    }
    // Check if we need to skip some entries to allow the selected entry
    // to be displayed.
    int start = 0;
    int selected = menu_get_entry_selected(menu);
    while (y + ((selected - start + 1) * MENU_LINE_HEIGHT) > SCREEN_H(s))
    {
        start++;
    }
    for (int ii = start; ii < entries; ii++)
    {
        const char *title = menu_entry_get_title(menu, ii, SCREEN_BUF(s), SCREEN_DRAW_BUF_SIZE);
        u8g2_SetDrawColor(&u8g2, 1);
        if (menu_is_entry_selected(menu, ii))
        {
            u8g2_DrawBox(&u8g2, 0, y, SCREEN_W(s), 12);
            u8g2_SetDrawColor(&u8g2, 0);
        }
        uint16_t tw = u8g2_GetStrWidth(&u8g2, title);
        uint16_t offset = screen_animation_offset(tw, SCREEN_W(s) - 2, &tw);
        u8g2_DrawStr(&u8g2, 0 - offset, y, title);
        y += MENU_LINE_HEIGHT;
    }
}

static void screen_format_bind_packet_name(air_bind_packet_t *packet, char *buf, size_t size)
{
    if (strlen(packet->name) > 0)
    {
        strlcpy(buf, packet->name, size);
    }
    else
    {
        air_addr_format(&packet->addr, buf, size);
    }
}

static void screen_draw_bind_request_from_tx(screen_t *s, air_bind_packet_t *packet)
{
    char name[AIR_MAX_NAME_LENGTH + 1];
    screen_format_bind_packet_name(packet, name, sizeof(name));

    snprintf(SCREEN_BUF(s), SCREEN_DRAW_BUF_SIZE, "Bind request from %s", name);
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    int height = screen_draw_multiline(SCREEN_BUF(s), 0, SCREEN_MULTILINEOPT_BOX);
    screen_draw_menu(s, &menu_bind_req, height + 1);
}

static void screen_draw_bind_request_info_from_rx(screen_t *s, air_bind_packet_t *packet)
{
    char name[AIR_MAX_NAME_LENGTH + 1];
    screen_format_bind_packet_name(packet, name, sizeof(name));

    snprintf(SCREEN_BUF(s), SCREEN_DRAW_BUF_SIZE, "%s is waiting for bind confirmation", name);
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    int top_height = screen_draw_multiline(SCREEN_BUF(s), 0, SCREEN_MULTILINEOPT_BOX);

    if ((packet->info.capabilities & AIR_CAP_SCREEN) && (packet->info.capabilities & AIR_CAP_BUTTON))
    {
        strlcpy(SCREEN_BUF(s), "Select 'Accept'", SCREEN_DRAW_BUF_SIZE);
    }
    else if (packet->info.capabilities & AIR_CAP_BUTTON)
    {
        strlcpy(SCREEN_BUF(s), "Press its button", SCREEN_DRAW_BUF_SIZE);
    }
    else
    {
        strlcpy(SCREEN_BUF(s), "Turn it off", SCREEN_DRAW_BUF_SIZE);
    }

    strlcat(SCREEN_BUF(s), " to bind it", SCREEN_DRAW_BUF_SIZE);
    screen_draw_multiline(SCREEN_BUF(s), top_height, SCREEN_MULTILINEOPT_NONE);
}

static void screen_draw_ota(screen_t *s)
{
    uint16_t tw;
    u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);
    u8g2_SetFontPosTop(&u8g2);
    screen_draw_label_value(s, NULL, "Updating...", s->internal.w, 0, 0);
    u8g2_DrawLine(&u8g2, 2, 16, s->internal.w - 4, 16);

    float progress = ota_progress_completion();

    uint16_t shm = s->internal.h / 2;
    uint16_t fx = 2;
    uint16_t fw = s->internal.w - 4;
    uint16_t fh = 6;
    uint16_t fy = shm - (fh / 2);
    if (s->internal.direction == SCREEN_DIRECTION_HORIZONTAL)
    {
        fy += 5;
    }
    u8g2_DrawFrame(&u8g2, fx, fy, fw, fh);
    if (progress > 0)
    {
        u8g2_DrawBox(&u8g2, fx, fy, fw * progress, fh);
    }

    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    if (progress >= 0)
    {
        snprintf(SCREEN_BUF(s), SCREEN_DRAW_BUF_SIZE, "%3d%%", (int)(100 * progress));
    }
    else
    {
        strlcpy(SCREEN_BUF(s), "---%", SCREEN_DRAW_BUF_SIZE);
    }
    tw = u8g2_GetStrWidth(&u8g2, SCREEN_BUF(s));
    u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, fy - 14, SCREEN_BUF(s));

    if (progress > 0)
    {
        strlcpy(SCREEN_BUF(s), "Remaining:", SCREEN_DRAW_BUF_SIZE);
        tw = u8g2_GetStrWidth(&u8g2, SCREEN_BUF(s));
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, fy + 9, SCREEN_BUF(s));

        int seconds = ota_estimated_time_remaining();
        snprintf(SCREEN_BUF(s), SCREEN_DRAW_BUF_SIZE, "%d:%02d", seconds / 60, seconds % 60);
        tw = u8g2_GetStrWidth(&u8g2, SCREEN_BUF(s));
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, fy + 21, SCREEN_BUF(s));
    }
}

static void screen_draw_frequencies(screen_t *s)
{
    air_freq_table_t freqs;
    size_t count = ARRAY_COUNT(freqs.freqs);

    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFontPosTop(&u8g2);

    if (!rc_get_frequencies_table(s->internal.rc, &freqs))
    {
        u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);
        screen_draw_label_value(s, NULL, "No frequencies table", s->internal.w, 0, 0);
        return;
    }

    u8g2_SetFont(&u8g2, u8g2_font_micro_tr);
    uint16_t fr_height = 0;
    uint16_t fr_width = 0;
    uint16_t x = 0;
    uint16_t y = 0;
    char *buf = SCREEN_BUF(s);

    switch (SCREEN_DIRECTION(s))
    {
    case SCREEN_DIRECTION_HORIZONTAL:
        fr_height = 2 * SCREEN_H(s) / count;
        fr_width = SCREEN_W(s) / 2;
        y = SCREEN_H(s) - (fr_height * (count / 2));
        break;
    case SCREEN_DIRECTION_VERTICAL:
        fr_height = SCREEN_H(s) / count;
        fr_width = SCREEN_W(s);
        y = SCREEN_H(s) - (fr_height * count);
        break;
    }

    uint16_t sy = y;

    uint16_t fr_mw = 0;
    for (int ii = 0; ii < count; ii++)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%02d", ii + 1);
        uint16_t fw = u8g2_GetStrWidth(&u8g2, buf);
        if (fw > fr_mw)
        {
            fr_mw = fw;
        }
    }

    fr_mw += 3;

    for (int ii = 0; ii < count; ii++)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%02d %.03fMHz", ii + 1, freqs.freqs[ii] / 1e6f);
        u8g2_DrawStr(&u8g2, x, y, buf);
        y += fr_height;
        if (ii == (count / 2) - 1 && fr_width < SCREEN_W(s))
        {
            x += fr_width;
            y = sy;
        }
    }
}

static void screen_draw_debug_info(screen_t *s)
{
    char *buf = SCREEN_BUF(s);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFontPosTop(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

    air_freq_table_t freqs;
    bool has_freqs = false;
    float freq_error = 0;
    float abs_freq_error = 0;
    if (rc_get_frequencies_table(s->internal.rc, &freqs))
    {
        for (int ii = 0; ii < ARRAY_COUNT(freqs.last_errors); ii++)
        {
            freq_error += fabsf(freqs.last_errors[ii]);
            float abs_error = fabsf(freqs.abs_errors[ii]);
            if (abs_error > fabsf(abs_freq_error))
            {
                abs_freq_error = freqs.abs_errors[ii];
            }
        }
        freq_error /= ARRAY_COUNT(freqs.last_errors);
        has_freqs = true;
    }

    uint16_t y = 0;
    if (has_freqs)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%.03fkHz", freq_error / 1e3f);
    }
    else
    {
        strncpy(buf, "---", SCREEN_DRAW_BUF_SIZE);
    }
    screen_draw_label_value(s, "Ferror:", buf, SCREEN_W(s), y, 3);
    y += 16;

    if (has_freqs)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%+.03fkHz", abs_freq_error / 1e3f);
    }
    else
    {
        strncpy(buf, "---", SCREEN_DRAW_BUF_SIZE);
    }
    screen_draw_label_value(s, "Abs. Ferror:", buf, SCREEN_W(s), y, 3);
    y += 16;

    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%.02f C", system_temperature());
    screen_draw_label_value(s, "Core Temp:", buf, SCREEN_W(s), y, 3);
}

static void screen_draw(screen_t *screen)
{
    menu_t *menu = menu_get_active();

    if (ota_is_in_progress())
    {
        if (menu != &menu_empty)
        {
            menu_set_active(&menu_empty);
        }
        screen_draw_ota(screen);
        return;
    }

    if (menu == &menu_empty)
    {
        menu_pop_active();
        menu = menu_get_active();
    }

    air_bind_packet_t packet;
    bool has_pending_bind_request = rc_has_pending_bind_request(screen->internal.rc, &packet);
    if (has_pending_bind_request && packet.role == AIR_ROLE_TX)
    {
        if (menu == &menu_bind_info)
        {
            menu_pop_active();
            menu = menu_get_active();
        }
        // We're an RX with a pending bind request from a TX.
        // Check that the bind req menu is on top, to handle
        // user interactions.
        if (menu != &menu_bind_req)
        {
            menu_set_active(&menu_bind_req);
        }
        screen_draw_bind_request_from_tx(screen, &packet);
        return;
    }

    if (has_pending_bind_request && packet.role == AIR_ROLE_RX_AWAITING_CONFIRMATION)
    {
        // We're a TX and we've been informed that there's an RX
        // waiting for bind confirmation.
        if (menu != &menu_bind_info)
        {
            menu_set_active(&menu_bind_info);
        }
        screen_draw_bind_request_info_from_rx(screen, &packet);
        return;
    }

    air_pairing_t alt_pairings[MENU_ALT_PAIRINGS_MAX];
    int alt_pairing_count = rc_get_alternative_pairings(screen->internal.rc, alt_pairings, ARRAY_COUNT(alt_pairings));
    if (alt_pairing_count > 0)
    {
        menu_set_alt_pairings(alt_pairings, alt_pairing_count);
        if (menu != &menu_alt_pairings)
        {
            menu_set_active(&menu_alt_pairings);
        }
        screen_draw_menu(screen, &menu_alt_pairings, 0);
        return;
    }

    // There's nothing overriding the screen, draw the normal interface
    while (menu == &menu_empty || menu == &menu_bind_req || menu == &menu_bind_info || menu == &menu_alt_pairings)
    {
        menu_pop_active();
        menu = menu_get_active();
    }
    if (menu != NULL && screen->internal.secondary_mode == SCREEN_SECONDARY_MODE_NONE)
    {
        screen_draw_menu(screen, menu, 0);
    }
    else
    {
        switch ((screen_secondary_mode_e)screen->internal.secondary_mode)
        {
        case SCREEN_SECONDARY_MODE_NONE:
            switch ((screen_main_mode_e)screen->internal.main_mode)
            {
            case SCREEN_MODE_MAIN:
                screen_draw_main(screen);
                break;
            case SCREEN_MODE_CHANNELS:
                screen_draw_channels(screen);
                break;
            case SCREEN_MODE_TELEMETRY:
                screen_draw_telemetry(screen);
                break;
            }
            break;
        case SCREEN_SECONDARY_MODE_FREQUENCIES:
            screen_draw_frequencies(screen);
            break;
        case SCREEN_SECONDARY_MODE_DEBUG_INFO:
            screen_draw_debug_info(screen);
            break;
        }
    }
}

void screen_update(screen_t *screen)
{
    if (screen->internal.splashing)
    {
        return;
    }

    char buf[SCREEN_DRAW_BUF_SIZE];

    screen->internal.w = u8g2_GetDisplayWidth(&u8g2);
    screen->internal.h = u8g2_GetDisplayHeight(&u8g2);
    screen->internal.direction = SCREEN_W(screen) > SCREEN_H(screen) ? SCREEN_DIRECTION_HORIZONTAL : SCREEN_DIRECTION_VERTICAL;
    screen->internal.buf = buf;
    u8g2_ClearBuffer(&u8g2);
    screen_draw(screen);
    u8g2_SendBuffer(&u8g2);
}

#endif