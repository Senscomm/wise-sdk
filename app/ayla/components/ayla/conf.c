/*
 * Copyright 2011-2014 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/tlv.h>
#include <ayla/conf_token.h>
#include <ayla/conf.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/endian.h>
#include <ayla/crc.h>
#include <ayla/clock.h>
#include <ayla/utf8.h>
#include <al/al_os_mem.h>
#include <al/al_persist.h>

#define CONF_SHOW_ALL	0	/* if 1, "conf show" shows even duplicates */
#define CONFIG_SETUP_MODE	"sys/setup_mode"

static const char * const conf_tokens[] = {
#define CONF_TOKEN(val, name)	[val] = #name,
#include <ayla/conf_tokens.h>
#undef CONF_TOKEN
};

struct conf_state conf_state;

u8 conf_mfg_mode = 1;
u8 conf_mfg_pending = 1;
u8 conf_setup_mode = 1;
u8 conf_setup_pending = 1;

/*
 * Dynamic pointer to config table.
 * This starts out pointing to the application's base config table,
 * but may be re-allocated to add entries.
 */
static const struct conf_entry * const *conf_master_table = conf_table;

static void conf_put_name_val(enum conf_token *path, int plen,
		struct ayla_tlv *val);

static s32 conf_get_int_common(struct ayla_tlv *);
static u32 conf_get_uint_common(struct ayla_tlv *);

void conf_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_CONF, fmt, args);
	ADA_VA_END(args);
}

int mfg_or_setup_mode_active(void)
{
	if (conf_mfg_mode | conf_setup_mode) {
		return 1;
	}
	return 0;
}

int mfg_or_setup_mode_ok(void)
{
	if (mfg_or_setup_mode_active()) {
		return 1;
	}
	printcli("not in mfg or setup mode");
	return 0;
}

/*
 * Check for mfg_mode.
 * For ADA, we don't use the separate mfg_mode, so consider setup_mode as good.
 */
int mfg_mode_active(void)
{
	return mfg_or_setup_mode_active();
}

int mfg_mode_ok(void)
{
	return mfg_or_setup_mode_ok();
}

int conf_save_as_factory(void)
{
	struct conf_state *state = &conf_state;

	if (state->save_as_factory) {
		return 1;
	}
	if (conf_mfg_mode | conf_mfg_pending |
	    conf_setup_mode | conf_setup_pending) {
		return 1;
	}
	return 0;
}

/*
 * Call conf_entry set op for specified node.
 * Returns 0 on success.
 */
enum conf_error conf_entry_set(int src, enum conf_token *tk, size_t ct,
				struct ayla_tlv *tlv)
{
	const struct conf_entry * const *tp;
	const struct conf_entry *ep;
	enum conf_error rc;

	for (tp = conf_master_table; (ep = *tp) != NULL; tp++) {
		if (ep->token == *tk) {
			conf_state.error = CONF_ERR_NONE;
			rc = ep->set(src, tk + 1, ct - 1, tlv);
			if (rc == CONF_ERR_NONE) {
				rc = conf_state.error;
			}
			if (rc != CONF_ERR_PATH) {
				return rc;
			}
		}
	}
	return CONF_ERR_PATH;
}

/*
 * Call conf_entry get op for specified node.
 * Returns 0 on success.
 */
enum conf_error conf_entry_get(int src, enum conf_token *tk, size_t ct)
{
	const struct conf_entry * const *tp;
	const struct conf_entry *ep;
	enum conf_error rc;

	for (tp = conf_master_table; (ep = *tp) != NULL; tp++) {
		if (ep->token == *tk) {
			conf_state.error = CONF_ERR_NONE;
			rc = ep->get(src, tk + 1, ct - 1);
			if (rc == CONF_ERR_NONE) {
				rc = conf_state.error;
			}
			if (rc != CONF_ERR_PATH) {
				return rc;
			}
		}
	}
	return CONF_ERR_PATH;
}

/*
 * return configuration string for token.
 */
const char *conf_string(enum conf_token token)
{
	if (token >= ARRAY_LEN(conf_tokens)) {
		return NULL;
	}
	return conf_tokens[token];
}

/*
 * return configuration token for string.
 */
enum conf_token conf_token_parse(const char *arg)
{
	unsigned int token;

	for (token = 0; token < ARRAY_LEN(conf_tokens); token++) {
		if (conf_tokens[token] != NULL &&
		    strcmp(conf_tokens[token], arg) == 0) {
			return (enum conf_token)token;
		}
	}
	return CT_INVALID_TOKEN;
}

size_t conf_tlv_len(const struct ayla_tlv *tlv)
{
	size_t tlen = tlv->len;
	u8 type = tlv->type;

	if (type & ATLV_FILE) {
		tlen |= (type & ~(u8)ATLV_FILE) << 8;
	}
	return tlen;
}

static u8 conf_table_follows(enum conf_token tok)
{
	return tok == CT_n || tok == CT_profile || tok == CT_mod;
}

static char *conf_path_format(char *buf, size_t len, int argc,
				const enum conf_token *argv)
{
	enum conf_token tok;
	u8 table = 0;
	ssize_t tlen = 0;

	buf[0] = '\0';
	while (argc-- > 0) {
		tok = *argv++;
		if (table) {
			tlen += snprintf(buf + tlen, len - tlen,
				"%u%s", tok, argc ? "/" : "");
			table = 0;
			continue;
		}
		table = conf_table_follows(tok);
		tlen += snprintf(buf + tlen, len - tlen, "%s%s",
		    conf_string(tok), argc ? "/" : "");
	}
	return buf;
}

int conf_path_parse(enum conf_token *tokens, int ntokens, const char *name)
{
	enum conf_token tok;
	int ntok = 0;
	unsigned long val;
	u8 table = 0;
	ssize_t tlen = 0;
	char path_buf[40];
	char *errptr;
	const char *cp;
	const char *elem = name;

	/* XXX TBD: replace this with the similar conf_str_to_tokens() */

	while (*elem) {
		cp = strchr(elem, '/');
		if (cp) {
			tlen = cp - elem;
		} else {
			tlen = strlen(elem);
		}
		if (tlen > sizeof(path_buf) - 1) {
			return -1;
		}
		memcpy(path_buf, elem, tlen);
		path_buf[tlen] = '\0';

		if (cp) {
			elem = cp + 1;
		} else {
			elem += tlen;
		}

		if (table) {
			table = 0;
			val = strtoul(path_buf, &errptr, 10);
			if (*errptr != '\0') {
				printcli("%s: strtoul parse failed on %s",
				    __func__, path_buf);
				return -1;
			}
			tok = (enum conf_token)val;
		} else {
			tok = conf_token_parse(path_buf);
			if (tok == CT_INVALID_TOKEN) {
				printcli("%s: token parse failed on %s",
				    __func__, path_buf);
				return -1;
			}
			table = conf_table_follows(tok);
		}
		if (!ntokens) {
			printcli("%s: too nany tokens for %s",
			    __func__, name);
			return -1;
		}
		*tokens++ = tok;
		ntokens--;
		ntok++;
	}
	return ntok;
}

static void conf_walk(struct conf_state *state)
{
	const struct conf_entry * const *tp;
	u8 conf_buf[CONF_VAL_MAX];

	state->error = CONF_ERR_NONE;
	state->next = conf_buf;
	state->rlen = sizeof(conf_buf);
	tp = conf_master_table;
	for (; (*tp) != NULL && state->error == CONF_ERR_NONE; tp++) {
		conf_cd_root((*tp)->token);
		if ((*tp)->export) {
			(*tp)->export();
		}
	}
}

/*
 * Save running configuration.
 */
static int conf_save_locked(void)
{
	struct conf_state *state = &conf_state;
	int rc;

	conf_walk(state);

	/*
	 * we have to do the following so that if a GET happens
	 * we return the new values instead of the old values.
	 */
	conf_mfg_mode = conf_mfg_pending;
	conf_setup_mode = conf_setup_pending;

	if (state->error == CONF_ERR_NONE) {
		al_persist_data_save_done();
		rc = 0;
	} else {
		conf_log(LOG_ERR "save: rlen %u error %u",
		    (unsigned int)state->rlen, state->error);
		rc = -1;
	}
	return rc;
}

int conf_save_config(void)
{
	int rc;

	conf_lock();
	rc = conf_save_locked();
	conf_unlock();
	return rc;
}

/*
 * Caller is going to save specific variables inside (*func).
 */
int conf_persist(enum conf_token root, void (*func)(void *arg), void *arg)
{
	struct conf_state *state = &conf_state;
	u8 conf_buf[CONF_VAL_MAX];
	int rc;

	conf_lock();
	state->error = CONF_ERR_NONE;
	state->next = conf_buf;
	state->rlen = sizeof(conf_buf);

	conf_cd_root(root);
	func(arg);
	rc = state->error;

	conf_unlock();
	return (rc != CONF_ERR_NONE);
}

/*
 * Save a single variable in the configuration.
 */
void conf_save_item(enum conf_token *path, size_t path_len,
	enum ayla_tlv_type type, const void *val, size_t len)
{
	struct conf_state *state = &conf_state;
	u8 conf_buf[CONF_VAL_MAX];

	conf_lock();
	state->error = CONF_ERR_NONE;
	state->next = conf_buf;
	state->rlen = sizeof(conf_buf);

	memcpy(state->path, path, (path_len - 1) * sizeof(*path));
	state->path_len = path_len - 1;
	conf_put(path[path_len - 1], type, val, len);
	conf_unlock();
}

ssize_t tlv_put(void *buf, size_t buflen, enum ayla_tlv_type type,
	const void *val, size_t len)
{
	struct ayla_tlv *tlv;

	if ((len > 0xff && type != ATLV_FILE) || len > 0x7eff ||
	    buflen < sizeof(*tlv) + len) {
		conf_state.error = CONF_ERR_LEN;
		memset(buf, 0, buflen);
		return -1;
	}
	tlv = buf;
	tlv->type = type | (len >> 8);
	tlv->len = len;
	memcpy(tlv + 1, val, len);
	return len + sizeof(*tlv);
}

ssize_t tlv_put_str(void *buf, size_t buflen, const char *val)
{
	return tlv_put(buf, buflen, ATLV_UTF8, val, strlen(val));
}

ssize_t tlv_put_int(void *buf, size_t buflen, s32 val)
{
	void *p;
	s16 vv;
	s8 v;
	u8 len;

	if (val <= MAX_S8 && val >= MIN_S8) {
		len = sizeof(v);
		v = (s8)val;
		p = &v;
	} else if (val <= MAX_S16 && val >= MIN_S16) {
		vv = (s16)val;
		p = &vv;
		len = sizeof(vv);
	} else {
		p = &val;
		len = sizeof(val);
	}
	return tlv_put(buf, buflen, ATLV_INT, p, len);
}

ssize_t tlv_put_uint(void *buf, size_t buflen, u32 val)
{
	void *p;
	u16 vv;
	u8 v;
	u8 len;

	if (val <= MAX_U8) {
		len = sizeof(v);
		v = (u8)val;
		p = &v;
	} else if (val <= MAX_U16) {
		vv = (u16)val;
		p = &vv;
		len = sizeof(vv);
	} else {
		p = &val;
		len = sizeof(val);
	}
	return tlv_put(buf, buflen, ATLV_UINT, p, len);
}

/*
 * Fill in response in buffer.
 */
void conf_resp(enum ayla_tlv_type type, const void *val, size_t len)
{
	struct conf_state *state = &conf_state;
	size_t tlen;

	tlen = tlv_put(state->next, state->rlen, type, val, len);
	state->next += tlen;
	state->rlen -= tlen;
}

void conf_resp_str(const char *str)
{
	conf_resp(ATLV_UTF8, str, strlen(str));
}

void conf_resp_u32(u32 val)
{
	u8 buf[4];

	if (val & 0xffff0000) {
		put_ua_be32(&buf, val);
		conf_resp(ATLV_UINT, buf, 4);
	} else if (val & 0xff00) {
		put_ua_be16(&buf, val);
		conf_resp(ATLV_UINT, buf, 2);
	} else {
		buf[0] = val;
		conf_resp(ATLV_UINT, buf, 1);
	}
}

void conf_resp_s32(s32 val)
{
	u8 buf[4];

	if (val < MIN_S16 || val > MAX_S16) {
		put_ua_be32(&buf, val);
		conf_resp(ATLV_INT, buf, 4);
	} else if (val < MIN_S8 || val > MAX_S8) {
		put_ua_be16(&buf, val);
		conf_resp(ATLV_INT, buf, 2);
	} else {
		buf[0] = val;
		conf_resp(ATLV_INT, buf, 1);
	}
}

void conf_resp_bool(u32 val)
{
	u8 v;

	v = val != 0;
	conf_resp(ATLV_BOOL, &v, sizeof(v));
}

int conf_cd(enum conf_token token)
{
	struct conf_state *state = &conf_state;

	ASSERT(state->path_len < ARRAY_LEN(state->path));
	state->path[state->path_len++] = token;
	return state->path_len - 1;
}

void conf_cd_in_parent(enum conf_token token)
{
	conf_cd_parent();
	conf_cd(token);
}

void conf_cd_parent(void)
{
	struct conf_state *state = &conf_state;
	if (state->path_len) {
		state->path_len--;
	}
}

void conf_cd_table(u8 index)
{
	conf_cd((enum conf_token)index);
}

void conf_cd_root(enum conf_token token)
{
	struct conf_state *state = &conf_state;

	state->path_len = 1;
	state->path[0] = token;
}

void conf_depth_restore(int path_len)
{
	struct conf_state *state = &conf_state;

	state->path_len = path_len;
}

static int conf_print_is_hidden(const enum conf_token *path, int plen)
{
	int i;

	for (i = 0; i < plen; i++) {
		if (path[i] == CT_key || path[i] == CT_private_key) {
			return 1;
		}
	}
	return 0;
}

static void conf_print(const char *type, const char *name,
			const enum conf_token *path, int plen,
			const char *val, size_t len)
{
	char out[40];			/* truncate output to fit this size */
	int show_hex = 0;
	size_t tlen;
	unsigned int i;
	size_t off = 0;

	if (conf_print_is_hidden(path, plen)) {
		printcli("  %s %s = %s",
		    type, name, val[0] ? "(set)" : "\"\"");
		return;
	}

	/*
	 * Show in hex if it doesn't look like good ASCII.
	 */
	for (i = 0; i < len; i++) {
		if (val[i] < 0x20 || val[i] >= 0x80) {
			show_hex = 1;
			break;
		}
	}
	if (show_hex) {
		out[0] = '\0';
		for (i = 0; i < len && off < sizeof(out) - 3; i++) {
			tlen = snprintf(out + off, sizeof(out) - off,
			    "%2.2x ", val[i]);
			if (tlen >= sizeof(out) - off) {
				break;
			}
			off += tlen;
		}
		printcli("  %s %s = (len %zu) %s", type, name, len, out);
	} else {
		tlen = len;
		if (tlen > sizeof(out) - 1) {
			tlen = sizeof(out) - 1;
		}
		memcpy(out, val, tlen);
		out[tlen] = '\0';
		if (len > tlen) {
			printcli("  %s %s = (len %zu) \"%s\"...",
			    type, name, len, out);
		} else {
			printcli("  %s %s = \"%s\"", type, name, out);
		}
	}
}

static void conf_put_state_val(enum conf_token token, struct ayla_tlv *val)
{
	struct conf_state *state = &conf_state;

	state->path[state->path_len] = token;
	conf_put_name_val(state->path, state->path_len + 1, val);
}

void conf_put(enum conf_token token, enum ayla_tlv_type type, const void *val,
    ssize_t len)
{
	struct conf_state *state = &conf_state;

	len = tlv_put(state->next, state->rlen, type, val, len);
	if (len < 0) {
		return;
	}
	conf_put_state_val(token, (struct ayla_tlv *)state->next);
}

void conf_delete(enum conf_token token)
{
	struct conf_state *state = &conf_state;

	if (tlv_put(state->next, state->rlen, ATLV_DELETE, NULL, 0) < 0) {
		return;
	}
	conf_put_state_val(token, (struct ayla_tlv *)state->next);
}

void conf_put_str(enum conf_token token, const char *val)
{
	conf_put(token, ATLV_UTF8, val, strlen(val));
}

/*
 * Put string if not empty.
 */
void conf_put_str_ne(enum conf_token token, const char *val)
{
	if (val[0]) {
		conf_put_str(token, val);
	} else {
		conf_delete(token);
	}
}

void conf_put_s32(enum conf_token token, s32 val)
{
	struct conf_state *state = &conf_state;
	ssize_t len;

	len = tlv_put_int(state->next, state->rlen, val);
	if (len < 0) {
		return;
	}
	conf_put_state_val(token, (struct ayla_tlv *)state->next);
}

void conf_put_s32_nz(enum conf_token token, s32 val)
{
	if (val) {
		conf_put_s32(token, val);
	} else {
		conf_delete(token);
	}
}

void conf_put_u32(enum conf_token token, u32 val)
{
	struct conf_state *state = &conf_state;
	ssize_t len;

	len = tlv_put_uint(state->next, state->rlen, val);
	if (len < 0) {
		return;
	}
	conf_put_state_val(token, (struct ayla_tlv *)state->next);
}

void conf_put_u32_nz(enum conf_token token, u32 val)
{
	if (val) {
		conf_put_u32(token, val);
	} else {
		conf_delete(token);
	}
}

void conf_put_u32_default(enum conf_token token, u32 val, u32 defaultVal)
{
	if (val != defaultVal) {
		/* only save non-default values in config */
		conf_put_u32(token, val);
	} else {
		conf_delete(token);
	}
}

void conf_set_error(enum conf_error error)
{
	conf_state.error = error;
}

/*
 * Get a value from a TLV, and return its length.
 * Set an error as a side-effect if it occurs.
 * Add NUL-termination if there is room whether or not it is a string.
 */
size_t conf_get(struct ayla_tlv *tlv, enum ayla_tlv_type type,
		void *val, size_t len)
{
	char *dest = val;
	size_t dlen = tlv->len;

	if (type == ATLV_FILE && (tlv->type & ATLV_FILE) != 0) {
		dlen |= (tlv->type & ~ATLV_FILE) << 8;
	} else if (tlv->type != type) {
		conf_state.error = CONF_ERR_TYPE;
		return 0;
	}
	if (dlen > len) {
		conf_state.error = CONF_ERR_LEN;
		return 0;
	}
	memcpy(val, TLV_VAL(tlv), dlen);
	if (dlen < len) {
		dest[dlen] = '\0';
	}
	return dlen;
}


static s32 conf_get_int_common(struct ayla_tlv *tlv)
{
	s32 val;
	s16 val16;

	switch (tlv->len) {
	case sizeof(u32):
		memcpy(&val, TLV_VAL(tlv), sizeof(val));
		break;
	case sizeof(u16):
		memcpy(&val16, TLV_VAL(tlv), sizeof(val16));
		val = val16;
		break;
	case sizeof(s8):
		val = *(s8 *)TLV_VAL(tlv);
		break;
	default:
		conf_state.error = CONF_ERR_LEN;
		val = 0;
		break;
	}
	return val;
}

static u32 conf_get_uint_common(struct ayla_tlv *tlv)
{
	u32 val;
	u16 val16;

	switch (tlv->len) {
	case sizeof(u32):
		memcpy(&val, TLV_VAL(tlv), sizeof(val));
		break;
	case sizeof(u16):
		memcpy(&val16, TLV_VAL(tlv), sizeof(val16));
		val = val16;
		break;
	case sizeof(u8):
		val = *(u8 *)TLV_VAL(tlv);
		break;
	default:
		conf_state.error = CONF_ERR_LEN;
		val = 0;
		break;
	}
	return val;
}

s32 conf_get_s32(struct ayla_tlv *tlv)
{
	if (tlv->type != ATLV_INT) {
		conf_state.error = CONF_ERR_TYPE;
		return 0;
	}
	return (s32)conf_get_int_common(tlv);
}

u32 conf_get_u32(struct ayla_tlv *tlv)
{
	if (tlv->type != ATLV_UINT) {
		conf_state.error = CONF_ERR_TYPE;
		return 0;
	}
	return conf_get_uint_common(tlv);
}

s16 conf_get_s16(struct ayla_tlv *tlv)
{
	s16 val;
	s32 tval;

	tval = conf_get_s32(tlv);
	val = (s16)tval;
	if (val != tval) {
		conf_state.error = CONF_ERR_RANGE;
		return 0;
	}
	return val;
}

u16 conf_get_u16(struct ayla_tlv *tlv)
{
	u16 val;
	u32 tval;

	tval = conf_get_u32(tlv);
	val = (u16)tval;
	if (val != tval) {
		conf_state.error = CONF_ERR_RANGE;
		return 0;
	}
	return val;
}

s8 conf_get_s8(struct ayla_tlv *tlv)
{
	s8 val;
	s32 tval;

	tval = conf_get_s32(tlv);
	val = (s8)tval;
	if (val != tval) {
		conf_state.error = CONF_ERR_RANGE;
		return 0;
	}
	return val;
}

u8 conf_get_u8(struct ayla_tlv *tlv)
{
	u8 val;
	u32 tval;

	tval = conf_get_u32(tlv);
	val = (u8)tval;
	if (val != tval) {
		conf_state.error = CONF_ERR_RANGE;
		return 0;
	}
	return val;
}

u8 conf_get_bit(struct ayla_tlv *tlv)	/* value must be 0 or 1 */
{
	u8 val;

	if (tlv->type == ATLV_BOOL) {
		val = *(u8 *)TLV_VAL(tlv);
	} else {
		val = conf_get_u8(tlv);
	}
	if (val > 1) {
		conf_state.error = CONF_ERR_RANGE;
		return 0;
	}
	return val;
}

s32 conf_get_int32(struct ayla_tlv *tlv)
{
	int value;

	if (tlv->type == ATLV_INT) {
		value = conf_get_int_common(tlv);
	} else if (tlv->type == ATLV_UINT) {
		value = (s32)conf_get_uint_common(tlv);
		if (value < 0) {
			conf_state.error = CONF_ERR_RANGE;
			return 0;
		}
	} else {
		conf_state.error = CONF_ERR_TYPE;
		return 0;
	}
	return value;
}

void conf_commit(void)
{
	const struct conf_entry * const *tp;

	for (tp = conf_master_table; (*tp) != NULL; tp++) {
		if ((*tp)->commit != NULL) {
			(*tp)->commit(0);
		}
	}
	al_persist_data_load_done();
}

enum conf_error conf_set_tlv(const struct conf_entry *entry,
				enum conf_token *tk,
				int ntokens, struct ayla_tlv *tlv)
{
	enum conf_error error;

	conf_state.error = CONF_ERR_NONE;
	error = entry->set(CONF_OP_SRC_CLI, tk, ntokens, tlv);
	return error == CONF_ERR_NONE ? conf_state.error : error;
}

enum conf_error conf_cli_set_tlv(enum conf_token *tk, int ntokens,
				struct ayla_tlv *tlv)
{
	enum conf_error error;

	error = conf_entry_set(CONF_OP_SRC_CLI, tk, ntokens, tlv);
	return error == CONF_ERR_NONE ? conf_state.error : error;
}

int conf_access(u32 type)
{
	int src, ss;

	src = CONF_OP_SRC(type);
	ss = CONF_OP_SS(type);

	if (src == CONF_OP_SRC_FILE) {
		return 0;
	}
	if (CONF_OP_IS_WRITE(type)) {
		if (ss == CONF_OP_SS_ID && !mfg_mode_active()) {
			/*
			 * Id settings only in mfg mode.
			 */
			return CONF_ERR_PERM;
		}
		if ((ss == CONF_OP_SS_OEM || ss == CONF_OP_SS_HW) &&
		    !mfg_or_setup_mode_active()) {
			/*
			 * OEM/HW settings only in mfg/setup mode.
			 */
			return CONF_ERR_PERM;
		}
		if (ss == CONF_OP_SS_OEM_MODEL && src != CONF_OP_SRC_MCU &&
		    !mfg_or_setup_mode_active()) {
			/*
			 * OEM model only in mfg/setup mode, unless set by MCU
			 */
			return CONF_ERR_PERM;
		}
		if ((ss == CONF_OP_SS_SERVER) && src != CONF_OP_SRC_FILE) {
			/*
			 * Server commands only set by file.
			 */
			return CONF_ERR_PERM;
		}
		if (ss == CONF_OP_SS_WIFI_REGION && src != CONF_OP_SRC_ADS &&
		    !mfg_or_setup_mode_active()) {
			/*
			 * wifi region changes only in mfg/setup mode or
			 * from ADS.
			 */
			return CONF_ERR_PERM;
		}
	}
	switch (src) {
	case CONF_OP_SRC_ADS:
		/*
		 * Everything goes.
		 */
		return 0;
	case CONF_OP_SRC_SERVER:
		switch (ss) {
		default:
			if (!CONF_OP_IS_WRITE(type)) {
				return 0;
			}
			return CONF_ERR_PERM;
		}
		/* fall through */
	case CONF_OP_SRC_MCU:
		switch (ss) {
		case CONF_OP_SS_PWR:
		case CONF_OP_SS_CLIENT_ENA:
		case CONF_OP_SS_CLIENT_REG:
		case CONF_OP_SS_CLIENT_SRV_REGION:
		case CONF_OP_SS_ETH:
		case CONF_OP_SS_OEM:
		case CONF_OP_SS_OEM_MODEL:
		case CONF_OP_SS_LOG:
		case CONF_OP_SS_WIFI:
		case CONF_OP_SS_WIFI_REGION:
		case CONF_OP_SS_SERVER_PROP:
		case CONF_OP_SS_SETUP_APP:
		case CONF_OP_SS_TIME:
		case CONF_OP_SS_HW:
		case CONF_OP_SS_HAP:
			return 0;
		default:
			if (!CONF_OP_IS_WRITE(type)) {
				return 0;
			}
			return CONF_ERR_PERM;
		}
		break;
	case CONF_OP_SRC_CLI:
		if (ss == CONF_OP_SS_SETUP_APP && !mfg_or_setup_mode_ok()) {
			/*
			 * IOS setup app can only be set in mfg/setup mode.
			 */
			return CONF_ERR_PERM;
		}
		return 0;
	default:
		break;
	}
	return CONF_ERR_PATH;
}

ssize_t conf_put_tokens(void *buf, size_t buf_len,
			enum conf_token *toks, int ntok)
{
	ssize_t rc;
	ssize_t len = 0;

	while (ntok--) {
		if (len >= buf_len) {
			return -1;
		}
		rc = utf8_encode((unsigned char *)buf + len, buf_len - len,
		    *toks++);
		if (rc < 0) {
			return -1;
		}
		len += rc;
	}
	return len;
}

/*
 * Display the configuration.
 */

void conf_show(void)
{
	conf_show_name(NULL);
}

/*
 * Show the configuration.  If name is non-NULL, show only matching items.
 */
int conf_show_name(const char *name)
{
	struct conf_state *state = &conf_state;

	conf_lock();
	state->show_name = name;
	state->error = CONF_ERR_NONE;
	state->show_conf = 1;
	state->show_conf_matched = 0;
	state->show_conf_section = AL_PERSIST_FACTORY;
	conf_walk(state);
	state->show_conf_section = AL_PERSIST_STARTUP;
	conf_walk(state);
	state->show_conf_section = AL_PERSIST_COUNT;	/* running */
	conf_walk(state);
	state->show_conf = 0;
	state->show_name = NULL;
	if (name && !state->show_conf_matched) {
		printcli("conf show: %s: not found", name);
	}
	conf_unlock();

	if (!name) {
		printcli("  types: f = factory, s = startup, r = running");
	}
	return 0;
}

/*
 * Check whether configuration contains only factory items.
 * For enabling setup mode, no startup configuration differences should exist.
 * Running config differences are allowed.  Avoiding these is difficult since
 * some may come from the host app at run time and can appear after an upgrade.
 * Returns 1 if only factory items exist.
 */
int conf_is_factory_fresh(void)
{
	struct conf_state *state = &conf_state;
	int fresh;

	conf_lock();
	state->show_name = NULL;
	state->error = CONF_ERR_NONE;
	state->show_conf = 1;
	state->no_print = 1;
	state->item_found = 0;
	state->show_conf_section = AL_PERSIST_STARTUP;
	conf_walk(state);
	state->show_conf = 0;
	state->no_print = 0;
	fresh = !state->item_found;
	conf_unlock();
	return fresh;
}


/*
 * Set a configuration item.
 *
 * "conf set" directly sets a persisted item without setting the running
 * configuration, so it will not be effective until after a reset.
 * If a "save" is done, the item may be overwritten by the running config.
 * If the name does not match any configuration item, it may still write
 * something, which would be ignored.
 *
 * Usually another command, such as "id" or "oem" would be used for routine
 * items.  This is a developer aid, not so much for routine use.
 */
static void conf_cli_set(const char *name, const char *val)
{
	enum al_err err;

	if (!mfg_or_setup_mode_ok()) {
		return;
	}
	err = al_persist_data_write(AL_PERSIST_FACTORY, name, val, strlen(val));
	if (err) {
		printcli("%s: write error %d: %s",
		    __func__, err, al_err_string(err));
	}
}

void conf_cli(int argc, char **argv)
{
	if (argc < 2 || !strcmp(argv[1], "show")) {
		if (argc < 3) {
			conf_show();
			return;
		}
		if (argc == 3) {
			conf_show_name(argv[2]);
			return;
		}
	}
	if (argc == 2 && !strcmp(argv[1], "save")) {
		conf_save_config();
		return;
	}
	if (argc == 4 && !strcmp(argv[1], "set")) {
		conf_cli_set(argv[2], argv[3]);
		return;
	}
	printcli("usage: conf show [name]");
	printcli("       conf set <name> <val>");
	printcli("       conf save");
}

/*
 * Converts a string path into conf tokens.
 */
int conf_str_to_tokens(char *haystack, enum conf_token *tk, int tk_len)
{
	int index = 0;
	char *cp;
	char *errptr;
	unsigned long ulval;

	while (*haystack != '\0') {
		if (index >= tk_len) {
			return -1;
		}
		cp = strchr(haystack, '/');
		if (cp) {
			*cp = '\0';
		}
		tk[index] = conf_token_parse(haystack);
		if (tk[index] == CT_INVALID_TOKEN) {
			/* check if haystack is an integer */
			ulval = strtoul(haystack, &errptr, 10);
			if (*errptr != '\0' || ulval > MAX_U8) {
				conf_log(LOG_ERR
				    "bad token for parsing %s", haystack);
				return -1;
			}
			tk[index] = (enum conf_token)ulval;
		}
		index++;
		if (!cp) {
			return index;
		}
		haystack = cp + 1;
	}

	return index;
}

/*
 * Converts a token path into a conf string.
 */
int conf_tokens_to_str(enum conf_token *tk, int tk_len, char *buf, int blen)
{
	conf_path_format(buf, blen, tk_len, tk);
	return strlen(buf);
}


static unsigned int conf_table_len(const struct conf_entry * const *table)
{
	unsigned int count = 0;

	while (*table++) {
		count++;
	}
	return count;
}

/*
 * Add a new entry to the end of the conf_master_table.
 */
void conf_table_entry_add(const struct conf_entry *entry)
{
	const struct conf_entry * const *old = conf_master_table;
	const struct conf_entry **new;
	unsigned int old_count;

	old_count = conf_table_len(old);

	new = al_os_mem_alloc((old_count + 2) * sizeof(*old));
	ASSERT(new);
	if (!new) {
		return;
	}

	memcpy(new, old, old_count * sizeof(*new));
	new[old_count] = entry;
	new[old_count + 1] = NULL;

	conf_master_table = new;
	if (old != conf_table) {
		al_os_mem_free((void *)old);
	}
}

/*
 * Start saving config items as factory config
 */
void conf_factory_start(void)
{
	struct conf_state *state = &conf_state;

	state->save_as_factory = 1;
}

/*
 * Stop saving config items as factory config
 */
void conf_factory_stop(void)
{
	struct conf_state *state = &conf_state;

	state->save_as_factory = 0;
}

/*
 * Get conf item from startup configuration, if present, or factory, otherwise.
 * This used to be adap_conf_get(), in the host application.
 * Returns the length of the value on success or the negative of an enum al_err
 * value on error.
 * If there is room in the buffer, the value will be NUL-terminated.
 */
ssize_t conf_persist_get(const char *name, void *buf, size_t len)
{
	ssize_t rc;

	rc = al_persist_data_read(AL_PERSIST_STARTUP, name, buf, len);
	if (rc < 0) {
		rc = al_persist_data_read(AL_PERSIST_FACTORY, name, buf, len);
	}
	if (rc >= 0 && rc < len) {
		((char *)buf)[rc] = '\0';
	}
	return rc;
}

/*
 * Get long value from the configuration.
 */
enum al_err conf_persist_get_long(const char *name, long *val)
{
	char buf[30];
	char *errptr;
	ssize_t rc;
	long lval;

	rc = conf_persist_get(name, buf, sizeof(buf));
	if (rc < 0) {
		return (enum al_err)(-rc);
	}
	ASSERT(rc < sizeof(buf));

	lval = strtol(buf, &errptr, 10);
	if (errptr == buf || *errptr) {
		return AL_ERR_INVAL_VAL;
	}
	*val = lval;
	return AL_ERR_OK;
}

static enum al_persist_section conf_persist_section(const char *name)
{
	struct conf_state *state = &conf_state;

	if (conf_setup_mode || conf_setup_pending ||
	    state->save_as_factory ||
	    !strcmp(name, CONFIG_SETUP_MODE)) {
		return AL_PERSIST_FACTORY;
	}
	return AL_PERSIST_STARTUP;
}

static void conf_show_item(const char *name,
		const enum conf_token *path, int plen,
		struct ayla_tlv *val_tlv, const void *val, size_t len)
{
	struct conf_state *state = &conf_state;
	enum al_persist_section section = state->show_conf_section;
	const char *section_name = "s";
	char rbuf[CONF_VAL_MAX + 1];
	ssize_t rlen;
	size_t match_len;
	size_t name_len;

	if (state->show_name) {
		/*
		 * If name starts with the name specified, and ends there or
		 * continues with a slash, consider it a match.
		 */
		name_len = strlen(name);
		match_len = strlen(state->show_name);
		if (name_len < match_len ||
		    memcmp(name, state->show_name, match_len) ||
		    (name_len > match_len &&
		     name[match_len] && name[match_len] != '/')) {
			return;
		}
		state->show_conf_matched = 1;
	}

	switch (section) {
	case AL_PERSIST_FACTORY:
		section_name = "f";
		/* fall-through */
	case AL_PERSIST_STARTUP:
		rlen = al_persist_data_read(section, name, rbuf, sizeof(rbuf));
		if (rlen < 0) {
			if (rlen == -AL_ERR_NOT_FOUND) {
				return;
			}
			printcli("  %s read error", name);
			break;
		}
		if (rlen >= sizeof(rbuf)) {
			rlen = sizeof(rbuf) - 1;
		}
		rbuf[rlen] = '\0';
		state->item_found = 1;
		if (state->no_print) {
			break;
		}
		conf_print(section_name, name, path, plen, rbuf, rlen);
		break;

	case AL_PERSIST_COUNT:		/* running config */
	default:
		rlen = conf_persist_get(name, rbuf, sizeof(rbuf));
		if (rlen == -AL_ERR_NOT_FOUND) {
			if (val_tlv->type == ATLV_DELETE) {
				break;
			}
			rlen = 0;	/* treat as empty string */
		}
		if (rlen >= 0 && rlen == len && !memcmp(val, rbuf, len)) {
			if (!CONF_SHOW_ALL) {
				break;
			}
		} else {
			state->item_found = 1;
		}
		if (state->no_print) {
			break;
		}
		conf_print("r", name, path, plen, val, len);
		break;
	}
}

/*
 * Set conf item to factory or startup configuration.
 * This used to be adap_conf_set(), in the host application.
 * Returns zero on success.
 */
enum al_err conf_persist_set(const char *name, const void *buf, size_t len)
{
	enum al_persist_section section;
	enum al_err err;
	char rbuf[CONF_VAL_MAX + 1];
	ssize_t rlen;

	section = conf_persist_section(name);

	/*
	 * If saving to startup, and the item is the same as the factory
	 * setting, erase the startup setting.
	 */
	if (section == AL_PERSIST_STARTUP) {
		rlen = al_persist_data_read(AL_PERSIST_FACTORY, name,
		    rbuf, sizeof(rbuf));
		if (rlen < 0) {
			rlen = 0;
		}
		if (rlen >= 0 && rlen == len && !memcmp(buf, rbuf, len)) {
			len = 0;	/* delete startup item */
		}
	}

	conf_log(LOG_DEBUG2 "%s: saving %s: %s len %zu", __func__,
	    section == AL_PERSIST_FACTORY ? "f" : "s", name, len);

	err = al_persist_data_write(section, name, buf, len);

	/*
	 * If we saved a factory item, make sure the startup item is erased.
	 * This should be unnecessary.
	 */
	if (!err && section == AL_PERSIST_FACTORY) {
		err = al_persist_data_write(AL_PERSIST_STARTUP, name, NULL, 0);
	}
	return err;
}

/*
 * Display a simple configuration item.
 */
static void conf_persist_show(const char *name, const void *buf, size_t len)
{
	struct conf_state *state = &conf_state;
	enum al_persist_section section = state->show_conf_section;
	const char *section_name = "s";
	char rbuf[CONF_VAL_MAX + 1];
	ssize_t rlen;

	switch (section) {
	case AL_PERSIST_FACTORY:
		section_name = "f";
		/* fall-through */
	case AL_PERSIST_STARTUP:
		rlen = al_persist_data_read(section, name, rbuf, sizeof(rbuf));
		if (rlen < 0) {
			if (rlen == -AL_ERR_NOT_FOUND) {
				return;
			}
			printcli("  %s read error", name);
			break;
		}
		if (rlen >= sizeof(rbuf)) {
			rlen = sizeof(rbuf) - 1;
		}
		rbuf[rlen] = '\0';
		if (state->no_print) {
			break;
		}
		conf_print(section_name, name, NULL, 0, rbuf, rlen);
		break;

	case AL_PERSIST_COUNT:		/* running config */
	default:
		rlen = conf_persist_get(name, rbuf, sizeof(rbuf));
		if (rlen == -AL_ERR_NOT_FOUND) {
			rlen = 0;	/* treat as empty string */
		}
		if (rlen >= 0 && rlen == len && !memcmp(buf, rbuf, len)) {
			if (!CONF_SHOW_ALL) {
				break;
			}
		}
		if (state->no_print) {
			break;
		}
		conf_print("r", name, NULL, 0, buf, len);
		break;
	}
}

/*
 * Export a simple config item.
 *
 * Depending on configuration state, This routine persists a config item
 * to flash, or displays it on the console.
 */
enum al_err conf_persist_export(const char *name, const void *buf, size_t len)
{
	struct conf_state *state = &conf_state;

	/*
	 * Show or save the configuration
	 */
	if (state->show_conf) {
		conf_persist_show(name, buf, len);
		return AL_ERR_OK;
	}
	return conf_persist_set(name, buf, len);
}

/*
 * Format value for persisting as ASCII string or binary blob.
 * May return a pointer into the supplied buffer or into the value TLV.
 * Updates the length.
 */
static const char *conf_tlv_persist_fmt(char *vbuf, size_t *lenp,
		struct ayla_tlv *val)
{
	size_t vbuf_len = *lenp;
	size_t tlen;
	size_t vlen = 0;
	u32 v32;
	u16 v16;

	tlen = conf_tlv_len(val);

	switch (val->type) {
	case ATLV_INT:
		switch (tlen) {
		case 1:
			vlen = snprintf(vbuf, vbuf_len, "%d",
			    *(s8 *)TLV_VAL(val));
			break;
		case 2:
			memcpy(&v16, TLV_VAL(val), sizeof(v16));
			vlen = snprintf(vbuf, vbuf_len, "%d", (s16)v16);
			break;
		case 4:
			memcpy(&v32, TLV_VAL(val), sizeof(v32));
			vlen = snprintf(vbuf, vbuf_len, "%ld", (s32)v32);
			break;
		default:
			/* Invalid size for ATLV_INT */
			ASSERT(1);
			break;
		}
		ASSERT(vlen < vbuf_len);
		break;
	case ATLV_UINT:
		switch (tlen) {
		case 1:
			vlen = snprintf(vbuf, vbuf_len, "%u",
			    *(u8 *)TLV_VAL(val));
			break;
		case 2:
			memcpy(&v16, TLV_VAL(val), sizeof(v16));
			vlen = snprintf(vbuf, vbuf_len, "%u", v16);
			break;
		case 4:
			memcpy(&v32, TLV_VAL(val), sizeof(v32));
			vlen = snprintf(vbuf, vbuf_len, "%lu", v32);
			break;
		default:
			/* Invalid size for ATLV_UINT */
			ASSERT(1);
			break;
		}
		ASSERT(tlen < vbuf_len);
		break;
	case ATLV_DELETE:
		vlen = 0;
		break;
	default:
		/*
		 * Treat all other values as binary.
		 */
		vbuf = TLV_VAL(val);
		vlen = tlen;
		break;
	}
	*lenp = vlen;
	return vbuf;
}

/*
 * Store TLV val with a name given in path to flash.
 */
static void conf_put_name_val(enum conf_token *path, int plen,
		struct ayla_tlv *val)
{
	struct conf_state *state = &conf_state;
	char name[AL_PERSIST_NAME_LEN_MAX + 1];
	const char *val_buf;
	char int_buf[40];
	size_t tlen;
	size_t vlen;

	tlen = conf_tokens_to_str(path, plen, name, sizeof(name));
	if (!tlen) {
		return;
	}

	vlen = sizeof(int_buf);
	val_buf = conf_tlv_persist_fmt(int_buf, &vlen, val);
	if (state->show_conf) {
		conf_show_item(name, path, plen, val, val_buf, vlen);
		return;
	}
	conf_persist_set(name, val_buf, vlen);
}

void conf_reset_factory(void)
{
	al_persist_data_erase(AL_PERSIST_STARTUP);
}
