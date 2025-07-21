/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

/*
 * Ayla schedule configuration.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ada/libada.h>
#include <ada/sched.h>
#include <ayla/nameval.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <ayla/tlv_access.h>
#include "conf.h"
#include "demo.h"

static u32 sched_saved_run_time;	/* should be in nvram with magic no. */
static u32 sched_test_time;

void sched_conf_load(void)
{
	enum ada_err err;

	/*
	 * Create schedules.
	 */
	err = ada_sched_dynamic_init(DEMO_SCHED_COUNT);
	AYLA_ASSERT(!err);
}

void adap_sched_run_time_write(u32 run_time)
{
	sched_saved_run_time = run_time;
}

u32 adap_sched_run_time_read(void)
{
	return sched_saved_run_time;
}

/*
 * Display an action.
 */
static void demo_sched_action_show(const struct ayla_tlv *atlv)
{
	char time_buf[30];
	const struct ayla_tlv *prop;
	const struct ayla_tlv *val_tlv;
	enum ayla_tlv_type type;
	char name[PROP_NAME_LEN];
	s32 val;
	size_t rlen;

	rlen = atlv->len;
	if (rlen < sizeof(*atlv) * 2) {
		printcli("sched test: malformed setprop TLV");
		return;
	}
	rlen -= sizeof(*atlv) * 2;

	prop = atlv + 1;
	if (prop->type != ATLV_NAME) {
		printcli("sched test: missing name");
		return;
	}
	if (prop->len > rlen || prop->len >= sizeof(name)) {
		printcli("sched_test: malformed setprop name len %u rlen %zu",
		    prop->len, rlen);
		return;
	}
	rlen -= prop->len;
	memcpy(name, prop + 1, prop->len);
	name[prop->len] = '\0';

	val_tlv = (struct ayla_tlv *)((u8 *)prop + prop->len +
	    sizeof(struct ayla_tlv));
	type = val_tlv->type;
	if (type != ATLV_INT && type != ATLV_BOOL) {
		printcli("sched_test: unsupported setprop type");
		return;
	}
	if (tlv_s32_get(&val, val_tlv)) {
		printcli("sched_test invalid setprop val");
		return;
	}
	clock_fmt(time_buf, sizeof(time_buf), sched_test_time);
	printcli("%s action set %s = %ld\n", time_buf, name, val);
}

static void demo_sched_test(char *name, char *count_arg, char *time_arg)
{
	unsigned long count;
	u32 time;
	char *errptr;
	char *sched_name;
	u8 tlvs[SCHED_TLV_LEN];
	size_t len;
	enum ada_err err;
	unsigned int i;

	count = strtoul(count_arg, &errptr, 10);
	if (errptr == count_arg || *errptr || count == 0) {
		printcli("invalid count");
		return;
	}

	if (time_arg) {
		time = (u32)strtoul(time_arg, &errptr, 10);
		if (errptr == time_arg || *errptr) {
			printcli("invalid time");
			return;
		}
	} else {
		time = clock_utc();
	}

	for (i = 0; i < DEMO_SCHED_COUNT; i++) {
		len = sizeof(tlvs);
		sched_name = NULL;
		err = ada_sched_get_index(i, &sched_name, tlvs, &len);
		if (err) {
			if (err == AE_NOT_FOUND) {
				continue;
			}
			printcli("sched_get_index failed");
			return;
		}
		if (!sched_name || strcmp(sched_name, name)) {
			continue;
		}
		if (!len) {
			printcli("schedule not set");
			return;
		}

		/*
		 * Run schedules from the specified time.
		 */
		while (count-- > 0) {
			sched_test_time = time;
			err = ada_sched_eval((struct ayla_tlv *)tlvs,
			    len, &time, demo_sched_action_show);
			if (err) {
				printcli("sched error %d", err);
				break;
			}
			if (!time || time == MAX_U32) {
				printcli("sched_eval: no more events");
				return;
			}
		}
		if (!err) {
			printcli("next time %lu", time);
		}
		return;
	}
	printcli("schedule not configured");
}

static const char demo_sched_cli_help[] =
    "sched test <sched-name> <action-count> [<time>]";

/*
 * Test a schedule by showing the next N actions to be run and when they
 * will occur.
 */
static wise_err_t demo_sched_cli(int argc, char **argv)
{
	if ((argc == 4 || argc == 5) && !strcmp(argv[1], "test")) {
		demo_sched_test(argv[2], argv[3], argc == 5 ? argv[4] : NULL);
		return 0;
	}
	printcli("usage: %s", demo_sched_cli_help);
	return 0;
}

	ayla_cmd_def(sched_command) = {
		.command = "sched",
		.help = demo_sched_cli_help,
		.func = demo_sched_cli,
	};

