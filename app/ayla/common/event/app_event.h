/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2018 Nest Labs, Inc.
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

#ifndef __APP_EVENT_H__
#define __APP_EVENT_H__

#include "FreeRTOS.h"

#define APP_EVENT_QUEUE_SIZE 10

struct app_event;
typedef void (*event_handler)(struct app_event *);

enum app_light_action
{
    kLightAction_On,
    kLightAction_Off,
    kLightAction_Mode,
    kLightAction_Mode2, /* Mode + set light bulb */
    kLightAction_Level,
    kLightAction_Temp,
    kLightAction_Color,
};

enum app_event_types
{
    kEventType_Button = 0,
    kEventType_Timer,
    kEventType_Light,
    kEventType_Install,
    kEventType_Occupancy,
    kEventType_Max,
};

typedef struct
{
    bool pressed;
} button_event_t;

typedef struct
{
    void *context;
    uint32_t value;
} timer_event_t;

typedef struct
{
    uint8_t action;
    int32_t actor;
    uint32_t value;
} light_event_t;

typedef struct
{
    void (*callback)(uint32_t);
    uint32_t arg;
} install_event_t;

typedef struct
{
    bool present;
} occupancy_event_t;

struct app_event
{
    uint16_t type;

    union
    {
        button_event_t button_event;
        timer_event_t timer_event;
        light_event_t light_event;
        install_event_t install_event;
        occupancy_event_t occupancy_event;
    };
};

extern xQueueHandle g_app_event_queue;

extern void app_event_init(void);
extern void app_event_install_handler(enum app_event_types type, event_handler fn);
extern void app_event_post(const struct app_event * event);
extern void app_event_dispatch(struct app_event * event);

#endif
