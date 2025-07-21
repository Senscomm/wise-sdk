/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifdef AYLA_IN_FIELD_PROVISION_SUPPORT
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <al/al_os_mem.h>
#include <al/al_random.h>
#include <al/al_rsa.h>
#include <al/al_persist.h>
#include <ayla/base64.h>
#include <ayla/conf.h>
#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ada/ada.h>
#include <ada/client.h>
#include <ada/server_req.h>
#include "client_int.h"
#include "client_lock.h"
#include "oem_int.h"

/*
 * In-field provisioning config item names.
 * These are chosen to use existing tokens.
 */
#define CLIENT_IFP_CONF_KEY_ID	"id/oem/key/id"
#define CLIENT_IFP_CONF_OEM_SIG	"id/oem/key"

/*
 * Calculate device signature based on hardware information
 */
#define BASE64_SRC_LEN(x) ((((x) + 3) / 4) * 3)
#define SIG_RAND_BASE64_LEN 8
#define SIG_RAND_LEN BASE64_SRC_LEN(SIG_RAND_BASE64_LEN)
#define SIG_RAW_LEN (ADA_CONF_KEYID_MAX + ADA_CONF_HWID_MAX + \
    ADA_CONF_OEM_MAX + SIG_RAND_BASE64_LEN + 3)

/*
 * Accumulated parse data to be persisted.
 */
struct client_ifp_parse {
	char *dsn;		/* DSN received */
	char *key;		/* public key in base64 */
	u8 *oem_key;		/* binary OEM cipher (encrypted OEM secret) */
	size_t oem_key_len;	/* length of OEM key */
};
static struct client_ifp_parse client_ifp_parse;

static void client_ifp_parse_reset(void)
{
	struct client_ifp_parse *ifp = &client_ifp_parse;

	al_os_mem_free(ifp->dsn);
	al_os_mem_free(ifp->key);
	al_os_mem_free(ifp->oem_key);
	memset(&ifp, 0, sizeof(ifp));
}

/*
 * Generate random string.
 */
static int client_ifp_random_get(char *b64, size_t b64_len)
{
	u8 base[SIG_RAND_LEN];
	size_t out_len;
	int rc;

	rc = al_random_fill(base, sizeof(base));
	if (rc) {
		return -1;
	}

	out_len = b64_len;
	rc = ayla_base64_encode(base, sizeof(base), b64, &out_len);
	if (rc != 0 || out_len >= b64_len) {
		return -1;
	}
	return 0;
}

int ada_client_cal_signature(const char *oem_id, const char *hw_id,
		const char *key_id, const char *prv_key, size_t key_len,
		char *sig, size_t sig_len)
{
	struct al_rsa_ctxt *ctx = NULL;
	char base[SIG_RAND_BASE64_LEN + 1];
	char raw[SIG_RAW_LEN + 1];
	int raw_len;
	int rc;

	rc = client_ifp_random_get(base, sizeof(base));
	if (rc) {
		return rc;
	}

	/* cat raw string */
	raw_len = snprintf(raw, sizeof(raw), "%s %s %s %s", oem_id, hw_id,
	    key_id, base);

	/* calculate rsa */
	ctx = al_rsa_ctxt_alloc();
	if (!ctx) {
		rc = -1;
		goto on_exit;
	}

	rc = al_rsa_prv_key_set(ctx, prv_key, key_len);
	if (rc != CLIENT_CONF_RSA_MOD_SIZE) {
		rc = -1;
		goto on_exit;
	}

	rc = al_rsa_sign(ctx, raw, raw_len, sig, sig_len);
	if (rc != CLIENT_CONF_RSA_MOD_SIZE) {
		rc = -1;
		goto on_exit;
	}

	rc = 0;
on_exit:
	al_rsa_ctxt_free(ctx);
	return rc;
}

static enum ada_err client_ifp_conf_get_key_id(void)
{
	struct ada_conf *cf = &ada_conf;
	char *key_id;
	size_t len;
	ssize_t rlen;

	if (cf->provision_key_id) {
		return AE_OK;
	}
	len = ADA_CONF_KEYID_MAX + 1;
	key_id = al_os_mem_alloc(len);
	if (!key_id) {
		return AE_ALLOC;
	}
	rlen = conf_persist_get(CLIENT_IFP_CONF_KEY_ID, key_id, len);
	if (rlen < 0 || rlen >= len) {
		al_os_mem_free(key_id);
		return AE_INVAL_STATE;
	}
	cf->provision_key_id = key_id;
	return AE_OK;
}

static enum ada_err client_ifp_conf_get_oem_sig(void)
{
	struct ada_conf *cf = &ada_conf;
	char *b64 = NULL;
	char *sig = NULL;
	size_t len;
	ssize_t rlen;
	int rc;

	if (cf->oem_signature && cf->oem_sig_len) {
		return AE_OK;
	}

	len = BASE64_LEN_EXPAND(ADA_CONF_OEM_SIGNATURE_MAX) + 1;
	b64 = al_os_mem_alloc(len);
	if (!b64) {
		goto alloc_fail;
	}
	rlen = conf_persist_get(CLIENT_IFP_CONF_OEM_SIG, b64, len);
	if (rlen <= 0 || rlen >= len) {
		goto no_sig;
	}
	len = ADA_CONF_OEM_SIGNATURE_MAX;
	sig = al_os_mem_alloc(len);
	if (!sig) {
		goto alloc_fail;
	}
	rc = ayla_base64_decode(b64, rlen, sig, &len);
	if (rc) {
		client_log(LOG_ERR "%s: decode failed", __func__);
		goto no_sig;
	}
	al_os_mem_free(b64);
	cf->oem_signature = sig;
	cf->oem_sig_len = len;
	return AE_OK;

alloc_fail:
	client_log(LOG_ERR "%s: alloc failed", __func__);
no_sig:
	al_os_mem_free(b64);
	al_os_mem_free(sig);
	return AE_INVAL_STATE;
}

/*
 * Read configuration and check if in-field provisioning can be done.
 */
static enum ada_err client_ifp_conf_get(void)
{
	enum ada_err err;

	if (!oem_model) {
		client_log(LOG_ERR "%s oem_model not set", __func__);
		return AE_INVAL_STATE;
	}
	err = client_ifp_conf_get_key_id();
	if (err) {
		client_log(LOG_ERR "%s key ID not set", __func__);
		return err;
	}
	err = client_ifp_conf_get_oem_sig();
	if (err) {
		client_log(LOG_ERR "%s OEM signature not set", __func__);
		return err;
	}
	return AE_OK;
}

/*
 * Handle parse of received DSN.
 */
static int client_idf_accept_dsn(const struct ame_decoder *dec, void *arg,
		struct ame_kvp *kvp)
{
	struct client_ifp_parse *ifp = &client_ifp_parse;
	size_t len;
	size_t out_len;
	enum ada_err err;

	len = kvp->value_length + 1;
	al_os_mem_free(ifp->dsn);
	ifp->dsn = al_os_mem_alloc(len);
	if (!ifp->dsn) {
		return 0;
	}
	out_len = len;
	err = dec->get_utf8(kvp, NULL, ifp->dsn, &out_len);
	if (err || out_len >= len) {
		client_log(LOG_ERR "%s: parse failed", __func__);
		return 0;
	}
	return 0;
}

static size_t base64_align(char *d, char *s, size_t len)
{
	size_t r_len = 0;

	while (len > 0) {
		if ((*s >= 'A' && *s <= 'Z') ||
		    (*s >= 'a' && *s <= 'z') ||
		    (*s >= '0' && *s <= '9') ||
		    (*s == '+') ||
		    (*s == '/') ||
		    (*s == '=')) {
			*d = *s;
			d++;
			r_len++;
		}
		s++;
		len--;
		if (*s == '\0') {
			break;
		}
	}
	*d = '\0';
	return r_len;
}

static int client_idf_accept_pub_key(const struct ame_decoder *dec, void *arg,
		struct ame_kvp *kvp)
{
	struct client_ifp_parse *ifp = &client_ifp_parse;
	char *key;
	u8 bin_key[CLIENT_CONF_PUB_KEY_LEN];
	size_t len;
	size_t out_len;
	size_t bin_len = sizeof(bin_key);
	enum ada_err err;
	const char key_start[] = "-----BEGIN RSA PUBLIC KEY-----";
	const char key_end[] = "-----END RSA PUBLIC KEY-----";
	int rc;

	len = kvp->value_length;
	al_os_mem_free(ifp->key);
	ifp->key = NULL;
	key = al_os_mem_alloc(len);
	if (!key) {
		client_log(LOG_ERR "%s: alloc len %zu failed", __func__, len);
		return 0;
	}

	out_len = len;
	err = dec->get_utf8(kvp, NULL, key, &out_len);
	if (err || out_len >= len) {
		client_log(LOG_ERR "%s: err %d len %zu", __func__, err, len);
		al_os_mem_free(key);
		return 0;
	}
	len = out_len;

	/*
	 * Skip '-----BEGIN RSA PUBLIC KEY-----' at start and
	 * remove '-----END RSA PUBLIC KEY-----' at end, if present.
	 */
	if (len > sizeof(key_start) - 1 + sizeof(key_end) - 1 &&
	    !memcmp(key, key_start, sizeof(key_start) - 1)) {
		len -= sizeof(key_start) - 1 + sizeof(key_end) - 1;
		len = base64_align(key, key + sizeof(key_start) - 1, len);
	}

	bin_len = sizeof(bin_key);
	rc = ayla_base64_decode(key, len, bin_key, &bin_len);
	if (rc) {
		client_log(LOG_ERR "%s: base64 decode err", __func__);
		al_os_mem_free(key);
		return 0;
	}
	ifp->key = key;
	return 0;
}

static int client_idf_accept_oem_key(const struct ame_decoder *dec, void *arg,
		struct ame_kvp *kvp)
{
	struct client_ifp_parse *ifp = &client_ifp_parse;
	char *key;
	size_t len;
	size_t out_len;
	u8 *bin_key = NULL;
	size_t bin_len;
	enum ada_err err;
	int rc;

	al_os_mem_free(ifp->oem_key);
	ifp->oem_key = NULL;
	ifp->oem_key_len = 0;

	len = kvp->value_length + 1;
	key = al_os_mem_alloc(len);
	if (!key) {
		client_log(LOG_ERR "%s: key alloc failed", __func__);
		return 0;
	}

	out_len = len;
	err = dec->get_utf8(kvp, NULL, key, &out_len);
	if (err || out_len >= len) {
		client_log(LOG_ERR "%s: err %d len %zu", __func__, err, len);
		goto fail;
	}
	len = out_len;

	bin_len = CONF_OEM_KEY_MAX;
	bin_key = al_os_mem_alloc(bin_len);
	if (!bin_key) {
		client_log(LOG_ERR "%s: bin alloc failed", __func__);
		goto fail;
	}
	rc = ayla_base64_decode(key, len, bin_key, &bin_len);
	if (rc) {
		client_log(LOG_ERR "%s: base64 decode err", __func__);
		goto fail;
	}
	ifp->oem_key = bin_key;
	ifp->oem_key_len = bin_len;
	al_os_mem_free(key);
	return 0;
fail:
	al_os_mem_free(bin_key);
	al_os_mem_free(key);
	return 0;
}

/*
 * JSON Parser tags for in-field response.
 */
static const struct ame_tag client_idf_ame_resp[] = {
	AME_TAG("dsn", NULL, client_idf_accept_dsn),
	AME_TAG("public_key", NULL, client_idf_accept_pub_key),
	AME_TAG("oem_cipher", NULL, client_idf_accept_oem_key),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_idf_ame_tags[] = {
	AME_TAG("", client_idf_ame_resp, NULL),	/* unnamed outer object */
	AME_TAG(NULL, NULL, NULL)
};

/*
 * Persist OEM key for in-field provisioning.
 */
static void client_ifp_oem_persist(void *arg)
{
	conf_factory_start();
	conf_put(CT_key, ATLV_FILE, oem_key, oem_key_len);
	conf_factory_stop();
}

/*
 * Persist DSN and public key for in-field provisioning.
 */
static void client_ifp_id_persist(void *arg)
{
	struct client_ifp_parse *ifp = arg;

	conf_factory_start();
	conf_put(CT_key, ATLV_FILE, ifp->key, strlen(ifp->key));

	/*
	 * Note: put DSN last to make sure other items are valid first.
	 * If DSN is not saved, on reset, provisioning will repeat.
	 */
	conf_put_str(CT_dev_id, conf_sys_dev_id);
	conf_factory_stop();
}

/*
 * Receive response from in-field provisioning request for DSN.
 */
static enum ada_err client_ifp_get_dsn_recv(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;
	struct client_ifp_parse *ifp = &client_ifp_parse;
	size_t dsn_len;
	int rc;

	ASSERT(client_locked);
	if (buf) {
		return client_recv_ame(hc, buf, len);
	}

	/*
	 * Payload complete.  Persist result.
	 */
	if (!ifp->dsn || !ifp->key || !ifp->oem_key || !ifp->oem_key_len) {
		client_log(LOG_ERR "%s: missing fields", __func__);
		goto out;
	}

	dsn_len = strlen(ifp->dsn) + 1;
	if (dsn_len >= CONF_DEV_ID_MAX) {
		client_log(LOG_ERR "%s: DSN too long", __func__);
		goto out;
	}

	memcpy(oem_key, ifp->oem_key, ifp->oem_key_len);
	oem_key_len = (u16)ifp->oem_key_len;
	rc = conf_persist(CT_oem, client_ifp_oem_persist, NULL);
	if (rc) {
		client_log(LOG_ERR "%s: persist failed", __func__);
		goto out;
	}
	client_conf_pub_key_set_base64(ifp->key);
	memcpy(conf_sys_dev_id, ifp->dsn, dsn_len);
	rc = conf_persist(CT_id, client_ifp_id_persist, ifp);
	if (rc) {
		client_log(LOG_ERR "%s: persist failed", __func__);
		goto out;
	}
	client_get_dev_id_pend(state);
out:
	client_ifp_parse_reset();
	client_tcp_recv_done(state);
	return AE_OK;
}

/*
 * GET DSN using in-field provisioning.
 */
void client_ifp_get_dsn(struct client_state *state)
{
	struct ada_conf *cf = &ada_conf;
	struct http_client *hc;
	char req[CLIENT_GET_REQ_LEN];
	size_t ame_len;
	char *buf;
	size_t buf_len;
	int rc;

	client_log(LOG_INFO "provision DSN");
	if (client_ifp_conf_get()) {
		return;
	}

	snprintf(req, sizeof(req) - 1, "/provisions/%s",
	    cf->provision_key_id);

	buf_len = BASE64_LEN_EXPAND(cf->oem_sig_len) + 1;
	buf = al_os_mem_alloc(buf_len);
	if (!buf) {
		client_log(LOG_ERR "%s: alloc failed", __func__);
		return;
	}
	rc = ayla_base64_encode(cf->oem_signature, cf->oem_sig_len,
	    buf, &buf_len);
	if (rc) {
		client_log(LOG_ERR "%s: base64 failed", __func__);
		al_os_mem_free(buf);
		return;
	}

	ame_len = snprintf(state->ame_buf, sizeof(state->ame_buf),
	    "{\"oem_model\":\"%s\",\"signature\":\"%s\"}",
	    oem_model, buf);

	al_os_mem_free(buf);

	hc = client_req_new(CCT_IN_FIELD_PROVISION);
	ASSERT(hc);
	hc->client_tcp_recv_cb = client_ifp_get_dsn_recv;
	state->conn_state = CS_WAIT_DSN;
	state->request = CS_POST_IN_FIELD;

	memset(&prop_recvd, 0, sizeof(prop_recvd));
	client_ame_init(&state->ame,
	    client_ame_stack, ARRAY_LEN(client_ame_stack),
	    client_ame_parsed_buf, sizeof(client_ame_parsed_buf),
	    client_idf_ame_tags, NULL);
	state->ame_init = 1;
	client_ifp_parse_reset();

	hc->body_buf = state->ame_buf;
	hc->body_buf_len = ame_len;
	hc->body_len = ame_len;

	client_req_start(hc, HTTP_REQ_POST, req, &http_hdr_content_json);
}
#endif /* AYLA_IN_FIELD_PROVISION_SUPPORT */
