/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <string.h>
#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <al/al_os_mem.h>
#include "ame.h"

#if defined(AME_ENCODER_TEST) || defined(AME_JSON_TEST)
#include <stdio.h>
#else
#include <ayla/log.h>
#include <ayla/mod_log.h>
#endif

const char *ame_type_names[] = {
	[AME_TYPE_UNKNOWN] = "unknown",
	[AME_TYPE_OBJECT] = "object",
	[AME_TYPE_ARRAY] = "array",
	[AME_TYPE_NULL] = "null",
	[AME_TYPE_BOOLEAN] = "boolean",
	[AME_TYPE_UTF8] = "utf8",
	[AME_TYPE_INTEGER] = "integer",
	[AME_TYPE_FLOAT] = "float",
	[AME_TYPE_OPAQUE] = "opaque"
};

const struct ame_tag ame_tag_partial = AME_TAG_PARTIAL("__partial__", NULL);
const struct ame_tag ame_tag_full = AME_TAG_FULL("__full__", NULL);
static const struct ame_tag ame_no_skip;

void ame_decoder_set_ext_mode(struct ame_decoder_state *ds, u8 on,
	u8 *parsed_data_buf, size_t buf_size)
{
	ds->data_buf = parsed_data_buf;
	ds->data_buf_size = buf_size;
	ds->data_len = 0;
	ds->ext_mode = on ? 1 : 0;
	ds->first_called = 0;
}

const char *ame_kvp_type_name(enum ame_type type)
{
	if (type >= AME_TYPE_COUNT) {
		type = AME_TYPE_UNKNOWN;
	}
	return ame_type_names[type];
}

enum ada_err ame_kvp_alloc(struct ame_decoder_state *ds,
    struct ame_kvp **new_kvp)
{
	struct ame_kvp *kvp;
	struct ame_kvp *parent;
	struct ame_kvp *child;

	*new_kvp = NULL;
	if (ds->kvp_used >= ds->kvp_count) {
		return AE_ALLOC;
	}
	kvp = &ds->kvp_stack[ds->kvp_used];
	ds->kvp_used += 1;
	memset(kvp, 0, sizeof(struct ame_kvp));
	parent = ds->parent;
	kvp->parent = parent;
	/* Insert as next child of parent */
	if (parent) {
		if (parent->child) {
			child = parent->child;
			while (child->next) {
				child = child->next;
			}
			child->next = kvp;
		} else {
			parent->child = kvp;
		}
	}
	*new_kvp = kvp;
	return AE_OK;
}

enum ada_err ame_kvp_pop(struct ame_decoder_state *ds)
{
	struct ame_kvp *kvp;
	struct ame_kvp *parent;
	struct ame_kvp *next;

	if (ds->kvp_used == 0) {
		return AE_OK;
	}
	ds->kvp_used -= 1;

	kvp = &ds->kvp_stack[ds->kvp_used];
	parent = kvp->parent;
	if (!parent) {
		return AE_OK;
	}

	if (parent->child == kvp) {
		if (ds->ext_mode) {
			if (kvp->key) {
				ds->data_len = ((u8 *)kvp->key
				    - ds->data_buf) - 1;
			} else if (kvp->value) {
				ds->data_len = ((u8 *)kvp->value
				    - ds->data_buf) - 1;
			} else {
				/* data_len does not change */
			}
		}
		parent->child = NULL;
		return AE_OK;
	}

	next = parent->child;
	while (next) {
		if (next->next == kvp) {
			next->next = NULL;
			return AE_OK;
		}
		next = next->next;
	}

	/* shouldn't ever get here */
	return AE_ERR;
}

/*
 * Find the corresponding node in the template tree.
 */
void ame_decoder_template_find(struct ame_decoder_state *ds,
				struct ame_kvp *kvp)
{
	struct ame_kvp *parent;
	const struct ame_tag *tnode;
	size_t len;

	if (kvp->template_node) {
		return;
	}

	/*
	 * If there is no template tree, set the template pointer non-NULL.
	 * We interpret a NULL kvp->template_node to mean the object
	 * should be skipped.
	 */
	tnode = ds->template_tree;
	if (!tnode) {
		kvp->template_node = &ame_no_skip;
		return;
	}

	parent = kvp->parent;
	if (parent) {
		tnode = parent->template_node;
		if (!tnode) {
			return;
		}
		tnode = tnode->subtags;
		if (!tnode) {
			return;
		}

		/*
		 * If parent's subtags indicates to deliver it
		 * piece-wise, set child tnodes to not be skipped.
		 */
		if (tnode == &ame_tag_partial) {
			kvp->template_node = tnode;
			return;
		}

		/*
		 * If parent's subtags indicates all sub objects should be
		 * parsed as well without callbacks, set child tnode the same.
		 */
		if (tnode == &ame_tag_full) {
			kvp->template_node = tnode;
			return;
		}
	}
	if (!tnode) {
		return;
	}
	for (; tnode->name; tnode++) {
		len = strlen(tnode->name);
		if (len != kvp->key_length ||
		    memcmp(tnode->name, kvp->key, len)) {
			continue;
		}

		kvp->template_node = tnode;
		break;
	}
}

/*
 * Find kvp child of parent with matching key.
 */
struct ame_kvp *ame_find_child(const struct ame_kvp *parent,
				const struct ame_key *key)
{
	struct ame_kvp *kvp;
	size_t len;

	if (!parent) {
		return NULL;
	}
	len = strlen(key->tag);
	for (kvp = parent->child; kvp; kvp = kvp->next) {
		if (len == kvp->key_length &&
		    !memcmp(kvp->key, key->tag, len)) {
			return kvp;
		}
	}
	return NULL;
}

/*
 * Find the immediate child with the specified key.
 * A null key pointer indicates to return the parent pointer.
 */
enum ada_err ame_get_child(struct ame_kvp *parent, const struct ame_key *key,
    struct ame_kvp **child)
{
	struct ame_kvp *kvp;

	if (!key) {
		*child = parent;
		return AE_OK;
	}

	kvp = ame_find_child(parent, key);
	if (!kvp) {
		return AE_NOT_FOUND;
	}
	*child = kvp;
	return AE_OK;
}

struct ame_decoder_state *ame_decoder_state_alloc(int stack_size)
{
	struct ame_decoder_state *ds;
	struct ame_kvp *kvp_stack;

	kvp_stack = (struct ame_kvp *)
	    al_os_mem_calloc(stack_size * sizeof(struct ame_kvp));
	if (!kvp_stack) {
		return NULL;
	}

	ds = (struct ame_decoder_state *)
	    al_os_mem_calloc(sizeof(struct ame_decoder_state));
	if (!ds) {
		free(kvp_stack);
		return NULL;
	}

	ame_decoder_stack_set(ds, kvp_stack, stack_size);

	return ds;
}

void ame_decoder_state_free(struct ame_decoder_state *ds)
{
	free(ds->kvp_stack);
	free(ds);
}

#ifdef AME_ENCODER_TEST
static u8 ame_test_input[256];
static u8 ame_test_buffer[1024];
u8 ame_test_output[256];
struct ame_kvp ame_test_stack[50];

AME_KEY(k_test, "test0", "test 0");

void ame_test_dump(const u8 *buf, size_t size)
{
	char hex[49];
	char ascii[17];
	int i = 0;
	int line = 0;
	int j;
	size_t offset;

	while (i < size) {
		offset = 0;
		for (j = 0; j < 16 && i < size; ++i, ++j, ++buf) {
			snprintf(&hex[offset], 4, "%02x ", *buf);
			if (*buf >= 0x20 && *buf <= 0x7e) {
				ascii[j] = *buf;
			} else {
				ascii[j] = '.';
			}
			offset += 3;
		}
		ascii[j] = '\0';
		while (j < 16) {
			snprintf(&hex[offset], 4, "   ");
			offset += 3;
			j++;
		}
		printcli("%04x: %s  %s\n", line++ * 16, hex, ascii);
	}
}

static void ame_test_dump_buffers(size_t input_length, size_t encoded_length)
{
	printcli("Input buffer\n");
	ame_test_dump(ame_test_input, input_length);
	printcli("Encoded buffer\n");
	ame_test_dump(ame_test_buffer, encoded_length);
	printcli("Output buffer\n");
	ame_test_dump(ame_test_output, input_length);
}

/*
 * Compare kvp value with expected value.  Print both on mismatch.
 * Return 0 on success.
 */
static int ame_test_kvp_value_match(const struct ame_kvp *kvp, char *expected)
{
	size_t len = strlen(expected);
	char val[80];

	if (!kvp->value) {
		printcli("error: value NULL expected \"%s\"", expected);
		return -1;
	}
	if (kvp->value_length != len || memcmp(kvp->value, expected, len)) {
		len = kvp->value_length;
		if (len >= sizeof(val)) {
			len = sizeof(val) - 1;
		}
		memcpy(val, kvp->value, len);
		val[len] = '\0';
		printcli("error: value \"%s\" expected \"%s\"", val, expected);
		return -1;
	}
	return 0;
}

static void ame_test_validate(size_t in_length, size_t out_length,
    size_t encoded_length)
{
	int i;

	if (out_length != in_length) {
		printcli("ERROR: length mismatch input %d, output %d\n",
		    in_length, out_length);
		ame_test_dump_buffers(in_length, encoded_length);
		return;
	}
	for (i = 0; i < in_length; ++i) {
		if (ame_test_output[i] != ame_test_input[i]) {
			printcli("ERROR: test_input[%d] = %02x,"
			    " test_output[%d] = %02x\n",
			    i, ame_test_input[i], i, ame_test_output[i]);
			ame_test_dump_buffers(in_length, encoded_length);
			return;
		}
	}
}

static void ame_test_string(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	int i;
	u8 j = 0x7f;
	size_t in_length = 128;
	size_t out_length = sizeof(ame_test_output);
	struct ame_kvp *kvp1;

	printcli("string\n");
	for (i = 0; i < in_length; ++i) {
		ame_test_input[i] = j--;
	}
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	enc->enc_utf8(es, 0, NULL, (char *)ame_test_input);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d\n", err);
		return;
	}
	err = dec->get_utf8(kvp1, NULL, (char *)ame_test_output,
	    &out_length);
	if (err) {
		printcli("error getting value %d\n", err);
		return;
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_test_validate(in_length-1, out_length, es->offset);
}

static void ame_test_opaque(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	int i;
	size_t in_length = sizeof(ame_test_input);
	size_t out_length = sizeof(ame_test_output);
	struct ame_kvp *kvp1;

	printcli("opaque\n");
	for (i = 0; i < in_length; ++i) {
		ame_test_input[i] = i;
	}
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	enc->enc_opaque(es, 0, NULL, ame_test_input, in_length);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d\n", err);
		return;
	}
	err = dec->get_opaque(kvp1, NULL, ame_test_output,
	    &out_length);
	if (err) {
		printcli("error getting value %d\n", err);
		return;
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_test_validate(in_length, out_length, es->offset);
}

static void ame_test_s64(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	s64 *in = (s64 *)ame_test_input;
	s64 *out = (s64 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("s64\n");
	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_s64(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_s64(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(8, 8, es->offset);
		if (!*in) {
			*in = 1;
		} else if (*in > 0) {
			*in = -*in;
		} else {
			*in = -*in << 1;
		}
	} while (*in);
}

static void ame_test_s32(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	s32 *in = (s32 *)ame_test_input;
	s32 *out = (s32 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("s32\n");
	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_s32(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_s32(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(4, 4, es->offset);
		if (!*in) {
			*in = 1;
		} else if (*in > 0) {
			*in = -*in;
		} else {
			*in = -*in << 1;
		}
	} while (*in);
}

static void ame_test_s16(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	s16 *in = (s16 *)ame_test_input;
	s16 *out = (s16 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("s16\n");
	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_s16(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_s16(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(2, 2, es->offset);
		if (!*in) {
			*in = 1;
		} else if (*in > 0) {
			*in = -*in;
		} else {
			*in = -*in << 1;
		}
	} while (*in);
}

static void ame_test_s8(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	s8 *in = (s8 *)ame_test_input;
	s8 *out = (s8 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("s8\n");
	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_s8(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_s8(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(1, 1, es->offset);
		if (!*in) {
			*in = 1;
		} else if (*in > 0) {
			*in = -*in;
		} else {
			*in = -*in << 1;
		}
	} while (*in);
}

static void ame_test_u64(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	u64 *in = (u64 *)ame_test_input;
	u64 *out = (u64 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("u64\n");
	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_u64(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_u64(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(4, 4, es->offset);
		if (!*in) {
			*in = 1;
		} else {
			*in <<= 1;
		}
	} while (*in);
}

static void ame_test_u32(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	u32 *in = (u32 *)ame_test_input;
	u32 *out = (u32 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("u32\n");
	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_u32(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_u32(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(4, 4, es->offset);
		if (!*in) {
			*in = 1;
		} else {
			*in <<= 1;
		}
	} while (*in);
}

static void ame_test_u16(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	u16 *in = (u16 *)ame_test_input;
	u16 *out = (u16 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("u16\n");
	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_u16(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_u16(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(2, 2, es->offset);
		if (!*in) {
			*in = 1;
		} else {
			*in <<= 1;
		}
	} while (*in);
}

static void ame_test_u8(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	u8 *in = (u8 *)ame_test_input;
	u8 *out = (u8 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("u8\n");
	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_u8(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_u8(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(1, 1, es->offset);
		if (!*in) {
			*in = 1;
		} else {
			*in <<= 1;
		}
	} while (*in);
}

static void ame_test_d32(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	s32 *in = (s32 *)ame_test_input;
	s32 *out = (s32 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("d32\n");

	*in = 0;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		    sizeof(ame_test_buffer));
		enc->enc_d32(es, 0, NULL, *in, 2);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_d32(kvp1, NULL, out, 2);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(4, 4, es->offset);
		if (!*in) {
			*in = 1;
		} else if (*in > 0) {
			*in = -*in;
		} else {
			*in = -*in << 1;
		}
	} while (*in);
}

static void ame_test_boolean(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	s32 *in = (s32 *)ame_test_input;
	s32 *out = (s32 *)ame_test_output;
	struct ame_kvp *kvp1;

	printcli("boolean\n");
	*in = 1;
	do {
		ame_encoder_buffer_set(es, ame_test_buffer,
		   sizeof(ame_test_buffer));
		enc->enc_boolean(es, 0, NULL, *in);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		ame_decoder_stack_set(ds, ame_test_stack,
		    ARRAY_LEN(ame_test_stack));
		ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
		err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
		if (err) {
			printcli("parse error %d\n", err);
			return;
		}
		err = dec->get_boolean(kvp1, NULL, out);
		if (err) {
			printcli("error getting value %d\n", err);
			return;
		}
		ame_test_dump(ame_test_buffer, es->offset);
		ame_test_validate(4, 4, es->offset);
		if (!*in) {
			break;
		}
		*in = 0;
	} while (1);
}

static void ame_test_null(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	size_t length;
	struct ame_kvp *kvp1;

	printcli("null\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	enc->enc_null(es, 0, NULL);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d\n", err);
		return;
	}
	length = sizeof(ame_test_output);
	*ame_test_output = 0xff;
	err = dec->get_utf8(kvp1, NULL, (char *)ame_test_output, &length);
	if (err) {
		printcli("error getting string value %d\n", err);
		return;
	}
	if (*ame_test_output != 0 || length != 0) {
		printcli("non-zero length string\n");
	}
	length = sizeof(ame_test_output);
	err = dec->get_opaque(kvp1, NULL, ame_test_output,
	    &length);
	if (err) {
		printcli("error getting opaque value %d", err);
		return;
	}
	if (length != 0) {
		printcli("non-zero length opaque\n");
	}
}

void ame_test_kvp_print(struct ame_kvp *kvp, unsigned int depth)
{
	char key[80];
	char val[80];
	size_t len;
	char indent[80];

	len = kvp->key_length;
	if (len >= sizeof(key)) {
		len = sizeof(key) - 1;
	}
	memcpy(key, kvp->key, len);
	key[len] = '\0';

	len = kvp->value_length;
	if (len >= sizeof(val)) {
		len = sizeof(val) - 1;
	}
	memcpy(val, kvp->value, len);
	val[len] = '\0';

	memset(indent, ' ', sizeof(indent) - 1);
	depth *= 4;
	if (depth >= sizeof(indent) - 1) {
		depth = sizeof(indent) - 1;
	}
	indent[depth] = '\0';

	printcli("%skvp %p child %p next %p type %u%s key \"%s\" val \"%s\"",
	    indent,
	    kvp, kvp->child, kvp->next, kvp->type,
	    kvp->template_node ? "" : " (no template)", key, val);
}

void ame_test_kvp_tree_print(struct ame_decoder_state *ds,
			struct ame_kvp *parent, unsigned int depth)
{
	struct ame_kvp *child;

	ame_test_kvp_print(parent, depth);
	for (child = parent->child; child; child = child->next) {
		ame_test_kvp_tree_print(ds, child, depth + 1);
	}
}

static int ame_test_kvp_used(struct ame_decoder_state *ds,
				unsigned int expected)
{
	unsigned int i;

	if (ds->kvp_used != expected) {
		printcli("incorrect kv_used count %zu expected %u",
		    ds->kvp_used, expected);
		printcli("---- kvp tree ----");
		ame_test_kvp_tree_print(ds, ds->kvp_stack, 0);
		printcli("\n");
		printcli("---- kvp stack ----");
		for (i = 0; i < ds->kvp_used; i++) {
			ame_test_kvp_print(ds->kvp_stack + i, 0);
		}
		return -1;
	}
	return 0;
}

static void ame_test_empty_object(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;

	printcli("empty object\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	enc->enc_prefix(es, EF_PREFIX_O);
	enc->enc_suffix(es, EF_SUFFIX_E);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d\n", err);
		return;
	}
	if (ame_test_kvp_used(ds, 1)) {
		return;
	}
	if (kvp1->type != AME_TYPE_OBJECT) {
		printcli("incorrect kv_type %d\n", kvp1->type);
	}

	if (ame_get_first(kvp1)) {
		printcli("child link not null\n");
	}
}

static void ame_test_empty_array(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;

	printcli("empty array\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	enc->enc_prefix(es, EF_PREFIX_A);
	enc->enc_suffix(es, EF_SUFFIX_Z);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d\n", err);
		return;
	}
	if (ame_test_kvp_used(ds, 1)) {
		return;
	}
	if (kvp1->type != AME_TYPE_ARRAY) {
		printcli("incorrect kv_type %d\n", kvp1->type);
	}

	if (ame_get_first(kvp1)) {
		printcli("child link not null\n");
	}
}

static void ame_test_simple_object(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	struct ame_kvp *kvp2;
	s32 in = 1234567890;
	s32 out;

	printcli("simple object\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	enc->enc_u32(es, EF_PREFIX_O | EF_SUFFIX_E, &k_test, in);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d\n", err);
		return;
	}
	if (ame_test_kvp_used(ds, 2)) {
		return;
	}
	if (kvp1->type != AME_TYPE_OBJECT) {
		printcli("incorrect kv_type %d\n", kvp1->type);
	}

	kvp2 = ame_get_first(kvp1);
	if (!kvp2) {
		printcli("no child\n");
		return;
	}
	if (ame_get_next(kvp2)) {
		printcli("child next not null\n");
	}
	if (kvp2->type != AME_TYPE_INTEGER) {
		printcli("child not an integer\n");
	}
	err = dec->get_s32(kvp1, &k_test, &out);
	if (err) {
		printcli("error getting value %d", err);
		return;
	}
	if (out != in) {
		printcli("value doesn't match, in %ld, out %ld", in, out);
	}
}

static void ame_test_simple_array(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	struct ame_kvp *kvp2;
	s64 in = 0x8000000000000000;
	s64 out;

	printcli("simple array\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	enc->enc_s64(es, EF_PREFIX_A | EF_SUFFIX_Z, NULL, in);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d\n", err);
		return;
	}
	if (ame_test_kvp_used(ds, 2)) {
		return;
	}
	if (kvp1->type != AME_TYPE_ARRAY) {
		printcli("incorrect kv_type %d\n", kvp1->type);
	}

	kvp2 = ame_get_first(kvp1);
	if (!kvp2) {
		printcli("no child\n");
		return;
	}
	if (ame_get_next(kvp2)) {
		printcli("child next not null\n");
	}
	if (kvp2->type != AME_TYPE_INTEGER) {
		printcli("child not integer\n");
		return;
	}
	err = dec->get_s64(kvp2, NULL, &out);
	if (err) {
		printcli("error getting value %d", err);
		return;
	}
	if (out != in) {
		printcli("value doesn't match, in %lld, out %lld", in, out);
	}
}

static void ame_test_mixed_object(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	struct ame_kvp *kvp2;
	size_t length;
	s32 out;
	struct ame_key ame_key;
	struct ame_key *key = &ame_key;

	printcli("mixed object\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	key->tag = "0";
	enc->enc_utf8(es, EF_PREFIX_O, key, "utf8");
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	key->tag = "1";
	enc->enc_opaque(es, EF_PREFIX_C, key, "opaque", 6);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	key->tag = "2";
	enc->enc_null(es, EF_PREFIX_C, key);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	key->tag = "3";
	enc->enc_boolean(es, EF_PREFIX_C, key, 0);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	key->tag = "4";
	enc->enc_s32(es, EF_PREFIX_C, key, 32);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	key->tag = "5";
	enc->enc_key(es, EF_PREFIX_C, key);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	enc->enc_prefix(es, EF_PREFIX_O);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	enc->enc_suffix(es, EF_SUFFIX_E);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	key->tag = "6";
	enc->enc_key(es, EF_PREFIX_C, key);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	enc->enc_s8(es, EF_PREFIX_O | EF_SUFFIX_E, &k_test, -1);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	key->tag = "7";
	enc->enc_key(es, EF_PREFIX_C, key);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	enc->enc_prefix(es, EF_PREFIX_A);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	enc->enc_suffix(es, EF_SUFFIX_Z);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	key->tag = "8";
	enc->enc_key(es, EF_PREFIX_C, key);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	enc->enc_s16(es, EF_PREFIX_A | EF_SUFFIX_Z, NULL, 0xffff);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	enc->enc_suffix(es, EF_SUFFIX_E);
	if (es->err) {
		printcli("encode error %d\n", es->err);
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (ame_test_kvp_used(ds, 12)) {
		return;
	}

	if (kvp1->type != AME_TYPE_OBJECT) {
		printcli("incorrect kv_type %d\n", kvp1->type);
	}

	key->tag = "0";
	length = sizeof(ame_test_output);
	err = dec->get_utf8(kvp1, key, (char *)ame_test_output, &length);
	if (err) {
		printcli("get_utf8 err %d\n", err);
		return;
	}
	if ((length != strlen("utf8")) ||
	    strncmp("utf8", (char *)ame_test_output, length)) {
		printcli("string value mismatch %s\n", ame_test_output);
	}

	key->tag = "1";
	length = sizeof(ame_test_output);
	err = dec->get_opaque(kvp1, key, ame_test_output, &length);
	if (err) {
		printcli("get_opaque err %d\n", err);
		return;
	}
	if ((length != strlen("opaque")) ||
	    strncmp("opaque", (char *)ame_test_output, length)) {
		printcli("opaque value mismatch %s\n", ame_test_output);
	}

	key->tag = "2";
	length = sizeof(ame_test_output);
	err = dec->get_opaque(kvp1, key, ame_test_output, &length);
	if (err) {
		printcli("get null err %d\n", err);
		return;
	}
	if (length != 0) {
		printcli("null value error\n");
	}

	key->tag = "3";
	err = dec->get_boolean(kvp1, key, &out);
	if (err) {
		printcli("get null err %d\n", err);
		return;
	}
	if (out) {
		printcli("boolean value error\n");
	}

	key->tag = "4";
	err = dec->get_s32(kvp1, key, &out);
	if (err) {
		printcli("get s32 err %d\n", err);
		return;
	}
	if (out != 32) {
		printcli("s32 value error %ld\n", out);
	}

	key->tag = "5";
	err = ame_get_child(kvp1, key, &kvp2);
	if (err || !kvp2) {
		printcli("no empty object\n");
		return;
	}
	if (ame_get_first(kvp2)) {
		printcli("empty object not empty\n");
	}

	key->tag = "6";
	err = ame_get_child(kvp1, key, &kvp2);
	if (err || !kvp2) {
		printcli("no test object\n");
		return;
	}
	err = dec->get_s32(kvp2, &k_test, &out);
	if (err) {
		printcli("get test s32 err %d\n", err);
	} else if (out != -1) {
		printcli("test s32 value error %ld\n", out);
	}

	key->tag = "7";
	err = ame_get_child(kvp1, key, &kvp2);
	if (err || !kvp2) {
		printcli("no empty array\n");
		return;
	}
	if (ame_get_first(kvp2)) {
		printcli("empty array not empty\n");
	}

	key->tag = "8";
	err = ame_get_child(kvp1, key, &kvp2);
	if (err || !kvp2) {
		printcli("no single element array\n");
		return;
	}
	kvp2 = ame_get_first(kvp2);
	if (!kvp2) {
		printcli("single element array is empty\n");
	} else if (ame_get_next(kvp2)) {
		printcli("single element array has too many elements\n");
	} else {
		err = dec->get_s32(kvp2, NULL, &out);
		if (err) {
			printcli("array get test s32 err %d\n", err);
		} else if (out != -1) {
			printcli("test s32 value error %ld\n", out);
		}
	}
	if (ame_get_next(kvp2)) {
		printcli("extra elements in array\n");
	}
}

static void ame_test_mixed_array(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	struct ame_kvp *kvp2;
	size_t length;
	s32 out;

	printcli("mixed array\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	enc->enc_utf8(es, EF_PREFIX_A, NULL, "utf8");
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_opaque(es, EF_PREFIX_C, NULL, "opaque", 6);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_null(es, EF_PREFIX_C, NULL);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_boolean(es, EF_PREFIX_C, NULL, 1);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_s32(es, EF_PREFIX_C, NULL, 32);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_prefix(es, EF_PREFIX_CO);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_suffix(es, EF_SUFFIX_E);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_s8(es, EF_PREFIX_CO | EF_SUFFIX_E, &k_test, -1);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_prefix(es, EF_PREFIX_CA);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_suffix(es, EF_SUFFIX_Z);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_s16(es, EF_PREFIX_CA | EF_SUFFIX_Z, NULL, 0xffff);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	enc->enc_suffix(es, EF_SUFFIX_Z);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}
	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (ame_test_kvp_used(ds, 12)) {
		return;
	}
	kvp1 = ame_test_stack;
	if (kvp1->type != AME_TYPE_ARRAY) {
		printcli("incorrect kv_type %d\n", kvp1->type);
	}

	kvp2 = ame_get_first(kvp1);
	if (!kvp2) {
		printcli("no utf8\n");
		return;
	}
	length = sizeof(ame_test_output);
	err = dec->get_utf8(kvp2, NULL, (char *)ame_test_output, &length);
	if (err) {
		printcli("get_utf8 err %d\n", err);
		return;
	}
	if ((length != strlen("utf8")) ||
	    strncmp("utf8", (char *)ame_test_output, length)) {
		printcli("string value mismatch %s\n", ame_test_output);
	}

	kvp2 = ame_get_next(kvp2);
	if (!kvp2) {
		printcli("no opaque\n");
		return;
	}
	length = sizeof(ame_test_output);
	err = dec->get_opaque(kvp2, NULL, ame_test_output, &length);
	if (err) {
		printcli("get_opaque err %d\n", err);
		return;
	}
	if ((length != strlen("opaque")) ||
	    strncmp("opaque", (char *)ame_test_output, length)) {
		printcli("opaque value mismatch %s\n", ame_test_output);
	}

	kvp2 = ame_get_next(kvp2);
	if (!kvp2) {
		printcli("no null\n");
		return;
	}
	length = sizeof(ame_test_output);
	err = dec->get_opaque(kvp2, NULL, ame_test_output, &length);
	if (err) {
		printcli("get null err %d\n", err);
		return;
	}
	if (length != 0) {
		printcli("null value error\n");
	}

	kvp2 = ame_get_next(kvp2);
	if (!kvp2) {
		printcli("no boolean\n");
		return;
	}
	err = dec->get_boolean(kvp2, NULL, &out);
	if (err) {
		printcli("get boolean err %d\n", err);
		return;
	}
	if (!out) {
		printcli("boolean value error\n");
	}

	kvp2 = ame_get_next(kvp2);
	if (!kvp2) {
		printcli("no s32\n");
		return;
	}
	err = dec->get_s32(kvp2, NULL, &out);
	if (err) {
		printcli("get s32 err %d\n", err);
		return;
	}
	if (out != 32) {
		printcli("s32 value error %ld\n", out);
	}

	kvp2 = ame_get_next(kvp2);
	if (!kvp2) {
		printcli("no empty object\n");
		return;
	}
	if (ame_get_first(kvp2)) {
		printcli("empty object not empty\n");
	}

	kvp1 = ame_get_next(kvp2);
	if (!kvp2) {
		printcli("no test object\n");
		return;
	}
	kvp2 = ame_get_first(kvp1);
	if (!kvp2) {
		printcli("test object is empty\n");
	} else if (ame_get_next(kvp2)) {
		printcli("test object has too many children\n");
	}
	err = dec->get_s32(kvp1, &k_test, &out);
	if (err) {
		printcli("get test s32 err %d\n", err);
	} else if (out != -1) {
		printcli("test s32 value error %ld\n", out);
	}

	kvp1 = ame_get_next(kvp1);
	if (!kvp1) {
		printcli("no empty array\n");
		return;
	}
	if (ame_get_first(kvp1)) {
		printcli("empty array not empty\n");
	}

	kvp1 = ame_get_next(kvp1);
	if (!kvp1) {
		printcli("no single element array\n");
		return;
	}
	kvp2 = ame_get_first(kvp1);
	if (!kvp2) {
		printcli("single element array is empty\n");
	} else if (ame_get_next(kvp2)) {
		printcli("single element array has too many elements\n");
	} else {
		err = dec->get_s32(kvp2, NULL, &out);
		if (err) {
			printcli("array get test s32 err %d\n", err);
		} else if (out != -1) {
			printcli("test s32 value error %ld\n", out);
		}
	}
	if (ame_get_next(kvp1)) {
		printcli("extra elements in array\n");
	}
}

static void ame_test_nested_object(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	s32 out;
	struct ame_key ame_key;
	struct ame_key *key = &ame_key;
	char tag[5];
	int i;
	int depth = 5;

	printcli("nested object\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	key->tag = tag;
	for (i = 0; i < depth; ++i) {
		snprintf(tag, sizeof(tag), "%d", i);
		enc->enc_key(es, EF_PREFIX_O, key);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
	}

	enc->enc_s32(es, EF_PREFIX_O | EF_SUFFIX_E, &k_test, 123);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}

	for (i = 0; i < depth; ++i) {
		enc->enc_suffix(es, EF_SUFFIX_E);
		if (es->err) {
			printcli("encode error %d\n", es->err);
			return;
		}
	}

	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (ame_test_kvp_used(ds, 7)) {
		return;
	}

	if (kvp1->type != AME_TYPE_OBJECT) {
		printcli("incorrect kv_type %d\n", kvp1->type);
		return;
	}

	for (i = 0; i < depth; ++i) {
		snprintf(tag, sizeof(tag), "%d", i);
		err = ame_get_child(kvp1, key, &kvp1);
		if (err) {
			printcli("get nested object child %d error %d\n",
			    i, es->err);
			return;
		}
		if (!kvp1) {
			printcli("get nested object child %d not found\n", i);
			return;
		}
	}

	err = dec->get_s32(kvp1, &k_test, &out);
	if (err) {
		printcli("get_s32 nested object error %d\n", es->err);
	}
	if (out != 123) {
		printcli("nested object out value error %ld\n", out);
	}
}

static void ame_test_nested_array(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	struct ame_kvp *kvp2;
	s32 out;
	int i;
	int depth = 5;
	u32 flags = EF_PREFIX_A;

	printcli("nested array\n");
	ame_encoder_buffer_set(es, ame_test_buffer, sizeof(ame_test_buffer));
	for (i = 0; i < depth; ++i) {
		enc->enc_prefix(es, flags);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		enc->enc_s32(es, 0, NULL, i);
		if (es->err) {
			printcli("encode error %d\n", es->err);
		}
		flags |= EF_PREFIX_C;
	}

	enc->enc_s32(es, EF_PREFIX_C, NULL, 456);
	if (es->err) {
		printcli("encode error %d\n", es->err);
		return;
	}

	for (i = 0; i < depth; ++i) {
		enc->enc_suffix(es, EF_SUFFIX_Z);
		if (es->err) {
			printcli("encode error %d\n", es->err);
			return;
		}
	}

	ame_test_dump(ame_test_buffer, es->offset);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, ame_test_buffer, es->offset);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (ame_test_kvp_used(ds, 11)) {
		return;
	}

	if (kvp1->type != AME_TYPE_ARRAY) {
		printcli("incorrect kv_type %d\n", kvp1->type);
		return;
	}

	for (i = 0; i < depth; ++i) {
		kvp2 = ame_get_first(kvp1);
		if (!kvp2) {
			printcli("get nested array first not found, %d\n", i);
			return;
		}
		err = dec->get_s32(kvp2, NULL, &out);
		if (err || out != i) {
			printcli("get_s32 error %d, expecting %d\n", err, i);
			return;
		}
		kvp1 = ame_get_next(kvp2);
		if (!kvp1) {
			printcli("get nested array next not found, %d\n", i);
			return;
		}
	}

	err = dec->get_s32(kvp1, NULL, &out);
	if (err) {
		printcli("get_s32 nested array error %d\n", es->err);
	}
	if (out != 456) {
		printcli("nested array out value error %ld\n", out);
	}
}

static void ame_test_whitespace(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	size_t length;
	u8 *string =
	    (u8 *)" \t\n\r{ \t\n\r\"test0\" \t\n\r:"
	    " \t\n\r\"w h\\ti\\nt\\re\\\"space\" \t\n\r} \t\n\r";
	char *expected = "w h\ti\nt\re\"space";

	printcli("whitespace\n");

	length = strlen((char *)string);
	ame_test_dump(string, length);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, string, length);
	err = dec->parse(ds, AME_PARSE_FULL, &kvp1);
	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (ame_test_kvp_used(ds, 2)) {
		return;
	}

	if (kvp1->type != AME_TYPE_OBJECT) {
		printcli("incorrect kv_type %d\n", kvp1->type);
		return;
	}

	length = sizeof(ame_test_output);
	err = dec->get_utf8(kvp1, &k_test, (char *)ame_test_output,
	    &length);
	if (err) {
		printcli("error fetching whitespace string %d\n", err);
		ame_test_dump(ame_test_output, sizeof(ame_test_output));
		return;
	}
	if (length != strlen(expected) || strcmp(expected,
	    (char *)ame_test_output)) {
		printcli("whitespace string mismatch\n");
		ame_test_dump(ame_test_output, sizeof(ame_test_output));
	}
}


static void ame_test_incr_empty_object(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	size_t length;
	u8 *string = (u8 *)"{}";
	printcli("incremental empty object\n");

	length = strlen((char *)string);
	ame_test_dump(string, length);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, string, length);
	err = dec->parse(ds, AME_PARSE_FIRST, &kvp1);
	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (kvp1) {
		printcli("object not empty\n");
	}
}

static void ame_test_incr_empty_array(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	size_t length;
	u8 *string = (u8 *)"[]";
	printcli("incremental empty array\n");

	length = strlen((char *)string);
	ame_test_dump(string, length);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, string, length);
	err = dec->parse(ds, AME_PARSE_FIRST, &kvp1);
	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (kvp1) {
		printcli("array not empty\n");
	}
}

static void ame_test_incr_array(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	size_t length;
	u8 *string = (u8 *)"[1,2,3,4,5]";
	u16 out = 0;
	u16 expected = 1;

	printcli("incremental array\n");

	length = strlen((char *)string);
	ame_test_dump(string, length);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, string, length);
	err = dec->parse(ds, AME_PARSE_FIRST, &kvp1);
	while (!err && kvp1) {
		if (kvp1->type != AME_TYPE_INTEGER) {
			printcli("incorrect kv_type %d\n", kvp1->type);
			return;
		}
		err = dec->get_u16(kvp1, NULL, &out);
		if (err) {
			printcli("error getting value, err %d\n", err);
			return;
		}
		printcli("out = %d", out);
		if (out != expected++) {
			printcli("value error\n");
			return;
		}
		err = dec->parse(ds, AME_PARSE_NEXT, &kvp1);
	}

	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (out != 5) {
		printcli("loop terminated early\n");
		return;
	}
}

static void ame_test_incr_object(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	size_t length;
	u8 *string = (u8 *)"{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}";
	u16 out = 0;
	u16 expected = 1;

	printcli("incremental object\n");

	length = strlen((char *)string);
	ame_test_dump(string, length);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, string, length);
	err = dec->parse(ds, AME_PARSE_FIRST, &kvp1);
	while (!err && kvp1) {
		if (kvp1->type != AME_TYPE_INTEGER) {
			printcli("incorrect kv_type %d\n", kvp1->type);
			return;
		}
		err = dec->get_u16(kvp1, NULL, &out);
		if (err) {
			printcli("error getting value\n, err %d\n", err);
			return;
		}
		printcli("out = %d", out);
		if (out != expected++) {
			printcli("value error\n");
			return;
		}
		err = dec->parse(ds, AME_PARSE_NEXT, &kvp1);
	}

	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}
	if (out != 5) {
		printcli("loop terminated early\n");
		return;
	}
}

static void ame_test_incr_misc1(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	size_t length;
	u8 *string = (u8 *)"[ \"abc\" ,1, true,false ,{\"a\":2}, [4,5,6]]";
	int i = 0;
	char *expected[] = {"abc", "1", "true", "false", "{\"a\":2}",
	    "[4,5,6]" };

	printcli("incremental misc1\n");

	length = strlen((char *)string);
	ame_test_dump(string, length);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	ame_decoder_buffer_set(ds, string, length);
	err = dec->parse(ds, AME_PARSE_FIRST, &kvp1);
	while (!err && kvp1) {

		if (i >= ARRAY_LEN(expected)) {
			printcli("unexpected element\n");
			return;
		}
		if (ame_test_kvp_value_match(kvp1, expected[i])) {
			return;
		}
		err = dec->parse(ds, AME_PARSE_NEXT, &kvp1);
		i++;
	}

	if (err) {
		printcli("parse error %d, offset %d\n", err, ds->error_offset);
		return;
	}

	if (i < ARRAY_LEN(expected)) {
		printcli("missing element\n");
	}
}

/* similar test to above but feeding data to decoder slowly */
static void ame_test_incr_misc2(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct ame_decoder *dec,
    struct ame_decoder_state *ds)
{
	enum ada_err err;
	struct ame_kvp *kvp1;
	size_t length;
	u8 *string = (u8 *)"[ \"abc\" ,1, true,false ,{\"a\":23}, [4 ,56,7]]";
	u8 buffer[50];
	size_t start = 0;
	size_t end = 0;
	size_t count;
	int i = 0;
	char *expected[] = {"abc", "1", "true", "false", "{\"a\":23}",
	    "[4 ,56,7]" };
	enum ame_parse_op op = AME_PARSE_FIRST;

	printcli("incremental misc2\n");

	length = strlen((char *)string);
	ame_test_dump(string, length);
	ame_decoder_stack_set(ds, ame_test_stack, ARRAY_LEN(ame_test_stack));
	while (start < length) {
		count = end - start;
		memcpy(buffer, &string[start], count);
		ame_decoder_buffer_set(ds, buffer, count);
		err = dec->parse(ds, op, &kvp1);
		if (err == AE_UNEXP_END && end < length) {
			end++;
			continue;
		} else if (err) {
			printcli("parse error %d, offset %d\n", err, start +
			    ds->error_offset);
			return;
		}
		start += ds->offset;
		if (start > length) {
			printcli("decoded past length, start %d, end %d\n",
			    start, end);
			return;
		}
		if (start > end + 1) {
			printcli("decoded past end, start %d, end %d\n",
			    start, end);
			return;
		}
		end = start;
		if (!kvp1) {
			/* done */
			break;
		}
		if (i >= ARRAY_LEN(expected)) {
			printcli("unexpected element\n");
			return;
		}
		if (ame_test_kvp_value_match(kvp1, expected[i])) {
			return;
		}
		op = AME_PARSE_NEXT;
		i++;
	}
	if (i < 5) {
		printcli("missing element\n");
	}
}

void ame_test(const struct ame_encoder *enc, struct ame_encoder_state *es,
    const struct ame_decoder *dec, struct ame_decoder_state *ds)
{
	ame_test_string(enc, es, dec, ds);
	ame_test_opaque(enc, es, dec, ds);
	ame_test_s64(enc, es, dec, ds);
	ame_test_s32(enc, es, dec, ds);
	ame_test_s16(enc, es, dec, ds);
	ame_test_s8(enc, es, dec, ds);
	ame_test_u64(enc, es, dec, ds);
	ame_test_u32(enc, es, dec, ds);
	ame_test_u16(enc, es, dec, ds);
	ame_test_u8(enc, es, dec, ds);
	ame_test_d32(enc, es, dec, ds);
	ame_test_boolean(enc, es, dec, ds);
	ame_test_null(enc, es, dec, ds);
	ame_test_empty_object(enc, es, dec, ds);
	ame_test_empty_array(enc, es, dec, ds);
	ame_test_simple_object(enc, es, dec, ds);
	ame_test_simple_array(enc, es, dec, ds);
	ame_test_mixed_object(enc, es, dec, ds);
	ame_test_mixed_array(enc, es, dec, ds);
	ame_test_nested_object(enc, es, dec, ds);
	ame_test_nested_array(enc, es, dec, ds);
	ame_test_whitespace(enc, es, dec, ds);
	ame_test_incr_empty_object(enc, es, dec, ds);
	ame_test_incr_empty_array(enc, es, dec, ds);
	ame_test_incr_array(enc, es, dec, ds);
	ame_test_incr_object(enc, es, dec, ds);
	ame_test_incr_misc1(enc, es, dec, ds);
	ame_test_incr_misc2(enc, es, dec, ds);
}
#endif
