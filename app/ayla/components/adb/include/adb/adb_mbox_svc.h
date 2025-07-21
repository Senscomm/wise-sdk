/*
 * Copyright 2021-2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADB_MBOX_SVC_H__
#define __AYLA_ADB_MBOX_SVC_H__

/**
 * Retrieves a pointer to the UUID for the Ayla GATT mailbox service.
 *
 * \returns a pointer to the UUID for the Ayla GATT mailbox service.
 */
const void *adb_mbox_svc_uuid_get(void);

/**
 * Begins registration of the Ayla GATT mailbox service.
 *
 * The Ayla GATT mailbox service provides message based communication with a
 * device over Bluetooth
 *
 * \param service is a pointer to a pointer that will be set to point
 * to the service's attribute table on return (may be NULL)
 * \returns zero if successful, otherwise, non-zero.
 */
int adb_mbox_svc_register(const struct adb_attr **service);

/**
 * Register callbacks to connect higher layer protocol to mailbox.
 *
 * \param session_alloc_cb is the function called when a new peer subscribes
 * to the outbox characteristic
 * \param session_down_cb is the function called when the peer disconnects
 * \param msg_rx_cb is the function called when a message is received via
 * the inbox characteristic.
 */
void adb_mbox_svc_callbacks_set(
    struct generic_session *(*session_alloc_cb)(void),
    void (*session_down_cb)(struct generic_session *gs),
    enum ada_err (*msg_rx_cb)(struct generic_session *gs, const u8 *buf,
    u16 length));
#endif
