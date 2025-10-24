/*
 *
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

#ifndef __LIGHTING_MGR_H__
#define __LIGHTING_MGR_H__

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "timers.h" // provides FreeRTOS timer support

#include "app_event.h"

typedef enum Action_e
{
    ON_ACTION = 0,
    OFF_ACTION,
    MODE_ACTION,
    LEVEL_ACTION,
    TEMP_ACTION,
    COLOR_ACTION,
    COLOR_ACTION_XY,
    COLOR_ACTION_HSV,
    COLOR_ACTION_CT,
    OCCUPANCY_PRESENT_ACTION,
    OCCUPANCY_CLEAR_ACTION,

    INVALID_ACTION
} Action_t;

typedef enum State_e
{
    kState_OnInitiated = 0,
    kState_OnCompleted,
    kState_OffInitiated,
    kState_OffCompleted,
    kState_LevelInitiated,
    kState_ModeInitiated,
    kState_TempInitiated,
    kState_ColorInitiated
} State_t;

typedef void (*callback_fn_initiated)(Action_t, int32_t actor);
typedef void (*callback_fn_completed)(Action_t);

struct lighting_mgr
{
    State_t state;
    bool auto_turn_off;
    uint32_t auto_turn_off_duration;
    bool auto_turn_off_timer_armed;
    callback_fn_initiated action_initiated_cb;
    callback_fn_completed action_completed_cb;
};

int lighting_mgr_init(struct lighting_mgr *lm);
bool lighting_mgr_is_light_on(struct lighting_mgr *lm);
bool lighting_mgr_is_action_in_progress(struct lighting_mgr *lm);
bool lighting_mgr_initiate_action(struct lighting_mgr *lm, int32_t actor,
        Action_t action, uint8_t * value);
void lighting_mgr_enable_auto_turn_off(struct lighting_mgr *lm, bool on);
void lighting_mgr_set_auto_turn_off_uuration(struct lighting_mgr *lm, uint32_t duration_sec);
void lighting_mgr_set_callbacks(struct lighting_mgr *lm, callback_fn_initiated action_initiated_cb,
        callback_fn_completed action_completed_cb);

struct lighting_mgr *LightMgr(void);

void lighting_mgr_cancel_timer(struct lighting_mgr *lm);
void lighting_mgr_start_timer(struct lighting_mgr *lm, uint32_t timeout_ms);

static void lm_timer_event_handler(TimerHandle_t xTimer);
static void lm_auto_turn_off_timer_event_handler(struct app_event * event);
static void lm_actuator_movement_timer_event_handler(struct app_event * event);

#endif
