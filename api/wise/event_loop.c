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
 * Inspired by ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and will provide wise Wi-Fi API as being ESP8266 style
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cmsis_os.h"

#include "lwip/netif.h"
#include "lwip/netifapi.h"

#include "wise_err.h"
#include "wise_log.h"
#include "wise_wifi.h"
#include "wise_event.h"
#include "wise_event_loop.h"

static const char* TAG __maybe_unused = "event";
static bool s_event_init_flag = false;
static osMessageQueueId_t s_event_queue = NULL;
static system_event_cb_t s_event_handler_cb = NULL;
static void *s_event_ctx = NULL;
static osThreadId_t *s_event_thread = NULL;

static wise_err_t wise_event_post_to_user(system_event_t *event)
{
	if (s_event_handler_cb)
		return (*s_event_handler_cb)(s_event_ctx, event);

	return WISE_OK;
}

static void wise_event_loop_task(void *pvParameters)
{
	while (s_event_init_flag) {
	    system_event_t evt;
	    if (osMessageQueueGet(s_event_queue, &evt, NULL, osWaitForever) == osOK) {
		    wise_err_t ret = wise_event_process_default(&evt);
		    if (ret != WISE_OK)
			    WISE_LOGE(TAG, "default event handler failed!");
		    ret = wise_event_post_to_user(&evt);
		    if (ret != WISE_OK)
			    WISE_LOGE(TAG, "post event to user fail!");
	    }
	}
}

system_event_cb_t wise_event_loop_set_cb(system_event_cb_t cb, void *ctx)
{
	system_event_cb_t old_cb = s_event_handler_cb;
	s_event_handler_cb = cb;
	s_event_ctx = ctx;
	return old_cb;
}

wise_err_t wise_event_send(system_event_t *event)
{
	if (s_event_queue == NULL) {
		WISE_LOGE(TAG, "Event loop not initialized via wise_event_loop_init, "
				"but wise_event_send called");
		return WISE_ERR_INVALID_STATE;
	}

	osStatus_t ret = osMessageQueuePut(s_event_queue, event, 0, 0);
	if (ret != osOK) {
		if (event)
			WISE_LOGE(TAG, "e=%d f", event->event_id);
		else
			WISE_LOGE(TAG, "e null");
		return WISE_FAIL;
	}
	return WISE_OK;
}

osMessageQueueId_t wise_event_loop_get_queue(void)
{
	return s_event_queue;
}

bool wise_event_get_init_flag(void) {
	return s_event_init_flag;
}

wise_err_t wise_event_loop_init(system_event_cb_t cb, void *ctx)
{
	osThreadAttr_t attr = {
		.name = "wise_event_loop_task",
		.stack_size = 3 * 1024,
		.priority = osPriorityNormal,
	};

	if (s_event_init_flag)
		return WISE_FAIL;

	s_event_init_flag = true;

	s_event_queue = osMessageQueueNew(16, sizeof(system_event_t), NULL);
	if (s_event_queue == NULL)
		return WISE_ERR_NO_MEM;

	s_event_thread = osThreadNew(wise_event_loop_task, NULL, &attr);

	if (s_event_thread == NULL)
		return WISE_ERR_NO_MEM;

	s_event_handler_cb = cb;
	s_event_ctx = ctx;

	return WISE_OK;
}

wise_err_t wise_event_loop_deinit(void)
{
	if (!s_event_init_flag)
		return WISE_FAIL;

	if (s_event_thread)
		osThreadTerminate(s_event_thread);

	if (s_event_queue)
		osMessageQueueDelete(s_event_queue);

	s_event_queue = NULL;
	s_event_handler_cb = NULL;
	s_event_ctx = NULL;
	s_event_init_flag = false;

	return WISE_OK;
}
