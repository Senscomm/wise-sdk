/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_BT_H__
#define __AYLA_AL_BT_H__

#include <platform/pfm_bt.h>

/**
 * Function IDs used in tracking which functions require Bluetooth
 * support to stay up.
 */
enum al_bt_function {
	AL_BT_FUNC_APP = 1,		/* application functions */
	AL_BT_FUNC_PROVISION,		/* provisioning functions */
	AL_BT_FUNC_LOCAL_CONTROL	/* local control functions */
};

/**
 * Initializes the Bluetooth abstraction layer.
 *
 * This function must be called prior to using any other al_bt APIs, other
 * than al_bt_wifi_timeout_set().
 *
 * \param register_cb is a callback function to register gatt services
 */
void al_bt_init(void (register_cb)(void));

/**
 * De-initializes the Bluetooth abstraction layer and frees hardware,
 * memory and other resources.
 *
 * NOTE: Once this function has been called, Bluetooth cannot be
 * reinitialized without a reboot.
 */
void al_bt_deinit(void);

/**
 * Indicates whether Bluetooth is enabled. If it is not up, other APIs that
 * rely on Nimble and/or the Bluetooth controller being up should not be
 * called.
 *
 * \returns 0 if not enabled, 1 if enabled
 */
int al_bt_enabled(void);

/**
 * Synchronization function to block BLE thread while waiting for
 * an asynchronous property operation.
 */
void al_bt_wait(void);

/**
 * Synchronization function to wake up BLE thread when an asynchronous
 * property operation completes.
 */
void al_bt_wakeup(void);

/**
 * Acquire mutex to serialize access to BT resources accessed by multiple
 * threads.
 */
void al_bt_lock(void);

/**
 * Release mutex.
 */
void al_bt_unlock(void);

/**
 * Global variable to track mutex state and ASSERT if it isn't as expected.
 */
extern u8 al_bt_locked;

/**
 * Reset BT conf subsystem items like bonds.
 */
void al_bt_conf_factory_reset(void);

/**
 * Set platform specific pairing mode.
 *
 * NOTE: Applications should use adb_pairing_mode_set, not this API.
 *
 * \param mode is the pairing mode to configure
 * \param duration is the duration in seconds to enable this mode, 0 means
 * no time limit.
 */
int al_bt_pairing_mode_set(enum adb_pairing_mode mode, u16 duration);

/**
 * Set a callback function to display the pairing passkey to the user.
 */
void al_bt_passkey_callback_set(void (*callback)(u32 passkey));

/**
 * Begins registration of a Bluetooth GATT service.
 *
 * This function is normally called by service specific registration
 * functions.
 *
 * The event callback will be called for any service state transition.
 *
 * \param service is a pointer to the attribute table.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_register_service(const struct adb_attr *service);

/**
 * Finds an attribute in an attribute table by UUID.
 *
 * Searches from the record after the parent until the desired attribute
 * is found, another attribute of the same type is encountered, or the end of
 * the table is encountered.
 *
 * \param parent is the parent of attribute to search under.
 * \param uuid2 is the UUID to search for.
 * \returns pointer to attribute, or NULL if not found.
 */
const struct adb_attr *al_bt_find_attr_by_uuid(const struct adb_attr *parent,
    const void *uuid);

/**
 * Finds an attribute in an attribute table by its handle.
 *
 * Searches all ADB registered attributes for the attribute with the
 * specified handle.
 *
 * \param handle to find.
 * \returns pointer to attribute, or NULL if not found.
 */
const struct adb_attr *al_bt_find_attr_by_handle(u16 handle);

/**
 * Finds a service by the platform specific argument.
 *
 * \param pfm_arg is the platform arg value to search for.
  * \returns pointer to service attribute, or NULL if not found.
 */
const struct adb_attr *al_bt_find_service_by_arg(const void *pfm_arg);

/**
 * Adds the connection handle to the ADB connection table.
 *
 * \param handle is the connection handle to add.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_connection_add(u16 handle);

/**
 * Deletes the connection handle from the ADB connection table.
 *
 * \param handle is the connection handle to delete.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_connection_delete(u16 handle);

/**
 * Terminates the connection identified by the handle.
 *
 * \param handle is the handle of the connection to terminate.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_connection_terminate(u16 handle);

/**
 * Returns the notification and indication mask with the bit assigned to the
 * specified connection handle set.
 *
 * \param handle is the connection for which the mask is desired.
 * \returns mask with one bit set for the connection or zero bits set if
 * the handle was not found.
 */
u8 al_bt_connection_mask(u16 handle);

/**
 * Get the next active connection handle from the connection table.
 *
 * \param handle is the returned connection handle.
 * \param cookie is a cookie used to iterate the table, initialize to zero
 * to get the first active connection in the table. On a successful get, the
 * cookie is updated for use in the next call.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_connection_next(u16 *handle, u16 *cookie);

/**
 * Set context to be associated with the connection handle.
 *
 * \param handle is the handle for the connection.
 * \param ctxt is a context value/pointer to associate with the connection.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_connection_ctxt_set(u16 handle, void *ctxt);

/**
 * Get context that was previously associated with the connection handle.
 *
 * \param handle is the handle for the connection.
 * \returns previously set context value/pointer.
 */
void *al_bt_connection_ctxt_get(u16 handle);

/**
 * Get current mtu for a connection.
 *
 * \param handle is the handle for the connection.
 * \param if successful, returns the current mtu setting, otherwise, unchanged.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_conn_mtu_get(u16 handle, u16 *mtu);

/**
 * Send notifications and indications on a single connection if
 * the client has indicated interest.
 *
 * \param handle is the handle for the connection.
 * \param bt_chr is a pointer to the relevant characteristic.
 * \param buf is a pointer to a buffer containing the data to write.
 * \param length of the data in the buffer.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_notify_conn(u16 handle, const struct adb_attr *bt_chr, u8 *buf,
    u16 length);

/**
 * Send notifications and indications to interested clients.
 *
 * \param bt_chr is a pointer to the relevant characteristic.
 * \param buf is a pointer to a buffer containing the data to write.
 * \param length of the data in the buffer.
 * \returns zero if successful, otherwise, non-zero.
 */
int al_bt_notify(const struct adb_attr *bt_chr, u8 *buf, u16 length);

/**
 * Set time limit for bluetooth wi-fi provisioning.
 *
 * \param time_ms is the limit in milliseconds from the start of provisioning
 * or from the current time, whichever is later.
 *
 * If time_ms is 0, Wi-Fi provisioning is not started until this is called
 * again, at which time it is started or extended.
 *
 * This may be called before or after al_bt_init().
 */
void al_bt_wifi_timeout_set(unsigned int time_ms);

/**
 * Set flag to keep Bluetooth up.
 *
 * \param func_id is the id of the function that needs Bluetooth to stay
 * up.
 */
void al_bt_keep_up_set(enum al_bt_function func_id);

/**
 * Clear flag to keep Bluetooth up. If all flags are cleared, Bluetooth
 * may be shut down.
 *
 * \param func_id is the id of the function that needs Bluetooth to stay
 * up.
 */
void al_bt_keep_up_clear(enum al_bt_function func_id);

#endif
