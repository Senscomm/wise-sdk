/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/clock.h>

static void time_cli_show(void)
{
	char buf[24];
	u32 utc_time = clock_utc();
	s32 abs_mins = timezone_info.mins;

	clock_fmt(buf, sizeof(buf), utc_time);
	printcli("UTC Time:   %s, Time Since Boot: %llu ms", buf,
	    clock_total_ms());
	clock_fmt(buf, sizeof(buf), clock_local(&utc_time));

	if (timezone_info.valid) {
		/*
		 * Note: Device definition for offset is minutes *west* of UTC.
		 * So the sign is backwards from normal usage.
		 * This means a PST (UTC-8) uses mins = +480.
		 */
		if (timezone_info.mins < 0) {
			abs_mins *= -1;
		}
		printcli("Local Time: %s, Timezone: UTC%c%2.2ld:%2.2ld",
		    buf, timezone_info.mins <= 0 ? '+' : '-',
		    abs_mins / 60, abs_mins % 60);
		if (daylight_info.valid) {
			clock_fmt(buf, sizeof(buf), daylight_info.change);
			printcli("DST Active: %u until %s UTC",
			    daylight_info.active, buf);
		}
	}
}

static void time_cli_set(const char *val)
{
	char buf[24];
	u32 old_time;
	u32 new_time;

	old_time = clock_utc();
	new_time = clock_parse(val);
	if (!new_time) {
		printcli("time set: invalid time");
		return;
	}

	/*
	 * Set the clock with the current clock source.
	 */
	if (clock_set(new_time, clock_source())) {
		printcli("time set: clock not set");
	}
	log_put(LOG_INFO "clock set by CLI");
	clock_fmt(buf, sizeof(buf), old_time);
	log_put(LOG_INFO "clock was %s UTC", buf);
	clock_fmt(buf, sizeof(buf), new_time);
	log_put(LOG_INFO "clock now %s UTC", buf);
}

const char ayla_time_cli_help[] = "time [set YYYY-MM-DDThh:mm:ss]";

void ayla_time_cli(int argc, char **argv)
{
	if (argc == 1) {
		time_cli_show();
		return;
	}
	if (argc == 3 && !strcmp(argv[1], "set")) {
		time_cli_set(argv[2]);
		return;
	}
	printcli("usage: %s", ayla_time_cli_help);
}
