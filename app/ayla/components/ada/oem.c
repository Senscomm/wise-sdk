/*
 * Copyright 2012-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/tlv.h>
#include <ayla/conf_token.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/parse.h>
#include <ada/client.h>
#include <ada/ada_conf.h>
#include "oem_int.h"

/*
 * The template version can be set using the global template_version[] or
 * using ada_client_set_oem_version().
 * The global is allocated by the host app in ADA.
 */
const char *ada_host_template_version = template_version;

/*
 * /oem configuration items.
 */
u8 oem_key[CONF_OEM_KEY_MAX];	/* encrypted OEM key */
u16 oem_key_len;		/* length of OEM key */

static const struct ada_conf_item oem_conf_items[] = {
	{ "oem/oem", ATLV_UTF8, oem, sizeof(oem)},
	{ "oem/model", ATLV_UTF8, oem_model, sizeof(oem_model)},
	{ ADA_CONF_OEM_KEY, ATLV_FILE, oem_key, sizeof(oem_key)},
	{ NULL }
};

/*
 * Persist the OEM ID and OEM model only if they were entered on the CLI.
 * This allows a compiled in version to be set before the configuration
 * is loaded without being overwritten unless the CLI was used.
 * If an empty string is entered with the CLI, the compiled-in default,
 * if non-empty, is used.
 */
static u8 oem_persist_id;
static u8 oem_persist_model;

/*
 * Export OEM configuration items.
 */
static void oem_export(void)
{
	conf_put_str_ne(CT_oem, oem_persist_id ? oem : "");
	conf_put_str_ne(CT_model, oem_persist_model ? oem_model : "");
	conf_put(CT_key, ATLV_FILE, oem_key, oem_key_len);
}

static void oem_version_export(void *arg)
{
	oem_export();
}

void oem_save(void)
{
	conf_persist(CT_oem, oem_version_export, NULL);
}

/*
 * Set encrypted OEM key.
 *
 * The "key" is an encrypted string (not an encryption key per se), which is
 * used to bind the DSN to an OEM Id and OEM model such that the device can
 * securely claim a DSN belongs to the OEM who manufactured the device. The
 * encryption is done using the device's public key, which is factory
 * configured along with the DSN. If the cloud can decrypt the string, it
 * trusts that the DSN is owned by the device and associates the OEM Id and
 * model with the DSN.
 *
 * The OEM key is an encrypted string:
 *
 * plaintext = "<utc_seconds> <oem_secret> <oem_id> <oem_model>"
 *
 * OEM key = encrypt(plaintext)
 *
 * oem_model_for_key is used for the encrypted string.
 * If the model is "*" in the encrypted string, the plaintext
 * oem model may be changed later without re-encrypting the key.
 */
enum conf_error oem_set_key(const char *oem_secret, size_t sec_len,
    const char *model)
{
	char buf[CONF_OEM_KEY_MAX + 1];
	char pub_key[CLIENT_CONF_PUB_KEY_LEN] = {0};
	int pub_key_len;
	enum al_err err;
	int rc;
	size_t len = sec_len;

	if (len == '\0') {
		oem_key_len = 0;
		goto out;
	}
	if (len > sizeof(buf) - 1) {
		return CONF_ERR_RANGE;
	}
	memcpy(buf, oem_secret, len);
	snprintf(buf + len, sizeof(buf) - len, " %s %s", oem, model);

	pub_key_len = adap_conf_pub_key_get(pub_key, sizeof(pub_key));
	if (pub_key_len <= 0) {
		conf_log(LOG_ERR "pub key not set");
		return CONF_ERR_RANGE;
	}

	rc = client_auth_encrypt(pub_key, pub_key_len,
	    oem_key, CONF_OEM_KEY_MAX, buf);
	if (rc < 0) {
		conf_log(LOG_ERR "oem_key encryption failed.  rc %d", rc);
		return CONF_ERR_RANGE;
	}
	oem_key_len = rc;
out:
	err = conf_persist_set(ADA_CONF_OEM_KEY, oem_key, oem_key_len);
	if (err) {
		conf_log(LOG_ERR "oem_key save failed");
		return CONF_ERR_WRITE;
	}
	return CONF_ERR_NONE;
}

/*
 * Set (and persist) the previously encrypted OEM key.
 */
enum ada_err oem_enc_key_set(u8 *key, size_t len)
{
	if (len > CONF_OEM_KEY_MAX) {
		return AE_INVAL_VAL;
	}
	memcpy(oem_key, key, len);
	oem_key_len = len;
	if (conf_persist_set(ADA_CONF_OEM_KEY, oem_key, oem_key_len)) {
		return AE_ERR;
	}
	return AE_OK;
}

/*
 * Set OEM items
 */
static enum conf_error
oem_set(int src, enum conf_token *token, size_t len, struct ayla_tlv *tlv)
{
	int op;

	if (len != 1) {
		goto err;
	}

	if (token[0] == CT_model) {
		op = CONF_OP_SS_OEM_MODEL;
	} else {
		op = CONF_OP_SS_OEM;
	}
	if (conf_access(op | CONF_OP_WRITE | src)) {
		return CONF_ERR_PERM;
	}

	switch (token[0]) {
	case CT_oem:
		conf_get(tlv, ATLV_UTF8, oem, sizeof(oem) - 1);
		oem_persist_id = 1;
		break;
	case CT_model:
		conf_get(tlv, ATLV_UTF8, oem_model, sizeof(oem_model) - 1);
		/* reset the client if set from MCU */
		if (src == CONF_OP_SRC_MCU) {
			client_server_reset();
		}
		oem_persist_model = 1;
		break;
	case CT_key:
		if (tlv->type == ATLV_UTF8) {
			return oem_set_key((char *)(tlv + 1), tlv->len,
			    oem_model);
		}
		oem_key_len = (u16)conf_get(tlv, ATLV_FILE,
		    oem_key, sizeof(oem_key));
		break;
	default:
		goto err;
	}
	return CONF_ERR_NONE;
err:
	return CONF_ERR_PATH;
}

/*
 * Handle get of sys or id config setting.
 */
static enum conf_error oem_get(int src, enum conf_token *token, size_t len)
{
	if (len != 1) {
		goto err;
	}
	if (conf_access(CONF_OP_SS_OEM | CONF_OP_READ | src)) {
		return CONF_ERR_PERM;
	}
	switch (token[0]) {
	case CT_oem:
		conf_resp_str(oem);
		break;
	case CT_model:
		conf_resp_str(oem_model);
		break;
	case CT_key:
		conf_resp(ATLV_FILE, oem_key, oem_key_len);
		break;
	default:
		goto err;
	}
	return CONF_ERR_NONE;
err:
	return CONF_ERR_PATH;
}

const struct conf_entry conf_oem_entry = {
	.token = CT_oem,
	.export = oem_export,
	.set = oem_set,
	.get = oem_get,
};

void oem_conf_load(void)
{
	const struct ada_conf_item *item;
	int len = 0;

	for (item = oem_conf_items; item->name; item++) {
		len = ada_conf_get_item(item);
		if (!strcmp(item->name, ADA_CONF_OEM_KEY) && len > 0) {
			oem_key_len = len;
		}

		/*
		 * Set persist flags for OEM ID and model if they come
		 * from the configuration.  Otherwise they get deleted.
		 */
		if (len > 0)  {
			if (!strcmp(item->name, "oem/oem")) {
				oem_persist_id = 1;
			}
			if (!strcmp(item->name, "oem/model")) {
				oem_persist_model = 1;
			}
		}
	}
}

/*
 * Set OEM or OEM model.
 * The maximum string length is CONF_OEM_MAX.
 * Returns zero on success, non-zero if invalid or too long.
 */
static int oem_set_string(char *dest, char *src)
{
	int len;

	if (!hostname_valid(src)) {
		return -1;
	}
	len = snprintf(dest, CONF_OEM_MAX + 1, "%s", src);
	if (len > CONF_OEM_MAX) {
		return -1;
	}
	return 0;
}

enum ada_err oem_id_set(char *id)
{
	if (!client_conf_server_change_en() && !mfg_or_setup_mode_active()) {
		return AE_INVAL_STATE;
	}
	if (oem_set_string(oem, id)) {
		return AE_INVAL_VAL;
	}
	oem_persist_id = 1;
	return AE_OK;
}

/*
 * Handle OEM CLI commands.
 */
void ada_conf_oem_cli(int argc, char **argv)
{
	char *model;

	if (argc <= 1 || !strcmp(argv[1], "help")) {
		printcli("oem: \"%s\"", oem);
		printcli("oem_model: \"%s\"", oem_model);
		printcli("oem_key: (%s set)", oem_key_len ? "is" : "not");
		return;
	}
	if (!strcmp(argv[1], "help")) {
		goto usage;
	}

	/*
	 * The oem id and key can be configured in the factory or changed for
	 * testing with an alternate service. The OEM model can only be changed
	 * in setup mode.
	 */
	if (!client_conf_server_change_en() && !mfg_or_setup_mode_ok()) {
		return;
	}
	if (argc == 2) {
		if (!strcmp(argv[1], "model") || !strcmp(argv[1], "key")) {
			printcli("error: invalid oem value");
			goto usage;	/* missed intended third argument */
		}
		if (oem_set_string(oem, argv[1])) {
			printcli("error: invalid oem value");
			return;
		}
		oem_persist_id = 1;
		return;
	}
	if (argc == 3 && !strcmp(argv[1], "model")) {
		if (!mfg_or_setup_mode_ok()) {
			return;
		}
		if (oem_set_string(oem_model, argv[2])) {
			printcli("error: invalid model value");
			return;
		}
		oem_persist_model = 1;
		return;
	}
	if ((argc == 3 || argc == 4) && !strcmp(argv[1], "key")) {
		model = argc == 4 ? argv[3] : oem_model;
		if (oem[0] == '\0' || model[0] == '\0') {
			printcli("error: oem and oem model "
			    "must be set before key");
			return;
		}
		/*
		 * Generate and set the OEM key given the OEM secret provided
		 * by the user.
		 */
		oem_set_key(argv[2], strlen(argv[2]), model);
		return;
	}
usage:
	printcli("usage: oem [help|show]");
	printcli("   or: oem <OEM-ID>");
	printcli("   or: oem model <OEM-model>");
	printcli("   or: oem key <OEM-secret> [<OEM-model>]");
}

int adap_conf_oem_key_get(void *buf, size_t len)
{
	if (oem_key_len > len) {
		return -1;
	}
	memcpy(buf, oem_key, oem_key_len);
	return (int)oem_key_len;
}

void ada_client_set_oem_version(const char *ver)
{
	ada_host_template_version = ver;
}
