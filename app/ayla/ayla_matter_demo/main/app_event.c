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

void app_init_event(void)
{
	g_app_event_queue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(struct app_event));
	AYLA_ASSERT(g_app_event_queue != NULL);
}

void app_post_event(const struct app_event * event)
{
    BaseType_t status;

    status = xQueueSend(g_app_event_queue, event, 1);
    if (!status) {
        log_put(LOG_ERR "Failed to post event to app task event queue");
    }
}

void app_dispatch_event(struct app_event * event)
{
    if (event->handler) {
        event->handler(event);
    } else {
        log_put(LOG_WARN "Event received with no handler. Dropping event.");
    }
}

