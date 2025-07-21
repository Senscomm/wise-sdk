/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADB_CONN_SVC_H__
#define __AYLA_ADB_CONN_SVC_H__

/**
 * Retrieves a pointer to the UUID for the Ayla GATT connection
 * service.
 *
 * \returns a pointer to the UUID for the Ayla GATT connection
 * service.
 */
const void *adb_conn_svc_get_uuid(void);

/**
 * Begins registration of the Ayla GATT connection service.
 *
 * The Ayla GATT connection service provides a means to write the setup token,
 * which is used to prove a user attempting to register a device is the user
 * who set the setup token via this service.
 *
 * \param service is a pointer to a pointer that will be set to point
 * to the service's attribute table on return (may be NULL)
 * \returns zero if successful, otherwise, non-zero.
 */
int adb_conn_svc_register(const struct adb_attr **service);

#endif
