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

#ifndef __LED_WIDGET_H__
#define __LED_WIDGET_H__

#include <stdbool.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "color_format.h"

enum led_id
{
    LED_LIGHT,
    LED_STATUS
};

enum led_color
{
    LED_RED,
    LED_GREEN,
    LED_BLUE
};

struct led_widget
{
    enum led_id led;
    int on;
    int off;
    bool color; /* true: color, false: white-only */
    RgbColor_t rgb;
    uint8_t level; /* 0-254 */
    uint8_t temp;  /* 0-100, % of cool white */
    bool state;
    TimerHandle_t timer;
};

/* Bind this LEDWidget with the specified LED */
void led_widget_init(struct led_widget *lw, enum led_id led);
/* Retrieve the name of this LED */
const char * led_widget_name(struct led_widget *lw);
/* Change the mode */
void led_widget_mode(struct led_widget *lw, bool color);
/* Change the color */
void led_widget_color(struct led_widget *lw, RgbColor_t color);
/* Specify the ON, OFF duration */
void led_widget_blink_on_off(struct led_widget *lw, int on, int off);
/* Specify evenly ON and OFF both to 'duration */
void led_widget_blink(struct led_widget *lw, int duration);

/* Set to ON or OFF */
void led_widget_set(struct led_widget *lw, bool state);
/* Get On/Off state */
bool led_widget_get(struct led_widget *lw);
/* Change light level */
void led_widget_set_level(struct led_widget *lw, uint8_t level);
/* Get current level*/
uint8_t led_widget_get_level(struct led_widget *lw);
/* Change light temperature */
void led_widget_set_temp(struct led_widget *lw, uint8_t temp);
/* Get current temperature*/
uint8_t led_widget_get_temp(struct led_widget *lw);

void led_widget_toggle(struct led_widget *lw);
void led_widget_do_set(struct led_widget *lw, bool state);

static void TimerHandler(TimerHandle_t xTimer);
void led_widget_do_blink(struct led_widget *lw);
void led_widget_start_timer(struct led_widget *lw, uint32_t timeout_ms);
void led_widget_cancel_timer(struct led_widget *lw);

#endif
