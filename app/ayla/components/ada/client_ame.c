/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 *  AME parser based on ame-json.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/base64.h>
#include <ayla/utf8.h>

#include "ame.h"
#include "ame_json.h"
#include "client_ame.h"

enum ada_err client_ame_init(struct ame_state *ctx,
		struct ame_kvp *stack, size_t stack_elems,
		u8 *parsed_buf, size_t buf_size,
		const struct ame_tag *list, void *cb_arg)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->enc = &ame_json_enc;
	ctx->dec = &ame_json_dec;
	ame_decoder_stack_set(&ctx->ds, stack, stack_elems);
	ame_decoder_set_ext_mode(&ctx->ds, 1, parsed_buf, buf_size);
	ame_decoder_tree_set(&ctx->ds, &ame_json_dec, list, cb_arg);
	return AE_OK;
}

enum ada_err client_ame_parse(struct ame_state *ctx,
		const u8 *data, int len)
{
	enum ada_err err;
	struct ame_decoder_state *ds = &ctx->ds;
	const struct ame_decoder *dec = ctx->dec;
	struct ame_kvp *kvp;
	enum ame_parse_op op;

	ame_decoder_buffer_set(ds, data, len);
	while (1) {
		kvp = NULL;
		op = (!ds->first_called) ? AME_PARSE_FIRST : AME_PARSE_NEXT;
		err = dec->parse(ds, op, &kvp);
		if (err) {
			break;
		}
		if (ds->offset >= ds->length) {
			break;
		}
	}
	return err;
}


#ifdef AME_ENCODER_TEST

#define DPS_RSP_(id, end)	\
	"{\"batch_id\":" #id ",\"status\":200,\"property\":{\"dsn\":null,"    \
	"\"name\":\"log\",\"base_type\":\"string\",\"value\":\"abcdef123\","  \
	"\"dev_time_ms\":\"2019-05-16T04:28:24Z\"}}" end

#define DPS_RSP(id)	DPS_RSP_(id, ",")

static const char batch_dps_ame_rsp[] =
	"{\"batch_datapoints\":["
	DPS_RSP(80)
	DPS_RSP(81)
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
	DPS_RSP_(145, "")
	"]}";


static int client_ame_accept_key_val(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	char key[256];
	char value[256];

	memcpy(key, kvp->key, kvp->key_length);
	key[kvp->key_length] = '\0';
	memcpy(value, kvp->value, kvp->value_length);
	value[kvp->value_length] = '\0';
	if (strcmp(key, "batch_id") == 0) {
		printcli("%s = %s\n", key, value);
	} else {
		printcli("\t%s = %s\n", key, value);
	}
	return AE_OK;
}

static const struct ame_tag client_ame_prop_tag[] = {
	AME_TAG("dsn", NULL, client_ame_accept_key_val),
	AME_TAG("name", NULL, client_ame_accept_key_val),
	AME_TAG("base_type", NULL, client_ame_accept_key_val),
	AME_TAG("value", NULL, client_ame_accept_key_val),
	AME_TAG("dev_time_ms", NULL, client_ame_accept_key_val),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_batch_dps_tag[] = {
	AME_TAG("batch_id", NULL, client_ame_accept_key_val),
	AME_TAG("status", NULL, client_ame_accept_key_val),
	AME_TAG("property", client_ame_prop_tag, NULL),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_batch_tag_list[] = {
	AME_TAG("batch_datapoints", client_ame_batch_dps_tag, NULL),
	AME_TAG(NULL, NULL, NULL)
};

void client_ame_test(void)
{
	enum ada_err err;
	struct ame_state ctx;
	struct ame_kvp stack[20];
	u8 buf[1024 + 512];
	void *cb_arg = NULL;
	const u8 *p;
	int round;

	for (round = 0; round < 2; round++) {
		printcli("[%d] Test client_ame_parse() start:\n\n", round);
		err = client_ame_init(&ctx, stack, ARRAY_LEN(stack),
		    buf, sizeof(buf), client_ame_batch_tag_list, cb_arg);
		if (err != AE_OK) {
			printcli("client_ame_init() fail.\n");
			return;
		}
		p = (const u8 *)batch_dps_ame_rsp;
		while (*p != '\0') {
			client_ame_parse(&ctx, p++, 1);
		}
		printcli("Test client_ame_parse() stop.\n\n");
	}
}

#endif

