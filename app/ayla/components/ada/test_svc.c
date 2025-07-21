/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stddef.h>
#include <string.h>

#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/timer.h>
#include <ayla/http.h>
#include <ayla/base64.h>
#include <ada/libada.h>
#include <ada/client_ota.h>
#include <ada/server_req.h>
#include <al/al_os_mem.h>
#include <al/al_aes.h>
#include <al/al_hash_sha256.h>
#include <al/al_random.h>
#include <al/al_rsa.h>

#include "ame.h"
#include "ame_json.h"
#include "test_svc_int.h"
#include "oem_int.h"
#include "client_int.h"
#include "client_lock.h"

#ifdef AYLA_TEST_SERVICE_SUPPORT

#define TS_KVP_STACK_SZ	15

struct ts_ticket_info {
	size_t conf_len;
	size_t sig_len;
	u32 setup_time;			/* UTC time setup process started */
	u32 exp_time;			/* UTC time ticket expires */
	char t_dsn_ct[CONF_DEV_ID_MAX];	/* outer, insecure test DSN */
	u8 iv[TS_IV_LEN];		/* iv used to encrypt config data */
	char host[TS_SVC_NAME_LEN];	/* UTF8 host nickname */
	u8 conf[TS_CONFIG_MAX_LEN];	/* encrypted test config */
	u8 signature[TS_CONFIG_SIG_LEN];/* config data signature */
	char unique_id[CONF_DEV_ID_MAX];/* unique ID of the device */
	char t_dsn[CONF_DEV_ID_MAX];	/* test DSN */
	char t_pub_key[BASE64_LEN_EXPAND(CLIENT_CONF_PUB_KEY_LEN) + 1];
					/* test device public key base64 */
	char t_oem[CONF_OEM_MAX];	/* OEM ID on test service */
	u8 t_oem_key[CONF_OEM_KEY_MAX];	/* test OEM key */
	size_t t_oem_key_len;
};

AME_KEY(k_host, "h", "host");	/* test host nickname */
AME_KEY(k_time, "t", "time");	/* test setup time */
AME_KEY(k_iv, "v", "IV");	/* iv for setup/config data encrption */
AME_KEY(k_exp, "e", "exp");	/* expiration time */
AME_KEY(k_conf, "c", "conf");	/* encrypted setup config */
AME_KEY(k_sig, "s", "sig");	/* config signature */
AME_KEY(k_id, "i", "id");	/* unique ID */
AME_KEY(k_dsn, "d", "DSN");	/* test DSN */
AME_KEY(k_pub_key, "p", "pubkey");/* test device pub key */
AME_KEY(k_oem, "o", "OEM");	/* test OEM ID */
AME_KEY(k_oem_key, "k", "key");	/* test OEM key */

static const u8 pubkey_us_test[TS_PUB_KEY_LEN] = {
	0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09,
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
	0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
	0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01,
	0x00, 0xd5, 0x43, 0xfa, 0x5b, 0x7e, 0x0f, 0xe9,
	0xf6, 0xcc, 0x13, 0x3c, 0xdf, 0xf8, 0xd8, 0x20,
	0x63, 0xfb, 0x39, 0xe1, 0xde, 0xf9, 0xf0, 0x2e,
	0x62, 0x42, 0x6e, 0x26, 0x04, 0x4c, 0xce, 0xbe,
	0x06, 0x28, 0x07, 0x7d, 0x4e, 0x6d, 0xc5, 0x61,
	0x45, 0xbb, 0x91, 0xf7, 0x71, 0xf4, 0x32, 0x6d,
	0xf2, 0x39, 0x9f, 0x6a, 0xad, 0x7f, 0x84, 0x8a,
	0xad, 0xab, 0xf5, 0x7e, 0x54, 0xd4, 0x07, 0xd2,
	0x47, 0xd5, 0x35, 0x17, 0xe9, 0x8b, 0x5c, 0x01,
	0xd5, 0x6a, 0x03, 0xd3, 0x81, 0x2f, 0x50, 0x9e,
	0x9c, 0xcf, 0x10, 0xa1, 0x89, 0xe5, 0x9b, 0xa1,
	0xd8, 0xf7, 0x2e, 0xeb, 0x8a, 0x56, 0x9b, 0xb6,
	0x3c, 0xc0, 0x31, 0xd5, 0xbd, 0x79, 0xf0, 0x9c,
	0x3c, 0x98, 0x62, 0x0b, 0x82, 0x4b, 0x03, 0xe8,
	0x58, 0xcd, 0x39, 0x4b, 0x66, 0xc0, 0x6d, 0xe8,
	0xd3, 0xe2, 0xb4, 0x2c, 0xa7, 0xcf, 0x26, 0x3a,
	0xec, 0x6d, 0x0c, 0x73, 0xd3, 0x7a, 0x8b, 0x27,
	0x18, 0x4c, 0x29, 0x75, 0xfa, 0xf1, 0x9d, 0x5b,
	0x4b, 0x27, 0x46, 0xe5, 0xa6, 0x3e, 0xc5, 0xe8,
	0xb9, 0xe7, 0xdc, 0x7d, 0xb2, 0x15, 0x48, 0xe9,
	0xd6, 0xed, 0x53, 0x53, 0x71, 0x71, 0x3b, 0x09,
	0x37, 0x2d, 0x41, 0xe4, 0xe3, 0x7b, 0x32, 0x98,
	0xb1, 0xe6, 0xbd, 0xaf, 0x55, 0x4c, 0xc5, 0x6e,
	0xe4, 0x9a, 0x5a, 0xe3, 0x16, 0xaf, 0xf5, 0x4e,
	0x1f, 0xf6, 0xe1, 0x14, 0xbb, 0x4a, 0xe9, 0x54,
	0x1a, 0x76, 0x67, 0x0e, 0xd8, 0x1e, 0xbf, 0xd5,
	0xa3, 0x8d, 0x12, 0x7a, 0xae, 0x48, 0x37, 0x33,
	0x8c, 0xb5, 0x6b, 0xf7, 0x44, 0xc8, 0x36, 0x66,
	0x63, 0x00, 0x83, 0xcb, 0x6e, 0x76, 0xb8, 0xf3,
	0xb4, 0xe1, 0x39, 0x65, 0x28, 0xc3, 0xc6, 0xe5,
	0xca, 0x3b, 0x2f, 0x00, 0x26, 0x61, 0xda, 0x82,
	0xbd, 0xad, 0xa3, 0x25, 0x5d, 0x0c, 0xe4, 0xb9,
	0x07, 0x02, 0x03, 0x01, 0x00, 0x01
};

static const u8 pubkey_cn_test[TS_PUB_KEY_LEN] = {
	0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09,
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
	0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
	0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01,
	0x00, 0xbc, 0x77, 0x4f, 0xf4, 0x68, 0x76, 0xf4,
	0xcc, 0x72, 0xed, 0x68, 0x49, 0x24, 0xbf, 0x5a,
	0xd2, 0x8e, 0xa6, 0x9c, 0x2e, 0x81, 0x9f, 0x26,
	0x88, 0x39, 0xdf, 0xe3, 0x22, 0x47, 0x7c, 0xdb,
	0xe5, 0xb3, 0x60, 0xc0, 0xa8, 0xdf, 0xe6, 0xf7,
	0x65, 0x9e, 0xb9, 0x78, 0x73, 0xaa, 0x94, 0x78,
	0x5d, 0xe5, 0x44, 0x4a, 0xe4, 0xbc, 0x76, 0x1f,
	0xef, 0xf2, 0x72, 0x28, 0x06, 0xc1, 0x08, 0xae,
	0x52, 0x5f, 0xe4, 0xfb, 0xe7, 0x41, 0xcc, 0x18,
	0x0e, 0xf0, 0xf6, 0x56, 0x6f, 0x04, 0xee, 0x37,
	0x70, 0x72, 0x5c, 0xf1, 0xc2, 0xcd, 0xd0, 0x88,
	0xa2, 0xd2, 0x46, 0x3f, 0xc9, 0xb5, 0x35, 0x3e,
	0x2e, 0x81, 0xe5, 0x8e, 0x5a, 0x93, 0x39, 0xb7,
	0xb6, 0x59, 0xc4, 0x1f, 0x5a, 0x9d, 0xd3, 0x7d,
	0x57, 0x14, 0x2d, 0x2c, 0x35, 0x03, 0x63, 0x9a,
	0x38, 0xc4, 0xff, 0x41, 0xbc, 0x0a, 0x24, 0xd2,
	0x83, 0x1f, 0x72, 0x9e, 0xee, 0xd9, 0xd7, 0xe5,
	0x1e, 0x3e, 0x1b, 0xf3, 0xa5, 0x36, 0x7c, 0x15,
	0x7c, 0xf9, 0x6b, 0xee, 0xa4, 0xef, 0x82, 0x95,
	0x25, 0xb6, 0x1a, 0x30, 0x76, 0x7a, 0x26, 0x0c,
	0xfd, 0xe0, 0x21, 0xce, 0x14, 0x70, 0xe8, 0xd7,
	0xc0, 0xdd, 0xea, 0xb1, 0x2f, 0xe5, 0x2a, 0xf8,
	0x43, 0x75, 0x2b, 0xd0, 0x1d, 0xc5, 0x59, 0x8a,
	0xf2, 0xa3, 0x7d, 0x7b, 0x2f, 0xb6, 0xb4, 0x79,
	0x8d, 0xa0, 0xab, 0x90, 0x28, 0x7d, 0xf2, 0x91,
	0x50, 0xc3, 0x4b, 0xa7, 0xc6, 0xc7, 0xa1, 0xed,
	0x20, 0x83, 0xb1, 0xce, 0x90, 0x15, 0x35, 0xcb,
	0x3a, 0x4e, 0x80, 0xb2, 0x72, 0x98, 0x4a, 0x46,
	0x26, 0x40, 0x0c, 0x54, 0x08, 0xbe, 0x6a, 0xfe,
	0xfb, 0xf3, 0x3a, 0x99, 0x87, 0x2e, 0x24, 0xd2,
	0xb6, 0x40, 0x53, 0x95, 0x4e, 0x4d, 0xc9, 0xf4,
	0xf6, 0xef, 0x5a, 0xc5, 0x0e, 0x05, 0x9e, 0x9c,
	0x21, 0x02, 0x03, 0x01, 0x00, 0x01
};

/*
 * Table of known services that can be used for testing. Only entries that
 * are enabled may be used. If no entries are enabled, switching to a test
 * service is not supported.
 */
static struct test_service test_svc_table[] = {
	{ "us-test", "mqtt-dev.aylanetworks.com", pubkey_us_test,
		"pool.ntp.aylanetworks.com",
	},
	{ "cn-test", "mqtt-field.ayla.com.cn", pubkey_cn_test,
		"pool.ntp.ayla.com.cn",
	},
};

int test_svc_enabled(void)
{
	const struct test_service *ts;

	for (ts = test_svc_table; ts < ARRAY_END(test_svc_table);
	    ++ts) {
		if (ts->enable) {
			return 1;
		}
	}
	return 0;
}

struct test_service *test_svc_lookup(const char *nickname)
{
	struct test_service *ts;

	if (!nickname) {
		return NULL;
	}

	for (ts = test_svc_table; ts < ARRAY_END(test_svc_table);
	    ++ts) {
		if (!strcasecmp(nickname, ts->nickname)) {
			return ts;
		}
	}
	return NULL;
}

/*
 * Enable use of the specified test server.
 *
 * Returns 0 on success.
 * Returns -1, otherwise.
 */
int ada_client_test_svc_enable(const char *nickname)
{
	struct test_service *ts = test_svc_lookup(nickname);

	if (ts) {
		ts->enable = 1;
		client_log(LOG_DEBUG "test server %s enabled", ts->nickname);
		return 0;
	}
	return -1;
}

/*
 * Generate a unique identifier for the device by hashing DSN and the OEM
 * key. The same identifier will be generated each call provided these
 * factory configured items do not change. The OEM key is included such that
 * the unique id isn't feasible to generate from information readily
 * obtainable from a production device.
 */
static enum ada_err test_svc_unique_id_gen(char *buf, size_t length)
{
	struct al_hash_sha256_ctxt *ctx;
	u8 hash[32];
	char *oem_key;
	int oem_key_len;
	enum ada_err err = AE_OK;

	oem_key = calloc(1, CONF_OEM_KEY_MAX);
	if (!oem_key) {
		return AE_ALLOC;
	}
	oem_key_len = adap_conf_oem_key_get(oem_key, CONF_OEM_KEY_MAX);
	if (oem_key_len <= 0) {
		err = AE_ERR;
		goto exit;
	}

	ctx = al_hash_sha256_ctxt_alloc();
	if (!ctx) {
		err = AE_ALLOC;
		goto exit;
	}
	al_hash_sha256_ctxt_init(ctx);
	al_hash_sha256_add(ctx, conf_sys_dev_id, strlen(conf_sys_dev_id));
	al_hash_sha256_add(ctx, oem_key, oem_key_len);
	al_hash_sha256_final(ctx, hash);
	al_hash_sha256_ctxt_free(ctx);

	snprintf(buf, length, "ID%013llu", *(u64 *)hash % 10000000000000);
exit:
	free(oem_key);
	return err;
}

/*
 * Generate the test service setup key by hashing test service host nickname,
 * setup time, DSN and device factory public key. The same key can be
 * regenerated given the test setup inputs (nickname and setup_time) without
 * saving additional information.
 *
 * Returns key length on success. Returns a negative value on failure.
 */
static int test_svc_setup_key_gen(const char *svc_nickname, u32 setup_time,
    void *key, size_t length)
{
	struct al_hash_sha256_ctxt *ctx;
	char *pub_key;
	int pub_key_len;
	int rc;

	if (length < TS_SETUP_KEY_LEN) {
		return AE_LEN;
	}

	pub_key = calloc(1, CLIENT_CONF_PUB_KEY_LEN);
	if (!pub_key) {
		return AE_ALLOC;
	}

	pub_key_len = adap_conf_pub_key_get(pub_key, CLIENT_CONF_PUB_KEY_LEN);
	if (pub_key_len <= 0) {
		rc = AE_ERR;
		goto exit;
	}

	ctx = al_hash_sha256_ctxt_alloc();
	if (!ctx) {
		rc = AE_ALLOC;
		goto exit;
	}
	al_hash_sha256_ctxt_init(ctx);
	al_hash_sha256_add(ctx, conf_sys_dev_id, strlen(conf_sys_dev_id));
	al_hash_sha256_add(ctx, &setup_time, sizeof(setup_time));
	al_hash_sha256_add(ctx, svc_nickname, strlen(svc_nickname));
	al_hash_sha256_add(ctx, pub_key, pub_key_len);
	al_hash_sha256_final(ctx, key);
	al_hash_sha256_ctxt_free(ctx);
	rc = TS_SETUP_KEY_LEN;

exit:
	memset(pub_key, 0, CLIENT_CONF_PUB_KEY_LEN);
	free(pub_key);
	return rc;
}

static int test_svc_encrypt_pub(const void *pub_key, size_t pub_key_len,
    void *in, size_t in_len, void *out, size_t out_len)
{
	struct al_rsa_ctxt *ctx;
	size_t key_len;
	int rc = -1;

	ctx = al_rsa_ctxt_alloc();
	if (!ctx) {
		return rc;
	}

	key_len = al_rsa_pub_key_set(ctx, pub_key, pub_key_len);
	if (!key_len) {
		client_log(LOG_ERR "%s key set failed %d", __func__, rc);
		goto free_key;
	}

	rc = al_rsa_encrypt(ctx, in, in_len, out, out_len);
	if (rc < 0) {
		client_log(LOG_ERR "%s encrypt failed %d", __func__, rc);
		goto free_key;
	}

free_key:
	al_rsa_ctxt_free(ctx);
	return rc;
}

static int test_svc_data_encrypt(u8 *key, size_t key_len, u8 *iv,
    void *data, size_t size, size_t data_len)
{
	struct al_aes_ctxt *aes_ctx;
	int pad;
	int rc;

	/*
	 * Pad the data out to multiple of 16 bytes using PKCS#7 padding, as
	 * defined in RFC5652 Sec. 6.3. All messages are padded, even those
	 * that are already a multiple of 16 bytes. The value in pad bytes is
	 * the total number of bytes of padding such that the padding can be
	 * stripped by removing that number of bytes from the end of the buffer.
	 */
	pad = 16 - (data_len & 0xf);
	if (data_len + pad > size) {
		client_log(LOG_ERR "%s length error", __func__);
		return -1;
	}
	memset(data + data_len, pad, pad);
	data_len += pad;

	aes_ctx = al_aes_ctxt_alloc();
	if (!aes_ctx) {
		client_log(LOG_ERR "%s AES alloc failed", __func__);
		return -1;
	}
	rc = al_aes_cbc_key_set(aes_ctx, key, key_len, iv, 0);
	if (rc) {
		client_log(LOG_ERR "%s key set failure", __func__);
		goto err;
	}
	rc = al_aes_cbc_encrypt(aes_ctx, data, data, data_len);
	if (rc) {
		client_log(LOG_ERR "%s encrypt failure", __func__);
		goto err;
	}
	al_aes_ctxt_free(aes_ctx);
	return data_len;
err:
	al_aes_ctxt_free(aes_ctx);
	return -1;
}

static int test_svc_data_decrypt(u8 *key, size_t key_len, u8 *iv,
    void *data, size_t data_len)
{
	struct al_aes_ctxt *aes_ctx;
	int rc;
	int i;
	u8 pad;
	u8 *ptr;

	if (data_len < 16 || (data_len & 0xf)) {
		client_log(LOG_WARN "%s length error", __func__);
		return -1;
	}
	aes_ctx = al_aes_ctxt_alloc();
	if (!aes_ctx) {
		client_log(LOG_ERR "%s AES alloc failed", __func__);
		return -1;
	}
	rc = al_aes_cbc_key_set(aes_ctx, key, key_len, iv, 1);
	if (rc) {
		client_log(LOG_ERR "%s key set failure", __func__);
		goto err;
	}
	rc = al_aes_cbc_decrypt(aes_ctx, data, data, data_len);
	if (rc) {
		client_log(LOG_WARN "%s decrypt failure", __func__);
		goto err;
	}
	ptr = (u8 *)data + data_len - 1;
	pad = *ptr--;
	if (pad > 16) {
padding_error:
		client_log(LOG_WARN "%s padding error", __func__);
		goto err;
	}
	for (i = 1; i < pad; ++i) {
		if (*ptr != pad) {
			goto padding_error;
		}
	}
	al_aes_ctxt_free(aes_ctx);
	return data_len - pad;

err:
	al_aes_ctxt_free(aes_ctx);
	return -1;
}

enum ada_err test_svc_setup_data_get(struct ts_setup_info *setup_info)
{
	u8 setup_key[TS_SETUP_KEY_LEN];
	size_t setup_key_len = sizeof(setup_key);
	size_t setup_data_len;
	struct test_service *ts;
	int rc;

	if (!client_conf_server_change_en()) {
		client_log(LOG_WARN "%s invalid state", __func__);
		return AE_INVAL_STATE;
	}

	/*
	 * Find the test service entry and check if it is enabled.
	 */
	ts = test_svc_lookup(setup_info->svc_name);
	if (!ts || !ts->enable) {
		return AE_NOT_FOUND;
	}

	setup_info->enc_setup_key_len = sizeof(setup_info->enc_setup_key);
	setup_info->data_len = sizeof(setup_info->data);

	/*
	 * Generate the setup data JSON object.
	 */
	rc = test_svc_unique_id_gen(setup_info->unique_id,
	    sizeof(setup_info->unique_id));
	if (rc) {
		return rc;
	}
	setup_data_len = snprintf((char *)setup_info->data,
	    setup_info->data_len,
	    "{\"i\":\"%s\",\"p\":\"%s\",\"m\":\"%s\",\"t\":%lu}",
	    setup_info->unique_id, conf_sys_model, oem_model,
	    setup_info->setup_time);
	if (setup_data_len >= setup_info->data_len) {
		return AE_LEN;
	}

	/*
	 * Generate the test setup key.
	 */
	rc = test_svc_setup_key_gen(setup_info->svc_name,
	    setup_info->setup_time, setup_key, setup_key_len);
	if (rc < 0) {
		return AE_ERR;
	}

	/*
	 * Encrypt the setup data with the test setup key.
	 */
	al_random_fill(setup_info->iv, sizeof(setup_info->iv));
	rc = test_svc_data_encrypt(setup_key, setup_key_len, setup_info->iv,
	    setup_info->data, setup_info->data_len, setup_data_len);
	if (rc < 0) {
		return AE_ERR;
	}
	setup_info->data_len = rc;

	/*
	 * Encrypt the test setup key with the public key of the test service.
	 */
	rc = test_svc_encrypt_pub(ts->pub_key, TS_PUB_KEY_LEN,
	    setup_key, setup_key_len, setup_info->enc_setup_key,
	    setup_info->enc_setup_key_len);
	if (rc < 0) {
		return AE_ERR;
	}
	setup_info->enc_setup_key_len = rc;

	return AE_OK;
}

static enum ada_err test_svc_ticket_decrypt(struct ts_ticket_info *ticket_info)
{
	u8 setup_key[TS_SETUP_KEY_LEN];
	u8 digest_calc[32];
	u8 digest_recv[32];
	struct al_hash_sha256_ctxt *ctx;
	struct test_service *ts;
	struct al_rsa_ctxt *key;
	size_t setup_key_len = sizeof(setup_key);
	int rc;

	/*
	 * Validate the signature.
	 */
	ctx = al_hash_sha256_ctxt_alloc();
	if (!ctx) {
		return AE_ALLOC;
	}
	al_hash_sha256_ctxt_init(ctx);
	al_hash_sha256_add(ctx, ticket_info->conf, ticket_info->conf_len);
	al_hash_sha256_final(ctx, digest_calc);
	al_hash_sha256_ctxt_free(ctx);

	ts = test_svc_lookup(ticket_info->host);
	if (!ts) {
		client_log(LOG_WARN "%s test service %s not found",
		   __func__, ticket_info->host);
		return AE_INVAL_VAL;
	}

	if (!ts->enable) {
		client_log(LOG_WARN "%s test service %s not enabled",
		    __func__, ticket_info->host);
		return AE_INVAL_VAL;
	}

	key = al_rsa_ctxt_alloc();
	if (!key) {
		client_log(LOG_ERR "%s key alloc failed", __func__);
		return AE_ERR;
	}

	rc = al_rsa_pub_key_set(key, ts->pub_key, TS_PUB_KEY_LEN);
	if (!rc) {
		client_log(LOG_ERR "%s key set failed %d", __func__, rc);
		return AE_ERR;
	}

	rc = al_rsa_verify(key, ticket_info->signature,
	    sizeof(ticket_info->signature), digest_recv, sizeof(digest_recv));
	al_rsa_ctxt_free(key);
	if (rc < 0) {
		client_log(LOG_WARN "%s signature decrypt failed %d", __func__,
		    rc);
		return AE_ERR;
	}

	if (memcmp(digest_calc, digest_recv, sizeof(digest_calc))) {
		client_log(LOG_WARN "%s invalid signature", __func__);
		return AE_ERR;
	}

	/*
	 * Regenerate the test setup key.
	 */
	rc = test_svc_setup_key_gen(ticket_info->host, ticket_info->setup_time,
	    setup_key, setup_key_len);
	if (rc < 0) {
		return AE_ERR;
	}

	/*
	 * Decrypt the config payload.
	 */
	rc = test_svc_data_decrypt(setup_key, setup_key_len, ticket_info->iv,
	    ticket_info->conf, ticket_info->conf_len);
	if (rc < 0) {
		return AE_ERR;
	}
	ticket_info->conf_len = rc;

	return AE_OK;
}

static enum ada_err test_svc_ticket_parse(const void *ticket,
    size_t ticket_len, struct ts_ticket_info *ticket_info)
{
	const struct ame_decoder *dec = &ame_json_dec;
	struct ame_decoder_state *ds;
	struct ame_kvp *root;
	const struct ame_key *ame_key;
	enum ada_err err;
	size_t length;

	ds = ame_decoder_state_alloc(TS_KVP_STACK_SZ);
	if (!ds) {
		return AE_ALLOC;
	}

	/*
	 * Parse the outer JSON.
	 */
	ame_decoder_buffer_set(ds, ticket, ticket_len);
	err = dec->parse(ds, AME_PARSE_FULL, &root);
	if (err) {
		client_log(LOG_ERR "%s err %d", __func__, err);
		goto exit;
	}

	length = sizeof(ticket_info->t_dsn_ct);
	err = dec->get_utf8(root, &k_dsn, ticket_info->t_dsn_ct, &length);
	if (err) {
		ame_key = &k_dsn;
decode_error_exit:
		client_log(LOG_ERR "%s get %s failed err %s(%d)", __func__,
		    ame_key->display_name, ada_err_string(err), err);
		goto exit;
	}
	length = sizeof(ticket_info->host);
	err = dec->get_utf8(root, &k_host, ticket_info->host,
	    &length);
	if (err) {
		ame_key = &k_host;
		goto decode_error_exit;
	}
	err = dec->get_u32(root, &k_time, &ticket_info->setup_time);
	if (err) {
		ame_key = &k_time;
		goto decode_error_exit;
	}
	err = dec->get_u32(root, &k_exp, &ticket_info->exp_time);
	if (err) {
		ame_key = &k_exp;
		goto decode_error_exit;
	}

	length = sizeof(ticket_info->iv);
	err = dec->get_opaque(root, &k_iv, ticket_info->iv,
	    &length);
	if (err) {
		ame_key = &k_iv;
		goto decode_error_exit;
	}
	ticket_info->conf_len = sizeof(ticket_info->conf);
	err = dec->get_opaque(root, &k_conf, ticket_info->conf,
	    &ticket_info->conf_len);
	if (err) {
		ame_key = &k_conf;
		goto decode_error_exit;
	}
	ticket_info->sig_len = sizeof(ticket_info->signature);
	err = dec->get_opaque(root, &k_sig, ticket_info->signature,
	    &ticket_info->sig_len);
	if (err) {
		ame_key = &k_sig;
		goto decode_error_exit;
	}

	/*
	 * Decrypt the config data.
	 */
	err = test_svc_ticket_decrypt(ticket_info);
	if (err) {
		goto exit;
	}

	/*
	 * Parse the decrypted config data JSON.
	 */
	ame_decoder_reset(ds);
	ame_decoder_buffer_set(ds, ticket_info->conf, ticket_info->conf_len);
	err = dec->parse(ds, AME_PARSE_FULL, &root);
	if (err) {
		client_log(LOG_ERR "%s err %d", __func__, err);
		goto exit;
	}
	length = sizeof(ticket_info->unique_id);
	err = dec->get_utf8(root, &k_id, ticket_info->unique_id, &length);
	if (err) {
		ame_key = &k_id;
		goto decode_error_exit;
	}
	length = sizeof(ticket_info->t_dsn);
	err = dec->get_utf8(root, &k_dsn, ticket_info->t_dsn, &length);
	if (err) {
		ame_key = &k_dsn;
		goto decode_error_exit;
	}
	length = sizeof(ticket_info->t_pub_key);
	err = dec->get_utf8(root, &k_pub_key, ticket_info->t_pub_key,
	    &length);
	if (err) {
		ame_key = &k_pub_key;
		goto decode_error_exit;
	}
	length = sizeof(ticket_info->t_oem);
	err = dec->get_utf8(root, &k_oem, ticket_info->t_oem, &length);
	if (err) {
		ame_key = &k_oem;
		goto decode_error_exit;
	}
	ticket_info->t_oem_key_len = sizeof(ticket_info->t_oem_key);
	err = dec->get_opaque(root, &k_oem_key, ticket_info->t_oem_key,
	    &ticket_info->t_oem_key_len);
	if (err) {
		ame_key = &k_oem_key;
		goto decode_error_exit;
	}

exit:
	ame_decoder_state_free(ds);
	return err;

}

enum ada_err test_svc_ticket_apply(const void *ticket, size_t ticket_len)
{
	enum ada_err err;
	int rc;
	struct ts_ticket_info *ticket_info;
	char id_str[CONF_DEV_ID_MAX];

	/*
	 * Check if this operation is currently permitted.
	 */
	if (!client_conf_server_change_en()) {
		client_log(LOG_WARN "%s invalid state", __func__);
		return AE_INVAL_STATE;
	}

	ticket_info = (struct ts_ticket_info *)
	    al_os_mem_alloc(sizeof(struct ts_ticket_info));
	if (!ticket_info) {
		err = AE_ALLOC;
		goto exit;
	}

	err = test_svc_ticket_parse(ticket, ticket_len, ticket_info);
	if (err) {
		goto exit;
	}

	/*
	 * Validate the ticket.
	 */
	if (strcmp(ticket_info->t_dsn_ct, ticket_info->t_dsn)) {
		client_log(LOG_WARN "%s DSN mismatch", __func__);
		err = AE_INVAL_VAL;
		goto exit;
	}

	if (strlen(ticket_info->t_dsn) >= CONF_DEV_ID_MAX) {
		client_log(LOG_WARN "%s invalid DSN", __func__);
		err = AE_INVAL_VAL;
		goto exit;
	}

	test_svc_unique_id_gen(id_str, sizeof(id_str));
	if (strcmp(id_str, ticket_info->unique_id)) {
		client_log(LOG_WARN "%s unique id mismatch", __func__);
		err = AE_INVAL_VAL;
		goto exit;
	}

	client_log(LOG_INFO "applying test ticket: %s %s",
	    ticket_info->host, ticket_info->t_dsn);

	client_unlock();
	rc = client_set_server(ticket_info->host);
	client_lock();
	if (rc) {
		err = AE_INVAL_VAL;
		goto exit;
	}
	strcpy(conf_sys_dev_id, ticket_info->t_dsn);

	err = oem_id_set(ticket_info->t_oem);
	if (err) {
		client_log(LOG_WARN "%s invalid OEM ID", __func__);
		goto exit;
	}

	if (client_conf_pub_key_set_base64(ticket_info->t_pub_key)) {
		client_log(LOG_WARN "%s public key set failed", __func__);
		err = AE_INVAL_VAL;
		goto exit;
	}

	err = oem_enc_key_set(ticket_info->t_oem_key,
	    ticket_info->t_oem_key_len);
	if (err) {
		client_log(LOG_WARN "%s OEM key set failed", __func__);
		goto exit;
	}

	conf_sys_test_mode_update(1);	/* set test mode flag in config */

	if (conf_save_config()) {
		client_log(LOG_WARN "%s OEM key set failed", __func__);
		err = AE_ERR;
	}

exit:
	free(ticket_info);
	return err;
}
#endif	/* AYLA_TEST_SERVICE_SUPPORT */

