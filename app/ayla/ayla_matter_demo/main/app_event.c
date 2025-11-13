/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <FreeRTOS.h>
#include <queue.h>

#include <ayla/assert.h>
#include <ayla/log.h>

#include "app_event.h"

xQueueHandle g_app_event_queue = (xQueueHandle)NULL;

static event_handler evt_handler[kEventType_Max] = {0,};

void app_event_init(void)
{
	g_app_event_queue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(struct app_event));
	AYLA_ASSERT(g_app_event_queue != NULL);
}

void app_event_install_handler(enum app_event_types type, event_handler fn)
{
    if (type >= kEventType_Max) {
        log_put(LOG_ERR "Invalid type");
        return;
    }

    evt_handler[type] = fn;
}

void app_event_post(const struct app_event * event)
{
    BaseType_t status;

    status = xQueueSend(g_app_event_queue, event, 1);
    if (!status) {
        log_put(LOG_ERR "Failed to post event to app task event queue");
    }
}

void app_event_dispatch(struct app_event * event)
{
    if (evt_handler[event->type]) {
        evt_handler[event->type](event);
    } else {
        log_put(LOG_WARN "Event received with no handler. Dropping event.");
    }
}

