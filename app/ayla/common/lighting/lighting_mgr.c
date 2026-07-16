/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
 *    All rights reserved.
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

#include <FreeRTOS.h>
#include <queue.h>

#include <ayla/log.h>

#include "lighting_mgr.h"
#include "color_format.h"

#include "led_widget.h"

#define ACTUATOR_MOVEMENT_PERIOS_MS (10)

struct led_widget g_led = {0, };

struct lighting_mgr g_lm = {0,};

TimerHandle_t g_light_timer = NULL;

struct lighting_mgr *LightMgr(void)
{
    return &g_lm;
}

int lighting_mgr_init(struct lighting_mgr *lm)
{
    // Create FreeRTOS sw timer for light timer.
    g_light_timer = xTimerCreate("lightTmr",       // Just a text name, not used by the RTOS kernel
                               1,                // == default timer period (mS)
                               false,            // no timer reload (==one-shot)
                               (void *) lm,    // init timer id = light obj context
                               lm_timer_event_handler // timer callback handler
    );

    if (g_light_timer == NULL)
    {
        log_put(LOG_ERR "sLightTimer timer create failed");
        return -1;
    }

    lm->state = kState_OffCompleted;
    lm->auto_turn_off_timer_armed = false;
    lm->auto_turn_off           = false;
    lm->auto_turn_off_duration   = 0;

    led_widget_init(&g_led, LED_LIGHT);

    return 0;
}

void lighting_mgr_set_callbacks(struct lighting_mgr *lm, callback_fn_initiated action_initiated_cb,
        callback_fn_completed action_completed_cb)
{
    lm->action_initiated_cb = action_initiated_cb;
    lm->action_completed_cb = action_completed_cb;
}

bool lighting_mgr_is_action_in_progress(struct lighting_mgr *lm)
{
    return (lm->state == kState_OffInitiated || lm->state == kState_OnInitiated);
}

bool lighting_mgr_is_light_on(struct lighting_mgr *lm)
{
    return (lm->state == kState_OnCompleted);
}

bool lighting_mgr_is_light_off(struct lighting_mgr *lm)
{
    return (lm->state == kState_OffCompleted);
}

void lighting_mgr_enable_auto_turn_off(struct lighting_mgr *lm, bool on)
{
    lm->auto_turn_off = on;
}

void lighting_mgr_set_auto_turn_off_duration(struct lighting_mgr *lm, uint32_t duration_sec)
{
    lm->auto_turn_off_duration = duration_sec;
}

bool lighting_mgr_initiate_action(struct lighting_mgr *lm, int32_t actor, Action_t action, uint8_t * value)
{
    bool action_initiated = false;
    State_t new_state;
    RgbColor_t rgb;

    if (g_light_timer == NULL)
        return action_initiated;

    log_put(LOG_DEBUG "[%s, %d] action: %d, state: %d", __func__, __LINE__, action, lm->state);

    switch (action)
    {
    case ON_ACTION:
        if (lm->state == kState_OffCompleted)
        {
            action_initiated = true;
            new_state        = kState_OnInitiated;
        }
        if (*value == 0)
            break;
    case LEVEL_ACTION:
        if (lm->state == kState_OnCompleted)
        {
            action_initiated = true;
#if 0 /* XXX: make things complicated. */
            /**
             * NEST HUB uses minimal as off state.
             */
            new_state = (*value == 0 ? kState_OffInitiated : kState_LevelInitiated);
#else
            new_state = kState_LevelInitiated;
#endif
        }
        break;
    case OFF_ACTION:
        if (lm->state == kState_OnCompleted)
        {
            action_initiated = true;
            new_state        = kState_OffInitiated;
        }
        break;
    case MODE_ACTION:
        if (lm->state == kState_OnCompleted)
        {
            action_initiated = true;
            new_state = kState_ModeInitiated;
        }
        break;
    case TEMP_ACTION:
        if (lm->state == kState_OnCompleted)
        {
            action_initiated = true;
            new_state = kState_TempInitiated;
        }
        break;
    case COLOR_ACTION: {
        rgb = *(RgbColor_t *)(value);
    }
    break;
    case COLOR_ACTION_XY: {
        XyColor_t xy = *(XyColor_t *)(value);
        rgb          = XYToRgb(led_widget_get_level(&g_led), xy.x, xy.y);
        log_put(LOG_DEBUG "LightingManager XY(%d,%d) to RGB(%d,%d,%d)", xy.x, xy.y, rgb.r, rgb.g, rgb.b);
    }
    break;
    case COLOR_ACTION_HSV: {
        HsvColor_t hsv = *(HsvColor_t *)(value);
        hsv.v          = led_widget_get_level(&g_led);
        rgb            = HsvToRgb(hsv);
        log_put(LOG_DEBUG "LightingManager HSV(%d,%d) to RGB(%d,%d,%d)", hsv.h, hsv.s, rgb.r, rgb.g, rgb.b);
    }
    break;
    case COLOR_ACTION_CT: {
        CtColor_t ct;
        ct.ctMireds = *(uint16_t *)(value);
        rgb         = CTToRgb(ct);
        log_put(LOG_DEBUG "LightingManager CT(%d) to RGB(%d,%d,%d)", ct.ctMireds, rgb.r, rgb.g, rgb.b);
    }
    break;
    default:
        log_put(LOG_ERR "LightMgr:Unknown");
        break;
    }

    if (action == COLOR_ACTION_XY || action == COLOR_ACTION_HSV || action == COLOR_ACTION_CT || action == COLOR_ACTION)
    {
        new_state        = kState_ColorInitiated;
        action_initiated = true;
        action           = COLOR_ACTION;
    }

    if (action_initiated)
    {
        if (lm->auto_turn_off_timer_armed && new_state == kState_OffInitiated)
        {
            // If auto turn off timer has been armed and someone initiates turning off,
            // cancel the timer and continue as normal.
            lm->auto_turn_off_timer_armed = false;

            lighting_mgr_cancel_timer(lm);
        }

        lighting_mgr_start_timer(lm, ACTUATOR_MOVEMENT_PERIOS_MS);

        // Since the timer started successfully, update the state and trigger callback
        lm->state = new_state;

        if (lm->action_initiated_cb)
        {
            lm->action_initiated_cb(action, actor);
        }

        if (lm->state == kState_OnInitiated)
        {
            log_put(LOG_DEBUG "LightingManager: turn on");
            led_widget_set(&g_led, true);
        }
        else if (lm->state == kState_OffInitiated)
        {
            log_put(LOG_DEBUG "LightingManager: turn off");
            led_widget_set(&g_led, false);
        }
        else if (lm->state == kState_ModeInitiated)
        {
            log_put(LOG_DEBUG "LightingManager: set mode %d", *value);
            led_widget_mode(&g_led, *value != 0);
        }
        else if (lm->state == kState_TempInitiated)
        {
            log_put(LOG_DEBUG "LightingManager: set temp %d", *value);
            led_widget_set_temp(&g_led, *value);
        }
        else if (lm->state == kState_LevelInitiated)
        {
            log_put(LOG_DEBUG "LightingManager: set level %d", *value);
            led_widget_set_level(&g_led, *value);
        }
        else if (lm->state == kState_ColorInitiated)
        {
            log_put(LOG_DEBUG "LightingManager: set color(%d,%d,%d)", rgb.r, rgb.g, rgb.b);
            led_widget_color(&g_led, rgb);
        }
    }

    return action_initiated;
}

void lighting_mgr_start_timer(struct lighting_mgr *lm, uint32_t timeout_ms)
{
    if (g_light_timer == NULL)
        return;

    if (xTimerIsTimerActive(g_light_timer))
    {
        log_put(LOG_WARN "app timer already started!");
        lighting_mgr_cancel_timer(lm);
    }

    // timer is not active, change its period to required value (== restart).
    // FreeRTOS- Block for a maximum of 100 ticks if the change period command
    // cannot immediately be sent to the timer command queue.
    if (xTimerChangePeriod(g_light_timer, (timeout_ms / portTICK_PERIOD_MS), 100) != pdPASS)
    {
        log_put(LOG_ERR "sLightTimer timer start() failed");
    }
}

void lighting_mgr_cancel_timer(struct lighting_mgr *lm)
{
    if (xTimerStop(g_light_timer, 0) == pdFAIL)
    {
        log_put(LOG_ERR "sLightTimer stop() failed");
    }
}

/* XXX: state change must be done asynchronously, i.e., in the timer task context
 *      because the app will wait for completion from its message loop.
 */
#if 0

void lm_timer_event_handler(TimerHandle_t xTimer)
{
    struct lighting_mgr * light = (struct lighting_mgr *)(pvTimerGetTimerID(xTimer));

    /* The timer event handler will be called in the context of the timer task
     * once g_light_timer expires. Post an event to the app queue with the actual Handler
     * so that the event can be handled in the context of the app task.
     */
    struct app_event event;
    event.type               = kEventType_Timer;
    event.timer_event.context = light;
    if (light->auto_turn_off_timer_armed)
    {
        event.handler = lm_auto_turn_off_timer_event_handler;
    }
    else
    {
        event.handler = lm_actuator_movement_timer_event_handler;
    }
#if 0
    app_post_event(&event);
#else
    event.handler(&event);
#endif
}

#else

void lm_timer_event_handler(TimerHandle_t xTimer)
{
    struct lighting_mgr * light = (struct lighting_mgr *)(pvTimerGetTimerID(xTimer));

    event_handler handler;
    struct app_event event;
    event.type               = kEventType_Timer;
    event.timer_event.context = light;
    if (light->auto_turn_off_timer_armed)
    {
        handler = lm_auto_turn_off_timer_event_handler;
    }
    else
    {
        handler = lm_actuator_movement_timer_event_handler;
    }

    handler(&event);
}

#endif

void lm_auto_turn_off_timer_event_handler(struct app_event * event)
{
    struct lighting_mgr * light = (struct lighting_mgr *)(event->timer_event.context);
    int32_t actor           = 0;

    // Make sure auto turn off timer is still armed.
    if (!light->auto_turn_off_timer_armed)
    {
        return;
    }

    light->auto_turn_off_timer_armed = false;

    log_put(LOG_DEBUG "Auto Turn Off has been triggered!");

    lighting_mgr_initiate_action(light, actor, OFF_ACTION, 0);
}

void lm_actuator_movement_timer_event_handler(struct app_event * event)
{
    Action_t actionCompleted = INVALID_ACTION;

    struct lighting_mgr * light = (struct lighting_mgr *)(event->timer_event.context);

    if (light->state == kState_OffInitiated)
    {
        light->state   = kState_OffCompleted;
        actionCompleted = OFF_ACTION;
    }
    else if (light->state == kState_OnInitiated)
    {
        light->state   = kState_OnCompleted;
        actionCompleted = ON_ACTION;
    }
    else if (light->state == kState_LevelInitiated)
    {
        light->state   = kState_OnCompleted;
        actionCompleted = LEVEL_ACTION;
    }
    else if (light->state == kState_ModeInitiated)
    {
        light->state   = kState_OnCompleted;
        actionCompleted = MODE_ACTION;
    }
    else if (light->state == kState_TempInitiated)
    {
        light->state   = kState_OnCompleted;
        actionCompleted = TEMP_ACTION;
    }
    else if (light->state == kState_ColorInitiated)
    {
        light->state   = kState_OnCompleted;
        actionCompleted = COLOR_ACTION;
    }

    if (actionCompleted != INVALID_ACTION)
    {
        if (light->action_completed_cb)
        {
            light->action_completed_cb(actionCompleted);
        }

        if (light->auto_turn_off &&
            (actionCompleted == ON_ACTION || actionCompleted == LEVEL_ACTION || actionCompleted == COLOR_ACTION
             || actionCompleted == MODE_ACTION || actionCompleted == TEMP_ACTION))
        {
            // Start the timer for auto turn off
            lighting_mgr_start_timer(light, light->auto_turn_off_duration * 1000);

            light->auto_turn_off_timer_armed = true;

            log_put(LOG_DEBUG "Auto Turn off enabled. Will be triggered in %lu seconds", light->auto_turn_off_duration);
        }
    }
}
