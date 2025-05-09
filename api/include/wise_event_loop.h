/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.
 */
// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Originated from esp_event_loop.h of ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and modified to provide wise Wi-Fi API as being ESP8266 style
 */

#ifndef __WISE_EVENT_LOOP_H__
#define __WISE_EVENT_LOOP_H__

#include <stdint.h>
#include <stdbool.h>

#include "wise_err.h"
#include "wise_event.h"
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  Application specified event callback function
  *
  * @param  void *ctx : reserved for user
  * @param  system_event_t *event : event type defined in this file
  *
  * @return WISE_OK : succeed
  * @return others : fail
  */
typedef wise_err_t (*system_event_cb_t)(void *ctx, system_event_t *event);
/**
  * @brief  Deinitialize event loop
  *
  * @return WISE_OK : succeed
  * @return others : fail
  */
wise_err_t wise_event_loop_deinit(void);


/**
  * @brief  get event loop Initialize flag
  *
  * @return s_event_init_flag
  */
bool wise_event_get_init_flag(void);

/**
  * @brief  Initialize event loop
  *         Create the event handler and task
  *
  * @param  system_event_cb_t cb : application specified event callback, it can be modified by call wise_event_set_cb
  * @param  void *ctx : reserved for user
  *
  * @return WISE_OK : succeed
  * @return others : fail
  */
wise_err_t wise_event_loop_init(system_event_cb_t cb, void *ctx);

/**
  * @brief  Set application specified event callback function
  *
  * @attention 1. If cb is NULL, means application don't need to handle
  *               If cb is not NULL, it will be call when an event is received, after the default event callback is completed
  *
  * @param  system_event_cb_t cb : callback
  * @param  void *ctx : reserved for user
  *
  * @return system_event_cb_t : old callback
  */
system_event_cb_t wise_event_loop_set_cb(system_event_cb_t cb, void *ctx);

/**
  * @brief  Get the queue used by event loop
  *
  * @attention : currently this API is used to initialize "q" parameter
  * of wifi_init structure.
  *
  * @return QueueHandle_t : event queue handle
  */
osMessageQueueId_t wise_event_loop_get_queue(void);


#ifdef __cplusplus
}
#endif

#endif /* __WISE_EVENT_LOOP_H__ */
