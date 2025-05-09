/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include "os/os.h"
#include "hal/hal_timer.h"
#include "os/os_trace_api.h"

/* Header files are in /wise/include/hal */
#include "hal/console.h"
#include "hal/pm.h"
#include "hal/irq.h"
#include "hal/device.h"
#include "hal/rom.h"
#include "hal/systimer.h"

struct device *g_ble_timer_dev;

int
hal_timer_init(int timer_num, void *cfg)
{
    (void)timer_num;
    (void)cfg;

    g_ble_timer_dev = device_get_by_name("systimer");
    if (!g_ble_timer_dev) {
        printk("ble systimer not found\n");
        return -1;
    }

    return 0;
}

int
_hal_timer_config(int timer_num, uint32_t freq_hz)
{
    (void)timer_num;
    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(hal_timer_config, &hal_timer_config, &_hal_timer_config);
#else
__func_tab__ int (*hal_timer_config)(int timer_num, uint32_t freq_hz)
= _hal_timer_config;
#endif

int
hal_timer_deinit(int timer_num)
{
    return 0;
}

uint32_t
hal_timer_get_resolution(int timer_num)
{
    return 0;
}

__ilm_ble__ uint32_t
_hal_timer_read(int timer_num)
{
    (void)timer_num;
    return systimer_get_counter(g_ble_timer_dev);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(hal_timer_read, &hal_timer_read, &_hal_timer_read);
#else
__func_tab__ uint32_t (*hal_timer_read)(int timer_num)
= _hal_timer_read;
#endif

int
hal_timer_delay(int timer_num, uint32_t ticks)
{
    uint32_t until;

    until = hal_timer_read(timer_num) + ticks;
    while ((int32_t)(hal_timer_read(timer_num) - until) <= 0) {
        /* Loop here till finished */
    }

    return 0;
}

int
_hal_timer_set_cb(int timer_num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    (void)timer_num;
    return systimer_set_cmp_cb(g_ble_timer_dev, (struct systimer *)timer, SYSTIMER_TYPE_WAKEUP_FULL, cb_func, arg);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(hal_timer_set_cb, &hal_timer_set_cb, &_hal_timer_set_cb);
#else
__func_tab__ int (*hal_timer_set_cb)(int timer_num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
= _hal_timer_set_cb;
#endif

__ilm_ble__ int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    uint32_t tick;

    tick = systimer_get_counter(g_ble_timer_dev) + ticks;
    return hal_timer_start_at(timer, tick);
}

__ilm_ble__ int
_hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    return systimer_start_at(g_ble_timer_dev, (struct systimer *)timer, tick);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(hal_timer_start_at, &hal_timer_start_at, &_hal_timer_start_at);
#else
__func_tab__ int (*hal_timer_start_at)(struct hal_timer *timer, uint32_t tick)
= _hal_timer_start_at;
#endif

__ilm_ble__ int
_hal_timer_stop(struct hal_timer *timer)
{
    return systimer_stop(g_ble_timer_dev, (struct systimer *)timer);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(hal_timer_stop, &hal_timer_stop, &_hal_timer_stop);
#else
__func_tab__ int (*hal_timer_stop)(struct hal_timer *timer)
= _hal_timer_stop;
#endif
