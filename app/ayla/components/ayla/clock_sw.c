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
#include <al/al_clock.h>
#include <ayla/clock.h>

u32 clock_set_mtime;

u64 clock_total_ms(void)
{
	return al_clock_get_total_ms();
}

u32 clock_ms_to_utc(u64 msecs)
{
	s64 utc = al_clock_get_utc_ms(NULL);
	u64 now = al_clock_get_total_ms();
	u64 boot_time_ms = utc - now;

	return (u32)((boot_time_ms + msecs) / 1000);
}

u32 clock_ms(void)
{
	return (u32)al_clock_get_total_ms();
}

/*
 * Get clock using software time since boot.
 * For compatiblity.  Host application should be using platform SDK directly.
 */
u32 clock_get(struct clock_time *ct_out)
{
	struct clock_time ct;
	u64 msec;

	msec = al_clock_get_utc_ms(NULL);

	ct.ct_sec = (u32)(msec / 1000);
	ct.ct_usec = (u32)((msec % 1000) * 1000);
	if (ct_out) {
		*ct_out = ct;
	}
	return ct.ct_sec;
}

/*
 * Set the clock to a new time.
 * Returns -1 if the src given is a lower priority than the current
 * source. Returns 0 on success
 */
int clock_set(u32 new_time, enum clock_src src)
{
	int rc;

	rc = al_clock_set(new_time, (enum al_clock_src)src);
	if (rc) {
		return rc;
	}
	clock_set_mtime = (u32)al_clock_get_total_ms();
	return 0;
}
