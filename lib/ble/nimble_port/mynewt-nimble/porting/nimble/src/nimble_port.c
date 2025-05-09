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

#include <stddef.h>
#include "os/os.h"
#include "os/os_cputime.h"
#include "sysinit/sysinit.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#if NIMBLE_CFG_CONTROLLER
#include "controller/ble_ll.h"
#include "nimble/transport.h"
#endif
#include "hal/console.h"
#include <stdio.h>

#include "hal/rom.h"

uint8_t initialized;

#if defined(CONFIG_BLE_NIMBLE_HCI_TRANSPORT_UART)
extern void ble_hci_scm2010_uart_init(void);
extern void ble_hci_scm2010_uart_deinit(void);
#elif defined(CONFIG_BLE_NIMBLE_HCI_TRANSPORT_SCM2010_IPC)
extern void ble_hci_scm2010_ipc_init(void);
extern void ble_hci_scm2010_ipc_deinit(void);
#endif

extern int hal_timer_init(int timer_num, void *cfg);
extern int hal_uart_init(int port, void *arg);

#if NIMBLE_CFG_HOST
static struct ble_npl_eventq g_eventq_dflt;
#endif

extern void os_msys_init(void);
extern void os_mempool_module_init(void);

extern int (*ble_ll_deinit)(void);

void
nimble_port_init(void)
{
	if (initialized) {
		printk("BLE already initialized\n");
		return;
	}

#if NIMBLE_CFG_HOST
    /* Initialize default event queue */
    ble_npl_eventq_init(&g_eventq_dflt);
#endif

    /* Initialize the global memory pool */
    os_mempool_module_init();
    os_msys_init();
    /* Initialize transport */
    ble_transport_init();
    /* Initialize host */
#if defined(NIMBLE_CFG_HOST)
    ble_transport_hs_init();
#endif

#if defined(CONFIG_BLE_NIMBLE_HCI_TRANSPORT_UART)
    ble_hci_scm2010_uart_init();
#elif defined(CONFIG_BLE_NIMBLE_HCI_TRANSPORT_SCM2010_IPC)
    ble_hci_scm2010_ipc_init();
#endif

    hal_timer_init(0, NULL);
    os_cputime_init(20000000);

    ble_transport_ll_init();

    nimble_port_freertos_init();

	initialized = 1;
}

int
nimble_port_deinit(void)
{
    if (ble_ll_deinit() < 0) {
        return -1;
    }

    nimble_port_freertos_deinit();

#if defined(CONFIG_BLE_NIMBLE_HCI_TRANSPORT_UART)
    ble_hci_scm2010_uart_deinit();
#elif defined(CONFIG_BLE_NIMBLE_HCI_TRANSPORT_SCM2010_IPC)
    ble_hci_scm2010_ipc_deinit();
#endif

    return 0;
}

#if NIMBLE_CFG_HOST
volatile int g_port_run;
struct ble_npl_event g_stop_ev;

static void nimble_port_stop_event(struct ble_npl_event *ev)
{
	g_port_run = 0;
}

void
nimble_port_run(void)
{
    struct ble_npl_event *ev;

	g_port_run = 1;
	ble_npl_event_init(&g_stop_ev, nimble_port_stop_event, NULL);

    while (g_port_run) {
        ev = ble_npl_eventq_get(&g_eventq_dflt, BLE_NPL_TIME_FOREVER);
        ble_npl_event_run(ev);
    }
}

struct ble_npl_eventq *
nimble_port_get_dflt_eventq(void)
{
    return &g_eventq_dflt;
}

void
nimble_port_stop(void)
{
	ble_npl_eventq_put(&g_eventq_dflt, &g_stop_ev);
}
#endif

#if NIMBLE_CFG_CONTROLLER
void
_nimble_port_ll_task_func(void *arg)
{
    extern void (*ble_ll_task)(void *);

    ble_ll_task(arg);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(nimble_port_ll_task_func, &nimble_port_ll_task_func, &_nimble_port_ll_task_func);
#else
__func_tab__ void (*nimble_port_ll_task_func)(void *arg)
= _nimble_port_ll_task_func;
#endif

#endif
