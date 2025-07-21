/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <sys/time.h>

#include <al/al_clock.h>
#include <al/al_os_lock.h>
#include <ayla/assert.h>
#include <ayla/clock.h>
#include <platform/pfm_clock.h>

#include <cmsis_os.h>

static s64 clock_at_boot;	/* ms, clock value since boot */
static struct al_lock *gp_clock_mutex;

/*
 * Memory that should be maintained across a reset if power is maintained.
 * Use this to save the clock source to avoid waiting for SNTP.
 * The check provides for the possibility that the area is garbled.
 * The value must also be one of the legal ones.
 */
#define __NOINIT_ATTR  __attribute__((section(".noinit")))
static enum al_clock_src gv_clock_src __NOINIT_ATTR;	/* clock source */
static enum al_clock_src gv_clock_src_check __NOINIT_ATTR; /* inverse */

static s64 hw_clock_get(void)
{
	static u64 last_ms;
	uint32_t ticks;
	u64 ms;

	if (osKernelGetTickFreq() == 1000) {	/* compile-time evaluated */
		ticks = osKernelGetTickCount();
	} else {
		ticks = ((osKernelGetTickCount() * 1000) / osKernelGetTickFreq());
	}
	ms = (u64)ticks;

	/*
	 * If the FreeRTOS tick count is only 32 bits, handle wrap-around.
	 */
	if (sizeof(ticks) == sizeof(u32)) {	/* compile-time evaluated */
		ms |= last_ms & 0xffffffff00000000ULL;
		if (ms < last_ms) {
			ms += 1ULL << 32;
		}
		last_ms = ms;
	}
	return ms;
}

static int pfm_clock_source_valid(enum al_clock_src clk_src)
{
	switch (clk_src) {
	case AL_CS_MIN:
	case AL_CS_DEF:
	case AL_CS_PERSIST:
	case AL_CS_HTTP:
	case AL_CS_LOCAL:
	case AL_CS_MCU_LO:
	case AL_CS_SNTP:
	case AL_CS_NTP:
	case AL_CS_MCU_HI:
		return 1;
	default:
		break;
	}
	return 0;
}

void pfm_clock_source_set(enum al_clock_src clk_src)
{
	gv_clock_src = clk_src;
	gv_clock_src_check = ~clk_src;
}

void pfm_clock_init(void)
{
	al_clock_init();
}

void al_clock_init(void)
{
	u32 utc_sec = CLOCK_START;
	struct timeval tm;

	gp_clock_mutex = al_os_lock_create();
	al_os_lock_lock(gp_clock_mutex);
	if (!gettimeofday(&tm, NULL)) {
		utc_sec = tm.tv_sec;
	}
	if (clock_gt(utc_sec, CLOCK_START)) {
		if (gv_clock_src != (enum al_clock_src)~gv_clock_src_check ||
		    !pfm_clock_source_valid(gv_clock_src)) {
			pfm_clock_source_set(AL_CS_LOCAL);
		}
	} else {
		pfm_clock_source_set(AL_CS_DEF);
	}
	clock_at_boot = hw_clock_get();
	al_os_lock_unlock(gp_clock_mutex);
}

int al_clock_set(u32 utc_sec, enum al_clock_src clk_src)
{
	struct timeval tm;

	if (clk_src < gv_clock_src || clk_src >= AL_CS_LIMIT) {
		return -1;
	}
	ASSERT(gp_clock_mutex);
	al_os_lock_lock(gp_clock_mutex);
	tm.tv_sec = utc_sec;
	tm.tv_usec = 500000;
	settimeofday(&tm, NULL);
	pfm_clock_source_set(clk_src);
	al_os_lock_unlock(gp_clock_mutex);
	return 0;
}

void al_clock_reset_src(void)
{
	pfm_clock_source_set(AL_CS_NONE);
}

u32 al_clock_get(enum al_clock_src *clk_src)
{
	/* Get seconds since 1970.1.1 00:00 UTC */
	return (u32)(al_clock_get_utc_ms(clk_src) / 1000);
}

s64 al_clock_get_utc_ms(enum al_clock_src *clk_src)
{
	/* Get milliseconds since 1970.1.1 00:00 UTC */
	s64 utc_ms;
	struct timeval tv;

	ASSERT(gp_clock_mutex);
	al_os_lock_lock(gp_clock_mutex);
	gettimeofday(&tv, NULL);
	if (tv.tv_sec > CLOCK_START && gv_clock_src == AL_CS_DEF) {
		pfm_clock_source_set(AL_CS_PERSIST);
	}
	utc_ms = (s64)tv.tv_sec * 1000 + tv.tv_usec / 1000;

	if (clk_src) {
		*clk_src = gv_clock_src;
	}
	al_os_lock_unlock(gp_clock_mutex);
	return (s64)utc_ms;
}

u64 al_clock_get_total_ms(void)
{
	/* Get milliseconds since boot */
	s64 ms;

	ms = hw_clock_get() - clock_at_boot;
	return ms;
}
