/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 * JSON-based message encoding and decoding support.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/base64.h>
#include <ayla/utf8.h>
#include "ame.h"
#include "ame_json.h"

/* #define AME_JSON_DEBUG */
/* #define AME_JSON_DEBUG2 */

#ifndef AME_JSON_TEST
#include <ayla/log.h>
#include <ayla/mod_log.h>
#endif

#ifdef AME_JSON_DEBUG2
static const char *ame_parser_state_names[] = AME_PARSER_STATE_NAMES;
#endif

static inline size_t ame_json_calc_len(struct ame_encoder_state *es)
{
	if (es->offset > es->size) {
		return 0;
	}
	return es->size - es->offset;
}

static inline void ame_json_enc_char(struct ame_encoder_state *es,
    const char c)
{
	if (es->offset < es->size) {
		es->buffer[es->offset] = (u8)c;
	}
	es->offset++;
}

static void ame_json_enc_quoted_string(struct ame_encoder_state *es,
    const char *string)
{
	char unicode_str[5];

	ame_json_enc_char(es, '"');
	while (*string != 0) {
		if (*string < 0x20) {
			ame_json_enc_char(es, '\\');
			switch (*string) {
			case '\b':
				ame_json_enc_char(es, 'b');
				break;
			case '\f':
				ame_json_enc_char(es, 'f');
				break;
			case '\n':
				ame_json_enc_char(es, 'n');
				break;
			case '\r':
				ame_json_enc_char(es, 'r');
				break;
			case '\t':
				ame_json_enc_char(es, 't');
				break;
			default:
				ame_json_enc_char(es, 'u');
				snprintf(unicode_str, sizeof(unicode_str),
				    "%04x", (u16)*(u8 *)string);
				ame_json_enc_char(es, unicode_str[0]);
				ame_json_enc_char(es, unicode_str[1]);
				ame_json_enc_char(es, unicode_str[2]);
				ame_json_enc_char(es, unicode_str[3]);
				break;
			}
		} else {
			switch (*string) {
			case '"':
			/* escaping the solidus character is optional
			case '/' :
			*/
			case '\\':
				ame_json_enc_char(es, '\\');
				ame_json_enc_char(es, *string);
				break;
			default:
				ame_json_enc_char(es, *string);
				break;
			}
		}
		string++;
	}
	ame_json_enc_char(es, '"');
}

static void ame_json_enc_prefix(struct ame_encoder_state *es, u32 flags)
{
	if (flags & EF_PREFIX_C) {
		ame_json_enc_char(es, ',');
	}
	if (flags & EF_PREFIX_A) {
		ame_json_enc_char(es, '[');
	}
	if (flags & EF_PREFIX_O) {
		ame_json_enc_char(es, '{');
	}
}

static void ame_json_enc_suffix(struct ame_encoder_state *es, u32 flags)
{
	if (flags & EF_SUFFIX_E) {
		ame_json_enc_char(es, '}');
	}
	if (flags & EF_SUFFIX_Z) {
		ame_json_enc_char(es, ']');
	}
	if (flags & EF_SUFFIX_M) {
		ame_json_enc_char(es, ',');
	}
}

/*
 * Encode the prefix and the key.
 * If the key is null, just encode the prefix.
 */
static void ame_json_enc_key(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key)
{
	ame_json_enc_prefix(es, flags);
	if (key) {
		ame_json_enc_quoted_string(es, key->tag);
		ame_json_enc_char(es, ':');
	}
}

static void ame_json_enc_s64(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s64 value)
{
	size_t count;

	ame_json_enc_key(es, flags, key);
	count = snprintf((char *)&es->buffer[es->offset],
	    ame_json_calc_len(es), "%lld", value);
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

static void ame_json_enc_s32(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s32 value)
{
	ame_json_enc_s64(es, flags, key, value);
}

static void ame_json_enc_s16(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s16 value)
{
	ame_json_enc_s64(es, flags, key, value);
}

static void ame_json_enc_s8(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s8 value)
{
	ame_json_enc_s64(es, flags, key, value);
}

static void ame_json_enc_u64(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, u64 value)
{
	size_t count;

	ame_json_enc_key(es, flags, key);
	count = snprintf((char *)&es->buffer[es->offset],
	    ame_json_calc_len(es), "%llu", value);
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

static void ame_json_enc_u32(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, u32 value)
{
	ame_json_enc_u64(es, flags, key, value);
}

static void ame_json_enc_u16(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, u16 value)
{
	ame_json_enc_u64(es, flags, key, value);
}

static void ame_json_enc_u8(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, u8 value)
{
	ame_json_enc_u64(es, flags, key, value);
}

static void ame_json_enc_d32(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s32 value, u8 scale)
{
	s32 factor = 1;
	s32 left;
	s32 right;
	size_t count;
	u8 i = scale;
	char format_str[16];

	ASSERT(scale < 10);

	while (i--) {
		factor *= 10;
	}
	left = value / factor;
	right = value % factor;
	/* must manage minus sign separately in case left is zero */
	if (value < 0) {
		left = -left;
		right = -right;
	}

	snprintf(format_str, sizeof(format_str), "%%s%%ld.%%0%uld", scale);

	ame_json_enc_key(es, flags, key);
	count = snprintf((char *)&es->buffer[es->offset],
	    ame_json_calc_len(es), format_str,
	    value < 0 ? "-" : "", left, right);
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

static void ame_json_enc_null(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key)
{
	size_t count;

	ame_json_enc_key(es, flags, key);
	count = snprintf((char *)&es->buffer[es->offset],
	    ame_json_calc_len(es), "%s", "null");
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

static void ame_json_enc_utf8(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, const char *utf8)
{
	if (utf8) {
		ame_json_enc_key(es, flags, key);
		ame_json_enc_quoted_string(es, utf8);
		ame_json_enc_suffix(es, flags);
	} else {
		ame_json_enc_null(es, flags, key);
	}
}

static void ame_json_enc_opaque(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, const void *data, size_t length)
{
	size_t outlen;
	if (data) {
		ame_json_enc_key(es, flags, key);
		ame_json_enc_char(es, '"');
		if (es->size > es->offset) {
			outlen = es->size - es->offset;
			if (!ayla_base64_encode(data, length,
			    &es->buffer[es->offset], &outlen)) {
				goto done;
			}
		}
		/* Insufficient space. Calculate space required. */
		outlen = 4 * ((length + 2) / 3);
done:
		es->offset += outlen;
		ame_json_enc_char(es, '"');
		ame_json_enc_suffix(es, flags);
	} else {
		ame_json_enc_null(es, flags, key);
	}
}

static void ame_json_enc_boolean(struct ame_encoder_state *es, u32 flags,
    const struct ame_key *key, s32 value)
{
	size_t count;

	ame_json_enc_key(es, flags, key);
	if (value) {
		count = snprintf((char *)&es->buffer[es->offset],
		    ame_json_calc_len(es), "%s", "true");
	} else {
		count = snprintf((char *)&es->buffer[es->offset],
		    ame_json_calc_len(es), "%s", "false");
	}
	es->offset += count;
	ame_json_enc_suffix(es, flags);
}

const struct ame_encoder ame_json_enc = {
	.enc_prefix = ame_json_enc_prefix,
	.enc_suffix = ame_json_enc_suffix,
	.enc_key = ame_json_enc_key,
	.enc_s64 = ame_json_enc_s64,
	.enc_s32 = ame_json_enc_s32,
	.enc_s16 = ame_json_enc_s16,
	.enc_s8 = ame_json_enc_s8,
	.enc_u64 = ame_json_enc_u64,
	.enc_u32 = ame_json_enc_u32,
	.enc_u16 = ame_json_enc_u16,
	.enc_u8 = ame_json_enc_u8,
	.enc_d32 = ame_json_enc_d32,
	.enc_utf8 = ame_json_enc_utf8,
	.enc_opaque = ame_json_enc_opaque,
	.enc_boolean = ame_json_enc_boolean,
	.enc_null = ame_json_enc_null,
};

static void ame_json_byte_forward(struct ame_decoder_state *ds)
{
	if (ds->ext_mode) {
		ASSERT(ds->data_len < ds->data_buf_size);
		if (ds->state == AME_STATE_NAME_STR ||
		    !ds->kvp || ds->kvp->template_node) {
			ds->data_len++;
		}
	}
	ds->offset++;
}

static enum ada_err ame_json_parse_string(struct ame_decoder_state *ds, u8 c)
{
	if (ds->state == AME_STATE_VALUE || ds->state == AME_STATE_NAME) {
		if (c != '"') {
			return AE_PARSE;
		}
		ds->escape = 0;
		ds->hexdigs = 0;
	} else if (ds->state == AME_STATE_VALUE_STR ||
	    ds->state == AME_STATE_NAME_STR) {
		if (ds->escape) {
			ds->escape = 0;
			switch (c) {
			case '"':
			case '/':
			case '\\':
			case 'b':
			case 'f':
			case 'r':
			case 'n':
			case 't':
				break;
			case 'u':
				ds->hexdigs = 4;
				break;
			default:
				/* invalid escape sequence */
				return AE_PARSE;
			}
		} else if (ds->hexdigs) {
			ds->hexdigs--;
			if (!((c >= '0' && c <= '9') ||
			    (c >= 'a' && c <= 'f') ||
			    (c >= 'A' && c <= 'F'))) {
				/* not a hex digit */
				return AE_PARSE;
			}
		} else {
			switch (c) {
			case '\\':
				ds->escape = 1;
				break;
			case '"':
				/* end of string */
				return AE_OK;
			default:
				if (c < 0x20) {
					/* unescaped control character */
					return AE_PARSE;
				}
				break;
			}
		}
	}
	return AE_IN_PROGRESS;
}

/*
 * Parse number.
 *
 * The caller must not consume the input character if we return AE_OK,
 * since that byte is not considered part of the number.
 */
static enum ada_err ame_json_parse_number(struct ame_decoder_state *ds,
		enum ame_type *type, u8 c)
{
	/* Parse it byte by byte. -1.234E123 */
	switch (ds->state) {
	case AME_STATE_VALUE:
		/* must be one of: '-', '0~9' */
		if (c == '-') {
			*type = AME_TYPE_INTEGER;
			ds->state = AME_STATE_VALUE_NUM_1;
			break;
		}
		if (c == '0') {
			*type = AME_TYPE_INTEGER;
			ds->state = AME_STATE_VALUE_NUM_0;
			break;
		}
		if (c >= '1' && c <= '9') {
			*type = AME_TYPE_INTEGER;
			ds->state = AME_STATE_VALUE_NUM_2;
			break;
		}
		return AE_PARSE;

	case AME_STATE_VALUE_NUM_0:
		/* after initial '0', must be '.', 'e', 'E', or end */
		if (c == '.') {
			*type = AME_TYPE_DECIMAL;
			ds->state = AME_STATE_VALUE_NUM_3;
			break;
		}
		if (c == 'e' || c == 'E') {
			*type = AME_TYPE_FLOAT;
			ds->state = AME_STATE_VALUE_NUM_4;
			break;
		}
		if (c >= '1' && c <= '9') {
			return AE_PARSE;
		}
		return AE_OK;

	case AME_STATE_VALUE_NUM_1:
		/* after '-' sign, must be one of: '0~9' */
		if (c == '0') {
			ds->state = AME_STATE_VALUE_NUM_0;
			break;
		}
		if (c >= '1' && c <= '9') {
			ds->state = AME_STATE_VALUE_NUM_2;
			break;
		}
		return AE_PARSE;

	case AME_STATE_VALUE_NUM_2:
		/* expect: '.', 'e', 'E', 0~9' */
		if (c == '.') {
			*type = AME_TYPE_DECIMAL;
			ds->state = AME_STATE_VALUE_NUM_3;
			break;
		}
		if (c == 'e' || c == 'E') {
			*type = AME_TYPE_FLOAT;
			ds->state = AME_STATE_VALUE_NUM_4;
			break;
		}
		if (c >= '0' && c <= '9') {
			break;
		}
		return AE_OK;

	case AME_STATE_VALUE_NUM_3:
		/* after decimal point. expect: '0~9', not end */
		if (c >= '0' && c <= '9') {
			ds->state = AME_STATE_VALUE_NUM_3A;
			break;
		}
		return AE_PARSE;

	case AME_STATE_VALUE_NUM_3A:
		/* after decimal point and digit. 'e', 'E', '0~9', or end */
		if (c == 'e' || c == 'E') {
			*type = AME_TYPE_FLOAT;
			ds->state = AME_STATE_VALUE_NUM_4;
			break;
		}
		if (c >= '0' && c <= '9') {
			break;
		}
		return AE_OK;

	case AME_STATE_VALUE_NUM_4:
		/* Immediately after 'e'.  Must be one of: '-', '+', '0~9' */
		if (c == '-' || c == '+') {
			ds->state = AME_STATE_VALUE_NUM_5;
			break;
		}
		if (c >= '0' && c <= '9') {
			ds->state = AME_STATE_VALUE_NUM_6;
			break;
		}
		return AE_PARSE;

	case AME_STATE_VALUE_NUM_5:
		/* in exponent immediately after sign: expect: '0~9' */
		if (c >= '0' && c <= '9') {
			ds->state = AME_STATE_VALUE_NUM_6;
			break;
		}
		return AE_PARSE;	/* value can't end here */

	case AME_STATE_VALUE_NUM_6:
		/* expect: '0~9' */
		if (c >= '0' && c <= '9') {
			break;
		}
		return AE_OK;

	default:
		return AE_PARSE;
	}
	return AE_IN_PROGRESS;
}

static enum ada_err ame_json_parse_literal(struct ame_decoder_state *ds,
    enum ame_type *type, u8 c)
{
	if (ds->state == AME_STATE_VALUE) {
		switch (c) {
		case 't':
			ds->literal = (const u8 *)"true";
			*type = AME_TYPE_BOOLEAN;
			break;
		case 'f':
			ds->literal = (const u8 *)"false";
			*type = AME_TYPE_BOOLEAN;
			break;
		case 'n':
			ds->literal = (const u8 *)"null";
			*type = AME_TYPE_NULL;
			break;
		default:
			return AE_INVAL_VAL;
		}
		ds->literal++;
	} else if (ds->state == AME_STATE_VALUE_LITERAL) {
		if (*ds->literal == '\0') {
			return AE_OK;	/* don't use terminating character */
		}
		if (c != *ds->literal) {
			return AE_PARSE;
		}
		ds->literal++;
	}
	return AE_IN_PROGRESS;
}


static void ame_json_set_kvp_value(struct ame_decoder_state *ds,
		struct ame_kvp *kvp)
{
	ame_decoder_template_find(ds, kvp);
	if (ds->ext_mode) {
		kvp->value = &ds->data_buf[ds->data_len];
	} else {
		kvp->value = &ds->buffer[ds->offset];
	}
}

static void ame_json_set_kvp_val_len(struct ame_decoder_state *ds,
		struct ame_kvp *kvp, int n)
{
	if (ds->ext_mode) {
		if (kvp->template_node) {
			kvp->value_length = &ds->data_buf[ds->data_len]
			    - (u8 *)kvp->value + n;
		}
	} else {
		kvp->value_length = &ds->buffer[ds->offset]
		    - (u8 *)kvp->value + n;
	}
}

static void ame_json_set_kvp_key(struct ame_decoder_state *ds,
		struct ame_kvp *kvp)
{
	if (ds->ext_mode) {
		kvp->key = &ds->data_buf[ds->data_len] + 1;
	} else {
		kvp->key = &ds->buffer[ds->offset] + 1;
	}
}

static void ame_json_set_kvp_key_len(struct ame_decoder_state *ds,
		struct ame_kvp *kvp)
{
	if (ds->ext_mode) {
		kvp->key_length = &ds->data_buf[ds->data_len] - (u8 *)kvp->key;

		ame_decoder_template_find(ds, ds->kvp);

		if (!kvp->template_node) {
			ASSERT(ds->data_len >= kvp->key_length);
			ds->data_len -= kvp->key_length;
			kvp->key_length = 0;
		}
	} else {
		kvp->key_length = &ds->buffer[ds->offset] - (u8 *)kvp->key;
	}
}

static void ame_json_pop_children(struct ame_decoder_state *ds,
		struct ame_kvp *parent)
{
	struct ame_kvp *child;
	size_t old_len;
	u8 *new;

	if (ds->ext_mode) {
		child = parent->child;
		if (child && (child->key || child->value)) {
			old_len = ds->data_len;
			if (child->key) {
				new = (u8 *)child->key;
			} else {
				new = (u8 *)child->value;
			}
			if (new > ds->data_buf && new[-1] == '\"') {
				new--;
			}
			if (new > ds->data_buf && new[-1] == ',') {
				new--;
			}
			ds->data_len = new - ds->data_buf;
			memset(new, 0, old_len - ds->data_len);
		}
	}
	ds->kvp_used = (parent - ds->kvp_stack) + 1;
	parent->child = NULL;
	parent->value = NULL;
}

/*
 * Handle completion callback for an individual item.
 */
static enum ada_err ame_json_kvp_parsed(struct ame_decoder_state *ds,
		struct ame_kvp *kvp)
{
	const struct ame_tag *tag = kvp->template_node;
	enum ada_err err = AE_OK;

	if (tag) {
		/*
		 * For the descendent of a partial object,
		 * pop the kvp node but keep the data.
		 */
		if (tag == &ame_tag_partial) {
			ds->kvp_used = kvp - ds->kvp_stack;
			return AE_OK;
		}
		if (tag->cb) {
			/* TODO change tag->cb to return enum ada_err */
			err = (enum ada_err)tag->cb(ds->dec,
			    ds->template_arg, kvp);

			/*
			 * For a partial object, indicate completion with
			 * NULL kvp.
			 */
			if (!err && tag->subtags == &ame_tag_partial) {
				err = (enum ada_err)tag->cb(ds->dec,
				    ds->template_arg, NULL);
			}
		}
		if (tag->subtags == &ame_tag_full) {
			return err;		/* don't pop children */
		}
	}
	if (ds->ext_mode && kvp->parent) {
		ame_json_pop_children(ds, kvp->parent);
	}
	return err;
}

/*
 * Handle partial completion of object parse.
 * Look through stack above the current element for one that requests
 * partial delivery.  If found, pass the partial object to it and remove
 * the data from the buffer.
 * Returns -1 if no partial parse found, or no data freed from buffer.
 */
static int ame_json_partial_parse(struct ame_decoder_state *ds)
{
	struct ame_kvp *kvp;
	const struct ame_tag *tag;
	size_t len;

	kvp = ds->kvp;
	if (!kvp) {
		kvp = ds->parent;
	}
	for (; kvp; kvp = kvp->parent) {
		tag = kvp->template_node;
		if (tag && tag != &ame_tag_partial &&
		    tag->subtags == &ame_tag_partial) {
			goto found;
		}
	}
	return -1;

found:
	ASSERT(kvp->value);
	len = &ds->data_buf[ds->data_len] - (u8 *)kvp->value;
	ASSERT(len <= MAX_U16);
	kvp->value_length = len;
	if (!len) {
		return -1;
	}
	if (tag->cb) {
		tag->cb(ds->dec, ds->template_arg, kvp);
	}
	ds->data_len -= len;
	kvp->value_length = 0;
	return 0;
}

static enum ada_err ame_json_parse(struct ame_decoder_state *ds,
    enum ame_parse_op op, struct ame_kvp **pkvp)
{
	enum ada_err err = AE_PARSE;
	enum ame_parser_state save_state;
	size_t save_offset;
	size_t save_used;
	struct ame_kvp *save_parent;
	u8 reuse_input;
	u8 c;

	ds->error_offset = 0;
	switch (op) {
	case AME_PARSE_FULL:
	default:
		ASSERT(ds->ext_mode == 0);
		ame_decoder_reset(ds);
		ds->kvp = NULL;
		ds->incr_parent = NULL;
		ds->is_first = 0;
		*pkvp = NULL;
		ds->pkvp = *pkvp;
		ds->incremental = 0;
		ds->err = AE_OK;
		break;
	case AME_PARSE_FIRST:
		ame_decoder_reset(ds);
		ds->kvp = NULL;
		ds->incr_parent = NULL;
		ds->is_first = 0;
		*pkvp = NULL;
		ds->pkvp = *pkvp;
		ds->incremental = 1;
		ds->first_called = 1;
		ds->err = AE_OK;
		break;
	case AME_PARSE_NEXT:
		ASSERT(ds->first_called);
		if (ds->err) {
			return ds->err;
		}
		if (ds->ext_mode) {
			if (ds->obj_parsed) {
				/* last object parsed, start a new one */
				ds->kvp = NULL;
				*pkvp = NULL;
				ds->pkvp = *pkvp;
				ds->incremental = 1;
				ds->incr_parent = ds->parent;
				if (!ds->incr_parent) {
					/* done parsing root object */
					return AE_OK;
				}
				/* pop the children objects */
				ame_json_pop_children(ds, ds->incr_parent);
				ds->obj_parsed = 0;
			} else {
				*pkvp = ds->pkvp;
			}
		} else {
			ds->kvp = NULL;
			ds->is_first = 0;
			*pkvp = NULL;
			ds->pkvp = *pkvp;
			ds->incremental = 1;
			ds->incr_parent = ds->parent;
			if (!ds->incr_parent) {
				/* done parsing root object */
				return AE_OK;
			}
			ds->kvp_used = (ds->incr_parent - ds->kvp_stack) + 1;
			ds->incr_parent->child = NULL;
			ds->incr_parent->value = NULL;
			break;
		}
		break;
	}

	/* save state so parser can be restarted */
	save_offset = ds->offset;
	save_state = ds->state;
	save_used = ds->kvp_used;
	save_parent = ds->parent;

	while (ds->offset < ds->length) {
		reuse_input = 0;
		c = ds->buffer[ds->offset];
#ifdef AME_JSON_DEBUG2
#ifdef AME_JSON_TEST
		printf("state %d %s offset %zu dl %zu used %zd "
		    "parent %ld: '%c'\n",
		    ds->state,
		    ds->state < ARRAY_LEN(ame_parser_state_names) ?
		    ame_parser_state_names[ds->state] : "err-range",
		    ds->offset, ds->data_len, ds->kvp_used,
		    ds->parent ? ds->parent - ds->kvp_stack : -1,
		    c);
#else
		log_put_mod(MOD_LOG_CLIENT, LOG_DEBUG2
		    "state %d %s offset %d, dl %zu used %d, parent %p: %c",
		    ds->state,
		    ds->state < ARRAY_LEN(ame_parser_state_names) ?
		    ame_parser_state_names[ds->state] : "err-range",
		    ds->offset, ds->data_len, ds->kvp_used, ds->parent,
		    c);
#endif
#endif
		if (ds->ext_mode) {
			if (ds->data_len >= ds->data_buf_size &&
			    ame_json_partial_parse(ds)) {
				goto parse_error;
			}
			/* copy byte to persistent buffer without advancing */
			ds->data_buf[ds->data_len] = c;
		}
		switch (ds->state) {
		case AME_STATE_VALUE:
			/* { here to prevent cstyle error */
			if (!ds->kvp) {
				err = ame_kvp_alloc(ds, &ds->kvp);
				if (err) {
					goto error_exit;
				}
			}
			switch (c) {
			case '[':
				ds->kvp->type = AME_TYPE_ARRAY;
				ame_json_set_kvp_value(ds, ds->kvp);
				ds->parent = ds->kvp;
				ds->is_first = 1;
				/* remain in value state */
				break;
			case '{':
				ds->kvp->type = AME_TYPE_OBJECT;
				ame_json_set_kvp_value(ds, ds->kvp);
				ds->parent = ds->kvp;
				ds->is_first = 1;
				ds->state = AME_STATE_NAME;
				break;
			case '"':
				ds->kvp->type = AME_TYPE_UTF8;
				err = ame_json_parse_string(ds, c);
				ASSERT(err == AE_IN_PROGRESS);
				ame_decoder_template_find(ds, ds->kvp);
				ame_json_byte_forward(ds);
				ame_json_set_kvp_value(ds, ds->kvp);
				ds->state = AME_STATE_VALUE_STR;
				continue;

			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				ame_json_set_kvp_value(ds, ds->kvp);
				err = ame_json_parse_number(ds,
				    &ds->kvp->type, c);
				ASSERT(err == AE_IN_PROGRESS);
				ame_json_byte_forward(ds);
				continue;
			case 't':
			case 'f':
			case 'n':
				ame_json_set_kvp_value(ds, ds->kvp);
				err = ame_json_parse_literal(ds,
				    &ds->kvp->type, c);
				ASSERT(err == AE_IN_PROGRESS);
				ame_json_byte_forward(ds);
				ds->state = AME_STATE_VALUE_LITERAL;
				continue;

			case ']':
				if (!ds->is_first) {
					goto parse_error;
				}
				/* empty array */
				ame_kvp_pop(ds);
				ds->kvp = NULL;
				reuse_input = 1;	/* reuse ']' */
				ds->state = AME_STATE_CONTINUE;
				break;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				goto skip;
			default:
				goto parse_error;
			}
			/* } to prevent cstyle error */
val_end:
			if (!ds->incr_parent && ds->parent) {
				ds->incr_parent = ds->parent;
			}
			if (ds->incremental && !*pkvp) {
				if (ds->kvp != ds->incr_parent) {
					*pkvp = ds->kvp;
					ds->pkvp = *pkvp;
				} else if (
				    ds->kvp->type == AME_TYPE_OBJECT ||
				    ds->kvp->type == AME_TYPE_ARRAY) {
					ds->kvp->value = NULL;
					ds->incr_parent = ds->kvp;
				}
			}

			/*
			 * no callback at start of array or object.
			 */
			/* { to prevent cstyle error */
			if (c == '[' || c == '{' || !ds->kvp) {
				ds->kvp = NULL;
				break;
			}
			/* } to prevent cstyle error */

			err = ame_json_kvp_parsed(ds, ds->kvp);
			ds->kvp = NULL;
			if (err) {
				if (!reuse_input) {
					ame_json_byte_forward(ds);
				}
				ds->obj_parsed = 1; /* TODO? */
				return err;
			}
			break;

		case AME_STATE_VALUE_NUM_0:
		case AME_STATE_VALUE_NUM_1:
		case AME_STATE_VALUE_NUM_2:
		case AME_STATE_VALUE_NUM_3:
		case AME_STATE_VALUE_NUM_3A:
		case AME_STATE_VALUE_NUM_4:
		case AME_STATE_VALUE_NUM_5:
		case AME_STATE_VALUE_NUM_6:
			err = ame_json_parse_number(ds, &ds->kvp->type, c);
			if (err == AE_IN_PROGRESS) {
				break;
			} else if (err == AE_OK) {
				/* current character is not part of number */
				ame_json_set_kvp_val_len(ds, ds->kvp, 0);
				ds->state = AME_STATE_CONTINUE;
				reuse_input = 1;	/* reuse terminator */
				goto val_end;
			}
			goto error_exit;

		case AME_STATE_VALUE_LITERAL:
			err = ame_json_parse_literal(ds, &ds->kvp->type, c);
			if (err == AE_IN_PROGRESS) {
				break;
			} else if (err == AE_OK) {
				ame_json_set_kvp_val_len(ds, ds->kvp, 0);
				ds->state = AME_STATE_CONTINUE;
				reuse_input = 1;	/* reuse terminator */
				goto val_end;
			}
			goto error_exit;

		case AME_STATE_VALUE_STR:
			err = ame_json_parse_string(ds, c);
			if (err == AE_IN_PROGRESS) {
				break;
			} else if (err == AE_OK) {
				ame_json_set_kvp_val_len(ds, ds->kvp, 0);
				ds->state = AME_STATE_CONTINUE;
				goto val_end;
			}
			goto error_exit;

		case AME_STATE_NAME_STR:
			err = ame_json_parse_string(ds, c);
			if (err == AE_IN_PROGRESS) {
				break;
			} else if (err == AE_OK) {
				goto name_string_end;
			}
			goto error_exit;

		case AME_STATE_NAME:
			/* { to prevent cstyle error */
			switch (c) {
			case '"':
				err = ame_kvp_alloc(ds, &ds->kvp);
				if (err) {
					goto error_exit;
				}
				ame_json_set_kvp_key(ds, ds->kvp);
				err = ame_json_parse_string(ds, c);
				if (err == AE_IN_PROGRESS) {
					ds->state = AME_STATE_NAME_STR;
					ame_json_byte_forward(ds);
					continue;
				} else if (err == AE_OK) {
name_string_end:
					ame_json_set_kvp_key_len(ds, ds->kvp);
					ds->state = AME_STATE_SEPARATOR;
					break;
				}
				goto error_exit;

			case '}':
				if (!ds->is_first) {
					goto parse_error;
				}
				/* empty object */
				ds->state = AME_STATE_CONTINUE;
				continue;	/* reuse last byte */
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				goto skip;
			default:
				goto parse_error;
			}
			/* } to prevent cstyle error */
			break;

		case AME_STATE_SEPARATOR:
			switch (c) {
			case ':':
				ds->state = AME_STATE_VALUE;
				break;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				goto skip;
			default:
				goto parse_error;
			}
			break;

		case AME_STATE_CONTINUE:
			/* { to prevent cstyle error */
			switch (c) {
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				goto skip;
			}
			ds->kvp1 = ds->parent;
			if (!ds->kvp1) {
				/* data after the end root value */
				goto parse_error;
			}
			ds->is_first = 0;
			switch (c) {
			case '}':
				if (ds->parent->type != AME_TYPE_OBJECT) {
					goto parse_error;
				}
				goto object_end;
			case ']':
				if (ds->parent->type != AME_TYPE_ARRAY) {
					goto parse_error;
				}
object_end:
				if (ds->kvp1->value) {
					ame_json_set_kvp_val_len(ds,
					    ds->kvp1, 1);
				}
				ds->parent = ds->kvp1->parent;

				err = ame_json_kvp_parsed(ds, ds->kvp1);
				if (err) {
					ame_json_byte_forward(ds);
					ds->obj_parsed = 1;
					return err;
				}
				break;
			case ',':
				if (ds->parent->type == AME_TYPE_OBJECT) {
					ds->state = AME_STATE_NAME;
				} else if (ds->parent->type ==
				    AME_TYPE_ARRAY) {
					ds->state = AME_STATE_VALUE;
				} else {
					goto parse_error;
				}
				break;
			default:
				goto parse_error;
			}
			/* } to prevent cstyle error */
			break;
		default:
			goto parse_error;
		}
		if (!reuse_input) {
			ame_json_byte_forward(ds);
		}
		continue;
skip:
		/* skip whitespace between fields without copying to ext buf */
		ds->offset++;
	}

	if (ds->ext_mode) {
		ame_json_partial_parse(ds);
		if (ds->parent) {
			return AE_UNEXP_END;
		}
		return AE_OK;
	}
	if (!ds->kvp_used) {
		/* no values found */
		err = AE_UNEXP_END;
		goto error_exit;
	}

	if (!ds->parent) {
		/* root object has been fully parsed */
		if (!ds->incremental) {
			if (ds->kvp_stack->type != AME_TYPE_UTF8) {
				ame_json_set_kvp_val_len(ds, ds->kvp_stack, 0);
			}
			*pkvp = ds->kvp_stack;
			ds->pkvp = *pkvp;
		}
		ASSERT(ds->data_len <= ds->data_buf_size);
		ds->obj_parsed = 1;
		return AE_OK;
	}

	if (ds->incremental && ds->parent == ds->incr_parent && *pkvp &&
	    (ds->state == AME_STATE_VALUE || ds->state == AME_STATE_NAME)) {
		/*
		 * Fully decoded an element of the parent and ready to begin
		 * processing the subsequent element. Avoid returning
		 * truncated numbers by waiting to return okay until a
		 * trailing token has been processed (i.e. state is back
		 * to AME_STATE_VALUE or AME_STATE_NAME).
		 */
		ASSERT(ds->data_len <= ds->data_buf_size);
		ds->obj_parsed = 1;
		return AE_OK;
	}

	err = AE_UNEXP_END;
	goto error_exit;

parse_error:
	err = AE_PARSE;
error_exit:
#ifdef AME_JSON_DEBUG
	if (err != AE_UNEXP_END) {
#ifdef AME_JSON_TEST
		printf("JSON parse err %d, offset %ld, c %c (%02x), state %d\n",
		    err, ds->offset, (char)c, c, ds->state);
#else
		printcli("JSON parse err %d, offset %d, c %c (%02x), state %d",
		    err, ds->offset, (char)c, c, ds->state);
#endif
	}
#endif
	ds->error_offset = ds->offset;
	ds->state = save_state;
	ds->offset = save_offset;
	ds->kvp_used = save_used;
	ds->parent = save_parent;
	ds->err = err;
	*pkvp = NULL;
	return err;
}

static enum ada_err ame_json_get_opaque(struct ame_kvp *parent,
    const struct ame_key *key, void *value, size_t *length)
{
	struct ame_kvp *kvp;
	enum ada_err err;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type == AME_TYPE_NULL) {
		*length = 0;
		return AE_OK;
	}
	if (kvp->type != AME_TYPE_UTF8) {
		return AE_INVAL_TYPE;
	}

	if (ayla_base64_decode(kvp->value, kvp->value_length, value, length)) {
		return AE_ERR;
	}

	return AE_OK;
}

static enum ada_err ame_json_get_utf8(struct ame_kvp *parent,
    const struct ame_key *key, char *value_in, size_t *length)
{
	struct ame_kvp *kvp;
	enum ada_err err;
	int escape = 0;
	int hexdigs = 0;
	int count = 0;
	const char *p;
	const char *end;
	char unicode_str[5];
	u32 unicode;
	char *errptr;
	char utf8_str[6];
	ssize_t utf8_len;
	char *value = value_in;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type == AME_TYPE_NULL) {
		goto done;
	}

	if (kvp->type != AME_TYPE_UTF8) {
		return AE_INVAL_TYPE;
	}

	p = (const char *)kvp->value;
	end = p + kvp->value_length;
	while (p < end && count < *length - 1) {
		if (escape) {
			escape = 0;
			switch (*p) {
			case 'b':
				*value++ = '\b';
				count++;
				break;
			case 'f':
				*value++ = '\f';
				count++;
				break;
			case 'n':
				*value++ = '\n';
				count++;
				break;
			case 'r':
				*value++ = '\r';
				count++;
				break;
			case 't':
				*value++ = '\t';
				count++;
				break;
			case 'u':
				hexdigs = 4;
				break;
			default:
				*value++ = *p;
				count++;
				break;
			}
		} else if (hexdigs) {
			unicode_str[4-hexdigs] = *p;
			if (--hexdigs == 0) {
				/* convert unicode to utf8 */
				unicode_str[4] = '\0';
				unicode = strtoul(unicode_str, &errptr, 16);
				if (errptr != &unicode_str[4]) {
					*value_in = '\0';
					return AE_ERR;
				}
				utf8_len = utf8_encode((u8 *)utf8_str,
				    sizeof(utf8_str), unicode);
				if (utf8_len == -1 ||
				    utf8_len > sizeof(utf8_str)) {
					*value_in = '\0';
					return AE_ERR;
				}
				if (utf8_len >= *length - count) {
					/* truncate before character */
					break;
				}
				memcpy(value, utf8_str, utf8_len);
				value += utf8_len;
				count += utf8_len;
			}
		} else {
			switch (*p) {
			case '\\':
				escape = 1;
				break;
			default:
				*value++ = *p;
				count++;
				break;
			}
		}
		p++;
	}

done:
	*value = '\0';
	*length = count;

	return AE_OK;
}

static enum ada_err ame_json_get_s64(struct ame_kvp *parent,
    const struct ame_key *key, s64 *value)
{
	char *tailptr;
	struct ame_kvp *kvp;
	enum ada_err err;
	s64 val;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type != AME_TYPE_INTEGER && kvp->type != AME_TYPE_DECIMAL) {
		return AE_INVAL_TYPE;
	}

	/*
	 * For decimals, strtoll() will truncate the value.
	 */
	val = strtoll((char *)kvp->value, &tailptr, 10);
	*value = val;
	return AE_OK;
}

static enum ada_err ame_json_get_s32(struct ame_kvp *parent,
    const struct ame_key *key, s32 *value)
{
	s64 result;
	enum ada_err err;

	err = ame_json_get_s64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_S32 || result < MIN_S32) {
		return AE_INVAL_VAL;
	}

	*value = (s32)result;
	return AE_OK;
}

static enum ada_err ame_json_get_s16(struct ame_kvp *parent,
    const struct ame_key *key, s16 *value)
{
	s64 result;
	enum ada_err err;

	err = ame_json_get_s64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_S16 || result < MIN_S16) {
		return AE_INVAL_VAL;
	}

	*value = (s16)result;
	return AE_OK;
}

static enum ada_err ame_json_get_s8(struct ame_kvp *parent,
    const struct ame_key *key, s8 *value)
{
	s64 result;
	enum ada_err err;

	err = ame_json_get_s64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_S8 || result < MIN_S8) {
		return AE_INVAL_VAL;
	}

	*value = (s8)result;
	return AE_OK;
}

static enum ada_err ame_json_get_u64(struct ame_kvp *parent,
    const struct ame_key *key, u64 *value)
{
	char *tailptr;
	struct ame_kvp *kvp;
	enum ada_err err;
	u64 val;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type != AME_TYPE_INTEGER && kvp->type != AME_TYPE_DECIMAL) {
		return AE_INVAL_TYPE;
	}

	/*
	 * For decimals, strtoull() will truncate the value.
	 */
	val = strtoull((char *)kvp->value, &tailptr, 10);
	*value = val;
	return AE_OK;
}

static enum ada_err ame_json_get_u32(struct ame_kvp *parent,
    const struct ame_key *key, u32 *value)
{
	u64 result;
	enum ada_err err;

	err = ame_json_get_u64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_U32) {
		return AE_INVAL_VAL;
	}

	*value = (u32)result;
	return AE_OK;
}

static enum ada_err ame_json_get_u16(struct ame_kvp *parent,
    const struct ame_key *key, u16 *value)
{
	u64 result;
	enum ada_err err;

	err = ame_json_get_u64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_U16) {
		return AE_INVAL_VAL;
	}

	*value = (u16)result;
	return AE_OK;
}

static enum ada_err ame_json_get_u8(struct ame_kvp *parent,
    const struct ame_key *key, u8 *value)
{
	u64 result;
	enum ada_err err;

	err = ame_json_get_u64(parent, key, &result);
	if (err) {
		return err;
	}

	if (result > MAX_U8) {
		return AE_INVAL_VAL;
	}

	*value = (u8)result;
	return AE_OK;
}

static enum ada_err ame_json_get_d32(struct ame_kvp *parent,
    const struct ame_key *key, s32 *value, u8 scale)
{
	struct ame_kvp *kvp;
	enum ada_err err;
	u8 i = 0;
	const char *p;
	const char *end;
	char in[32];
	int point;
	int exponent = 0;
	s64 temp;
	char *errptr;

	ASSERT(scale < 10);

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type != AME_TYPE_INTEGER &&
	    kvp->type != AME_TYPE_DECIMAL &&
	    kvp->type != AME_TYPE_FLOAT) {
		return AE_INVAL_TYPE;
	}

	p = (const char *)kvp->value;
	end = p + kvp->value_length;

	if (*p == '-') {
		in[i++] = *p++;
	}
	while (p < end && *p >= '0' && *p <= '9') {
		in[i++] = *p++;
		if (i >= sizeof(in)) {
			return AE_INVAL_VAL;
		}
	}

	point = i;
	if (p < end && *p == '.') {
		p++;
		while (p < end && *p >= '0' && *p <= '9') {
			in[i++] = *p++;
			if (i >= sizeof(in)) {
				return AE_INVAL_VAL;
			}
		}
	}
	in[i]  = '\0';

	if (p < end && (*p == 'e' || *p == 'E')) {
		p++;
		errno = 0;
		exponent = strtol(p, &errptr, 10);
		if (errno || errptr == p || *errptr) {
			return AE_INVAL_VAL;
		}
	}

	point = point + scale + exponent;
	if (point < 0) {
		*value = 0;
		return AE_OK;
	} else if (point >= sizeof(in)) {
		return AE_INVAL_VAL;
	} else if (point < i) {
		in[point] = '\0';
	}

	/*
	 * In bc, errno is not set if there is no thread-local storage.
	 * Use a 64-bit parse and check range here.
	 */
	errno = 0;
	temp = strtoll(in, &errptr, 10);
	if (errno || errptr == in || *errptr) {
		return AE_INVAL_VAL;
	}
	if (temp > MAX_S32 || temp < MIN_S32) {
		return AE_INVAL_VAL;
	}
	while (i < point) {
		if (temp > MAX_S32 / 10 || temp < MIN_S32 / 10) {
			return AE_INVAL_VAL;
		}
		temp *= 10;
		i++;
	}

	*value = (s32)temp;
	return AE_OK;
}

static enum ada_err ame_json_get_boolean(struct ame_kvp *parent,
    const struct ame_key *key, s32 *value)
{
	struct ame_kvp *kvp;
	enum ada_err err;

	err = ame_get_child(parent, key, &kvp);
	if (err) {
		return err;
	}

	if (kvp->type != AME_TYPE_BOOLEAN) {
		return AE_INVAL_TYPE;
	}

	if (*(char *)kvp->value == 'f') {
		*value = 0;
	} else {
		*value = 1;
	}
	return AE_OK;
}

const struct ame_decoder ame_json_dec = {
	.parse = ame_json_parse,
	.get_opaque = ame_json_get_opaque,
	.get_utf8 = ame_json_get_utf8,
	.get_s64 = ame_json_get_s64,
	.get_s32 = ame_json_get_s32,
	.get_s16 = ame_json_get_s16,
	.get_s8 = ame_json_get_s8,
	.get_u64 = ame_json_get_u64,
	.get_u32 = ame_json_get_u32,
	.get_u16 = ame_json_get_u16,
	.get_u8 = ame_json_get_u8,
	.get_d32 = ame_json_get_d32,
	.get_boolean = ame_json_get_boolean,
};

#ifdef AME_ENCODER_TEST
static void ame_json_test_d32_hlpr2(const struct ame_decoder *dec,
    struct ame_decoder_state *ds, const char *in, u8 scale, s32 expect,
    enum ada_err exp_err)
{
	enum ada_err err;
	s32 *out = (s32 *)ame_test_output;
	size_t length = strlen(in);
	struct ame_kvp *kvp1;

	ame_test_dump((u8 *)in, length);

	ame_decoder_stack_set(ds, ame_test_stack,
	    ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, (u8 *)in, length);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		if (exp_err == err) {
			return;
		}
		printcli("parse error %d offset %zu", err, ds->error_offset);
		return;
	}
	err = dec->get_d32(kvp1, NULL, out, scale);
	if (err) {
		if (exp_err == err) {
			return;
		}
		if (exp_err) {
			printcli("error expected: error %d "
			    "getting value error %d",
			    exp_err, err);
			return;
		}
		printcli("error: getting value error %d", err);
		return;
	}
	if (exp_err) {
		printcli("error expected: error %d decoded: %ld",
		    exp_err, *out);
		return;
	}
	if (*out != expect) {
		printcli("error expected: %ld, decoded: %ld\n", expect, *out);
	}
}

static void ame_json_test_d32_hlpr(const struct ame_decoder *dec,
    struct ame_decoder_state *ds, const char *in, u8 scale, s32 expect)
{
	ame_json_test_d32_hlpr2(dec, ds, in, scale, expect, AE_OK);
}

static void ame_json_parse_err_test(const struct ame_decoder *dec,
    struct ame_decoder_state *ds, const char *in)
{
	ame_json_test_d32_hlpr2(dec, ds, in, 0, -1, AE_PARSE);
}

static void ame_json_range_err_test(const struct ame_decoder *dec,
    struct ame_decoder_state *ds, const char *in, u8 scale)
{
	ame_json_test_d32_hlpr2(dec, ds, in, scale, -1, AE_INVAL_VAL);
}

static void ame_json_test_d32(const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	printcli("JSON d32 tests\n");
	ame_json_test_d32_hlpr(dec, ds, "0", 2, 0);
	ame_json_test_d32_hlpr(dec, ds, "1", 2, 100);
	ame_json_test_d32_hlpr(dec, ds, "-1", 2, -100);
	ame_json_test_d32_hlpr(dec, ds, "1e-2", 3, 10);
	ame_json_test_d32_hlpr(dec, ds, "-1E-2", 3, -10);
	ame_json_test_d32_hlpr(dec, ds, "1.234", 2, 123);
	ame_json_test_d32_hlpr(dec, ds, "-1.234e1", 2, -1234);
	ame_json_test_d32_hlpr(dec, ds, "-12.34e+3", 2, -1234000);
	ame_json_test_d32_hlpr(dec, ds, "-1234e-5", 2, -1);
	ame_json_test_d32_hlpr(dec, ds, "0.123456e2", 2, 1234);
	ame_json_test_d32_hlpr(dec, ds, "-0.123456e2", 2, -1234);
	ame_json_test_d32_hlpr(dec, ds, "0.123456e2", 4, 123456);
	ame_json_test_d32_hlpr(dec, ds, "-0.123456E2", 4, -123456);
	ame_json_test_d32_hlpr(dec, ds, "0.123456E-2", 4, 12);
	ame_json_test_d32_hlpr(dec, ds, "-0.1234567e+2", 4, -123456);
	ame_json_test_d32_hlpr(dec, ds, "1e-100", 4, 0);
	ame_json_test_d32_hlpr(dec, ds, "2147483647", 0, 2147483647);
	ame_json_test_d32_hlpr(dec, ds, "-2147483648", 0, -2147483648);

	printcli("JSON d32 negative tests\n");
	ame_json_parse_err_test(dec, ds, "+1");
	ame_json_parse_err_test(dec, ds, "1a2b3c");
	/* value too large */
	ame_json_range_err_test(dec, ds, "2147483648", 0);
	ame_json_range_err_test(dec, ds, "-2147483649", 0);
	ame_json_range_err_test(dec, ds, "1e100", 4);
	/* invalid leading zeros in number or exponent */
	ame_json_parse_err_test(dec, ds, "00");
	ame_json_parse_err_test(dec, ds, "01");
	/* missing digit after after exponent */
	ame_json_parse_err_test(dec, ds, "[1e+]");
	ame_json_parse_err_test(dec, ds, "[1e-]");
	ame_json_parse_err_test(dec, ds, "1e+");
	ame_json_parse_err_test(dec, ds, "1e-");
	ame_json_parse_err_test(dec, ds, "[1e,]");
	ame_json_parse_err_test(dec, ds, "1e");
	/* trailing comma */
	ame_json_parse_err_test(dec, ds, "[1,2,3,]");
	/* ending in decimal point */
	ame_json_parse_err_test(dec, ds, "123.");
	ame_json_parse_err_test(dec, ds, "[123.]");
	ame_json_parse_err_test(dec, ds, "[123.e5]");
	/* embedded space */
	ame_json_parse_err_test(dec, ds, "1 2 3");
}

/*
 * Form test batch responses where id is a number and status is that number
 * followed by 1.
 */
#define DPS_RSP_(id, val, end)	\
	"{\"batch_id\":" #id ",\"status\" : " #id "1 ," \
	"\"property\":{\"dsn\":null,"    \
	"\"name\":\"log\",\"base_type\":\"string\",\"value\":\"" val "\","  \
	"\"dev_time_ms\":\"\\t2019-05-16T04:28:24Z\"}}" end

#define DPS_RSP(id)	DPS_RSP_(id, "123456", ",")

#define DPS_STRING_10 "123456789 "
#define DPS_STRING_20 DPS_STRING_10 DPS_STRING_10
#define DPS_STRING_40 DPS_STRING_20 DPS_STRING_20
#define DPS_STRING_80 DPS_STRING_40 DPS_STRING_40
#define DPS_STRING_160 DPS_STRING_80 DPS_STRING_80
#define DPS_STRING_320 DPS_STRING_160 DPS_STRING_160
#define DPS_STRING_640 DPS_STRING_320 DPS_STRING_320
#define DPS_STRING_1280 DPS_STRING_640 DPS_STRING_640

static const char ame_json_test_in_small[] =
	"{\"batch_datapoints\":["
	DPS_RSP_(145, "", "")
	"]}";

#define AME_JSON_TEST_ID_MIN 80
#define AME_JSON_TEST_ID_LIMIT 146

static u8 ame_json_test_status_seen[AME_JSON_TEST_ID_LIMIT];

static const char ame_json_test_in[] =
	"{\"batch_datapoints\":["
	DPS_RSP(80)
	DPS_RSP_(81, DPS_STRING_1280, ",")
	DPS_RSP(82)
	DPS_RSP(83)
	DPS_RSP(84)
	DPS_RSP(85)
	DPS_RSP(86)
	DPS_RSP(87)
	DPS_RSP(88)
	DPS_RSP(89)
	DPS_RSP(90)
	DPS_RSP(91)
	DPS_RSP(92)
	DPS_RSP(93)
	DPS_RSP(94)
	DPS_RSP(95)
	DPS_RSP(96)
	DPS_RSP(97)
	DPS_RSP(98)
	DPS_RSP(99)
	DPS_RSP(100)
	DPS_RSP(101)
	DPS_RSP(102)
	DPS_RSP(103)
	DPS_RSP(104)
	DPS_RSP(105)
	DPS_RSP(106)
	DPS_RSP(107)
	DPS_RSP(108)
	DPS_RSP(109)
	DPS_RSP(110)
	DPS_RSP(111)
	DPS_RSP(112)
	DPS_RSP(113)
	DPS_RSP(114)
	DPS_RSP(115)
	DPS_RSP(116)
	DPS_RSP(117)
	DPS_RSP(118)
	DPS_RSP(119)
	DPS_RSP(120)
	DPS_RSP(121)
	DPS_RSP(122)
	DPS_RSP(123)
	DPS_RSP(124)
	DPS_RSP(125)
	DPS_RSP(126)
	DPS_RSP(127)
	DPS_RSP(128)
	DPS_RSP(129)
	DPS_RSP(130)
	DPS_RSP(131)
	DPS_RSP(132)
	DPS_RSP(133)
	DPS_RSP(134)
	DPS_RSP(135)
	DPS_RSP(136)
	DPS_RSP(137)
	DPS_RSP(138)
	DPS_RSP(139)
	DPS_RSP(140)
	DPS_RSP(141)
	DPS_RSP(142)
	DPS_RSP(143)
	DPS_RSP(144)
	DPS_RSP_(145, "", "")
	"]}";

/*
 * Tree of desired JSON tags for POST batch response.
 */
static const struct ame_tag ame_json_test_template4[] = {
	AME_TAG("batch_id", NULL, NULL),
	AME_TAG("status", NULL, NULL),
	AME_TAG_END
};

static const struct ame_tag ame_json_test_template3[] = {
	AME_TAG("", ame_json_test_template4, NULL),
	AME_TAG_END
};

static const struct ame_tag ame_json_test_template2[] = {
	AME_TAG("batch_datapoints", ame_json_test_template3, NULL),
	AME_TAG_END
};

static const struct ame_tag ame_json_test_template[] = {
	AME_TAG("", ame_json_test_template2, NULL),
	AME_TAG_END
};

static int ame_json_incr_test_accept(struct ame_decoder_state *ds,
				struct ame_kvp *parent)
{
	const struct ame_decoder *dec = &ame_json_dec;
	enum ada_err err;
	u16 id;
	u16 status = 0;
	u16 expect = 0;

	/* first batch ID comes in as the top level array */
	/* for now, use main object to find batch array */
	parent = ame_find_child(ds->kvp_stack, &ame_json_test_batch_dp_key);
	if (!parent || parent->type != AME_TYPE_ARRAY) {
		printcli("batch_resp: array not found");
		return 0;
	}
	parent = parent->child;
	if (!parent) {
		printcli("batch_resp: no child in array");
		return 0;
	}
	if (parent->type != AME_TYPE_OBJECT) {
		printcli("%s: parent type %u", __func__, parent->type);
		return -1;
	}
	err = dec->get_u16(parent, &ame_json_test_batch_key, &id);
	if (err) {
		printcli("%s: batch_id err %d", __func__, err);
		return -1;
	}

	err = dec->get_u16(parent, &ame_json_test_status_key, &status);
	if (err) {
		printcli("%s: status err %d", __func__, err);
		return -1;
	}

	expect = id * 10 + 1;		/* DSP_RSP macro forms this status */
	if (status != expect) {
		printcli("%s: batch_id status mismatch: "
		   "id %u status %u expected %u",
		    __func__, id, status, expect);
		return -1;
	}
	if (id < ARRAY_LEN(ame_json_test_status_seen)) {
		ame_json_test_status_seen[id]++;
	} else {
		printcli("%s: id %u out of range", __func__, id);
	}
	return 0;
}

static unsigned int ame_json_incr_status_check(u16 first_id, u16 last_id,
				unsigned int expected)
{
	u16 id;
	u16 count;
	unsigned int errors = 0;

	for (id = first_id; id <= last_id; id++) {
		if (id >= ARRAY_LEN(ame_json_test_status_seen)) {
			count = 0;
		} else {
			count = ame_json_test_status_seen[id];
		}
		if (count != expected) {
			printcli("%s: id %u expected %u saw %u",
			   __func__, id, expected, count);
			errors++;
		}
	}
	return errors;
}

static void ame_json_incr_test(const char *test_in,
		int (*handler)(struct ame_decoder_state *, struct ame_kvp *),
		enum ada_err exp_err)
{
	enum ada_err err;
	struct ame_json_test_state {
		struct ame_decoder_state dstate;
		struct ame_kvp stack[80];
		u8 buf[200];
		const u8 *next_in;
		u8 done;
	} state[2];
	struct ame_json_test_state *sp;
	int decoder = 0;
	struct ame_decoder_state *ds;
	struct ame_kvp *kvp;
	u8 in[10];
	int round;
	size_t len;
	unsigned int errors = 0;
	unsigned int done;

	if (strlen(test_in) < 100) {
		printcli("%s: in \"%s\"", __func__, test_in);
	}
	memset(ame_json_test_status_seen, 0, sizeof(ame_json_test_status_seen));
	for (round = 0; round < 2; round++) {
		printcli("%s: start %u", __func__, round);

		/* initialize each decoder */
		done = 0;
		for (decoder = 0; decoder < ARRAY_LEN(state); decoder++) {
			sp = &state[decoder];
			memset(sp, 0, sizeof(*sp));
			ds = &sp->dstate;
			ame_decoder_stack_set(ds,
			    sp->stack, ARRAY_LEN(sp->stack));
			ame_decoder_set_ext_mode(ds, 1,
			    sp->buf, sizeof(sp->buf));
			ame_decoder_template_set(ds, ame_json_test_template);
			sp->next_in = (const u8 *)test_in;
		}

		/*
		 * Send a different number of bytes to each decoder until
		 * all have finished handling input.
		 * The first decoder will finish last.
		 */
		for (decoder = 0;; decoder = (decoder + 1) % ARRAY_LEN(state)) {
			sp = &state[decoder];
			ds = &sp->dstate;
			if (sp->done) {
				continue;
			}
			for (len = 0; len < decoder + 1; len++) {
				if (!*sp->next_in) {
					break;
				}
				in[len] = *sp->next_in++;
			}
			if (!len) {
				sp->done = 1;
				if (++done >= ARRAY_LEN(state)) {
					break;
				}
				continue;
			}
			in[len] = 'x';		/* invalid, past input */
			in[len + 1] = '\0';
#ifdef AME_JSON_DEBUG2
			printcli("decoder %u len %u in \"%s\"",
			    decoder, len, in);
#endif
			ame_decoder_buffer_set(ds, in, len);
			while (!(err = ame_json_dec.parse(ds,
			    ds->first_called ?
			    AME_PARSE_NEXT : AME_PARSE_FIRST, &kvp))) {
				ASSERT(kvp);
				if (handler && handler(ds, kvp)) {
					errors++;
				}
			}
			if (err != AE_UNEXP_END) {
				if (err == exp_err) {
					continue;
				}
				if (exp_err) {
					printcli("%s: decoder %u "
					    "expected err %d",
					     __func__, decoder, exp_err);
				}
				printcli("%s: decoder %u %p: "
				    "parse err %d offset %d in \"%s\"",
				    __func__, decoder, ds, err,
				    (sp->next_in - (u8 *)test_in) - len +
				    ds->error_offset, in);
				errors++;
				sp->done = 1;
				if (++done >= ARRAY_LEN(state)) {
					break;
				}
			}
		}
		printcli("%s: end round %u errors %u", __func__, round, errors);
	}
}

void ame_json_test(void)
{
	struct ame_encoder_state encoder_state;
	struct ame_decoder_state decoder_state;

	ame_test(&ame_json_enc, &encoder_state, &ame_json_dec, &decoder_state);
	ame_json_test_d32(&ame_json_dec, &decoder_state);

	ame_json_incr_test(ame_json_test_in,
	    ame_json_incr_test_accept, AE_OK);
	ame_json_incr_status_check(AME_JSON_TEST_ID_MIN,
	    AME_JSON_TEST_ID_LIMIT - 1, 4);

	ame_json_incr_test(ame_json_test_in_small,
	    ame_json_incr_test_accept, AE_OK);

	ame_json_incr_test("[false,\"\\u1234\\\"\",null", NULL, AE_UNEXP_END);
	ame_json_incr_test("[false,\"\\u1234\\\"\",null", NULL, AE_UNEXP_END);
	ame_json_incr_test("[true,\"\\u1234\\\"\",false", NULL, AE_UNEXP_END);
	ame_json_incr_test("[true,\"1\\u1234\\\"\",false]", NULL, AE_OK);
	ame_json_incr_test("[true,\"\\u1234\\\"\",1,false]", NULL, AE_OK);
	ame_json_incr_test("[true,\"\\u1234\\\"\",1,false]", NULL, AE_OK);
}
#endif
