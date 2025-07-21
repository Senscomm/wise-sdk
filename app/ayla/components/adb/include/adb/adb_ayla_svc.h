/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADB_AYLA_SVC_H__
#define __AYLA_ADB_AYLA_SVC_H__

/**
 * Retrieves a pointer to the UUID for the Ayla GATT service.
 *
 * \returns a pointer to the UUID for the Ayla GATT service.
 */
const void *adb_ayla_svc_uuid_get(void);

/**
 * Sets a callback to be called when the identify characteristic is
 * written.
 *
 * This feature is intended to enable end users to identify which
 * physical device corresponds to a device listed in a user interface.
 * For example, if there are multiple devices present and a user wants
 * to interact with a specific one, the user may tap a button in the
 * UI to identify the device corresponding to a device listed in the UI.
 * The device corresponding to the tapped button would receive this
 * callback and provide an indication to the user, such as briefly blink
 * an LED or emit a sound, to enable the user to associate the device
 * listed in the UI with an actual physical device. It is up to the
 * application to provide an appropriate identification signal to the
 * end user when this callback is called.
 *
 * \param cb is the callback function
 * \returns zero if successful, otherwise, non-zero.
 */
int adb_ayla_svc_identify_cb_set(void (*cb)(void));

/**
 * Begins registration of the Ayla GATT service.
 *
 * The Ayla GATT service provides access to basic information about the
 * device, including:
 *
 *	- Device Serial Number (DSN)
 *	- OEM identifier
 *	- OEM model string
 *	- Device template version
 *	- Identification virtual push button (see adb_ayla_svc_set_identify_cb)
 *	- Display name of device
 *
 * \param service is a pointer to a pointer that will be set to point
 * to the service's attribute table on return (may be NULL)
 * \returns zero if successful, otherwise, non-zero.
 */
int adb_ayla_svc_register(const struct adb_attr **service);

/**
 * Gets a pointer to the Ayla-Id in string format.
 *
 * \returns Ayla-Id string or NULL if it is not available
 */
const char *adb_ayla_svc_ayla_id_get(void);

/**
 * Generates a new resolvable randomized id.
 *
 * The resolvable random id allows a mobile device that knows the LAN IP key
 * for the device to find the device from advertisements. The random id
 * changes over time to avoid it being used to track the device, as would
 * be possible with a device specific fixed identifier such as the DSN.
 */
enum ada_err adb_ayla_svc_random_id_update(void);

/**
 * Gets local control service advertisement data, which includes the
 * resolvable randomized id.
 *
 * \param length is a pointer to the length return value
 * \returns returns a pointer to the data if available. If not available,
 * returns NULL and length 0.
 */
const u8 *adb_ayla_svc_data_get(u8 *length);

/**
 * Gets the current passkey generated form randomized id and LAN IP key.
 *
 * \returns passkey if available or MAX_U32 if not available
 */
u32 adb_ayla_svc_passkey_get(void);

#endif
