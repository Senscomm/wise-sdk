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

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sysinit/sysinit.h>
#include <hal/ipc.h>
#include <nimble/ble.h>
#include <nimble/transport.h>
#include "hal/rom.h"

#define DEBUG_IPC_HCI   0

#if DEBUG_IPC_HCI
#define hci_dbg printf
#else
#define hci_dbg(...)
#endif

#if MYNEWT_VAL(BLE_CONTROLLER)
static ipc_listener_t g_ble_cmd_listener;
#endif
#if !MYNEWT_VAL(BLE_CONTROLLER)
static ipc_listener_t g_ble_evt_listener;
#endif
static ipc_listener_t g_ble_acl_listener;
static struct device *ble_hci_ipc_dev;

#if MYNEWT_VAL(BLE_CONTROLLER)
static int
scm2010_ble_hci_evt_tx(void *buf)
{
    ipc_payload_t *hci;
    uint8_t *evt = buf;
    int len = 2 + evt[1];
    int rc = BLE_ERR_MEM_CAPACITY;

#if DEBUG_IPC_HCI
    int i;
    hci_dbg("TXE:%3d, ", len);
    for (i = 0; i < len; i++) {
        hci_dbg("%02x ", evt[i]);
    }
    hci_dbg("\n");
#endif

    hci = ipc_alloc(ble_hci_ipc_dev, len, false, true,
                    IPC_MODULE_BLE, IPC_CHAN_EVENT, IPC_TYPE_REQUEST);
    if (!hci) {
        printf("error bt ipc alloc\n");
        goto transport_free;
    }

    ipc_copyto(ble_hci_ipc_dev, evt, len, hci, 0);
    rc = ipc_transmit(ble_hci_ipc_dev, hci);

transport_free:
    ble_transport_free(buf);

    return (rc < 0) ? BLE_ERR_MEM_CAPACITY : 0;
}

static int
scm2010_ble_hci_cmd_rx(ipc_payload_t *payload, void *priv)
{
    struct device *dev = (struct device *)priv;
    uint8_t *buf = payload->data;
    uint8_t *cmd;
    int len = payload->data[2] + 3;

#if DEBUG_IPC_HCI
    int i;
    hci_dbg("RXC:%3d, ", len);
    for (i = 0; i < len; i++) {
        hci_dbg("%02x ", buf[i]);
    }
    hci_dbg("\n");
#endif
    cmd = ble_transport_alloc_cmd();
    if (!cmd) {
        printf("error bt cmd buf\n");
        ipc_free(dev, payload);
        return 0;
    }

    ipc_copyfrom(dev, cmd, 256, payload, 0); /* max limited by transport POOL_CMD_SIZE */

    ble_transport_to_ll_cmd(cmd);

    ipc_free(dev, payload);

    return 0;
}

#endif

#if !MYNEWT_VAL(BLE_CONTROLLER)
static int
scm2010_ble_hci_cmd_tx(void *buf)
{
    ipc_payload_t *hci;
    uint8_t *cmd = buf;
    int len = 3 + cmd[2];
    int rc = BLE_ERR_MEM_CAPACITY;

#if DEBUG_IPC_HCI
    int i;
    hci_dbg("TXC:%3d, ", len);
    for (i = 0; i < len; i++) {
        hci_dbg("%02x ", cmd[i]);
    }
    hci_dbg("\n");
#endif

    hci = ipc_alloc(ble_hci_ipc_dev, len, false, true,
                    IPC_MODULE_BLE, IPC_CHAN_CONTROL, IPC_TYPE_REQUEST);
    if (!hci) {
        printf("error bt ipc alloc\n");
        goto transport_free;
    }

    ipc_copyto(ble_hci_ipc_dev, cmd, len, hci, 0);
    rc = ipc_transmit(ble_hci_ipc_dev, hci);

transport_free:
    ble_transport_free(buf);

    return (rc < 0) ? BLE_ERR_MEM_CAPACITY : 0;
}

static int
scm2010_ble_hci_evt_rx(ipc_payload_t *payload, void *priv)
{
    struct device *dev = (struct device *)priv;
    uint8_t *buf = payload->data;
    uint8_t *evt;
    int len = payload->data[1] + 2;

#if DEBUG_IPC_HCI
    int i;
    hci_dbg("RXE:%3d, ", len);
    for (i = 0; i < len; i++) {
        hci_dbg("%02x ", buf[i]);
    }
    hci_dbg("\n");
#endif
    evt = ble_transport_alloc_cmd();
    if (!evt) {
        printf("error bt cmd buf\n");
        ipc_free(dev, payload);
        return 0;
    }

    ipc_copyfrom(dev, evt, MYNEWT_VAL(BLE_TRANSPORT_EVT_SIZE), payload, 0);

    ble_transport_to_hs_evt(evt);

    ipc_free(dev, payload);

    return 0;
}
#endif

static int
scm2010_ble_hci_acl_tx(struct os_mbuf *om)
{
    ipc_payload_t *hci;
    uint8_t len = OS_MBUF_PKTLEN(om);
    int rc = BLE_ERR_MEM_CAPACITY;

#if DEBUG_IPC_HCI
    int i;
    hci_dbg("TXA:%3d, ", len);
    for (i = 0; i < len; i++) {
        hci_dbg("%02x ", om->om_data[i]);
    }
    hci_dbg("\n");
#endif

    hci = ipc_alloc(ble_hci_ipc_dev, len, false, true,
                    IPC_MODULE_BLE, IPC_CHAN_DATA, IPC_TYPE_REQUEST);
    if (!hci) {
        printf("error bt ipc alloc\n");
        goto transport_free;
    }

    ipc_copyto(ble_hci_ipc_dev, om->om_data, len, hci, 0);     /* XXX: what if chained? */
    rc = ipc_transmit(ble_hci_ipc_dev, hci);

transport_free:
    os_mbuf_free_chain(om);

    return (rc < 0) ? BLE_ERR_MEM_CAPACITY : 0;
}

static int
scm2010_ble_hci_acl_rx(ipc_payload_t *payload, void *priv)
{
    struct device *dev = (struct device *)priv;
    uint8_t *buf = payload->data;
    int len = payload->data[2] + 4;
    struct os_mbuf *om;

#if DEBUG_IPC_HCI
    int i;
    hci_dbg("RXA:%3d, ", len);
    for (i = 0; i < len; i++) {
        hci_dbg("%02x ", buf[i]);
    }
    hci_dbg("\n");
#endif

#if MYNEWT_VAL(BLE_CONTROLLER)
    om = ble_transport_alloc_acl_from_hs();
#endif
#if !MYNEWT_VAL(BLE_CONTROLLER)
    om = ble_transport_alloc_acl_from_ll();
#endif
    if (!om) {
        printf("error bt acl buf\n");
        ipc_free(dev, payload);
        return 0;
    }

    ipc_copyfrom(dev, om->om_data, MYNEWT_VAL(BLE_TRANSPORT_ACL_SIZE), payload, 0);
    ipc_free(dev, payload);

    OS_MBUF_PKTLEN(om) = *(uint16_t *)&om->om_data[2] + 4;
    om->om_len = OS_MBUF_PKTLEN(om);

#if MYNEWT_VAL(BLE_CONTROLLER)
    ble_transport_to_ll_acl(om);
#endif
#if !MYNEWT_VAL(BLE_CONTROLLER)
    ble_transport_to_hs_acl(om);
#endif

    return 0;
}

#if MYNEWT_VAL(BLE_CONTROLLER)
int
_ble_transport_to_hs_evt_impl(void *buf)
{
    return scm2010_ble_hci_evt_tx(buf);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_transport_to_hs_evt_impl, &ble_transport_to_hs_evt_impl, &_ble_transport_to_hs_evt_impl);
#else
__func_tab__ int (*ble_transport_to_hs_evt_impl)(void *buf)
= _ble_transport_to_hs_evt_impl;
#endif

int
_ble_transport_to_hs_acl_impl(struct os_mbuf *om)
{
    return scm2010_ble_hci_acl_tx(om);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_transport_to_hs_acl_impl, &ble_transport_to_hs_acl_impl, &_ble_transport_to_hs_acl_impl);
#else
__func_tab__ int (*ble_transport_to_hs_acl_impl)(struct os_mbuf *om)
= _ble_transport_to_hs_acl_impl;
#endif

void
ble_transport_hs_init(void)
{
}
#endif

#if !MYNEWT_VAL(BLE_CONTROLLER)
int
ble_transport_to_ll_cmd_impl(void *buf)
{
    return scm2010_ble_hci_cmd_tx(buf);
}

int
ble_transport_to_ll_acl_impl(struct os_mbuf *om)
{
    return scm2010_ble_hci_acl_tx(om);
}

void
ble_transport_ll_init(void)
{
}
#endif

void
ble_hci_scm2010_ipc_init(void)
{
    SYSINIT_ASSERT_ACTIVE();

    ble_hci_ipc_dev = device_get_by_name("scm2010-ipc");

#if MYNEWT_VAL(BLE_CONTROLLER)
    g_ble_cmd_listener.cb = scm2010_ble_hci_cmd_rx;
    g_ble_cmd_listener.type = IPC_TYPE_REQUEST;
    g_ble_cmd_listener.priv = ble_hci_ipc_dev;
    if (ipc_addcb(ble_hci_ipc_dev, IPC_MODULE_BLE, IPC_CHAN_CONTROL, &g_ble_cmd_listener))
        printf("%s not success\n", __func__);
#endif
#if !MYNEWT_VAL(BLE_CONTROLLER)
    g_ble_evt_listener.cb = scm2010_ble_hci_evt_rx;
    g_ble_evt_listener.type = IPC_TYPE_REQUEST;
    g_ble_evt_listener.priv = ble_hci_ipc_dev;
    if (ipc_addcb(ble_hci_ipc_dev, IPC_MODULE_BLE, IPC_CHAN_EVENT, &g_ble_evt_listener))
        printf("%s not success\n", __func__);
#endif

    g_ble_acl_listener.cb = scm2010_ble_hci_acl_rx;
    g_ble_acl_listener.type = IPC_TYPE_REQUEST;
    g_ble_acl_listener.priv = ble_hci_ipc_dev;
    if (ipc_addcb(ble_hci_ipc_dev, IPC_MODULE_BLE, IPC_CHAN_DATA, &g_ble_acl_listener))
        printf("%s not success\n", __func__);

    printf("ble hci ipc init\n");
}

void
ble_hci_scm2010_ipc_deinit(void)
{
    ble_hci_ipc_dev = device_get_by_name("scm2010-ipc");

#if MYNEWT_VAL(BLE_CONTROLLER)
    ipc_delcb(ble_hci_ipc_dev, IPC_MODULE_BLE, IPC_CHAN_CONTROL, &g_ble_cmd_listener);
#endif
#if !MYNEWT_VAL(BLE_CONTROLLER)
    ipc_delcb(ble_hci_ipc_dev, IPC_MODULE_BLE, IPC_CHAN_EVENT, &g_ble_evt_listener);
#endif
    ipc_delcb(ble_hci_ipc_dev, IPC_MODULE_BLE, IPC_CHAN_DATA, &g_ble_acl_listener);
}
