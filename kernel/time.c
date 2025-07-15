#include <stdint.h>

#include <sys/time.h>

#include <FreeRTOS/FreeRTOS.h>
#include "FreeRTOS_tick_config.h"

#define time_mtime_to_usec(m)	(m / (((CONFIG_XTAL_CLOCK_HZ / CONFIG_MTIME_CLK_DIV) / 1000000)))

#define USEC_PER_SEC			1000000

struct timeval g_timebase = {
	.tv_sec = 1640995200, /* 2022-01-01T00:00:00 */
};

int _gettimeofday(struct timeval *tv, struct timezone *tz)
{
	uint64_t now;
	uint32_t carry;

	(void)tz;

	now = time_mtime_to_usec(prvReadMtime());

	tv->tv_sec = now / 1000000;
	tv->tv_usec = now % 1000000;

	tv->tv_sec += g_timebase.tv_sec;
	tv->tv_usec += g_timebase.tv_usec;

	if (tv->tv_usec >= USEC_PER_SEC) {
		carry = tv->tv_usec / USEC_PER_SEC;
		tv->tv_sec += carry;
		tv->tv_usec -= (carry * USEC_PER_SEC);
	}

	return 0;
}

int _settimeofday(struct timeval *tv, struct timezone *tz)
{
	struct timeval ts;
	uint64_t now;

	(void)tz;

	now = time_mtime_to_usec(prvReadMtime());

	g_timebase.tv_sec = tv->tv_sec;
	g_timebase.tv_usec = tv->tv_usec;

	ts.tv_sec = now / 1000000;
	ts.tv_usec = now % 1000000;

	if (g_timebase.tv_usec < ts.tv_usec) {
		g_timebase.tv_usec += USEC_PER_SEC;
		g_timebase.tv_sec--;
	}

	g_timebase.tv_sec -= ts.tv_sec;
	g_timebase.tv_usec -= ts.tv_usec;

	return 0;
}
