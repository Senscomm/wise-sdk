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

#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ada/libada.h>

#include <adb/adb.h>
#include <al/al_bt.h>

#ifdef AYLA_BLUETOOTH_SUPPORT

#define ADB_CONN_EVENT_HANDLER_CT	2

struct adb_conn_event_handler {
	void (*handler)(enum adb_conn_event event, u16 conn, void *arg);
	void *arg;
};

static struct adb_conn_event_handler
    adb_conn_event_handlers[ADB_CONN_EVENT_HANDLER_CT];

static void (*adb_event_handler)(enum adb_event event);

static enum adb_pairing_mode adb_pairing_mode;

const char * const adb_pairing_mode_names[] = {	\
	[ADB_PM_DISABLED] = "disabled",		\
	[ADB_PM_NO_PASSKEY] = "no-passkey",	\
	[ADB_PM_CONFIGURED_PASSKEY] = "config-passkey",	\
	[ADB_PM_RANDOM_PASSKEY] = "random-passkey",	\
	[ADB_PM_AYLA_PASSKEY] = "ayla-passkey"
};

void adb_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_BT, fmt, args);
	ADA_VA_END(args);
}

void adb_dump_log(const char *prefix, const u8 *buf, u16 length,
    u8 (*redact_cb)(int offset, u8 c))
{
	log_dump(MOD_LOG_BT, LOG_SEV_DEBUG2, prefix, buf, length, redact_cb);
}

void adb_snprint_addr(const u8 *addr, char *buf, size_t length)
{
	snprintf(buf, length, "%02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4],
	    addr[3], addr[2], addr[1], addr[0]);
}

enum ada_err adb_conn_event_register(void (*handler)(enum adb_conn_event event,
    u16 conn, void *arg), void *arg)
{
	struct adb_conn_event_handler *hp;

	al_bt_lock();
	for (hp = adb_conn_event_handlers;
	    hp < ARRAY_END(adb_conn_event_handlers); hp++) {
		if (!hp->handler) {
			hp->handler = handler;
			hp->arg = arg;
			al_bt_unlock();
			return AE_OK;
		}
	}
	al_bt_unlock();

	return AE_ALLOC;
}

void adb_conn_event_deregister(void (*handler)(enum adb_conn_event event,
    u16 conn, void *arg))
{
	struct adb_conn_event_handler *hp;

	al_bt_lock();
	for (hp = adb_conn_event_handlers;
	    hp < ARRAY_END(adb_conn_event_handlers); hp++) {
		if (hp->handler == handler) {
			hp->handler = NULL;
			break;
		}
	}
	al_bt_unlock();
}

void adb_conn_event_notify(enum adb_conn_event event, u16 conn)
{
	struct adb_conn_event_handler *hp;

	al_bt_lock();
	for (hp = adb_conn_event_handlers;
	    hp < ARRAY_END(adb_conn_event_handlers); hp++) {
		al_bt_unlock();
		if (hp->handler) {
			hp->handler(event, conn, hp->arg);
		}
		al_bt_lock();
	}
	al_bt_unlock();
}

enum ada_err adb_event_register(void (*handler)(enum adb_event event))
{
	adb_event_handler = handler;

	return AE_OK;
}

void adb_event_notify(enum adb_event event)
{
	if (adb_event_handler) {
		adb_event_handler(event);
	}
}

enum adb_att_err adb_chr_read(u16 conn, const struct adb_attr *attr, u8 *buf,
    u16 *length)
{
	struct adb_chr_info *chr_info = attr->info;

	if (!chr_info || !chr_info->value) {
		return ADB_ATT_UNLIKELY;
	}
	if (chr_info->value_length < *length) {
		*length = chr_info->value_length;
	}
	memcpy(buf, chr_info->value, *length);

	return ADB_ATT_SUCCESS;
}

enum adb_att_err adb_chr_read_str(u16 conn, const struct adb_attr *attr,
    u8 *buf, u16 *length)
{
	struct adb_chr_info *chr_info = attr->info;
	u16 str_len;

	if (!chr_info || !chr_info->value) {
		return ADB_ATT_UNLIKELY;
	}
	str_len = strlen(chr_info->value);
	if (str_len < *length) {
		*length = str_len;
	}
	memcpy(buf, chr_info->value, *length);

	return ADB_ATT_SUCCESS;
}

enum adb_att_err adb_chr_write(u16 conn, const struct adb_attr *attr,
    u8 *buf, u16 length)
{
	struct adb_chr_info *chr_info = attr->info;

	if (!chr_info || !chr_info->value) {
		return ADB_ATT_UNLIKELY;
	}
	if (length > chr_info->max_val_len) {
		return ADB_ATT_INVALID_ATTR_VALUE_LEN;
	}
	memcpy(chr_info->value, buf, length);
	chr_info->value_length = length;

	return ADB_ATT_SUCCESS;
}

enum adb_att_err adb_chr_write_str(u16 conn, const struct adb_attr *attr,
    u8 *buf, u16 length)
{
	struct adb_chr_info *chr_info = attr->info;

	if (!chr_info || !chr_info->value || !chr_info->max_val_len) {
		return ADB_ATT_UNLIKELY;
	}
	if (length > chr_info->max_val_len - 1) {
		return ADB_ATT_INVALID_ATTR_VALUE_LEN;
	}
	memcpy(chr_info->value, buf, length);
	((char *)chr_info->value)[length] = '\0';
	chr_info->value_length = length;

	return ADB_ATT_SUCCESS;
}

void adb_pairing_mode_set(enum adb_pairing_mode mode, u16 duration)
{
	ASSERT(mode >= ADB_PM_DISABLED && mode <= ADB_PM_AYLA_PASSKEY);

	adb_pairing_mode = mode;
	al_bt_pairing_mode_set(mode, duration);
}

enum adb_pairing_mode adb_pairing_mode_get(void)
{
	return adb_pairing_mode;
}

const char *adb_pairing_mode_name_get(void)
{
	if (adb_pairing_mode < 0 ||
	    adb_pairing_mode > ARRAY_LEN(adb_pairing_mode_names)) {
		return "unknown";
	}
	return adb_pairing_mode_names[adb_pairing_mode];
}

#endif /* AYLA_BLUETOOTH_SUPPORT */
