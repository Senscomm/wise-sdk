/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_TEST_SVC_INT_H__
#define __AYLA_TEST_SVC_INT_H__

#define TS_CONFIG_SIG_LEN	256	/* test config signature length */
#define TS_PUB_KEY_LEN		294	/* length of test service public keys */
#define TS_SETUP_KEY_LEN	32	/* test setup key length */
#define TS_ENC_SETUP_KEY_LEN	256	/* encrypted test setup key length */
#define TS_IV_LEN		16	/* length of IV used with setup key */
#define TS_SVC_NAME_LEN		32	/* max length of service nickname */
#define TS_CONFIG_MAX_LEN	1024	/* max config data length */
#define TS_SETUP_DATA_LEN	(8 * 16) /* must be multiple of 16 */

struct test_service {
	const char *nickname;		/* nickname of entry */
	const char *hostname;		/* service host name */
	const void *pub_key;		/* service public key */
	const char *ntp_server;		/* NTP server for test service */
	u8 enable:1;			/* entry enable control */
};

struct ts_setup_info {
	u32 setup_time;			/* UTC time setup process started */
	size_t enc_setup_key_len;
	size_t data_len;
	char svc_name[TS_SVC_NAME_LEN];	/* test service nickname */
	char unique_id[CONF_DEV_ID_MAX];/* unique ID of the device */
	u8 enc_setup_key[TS_ENC_SETUP_KEY_LEN]; /* encrypted test setup key */
	u8 iv[TS_IV_LEN];		/* iv used to encrypt setup data */
	u8 data[TS_SETUP_DATA_LEN];	/* encrypted test setup data */
};

/*
 * Check if changing to a test server is enabled.
 */
int test_svc_enabled(void);

/*
 * Lookup test server by checking if the provided name is either a nickname or
 * the full DNS host name of an entry in the test_svc_table.
 *
 * Returns test_service entry if the the name is listed in the
 * test_svc_table.
 * Returns NULL otherwise.
 */
struct test_service *test_svc_lookup(const char *name);

/*
 * Get test setup data.
 *
 * The svc_name and setup_time fields in the structure are input parameters.
 * All else are output parameters set by this function.
 */
enum ada_err test_svc_setup_data_get(struct ts_setup_info *setup_info);

/*
 * Apply a test setup ticket.
 *
 * This changes the credentials and config of the device to the values
 * specified in the ticket. A factory reset returns the device to its
 * factory configured credentials.
 */
enum ada_err test_svc_ticket_apply(const void *ticket, size_t length);

#endif
