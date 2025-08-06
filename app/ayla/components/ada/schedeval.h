/*
 * Copyright 2013 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Ayla Networks, Inc.
 */
#ifndef __AYLA_SCHEDEVAL_H__
#define __AYLA_SCHEDEVAL_H__

#include <ada/sched.h>

#define SCHED_OFF_MAX (12 * 60 * 60)	/* max offset from sunrise/sunset */

#define SCHED_SOLAR_NONE (-32768)	/* value indicating no sunrise/sunset */
#define SCHED_SOLAR_MAX	(24 * 60)	/* max minutes in solar table */

#define SCHED_TIME_UNSPEC MAX_U32	/* unspecified time */

/*
 * Values for ATLV_REF TLV.
 */
enum sched_time_ref {
	SCHED_REF_ABS = 0,	/* times are positive from midnight */
	SCHED_REF_SUNRISE = 1,	/* times are relative to sunrise */
	SCHED_REF_SUNSET = 2,	/* times are relative to sunset */
	SCHED_REF_LIMIT		/* limit value for ref token (keep this last) */
};

/*
 * Relative time.
 * Time is a signed offset relative to reference: midnight, sunrise, or sunset.
 */
struct sched_rel_time {
	u8	spec;		/* non-zero if time value specified */
	u8	utc;		/* non-zero if schedule uses UTC time */
	enum sched_time_ref ref; /* time reference */
	s32	time;		/* time relative to reference */
};

/*
 * Solar table.
 * One entry for each week, starting December 21, 2020.
 * The value is the time of sunrise or sunset, before or after 00:00 UTC.
 * Values from cloud are big-endian, but converted on receipt to native.
 */
struct sched_solar_table {
	u8	ref_type;	/* enum sched_time_ref */
	u8	_resvd;		/* reserved */
	s16	mins[53];	/* minutes before/after 00:00 UTC */
};

struct sched_prop {
	char name[SCHED_NAME_LEN];	/* name of schedule */
	u8 updated;			/* non-zero if new value set */
	u8 len;				/* length of schedule */
	u8 tlvs[SCHED_TLV_LEN];		/* base64-decoded tlvs of schedule */
};

/*
 * Parsed schedule items.
 */
struct schedule {
	u32 start_date;		/* inclusive */
	u32 end_date;		/* inclusive */
	u32 days_of_month;	/* 32-bit mask. last bit = last day of month */
	struct sched_rel_time start_time_each_day;
	struct sched_rel_time end_time_each_day;
	u32 duration;		/* superseded by end_time_each_day */
	u32 interval;		/* start every X secs since start time */
	u16 months_of_year;	/* 12-bit mask. bit 0 = jan, bit 11 = dec */
	u8 days_of_week;	/* 7-bit mask. bit 0 = sunday, bit 6 = sat */
	u8 day_occur_in_month;	/* 6-bit mask. 0x80 means last occur */
	unsigned is_utc:1;	/* 1 if the schedule is for UTC */
};

/*
 * Takes a schedtlv structure and fills a schedule structure with
 * the information.
 * Uses the current UTC time to calculate when the next event will
 * occur in UTC time. Returns this value.
 * If it returns 0 or MAX_U32, no more events are set to occur for
 * this schedule.
 */
u32 sched_evaluate(struct sched_prop *schedtlv, u32 time);

/*
 * set an action pending, but don't run it yet.
 */
void sched_action_pend(const struct ayla_tlv *);

/*
 * set the pointer to the sunrise or sunset table.
 */
int sched_solar_table_set(const struct sched_solar_table *table);

/*
 * Get the sunrise or sunset time as seconds from 00:00 UTC on the given day.
 * Returns SCHED_TIME_UNSPEC if no sunrise or sunset on the day.
 */
u32 sched_solar_time(enum sched_time_ref ref, u32 day_start);

/*
 * Return solar day 0 thru 365, starting with December 21.
 * Time is the UTC time since 1970.
 */
u32 sched_solar_day(u32 time);

int sched_eval_print(const struct sched_prop *schedtlv);

#endif /* __AYLA_SCHEDEVAL_H__ */
