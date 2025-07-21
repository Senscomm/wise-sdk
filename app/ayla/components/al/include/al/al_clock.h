/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_CLOCK_H__
#define __AYLA_AL_COMMON_CLOCK_H__

#include <al/al_utypes.h>

/**
 * @file
 * Platform clock Interfaces
 */

/**
 * Time source codes.
 * Larger numbers indicate more reliable clock sources.
 * Do not change the existing numbers, for upgrade compatibility.
 */
enum al_clock_src {
	AL_CS_NONE = 0,		/**< never been set. */
	AL_CS_MIN = 0x1120,	/**< The min source. */
	AL_CS_DEF = 0x1130,	/**< Set to CLOCK_START. */
	AL_CS_PERSIST = 0x1135,	/**< From persisted data store(flash/nvram) */
	AL_CS_HTTP = 0x113c,	/**< From cloud via HTTP request. */
	AL_CS_LOCAL = 0x1140,	/**< Set by internal web server. */
	AL_CS_MCU_LO = 0x1250,	/**< Set by MCU. low priority. */
	AL_CS_SERVER = 0x1260,	/**< Set using server time. */
	AL_CS_SNTP = AL_CS_SERVER, /**< Set using SNTP, not kept in sync. */
	AL_CS_NTP = 0x1270,	/**< Set using NTP. */
	AL_CS_MCU_HI = 0x1280,	/**< Set by MCU. high priority. */
	AL_CS_LIMIT		/**< Must be last. */
};

/**
 * Reset clock source
 * This function only can be used in test framework.
 */
void al_clock_reset_src(void);

/**
 * Set clock
 *
 * \param timestamp is the time as the number of seconds since 1970-01-01
 * 00:00 (UTC).
 * \param clock_src is the time source
 *
 * \returns 0 on success.
 */
int al_clock_set(u32 timestamp, enum al_clock_src clock_src);

/**
 * Get clock
 *
 * \param clock_src is the buffer to retrieve the clock source, it can be NULL.
 *
 * \returns the time as the number of seconds since 1970-01-01 00:00 (UTC).
 */
u32 al_clock_get(enum al_clock_src *clock_src);

/**
 * Get clock (ms)
 *
 * \param clock_src is the buffer to retrieve the clock source, it can be NULL.
 *
 * \returns the time as the number of milliseconds since 1970-01-01 00:00 (UTC).
 */
s64 al_clock_get_utc_ms(enum al_clock_src *clock_src);

/**
 * Get 64 bits system tick
 *
 * \returns the system tick since the system boots.
 */
u64 al_clock_get_total_ms(void);

/**
 * Initialize clocks.
 */
void al_clock_init(void);

#endif /* __AYLA_AL_COMMON_CLOCK_H__ */
