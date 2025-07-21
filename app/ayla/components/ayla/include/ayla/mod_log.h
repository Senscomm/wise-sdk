/*
 * Copyright 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_MOD_LOG_H__
#define __AYLA_MOD_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Module log numbers.
 * These index the log_mod table.
 * These must not change, since they are stored in flash for crash logs.
 */
PREPACKED_ENUM enum mod_log_id {
	MOD_LOG_MOD = LOG_MOD_DEFAULT,
	MOD_LOG_CLIENT = LOG_MOD_APP_BASE,
	MOD_LOG_CONF,
	MOD_LOG_DNSS,
	MOD_LOG_HOST,		/* was netsim - still used in service */
	MOD_LOG_NOTIFY,		/* obsolete - still used in service */
	MOD_LOG_SERVER,
	MOD_LOG_WIFI,
	MOD_LOG_SSL,
	MOD_LOG_LOGGER,
	MOD_LOG_IO,
	MOD_LOG_SCHED,
	MOD_LOG_ETHERNET,
	MOD_LOG_TEST,
	MOD_LOG_BT,
	MOD_LOG_FIREDOME,
	MOD_LOG_MQTT,
	__MOD_LOG_LIMIT		/* must be last */
} PACKED_ENUM;
ASSERT_SIZE(enum, mod_log_id, 1);
ASSERT_COMPILE(mod_log_ct, (int)__MOD_LOG_LIMIT - 1 <= (int)LOG_MOD_APP_MAX);

#define MOD_LOG_NAMES {				\
	[MOD_LOG_CLIENT] = "client",		\
	[MOD_LOG_CONF] = "conf",		\
	[MOD_LOG_DNSS] = "dnss",		\
	[MOD_LOG_MOD] = "mod",			\
	[MOD_LOG_HOST] = "host",		\
	[MOD_LOG_SERVER] = "server",		\
	[MOD_LOG_SSL] = "ssl",			\
	[MOD_LOG_WIFI] = "wifi",		\
	[MOD_LOG_LOGGER] = "log-client",        \
	[MOD_LOG_IO] = "io",			\
	[MOD_LOG_SCHED] = "sched",		\
	[MOD_LOG_ETHERNET] = "eth",		\
	[MOD_LOG_TEST] = "test",		\
	[MOD_LOG_BT] = "bt",			\
	[MOD_LOG_FIREDOME] = "firedome",	\
	[MOD_LOG_MQTT] = "mqtt",		\
}

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_MOD_LOG_H__ */
