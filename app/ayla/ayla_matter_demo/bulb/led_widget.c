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

#include "bp5758d.h"

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
        bp5758d_set_standby(!state);
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

    r = (rgb.r * (bp5758d_get_max_level() - bp5758d_get_min_level())) / 255;
    g = (rgb.g * (bp5758d_get_max_level() - bp5758d_get_min_level())) / 255;
    b = (rgb.b * (bp5758d_get_max_level() - bp5758d_get_min_level())) / 255;

    switch (lw->led)
    {
    case LED_LIGHT:
        bp5758d_set_rgbcw_channel(r, g, b, 0, 0);
        break;
    case LED_STATUS:
        break;
    }
}

static void led_widget_control_white(struct led_widget *lw)
{
    uint32_t target = 0;
    uint16_t cool, warm;

    target = (lw->level == 0) ? 0 :\
             (((bp5758d_get_max_level() - bp5758d_get_min_level()) * lw->level) / 254);
    cool = (uint16_t)((target * lw->temp) / 100);
    warm = (uint16_t)((target * (100 - lw->temp)) / 100);

    if (lw->led == LED_LIGHT)
    {
        bp5758d_set_rgbcw_channel(0, 0, 0, cool, warm);
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
        u8 r = lw->rgb.r;
        u8 g = lw->rgb.g;
        u8 b = lw->rgb.b;

        r = (r * level) / lw->level;
        g = (g * level) / lw->level;
        b = (b * level) / lw->level;

        lw->rgb.r = r;
        lw->rgb.g = g;
        lw->rgb.b = b;
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
