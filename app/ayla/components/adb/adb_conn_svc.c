/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stddef.h>
#include <string.h>

#include <ada/libada.h>
#include <ada/client.h>
#include <ayla/log.h>
#include <adb/adb.h>
#include <al/al_bt.h>
#include <adb/adb_conn_svc.h>

#ifdef AYLA_BLUETOOTH_SUPPORT

static enum adb_att_err adb_conn_svc_setup_token_write_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length);

static const AL_BT_UUID128(setup_token_uuid,
	/* 7E9869ED-4DB3-4520-88EA-1C21EF1BA834 */
	0x34, 0xa8, 0x1b, 0xef, 0x21, 0x1c, 0xea, 0x88,
	0x20, 0x45, 0xb3, 0x4d, 0xed, 0x69, 0x98, 0x7e);

static struct adb_chr_info setup_token;

static const AL_BT_UUID128(conn_svc_uuid,
	/* 1CF0FE66-3ECF-4D6E-A9FC-E287AB124B96 */
	0xdc, 0x9a, 0xd5, 0x5b, 0xb2, 0xfa, 0x36, 0xae,
	0x73, 0x48, 0xb6, 0x59, 0x41, 0xec, 0xe3, 0xfc);

static struct adb_service_info conn_svc = {
	.is_primary = 1,
};

static const struct adb_attr conn_svc_table[] = {
	ADB_SERVICE("conn_svc", &conn_svc_uuid, &conn_svc),
	ADB_CHR("setup_token", &setup_token_uuid,
	    AL_BT_AF_WRITE | AL_BT_AF_WRITE_ENC,
	    NULL, adb_conn_svc_setup_token_write_cb, &setup_token),
	ADB_SERVICE_END()
};

static enum adb_att_err adb_conn_svc_setup_token_write_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length)
{
	char setup_token[CLIENT_SETUP_TOK_LEN]; /* connection setup token */

	if (length > CLIENT_SETUP_TOK_LEN - 1) {
		length = CLIENT_SETUP_TOK_LEN - 1;
	}

	memcpy(setup_token, buf, length);
	setup_token[length] = '\0';

	client_set_setup_token(setup_token);

	return ADB_ATT_SUCCESS;
}

const void *adb_conn_svc_get_uuid(void)
{
	return &conn_svc_uuid;
}

int adb_conn_svc_register(const struct adb_attr **service)
{
	if (service) {
		*service = conn_svc_table;
	}
	return al_bt_register_service(conn_svc_table);
}

#endif /* AYLA_BLUETOOTH_SUPPORT */
