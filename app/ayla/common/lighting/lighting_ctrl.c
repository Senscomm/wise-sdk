/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <timers.h>

#include <al/al_clock.h>

#include <ayla/assert.h>
#include <ayla/log.h>

#include "u-boot/list.h"

#include "lighting_mgr.h"
#include "lighting_ctrl.h"

typedef struct
{
    struct app_event evt;
    struct list_head list;
} lc_event_t;

static struct
{
    struct list_head list;
    lc_event_t *cur;
    int num;
    u64 until;
    lc_callback_completed cb;

    SemaphoreHandle_t mutex;
    TimerHandle_t timer;
} lc;

static void lc_serve_one_event( lc_event_t *lce)
{
    struct app_event *evt = &lce->evt;

    switch (evt->type) {
    case kEventType_Light:
    case kEventType_Timer:
        app_event_post(evt);
        break;
    default:
        log_put(LOG_WARN "[%s] Unsupported event (%d)", __func__,
                evt->type);
    }
}

static void lc_serve_next(void)
{
    xSemaphoreTake(lc.mutex, portMAX_DELAY);

    if (list_empty(&lc.list)) {
        xSemaphoreGive(lc.mutex);
        return;
    }

    if (lc.until != 0 && al_clock_get_total_ms() > lc.until) {
        xSemaphoreGive(lc.mutex);
        lighting_ctrl_terminate(true, false);
        return;
    }

    if (list_is_last(&lc.cur->list, &lc.list)) {
        if (lc.num > 0 && --lc.num == 0) {
            xSemaphoreGive(lc.mutex);
            lighting_ctrl_terminate(false, false);
            return;
        } else {
            lc.cur = list_first_entry(&lc.list, lc_event_t, list);
        }
    } else {
        lc.cur = list_entry(lc.cur->list.next, lc_event_t, list);
    }

    lc_serve_one_event(lc.cur);

    xSemaphoreGive(lc.mutex);

}

static void lc_timer_evt_handler(struct app_event *evt)
{
    if (evt->timer_event.value > 0) {
        xTimerChangePeriod(lc.timer, pdMS_TO_TICKS(evt->timer_event.value), 0);
    } else {
        lc_serve_next();
    }
}

static void lc_timer_callback(TimerHandle_t xTimer)
{
    lc_serve_next();
}

static void lc_callback_fn_initiated(Action_t action, int32_t actor)
{
}

static void lc_callback_fn_completed(Action_t action)
{
    lc_serve_next();
}

void lighting_ctrl_init(void)
{
    INIT_LIST_HEAD(&lc.list);
    lc.cur = NULL;

    lc.mutex = xSemaphoreCreateMutex();
    if (lc.mutex == NULL)
    {
        log_put(LOG_ERR "[%s] mutex create failed", __func__);
        return;
    }

    lc.timer = xTimerCreate("lightCtl", 1, false, NULL, lc_timer_callback);
    if (lc.timer == NULL)
    {
        log_put(LOG_ERR "[%s] timer create failed", __func__);
        return;
    }

    /* XXX: the sole user for now. */
    lighting_mgr_set_callbacks(LightMgr(), lc_callback_fn_initiated,
            lc_callback_fn_completed);

    app_event_install_handler(kEventType_Timer, lc_timer_evt_handler);
}

void lighting_ctrl_add_event(struct app_event *evt)
{
    lc_event_t *lce;

    lce = malloc(sizeof(*lce));
    memcpy(&lce->evt, evt, sizeof(*evt));

    xSemaphoreTake(lc.mutex, portMAX_DELAY);

    if (lc.cur) {
        log_put(LOG_ERR "Can't add an event to a running control.");
        free(lce);
        xSemaphoreGive(lc.mutex);
        return;
    }

    list_add_tail(&lce->list, &lc.list);
    xSemaphoreGive(lc.mutex);
}

void lighting_ctrl_run(int num, u32 timeout, lc_callback_completed cb)
{
    xSemaphoreTake(lc.mutex, portMAX_DELAY);

    if (list_empty(&lc.list)) {
        log_put(LOG_ERR "Can't run an empty control.");
        xSemaphoreGive(lc.mutex);
        return;
    }

    /* XXX: interpret 0 as 1.
     */
    lc.num = (num == 0 ? 1 : num);
    if (timeout != 0) {
        lc.until = al_clock_get_total_ms() + (u64)timeout;
    } else {
        lc.until = 0;
    }
    lc.cb = cb;

    lc.cur = list_first_entry(&lc.list, lc_event_t, list);
    lc_serve_one_event(lc.cur);

    xSemaphoreGive(lc.mutex);
}

void lighting_ctrl_terminate(bool timeout, bool silent)
{
    lc_event_t *lce, *tmp;
    lc_callback_completed cb;

    xSemaphoreTake(lc.mutex, portMAX_DELAY);

    if (list_empty(&lc.list)) {
        xSemaphoreGive(lc.mutex);
        return;
    }

    list_for_each_entry_safe(lce, tmp, &lc.list, list) {
        list_del(&lce->list);
        free(lce);
    }

    cb = lc.cb;
    lc.cur = NULL;

    xSemaphoreGive(lc.mutex);

    if (!silent && cb) {
        (*cb)(timeout);
    }
}

