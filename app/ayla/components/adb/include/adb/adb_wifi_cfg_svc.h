/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADB_WIFI_CFG_SVC_H__
#define __AYLA_ADB_WIFI_CFG_SVC_H__

/**
 * Retrieves a pointer to the UUID for the Ayla GATT Wi-Fi configuration
 * service.
 *
 * \returns a pointer to the UUID for the Ayla GATT Wi-Fi configuration
 * service.
 */
const void *adb_wifi_cfg_svc_get_uuid(void);

/**
 * Begins registration of the Ayla GATT Wi-Fi configuration service.
 *
 * The Ayla GATT Wi-Fi configuration service provides a means to configure
 * a device's Wi-Fi connection settings via a Bluetooth connection using
 * a mobile application that supports this service using Ayla's mobile
 * application SDK.
 *
 * \param service is a pointer to a pointer that will be set to point
 * to the service's attribute table on return (may be NULL)
 * \returns zero if successful, otherwise, non-zero.
 */
int adb_wifi_cfg_svc_register(const struct adb_attr **service);

#endif
