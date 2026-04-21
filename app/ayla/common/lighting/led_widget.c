/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <assert.h>
#include <stdio.h>

#include <ayla/log.h>

#include "led_widget.h"

// #include "bp5758d.h"
#include "bp1638cj.h"

void TimerHandler(TimerHandle_t xTimer)
{
    struct led_widget * lw = (struct led_widget *) pvTimerGetTimerID(xTimer);
    led_widget_toggle(lw);
}

void led_widget_start_timer(struct led_widget *lw, uint32_t timeout_ms)
{
    if (xTimerIsTimerActive(lw->timer))
        led_widget_cancel_timer(lw);

    assert(xTimerChangePeriod(lw->timer, timeout_ms / portTICK_PERIOD_MS, 100) == pdPASS);
}

void led_widget_cancel_timer(struct led_widget *lw)
{
    assert(xTimerStop(lw->timer, 0) == pdPASS);
}

void led_widget_init(struct led_widget *lw, enum led_id led)
{
    lw->led   = led;
    lw->level = 0;
    lw->color = false;

    lw->timer = xTimerCreate(led_widget_name(lw),
                          1,             // == default timer period (mS)
                          false,         // no timer reload (==one-shot)
                          (void *) lw, // init timer id = app task obj context
                          TimerHandler); // timer callback handler
}

void led_widget_mode(struct led_widget *lw, bool color)
{
    lw->color = color;
}

const char * led_widget_name(struct led_widget *lw)
{
    const char * name;

    switch (lw->led)
    {
    case LED_LIGHT:
        name = "LED_LIGHT";
        break;
    case LED_STATUS:
        name = "LED_STATUS";
        break;
    default:
        name = "LED UNKNOWN";
        break;
    }

    return name;
}

void led_widget_toggle(struct led_widget *lw)
{
    led_widget_set(lw, !lw->state);
    led_widget_start_timer(lw, lw->state ? lw->on : lw->off);
}

void led_widget_do_blink(struct led_widget *lw)
{
    led_widget_start_timer(lw, lw->on);
}

void led_widget_do_set(struct led_widget *lw, bool state)
{
    switch (lw->led)
    {
    case LED_LIGHT:
#if 0
        bp5758d_set_standby(!state);
#else
        /*  TODO: Only control on/off */
        bp1638cj_set_standby(!state);
#endif
        break;
    case LED_STATUS:
#ifdef __no_stub__
        filogic_led_status_toggle(state);
#endif /* __no_stub__ */
        break;
    }
    lw->state = state;
}

void led_widget_set(struct led_widget *lw, bool state)
{
    led_widget_cancel_timer(lw);
    led_widget_do_set(lw, state);
    log_put(LOG_DEBUG "%s %s\n", led_widget_name(lw), state ? "on" : "off");
}

bool led_widget_get(struct led_widget *lw)
{
    return lw->state;
}

void led_widget_blink_on_off(struct led_widget *lw, int on, int off)
{
    if (lw->on != on || lw->off != off)
    {
        lw->on  = on;
        lw->off = off;
        log_put(LOG_DEBUG "%s blink: on %d off %d\n", led_widget_name(lw), lw->on, lw->off);
        led_widget_do_blink(lw);
    }
}

void led_widget_blink(struct led_widget *lw, int duration)
{
    led_widget_blink_on_off(lw, duration, duration);
}

void led_widget_color(struct led_widget *lw, RgbColor_t rgb)
{
    uint32_t r, g, b;
    HsvColor_t hsv;

    lw->rgb = rgb;

    if (lw->color == false) {
        return;
    }

    hsv = RgbToHsv(rgb.r, rgb.g, rgb.b);
    lw->level = hsv.v;

#if 0
    r = (rgb.r * (bp5758d_get_max_level() - bp5758d_get_min_level())) / 255;
    g = (rgb.g * (bp5758d_get_max_level() - bp5758d_get_min_level())) / 255;
    b = (rgb.b * (bp5758d_get_max_level() - bp5758d_get_min_level())) / 255;
#else
    /* no need to convert if range in [0, 255] */
    // r = (rgb.r * (bp1638cj_get_max_level() - bp1638cj_get_min_level())) / 255;
    // g = (rgb.g * (bp1638cj_get_max_level() - bp1638cj_get_min_level())) / 255;
    // b = (rgb.b * (bp1638cj_get_max_level() - bp1638cj_get_min_level())) / 255;
    r = rgb.r;
    g = rgb.g;
    b = rgb.b;
#endif

    switch (lw->led)
    {
    case LED_LIGHT:
#if 0
        bp5758d_set_rgbcw_channel(r, g, b, 0, 0);
#else
        bp1638cj_set_rgbcw_channel(r, g, b, 0, 0);
#endif
        break;
    case LED_STATUS:
        break;
    }
}

static void led_widget_control_white(struct led_widget *lw)
{
#if 0
    uint32_t target = 0;
    uint16_t cool, warm;
    target = (lw->level == 0) ? 0 :\
             (((bp5758d_get_max_level() - bp5758d_get_min_level()) * lw->level) / 254);
    cool = (uint16_t)((target * lw->temp) / 100);
    warm = (uint16_t)((target * (100 - lw->temp)) / 100);
#else
    /* For bp1638cj, in white mode: two channel - temp & level. */
    uint16_t temp_255;
#endif


    if (lw->led == LED_LIGHT)
    {
#if 0
        bp5758d_set_rgbcw_channel(0, 0, 0, cool, warm);
#else
        /* temp in [0,100], level in [0, 254] */
        temp_255 = lw->temp * 255 / 100;
        bp1638cj_set_rgbcw_channel(0, 0, 0, temp_255, lw->level);
#endif
    }
    else if (lw->led == LED_STATUS)
    {
    }
    else
    {
        assert(0);
    }
}

void led_widget_set_level(struct led_widget *lw, uint8_t level)
{
    if (lw->color == false) {
        lw->level = level;
        led_widget_control_white(lw);
    } else {
        // For matter, level means hsv.v
        // 1. use RGB convert to HSV
        // 2. use same hsv.h, hsv.s, level convert to RGB
        HsvColor_t cur_hsv;
        cur_hsv = RgbToHsv(lw->rgb.r, lw->rgb.g, lw->rgb.b);
        cur_hsv.v = level;
        printf("Old RGB: %u|%u|%u.\n", lw->rgb.r, lw->rgb.g, lw->rgb.b);
        lw->rgb = HsvToRgb(cur_hsv);
        printf("NEW RGB: %u|%u|%u.\n", lw->rgb.r, lw->rgb.g, lw->rgb.b);
        lw->level = level;
        led_widget_color(lw, lw->rgb);
    }
}

uint8_t led_widget_get_level(struct led_widget *lw)
{
    return lw->level;
}

void led_widget_set_temp(struct led_widget *lw, uint8_t temp)
{
    if (temp <= 100)
    {
        lw->temp = temp;
    }
    else
    {
        return;
    }

    if (lw->color == false) {
        led_widget_control_white(lw);
    }
}


uint8_t led_widget_get_temp(struct led_widget *lw)
{
    return lw->temp;
}
