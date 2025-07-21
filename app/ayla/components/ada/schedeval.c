/*
 * Copyright 2013-2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

/*
 * Schedule analyzer and interpreter.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/tlv.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ada/err.h>
#include <ada/sched.h>
#include <al/al_os_mem.h>
#include "schedeval.h"

#include <ayla/tlv_access.h>
#include <ayla/endian.h>

#define SCHED_LIB_VERSION 1

#define SCHED_WEEK_DAY_MASK	0x7f	/* mask for 7 week days */
#define SCHED_MONTH_WEEK_MASK	0x1f	/* mask for possible weeks of month */
#define SCHED_LAST_WEEK_MASK	0x80	/* mask for last week of month */
#define SCHED_MONTH_DAYS_MASK	0x7fffffff /* mask for 31 days of month */
#define SCHED_LAST_DAY_MASK	0x80000000 /* mask for last day in month */

#define SCHED_TIME_FMT_LEN	32	/* buffer len for sched_time_format() */

/*
 * Sunrise and sunset tables.
 */
static struct sched_solar_table *sched_table[2];

static int sched_determine_range(const struct schedule *schedule, u32 time,
	u32 *range_start, u32 *range_end, u8 toplevel);

/*
 * Initializes the schedule with defaults
 */
static void sched_init_schedule(struct schedule *sched)
{
	memset(sched, 0, sizeof(*sched));
	sched->days_of_month = ~0;
	sched->months_of_year = ~0;
	sched->days_of_week = ~0;
	sched->day_occur_in_month = ~0;
}

/*
 * Set sunrise or sunset table.
 */
int sched_solar_table_set(const struct sched_solar_table *table)
{
	enum sched_time_ref ref;
	struct sched_solar_table *dest;
	unsigned int i;

	ref = (enum sched_time_ref)table->ref_type;
	switch (ref) {
	case SCHED_REF_SUNRISE:
	case SCHED_REF_SUNSET:
		/*
		 * Copy table into host order.
		 */
		dest = al_os_mem_calloc(sizeof(*dest));
		if (!dest) {
			sched_log(LOG_ERR "solar table alloc failed");
			return -1;
		}
		dest->ref_type = ref;
		for (i = 0; i < ARRAY_LEN(table->mins); i++) {
			dest->mins[i] = (s16)get_ua_be16(&table->mins[i]);
			if (dest->mins[i] < -SCHED_SOLAR_MAX ||
			    dest->mins[i] > SCHED_SOLAR_MAX) {
				sched_log(LOG_WARN "invalid solar table val");
				free(dest);
				return -1;
			}
		}
		free(sched_table[ref - SCHED_REF_SUNRISE]);
		sched_table[ref - SCHED_REF_SUNRISE] = dest;
		sched_log(LOG_DEBUG "updated solar table %u", ref);
		break;
	default:
		sched_log(LOG_WARN "invalid solar table type");
		return -1;
	}
	return 0;
}

/*
 * Return 0 if reference to relative time can be used.
 * Return -1 if invalid.
 * Return 1 if valid, but required solar table is not present.
 */
static int sched_ref_validity_test(u32 value)
{
	enum sched_time_ref ref;

	if (value >= (u32)SCHED_REF_LIMIT) {
		return -1;
	}
	ref = (enum sched_time_ref)value;
	switch (ref) {
	case SCHED_REF_ABS:
		return 0;
	case SCHED_REF_SUNRISE:
	case SCHED_REF_SUNSET:
		if (!sched_table[ref - SCHED_REF_SUNRISE]) {
			SCHED_LOGF(LOG_WARN, "sched needs sun%s table",
			    ref == SCHED_REF_SUNRISE ? "rise" : "set");
			return 1;
		}
		return 0;
	default:
		break;
	}
	return -1;
}

/*
 * Set relative time from TLV.
 * Returns 0 on success, -1 on error.
 */
static int sched_reltime_set(struct sched_rel_time *rt,
	const struct ayla_tlv *atlv,
	enum sched_time_ref ref, u8 utc)
{
	s32 sval;
	u32 uval;

	/*
	 * If time is relative to midnight, it is read as
	 * an unsigned number.  Otherwise it is signed.
	 * For example, 17:00 would be two bytes 0xef10 which
	 * must not be sign extended.
	 */
	switch (ref) {
	case SCHED_REF_ABS:
		if (tlv_u32_get(&uval, atlv)) {
			return -1;
		}
		if (uval > 24 * 60 * 60) {
			return -1;
		}
		sval = uval;
		break;
	case SCHED_REF_SUNRISE:
	case SCHED_REF_SUNSET:
		if (tlv_s32_get(&sval, atlv)) {
			SCHED_LOGF(LOG_WARN, "sched start or end time_each_day "
			    "invalid s32 TLV");
			return -1;
		}
		if (!sched_table[ref - SCHED_REF_SUNRISE]) {
			SCHED_LOGF(LOG_WARN, "sched start or end time_each_day "
			    "%s sun%s but no solar table",
			    sval < 0 ? "before" : "after",
			    ref == SCHED_REF_SUNRISE ? "rise" : "set");
			return -1;
		}
		if (sval > (signed)SCHED_OFF_MAX) {
			SCHED_LOGF(LOG_WARN, "sched start or end time_each_day "
			    "offset %d exceeds max %d", sval, SCHED_OFF_MAX);
			SCHED_LOGF(LOG_WARN, "sched start or end time_each_day "
			    "offset %x exceeds max %x", sval, SCHED_OFF_MAX);
			return -1;
		}
		if (sval < (signed)-SCHED_OFF_MAX) {
			SCHED_LOGF(LOG_WARN, "sched start or end time_each_day "
			    "offset %d is less than min %d",
			    sval, -SCHED_OFF_MAX);
			return -1;
		}
		break;
	default:
		return -1;
	}
	rt->time = sval;
	rt->ref = ref;
	rt->utc = utc;
	rt->spec = 1;
	return 0;
}

/*
 * Convert the time to the solar day, starting with December 21 == day 0.
 */
u32 sched_solar_day(u32 time)
{
	u32 days;

	days = time / (24 * 60 * 60);
	days += 11;	/* difference between Jan 1 and previous Dec 21 */

	/*
	 * The epoch year (1970) was not a leap year; adjust for that.
	 */
	days += 365 + 366;	/* for difference between 1968 and 1970 */

	/*
	 * Take days modulo 365.25.
	 */
	days = ((days * 4) % ((365 * 4) + 1)) / 4;
	return days;
}

/*
 * Return the time of sunrise or sunset inside the given day in seconds UTC.
 * day_time is in seconds since 1970 for 00:00 UTC on the day.
 * Returns SCHED_TIME_UNSPEC if no sunrise or sunset on the day.
 */
u32 sched_solar_time(enum sched_time_ref ref, u32 day_time)
{
	const struct sched_solar_table *table;
	u32 day;
	u32 week;
	int offset;
	int offset1;
	const int day_in_secs = 24 * 60 * 60;	/* one day in seconds */

	table = sched_table[ref - SCHED_REF_SUNRISE];
	if (!table) {
		return SCHED_TIME_UNSPEC;	/* cannot happen */
	}
	day = sched_solar_day(day_time);
	week = day / 7;
	day = day % 7;
	if (week > ARRAY_LEN(table->mins)) {
		return SCHED_TIME_UNSPEC;	/* cannot happen */
	}

	offset = (int)table->mins[week];
	if (offset == SCHED_SOLAR_NONE) {
		return SCHED_TIME_UNSPEC;	/* no event that day */
	}

	/*
	 * Interpolate between two weeks.
	 */
	offset *= 60;
	if (day) {
		week = (week + 1) % ARRAY_LEN(table->mins);
		offset1 = (s32)table->mins[week] * 60;

		/*
		 * Allow for wrap-around when the offsets cross midnight UTC.
		 */
		if (ABS(offset1 - offset) > day_in_secs / 2) {
			if (offset1 > offset) {
				offset += day_in_secs;
			} else {
				offset1 += day_in_secs;
			}
		}
		offset = (offset * (int)(7 - day) + offset1 * (int)day) / 7;
		if (offset >= day_in_secs) {
			offset -= day_in_secs;
		}
	}
	return (u32)(offset + day_time);
}

#ifdef SCHED_TEST
/*
 * Print a time of day (HH:MM:SS) in supplied buffer.
 * Buffer should be at sized SCHED_TIME_FMT_LEN.
 */
static char *sched_time_format(u32 time, char *buf, size_t len)
{
	snprintf(buf, len, "%2.2lu:%2.2lu:%2.2lu",
	    (time / (60 * 60)) % 24, (time / 60) % 60, time % 60);
	return buf;
}

/*
 * Print relative time for tests.
 */
static void sched_reltime_print(const struct sched_rel_time *rt,
		const char *tlv_type)
{
	s32 offset;
	char *sign = "";
	char *rel = "";
	char buf[SCHED_TIME_FMT_LEN];

	offset = rt->time;
	switch (rt->ref) {
	case SCHED_REF_ABS:
		break;
	case SCHED_REF_SUNRISE:
		rel = "sunrise";
		sign = "after";
		break;
	case SCHED_REF_SUNSET:
		rel = "sunset";
		sign = "after";
		break;
	default:
		rel = "unknown";
		sign = "after";
		break;
	}
	if (rt->ref != SCHED_REF_ABS && offset < 0) {
		offset = -offset;
		sign = "before";
	}
	printf("TLV %s %s %s %s\n",
	    tlv_type, sched_time_format(offset, buf, sizeof(buf)), sign, rel);
}
#endif /* SCHED_TEST */

/*
 * Get local start or end time each day value in schedule time, ~0 if not set.
 * Schedule time is either UTC or local, depending on settings.
 * With reference to sunrise/sunset, this varies depending on the date.
 *
 * day is 00:00 schedule time (local or UTC) on the day in question.
 * Returns SCHED_TIME_UNSPEC on error.
 */
static u32 sched_reltime_get(const struct sched_rel_time *rt, u32 day)
{
	u32 time = day;
	u32 local;

	if (!rt->spec) {
		return SCHED_TIME_UNSPEC;
	}
	switch (rt->ref) {
	case SCHED_REF_ABS:
	default:
		time += rt->time;
		break;
	case SCHED_REF_SUNRISE:
	case SCHED_REF_SUNSET:
		/*
		 * Time will be 00:00 on some day UTC.
		 */
		time = sched_solar_time(rt->ref, time);

		/*
		 * If there was no sunrise or sunset that day, assume it
		 * happened at noon.
		 */
		if (time == SCHED_TIME_UNSPEC) {
			time = day + 12 * 60 * 60;
		}
		time += rt->time;
		if (!rt->utc) {
			/*
			 * Convert time to local time inside the UTC day.
			 * If we end up in the previous or next day, reconvert
			 * to get the correct DST correction.
			 */
			local = clock_local(&time);
			if (local < day) {
				time += 24 * 60 * 60;
				local = clock_local(&time);
			} else if (local > day + 24 * 60 * 60) {
				time -= 24 * 60 * 60;
				local = clock_local(&time);
			}
			time = local;
		}
		break;
	}
	if (time >= MAX_U32) {
		return MAX_U32;
	}
	return time;
}

/*
 * Return non-zero if the schedule's time range carries over to the next day.
 * The range could be from sunset to sunrise, for example, or
 * from sunset to 1 AM.
 */
static u8 sched_time_carryover(const struct schedule *schedule, u32 day)
{
	return schedule->start_time_each_day.spec &&
	    schedule->end_time_each_day.spec &&
	    sched_reltime_get(&schedule->end_time_each_day, day) <
	    sched_reltime_get(&schedule->start_time_each_day, day);
}

/*
 * Checks if the mask_to_check bit has been set in valid_mask
 */
static int sched_check_mask(u16 valid_mask, u8 mask_to_check)
{
	return valid_mask & (1 << (mask_to_check - 1));
}

/*
 * Checks if the day in clk is valid according to the requirements
 * of the schedule. Returns 1 if clk satisfies the day reqs.
 */
static int sched_check_day(const struct schedule *schedule,
		const struct clock_info *clk)
{
	u8 dayofwk = clk->day_of_week;
	u8 day_occur = 1 << (clk->day_occur_in_month - 1);
	u32 day_of_month = 1 << (clk->days - 1);

	if (dayofwk == 7) {
		/* dayofwk = 7 means a sunday */
		dayofwk = 0;
	}
	dayofwk = 1 << dayofwk;

	if ((schedule->days_of_month >> 31) && !clk->days_left_in_month) {
		/* special case: 0x80000000 means last day of the month */
		day_of_month = MAX_U32;
	}

	if ((schedule->day_occur_in_month & SCHED_LAST_WEEK_MASK) &&
	    clk->days_left_in_month < 7) {
		/* special case: 0x80 means last occurence in month */
		day_occur = SCHED_LAST_WEEK_MASK;
	}

	return (schedule->days_of_week & dayofwk) &&
	    (schedule->day_occur_in_month & day_occur) &&
	    (schedule->days_of_month & day_of_month);
}

static int sched_day_spec_is_given(const struct schedule *schedule)
{
	u8 daysofwk = schedule->days_of_week;
	u8 dayoccur = schedule->day_occur_in_month;
	u32 daysofmon = schedule->days_of_month;

	daysofwk |= ~SCHED_WEEK_DAY_MASK;
	dayoccur |= ~(SCHED_MONTH_WEEK_MASK | SCHED_LAST_WEEK_MASK);
	if (daysofmon == SCHED_MONTH_DAYS_MASK) {
		daysofmon |= SCHED_LAST_DAY_MASK;
	}

	if ((u8)~daysofwk || (u8)~dayoccur || ~daysofmon) {
		return 1;
	}
	return 0;
}

/*
 * If clk is on a valid day, start will go back to when this range
 * began and end will go to when the range ends.
 * If clk is not on a valid day, start will be the first valid day
 * from min limit and end will be the end of that range.
 * Both start and end are bound by min_limit and max_limit.
 */
static void sched_day_find_range(const struct clock_info *clk,
	const struct schedule *schedule, u32 min_limit, u32 max_limit,
	u32 *start, u32 *end)
{
	struct clock_info bound;

	memcpy(&bound, clk, sizeof(bound));
	if (!sched_day_spec_is_given(schedule)) {
		/* if neither of the day-related fields are set in schedule */
		*start = min_limit;
		*end = max_limit;
		return;
	}
	if (sched_check_day(schedule, &bound)) {
		if (min_limit >= bound.day_start) {
			*start = min_limit;
			goto find_end;
		}
		do {
			clock_decr_day(&bound);
		} while (min_limit <= bound.day_start &&
		    sched_check_day(schedule, &bound) &&
		    bound.day_start);
		*start = (min_limit >= (bound.day_end + 1)  ?
		    min_limit : (bound.day_end + 1));
		memcpy(&bound, clk, sizeof(bound));
		goto find_end;
	}
	clock_fill_details(&bound, min_limit);
	while (!sched_check_day(schedule, &bound) &&
	    bound.day_end <= max_limit &&
	    bound.day_end != MAX_U32) {
		clock_incr_day(&bound);
	}
	if (!sched_check_day(schedule, &bound)) {
		/* found no valid future range */
		*start = 0;
		*end = 0;
		return;
	}
	*start = (min_limit > bound.day_start ? min_limit : bound.day_start);
find_end:
	while (bound.day_start <= max_limit &&
	    sched_check_day(schedule, &bound) &&
	    bound.day_end != MAX_U32) {
		clock_incr_day(&bound);
	}
	*end = (max_limit < bound.day_start) ?
	    max_limit : bound.day_start;
}

/*
 * If clk is on a valid month, start will go back to when this range
 * began and end will go to when the range ends.
 * If clk is not on a valid month, start will be the first valid month
 * from min limit and end will be the end of that range.
 * Both start and end are bound by min_limit and max_limit.
 */
static void sched_month_find_range(const struct clock_info *clk,
	const struct schedule *sched, u32 min_limit, u32 max_limit,
	u32 *start, u32 *end)
{
	struct clock_info bound;
	u16 valid_months = sched->months_of_year;

	memcpy(&bound, clk, sizeof(bound));
	if (!(u16)~valid_months) {
		*start = min_limit;
		*end = max_limit;
		return;
	}
	if (sched_check_mask(valid_months, bound.month)) {
		if (min_limit >= bound.month_start) {
			*start = min_limit;
			goto find_end;
		}
		do {
			clock_decr_month(&bound);
		} while (bound.month_start >= min_limit &&
		    sched_check_mask(valid_months, bound.month) &&
		    bound.month_start);
		*start = (min_limit > (bound.month_end + 1)  ?
		    min_limit : (bound.month_end + 1));
		memcpy(&bound, clk, sizeof(bound));
		goto find_end;
	}
	clock_fill_details(&bound, min_limit);
	while (!sched_check_mask(valid_months, bound.month) &&
	    bound.month_end <= max_limit &&
	    bound.month_end != MAX_U32) {
		clock_incr_month(&bound);
	}
	if (!sched_check_mask(valid_months, bound.month)) {
		/* found no valid future range */
		*start = 0;
		*end = 0;
		return;
	}
	*start = (min_limit > bound.month_start ?
	    min_limit : bound.month_start);
find_end:
	while (bound.month_start <= max_limit &&
	    sched_check_mask(valid_months, bound.month) &&
	    bound.month_end != MAX_U32) {
		clock_incr_month(&bound);
	}
	*end = (max_limit < bound.month_start - 1) ?
	    max_limit : bound.month_start - 1;
}

/*
 * Adjusts tmax if the interval lasts across a SF boundary.
 */
static void sched_adjust_boundary_if_crossover(u32 tmin, u32 *tmax,
					u32 duration)
{
	u32 tmp1;
	u32 tmp2;

	tmp1 = clock_local(&daylight_info.change);
	tmp2 = tmp1 - DAYLIGHT_OFFSET;
	if (tmin >= tmp2 && tmin < tmp1) {
		*tmax = tmp1 + duration;
	} else if (tmin <= tmp2 && *tmax >= tmp2) {
		if (daylight_info.active) {
			*tmax -= DAYLIGHT_OFFSET;
		} else {
			*tmax += DAYLIGHT_OFFSET;
		}
	}
}

/*
 * Given a schedule, and the current time, this function will:
 * Case A: Time falls within the schedule interval. range_start
 * is set to the begininng of the interval and range_end is the end.
 * Case B: Time is before the scheduled interval. range_start
 * will be set to the beginning of the next interval.
 * Case C: No more events in the schedule after time. range_start
 * and range_end will both be set to 0.
 * This function will give the overall range that the time falls in.
 * It does not take into account intervals.
 */
static int sched_determine_big_range(const struct schedule *schedule,
		u32 time, u32 *range_start, u32 *range_end, u8 toplevel)
{
	struct clock_info clk;
	struct clock_info bound;
	u32 tmin = 1;
	u32 tmax = MAX_U32;
	u32 start;
	u32 end;
	u32 tmp1;
	u32 tmp2;
	u8 time_carryover;
	u32 duration = schedule->duration;
	u32 max_tmax = MAX_U32;
	u8 in_range = 1;
	u8 duration_no_interval = schedule->duration && !schedule->interval;

	/*
	 * If start date is specified, set tmin to the start of that day.
	 * Same with end_date.
	 */
	if (schedule->start_date) {
		clock_fill_details(&bound, schedule->start_date);
		if (bound.day_start > tmin) {
			tmin = bound.day_start;
		}
	}
	if (schedule->end_date) {
		clock_fill_details(&bound, schedule->end_date);
		if (bound.day_end < tmax) {
			tmax = bound.day_end;
		}
		max_tmax = tmax;
	}

	/*
	 * If the range is empty, no more to do.
	 */
	if (tmin >= tmax) {
		goto no_more_events;
	}

	/*
	 * If we're at the end time, determine start of the range.
	 */
	if (time == tmax) {
		/*
		 * right on end date, we need to go through and figure
		 * out the start of this interval. can't just goto done.
		 * I believe finding the range of time-1 should take care
		 * of this.
		 */
		time--;
	} else if (time > tmax) {
		if (toplevel) {
			clock_fill_details(&clk, time);
			goto check_carryover;
		}
		goto no_more_events;
	} else if (time < tmin) {
		in_range = 0;
	}

	/*
	 * If not in range, before schedule start, find next range start.
	 */
	if (!in_range) {
find_next_event:
		if (tmin >= tmax) {
			goto no_more_events;
		}

		/*
		 * Find next valid month after tmin.
		 */
		clock_fill_details(&clk, tmin);
		sched_month_find_range(&clk, schedule, tmin, tmax, &start,
		    &end);
		if (!start) {
			goto no_more_events;
		}

		/*
		 * Find next valid day after start.
		 */
		tmin = start;
		tmax = end;
		clock_fill_details(&clk, tmin);
		sched_day_find_range(&clk, schedule, tmin, tmax,
		    &start, &end);
		if (!start) {
			if (tmax >= MAX_U32) {
				goto no_more_events;
			}
			tmin = tmax + 1;
			tmax = max_tmax;
			goto find_next_event;
		}
		tmin = start;
		tmax = end;

		/*
		 * If start time each day is specified, set tmin to that.
		 * But don't reduce tmin.
		 */
set_start_time_each_day:
		if (schedule->start_time_each_day.spec) {
			clock_fill_details(&clk, tmin);
			tmp1 = sched_reltime_get(&schedule->start_time_each_day,
			    clk.day_start);
			if (tmp1 > tmin) {
				tmin = tmp1;
			}
		}

		/*
		 * If a duration is given without an interval, that's the
		 * big range duration.  Set tmax with that.
		 * end_time_each_day may not also be specified.
		 */
		if (duration_no_interval) {
			if (schedule->end_time_each_day.spec) {
				SCHED_LOGF(LOG_WARN, "sched duration error");
				goto no_more_events;
			}
			tmax = tmin + duration;

			/*
			 * If tmax is in the past, the whole range is in past.
			 * Advance tmin to next day.
			 */
			if (time > tmax) {
				goto next_day;
			}
			/* Check durations that cross SF daylight boundaries */
			if (!schedule->is_utc && daylight_info.valid &&
			    !daylight_info.active) {
				sched_adjust_boundary_if_crossover(tmin, &tmax,
				    duration);
			}
		}
		goto done;
	}

	/*
	 * In range of dates covered by schedule.  Find next day range.
	 */
	clock_fill_details(&clk, time);
	if (!sched_check_mask(schedule->months_of_year, clk.month)) {
		if (toplevel) {
			goto check_carryover;
		}
		goto no_more_events;
	}

	/*
	 * Get the start/end values for the current interval based on
	 * month range.
	 */
	sched_month_find_range(&clk, schedule, tmin, tmax, &start, &end);
	if (!start) {
		goto no_more_events;
	}
	tmin = start;
	tmax = end;

	if (!sched_day_spec_is_given(schedule) && schedule->interval) {
		/* if neither of the day-related fields are set in an interval
		 * schedule. i.e. Every other day in the month of June 2013 */
		if (schedule->end_time_each_day.spec) {
			/*
			 * end time specified.
			 */
			goto check_time;
		}
		clock_fill_details(&clk, tmin);
		if (schedule->start_time_each_day.spec) {
			tmin = sched_reltime_get(&schedule->start_time_each_day,
			    clk.day_start);
		}
		if (time > tmax) {
			goto next_day;
		}
		if (time < tmin) {
			in_range = 0;
		}
		goto check_duration;
	}

	/*
	 * Day is specified, check it.
	 */
	if (!sched_check_day(schedule, &clk)) {
		if (toplevel) {
			goto check_carryover;
		}
		goto no_more_events;
	}
	/*
	 * Get the start/end values for the current interval based on
	 * day range.
	 */
	sched_day_find_range(&clk, schedule, tmin, tmax, &start, &end);
	if (!start) {
		in_range = 0;
		tmin = tmax + 1;
		tmax = max_tmax;
		goto find_next_event;
	}
	tmin = start;
	tmax = end;
	if (tmin > time) {
		in_range = 0;
		goto set_start_time_each_day;
	}
check_time:
	if (schedule->start_time_each_day.spec ||
	    schedule->end_time_each_day.spec) {
		tmp1 = tmin;
		tmp2 = tmax;
		if (schedule->start_time_each_day.spec) {
			if (schedule->end_time_each_day.spec ||
			    duration_no_interval) {
				tmp1 = sched_reltime_get(
				    &schedule->start_time_each_day,
				    clk.day_start);
			} else {
				clock_fill_details(&bound, tmin);
				tmp1 = sched_reltime_get(
				    &schedule->start_time_each_day,
				    bound.day_start);
			}
		}
		if (schedule->end_time_each_day.spec) {
			if (schedule->start_time_each_day.spec) {
				tmp2 = sched_reltime_get(
				    &schedule->end_time_each_day,
				    clk.day_start);
			} else {
				clock_fill_details(&bound, tmax);
				tmp2 = sched_reltime_get(
				    &schedule->end_time_each_day,
				    bound.day_start);
			}
		}
		if (!sched_time_carryover(schedule, clk.day_start)) {
			tmin = tmp1;
			if (tmin > time) {
				in_range = 0;
			}
			if (schedule->end_time_each_day.spec) {
				if (time <= tmp2) {
					tmax = tmp2;
					goto done;
				}
				goto next_day;
			}
			goto check_duration;
		}
		if (time <= tmp2 && (toplevel || in_range)) {
			goto check_carryover;
		}
		tmin = tmp1;
		if (schedule->end_time_each_day.spec) {
			tmax = sched_reltime_get(&schedule->end_time_each_day,
			    clk.day_end + 1);
		} else {
			tmax = clk.day_end;
		}
		if (time < tmin) {
			in_range = 0;
		}
		goto done;
	}
check_duration:
	if (duration_no_interval) {
		if (schedule->end_time_each_day.spec) {
			SCHED_LOGF(LOG_WARN, "sched duration error");
			goto no_more_events;
		}
		tmax = tmin + duration;
		/* Check durations that cross SF daylight boundaries */
		if (!schedule->is_utc && daylight_info.valid &&
		    !daylight_info.active) {
			sched_adjust_boundary_if_crossover(tmin, &tmax,
			    duration);
		}
		if (time > tmax) {
			if (toplevel) {
				goto check_carryover;
			}
			goto next_day;
		}
	}

done:
	*range_start = tmin;
	*range_end = tmax;

	return in_range;

check_carryover:
	/* TODO check date passed */
	time_carryover = sched_time_carryover(schedule, tmin);
	if (!time_carryover && !duration) {
		goto next_day;
	}
	clock_fill_details(&clk, time);
	if (time_carryover) {
		clock_decr_day(&clk);
		if (sched_check_mask(schedule->months_of_year, clk.month) &&
		    sched_check_day(schedule, &clk)) {
			tmp1 = sched_reltime_get(&schedule->end_time_each_day,
			    clk.day_end + 1);
			if (time <= tmp1) {
				tmin = sched_reltime_get(
				    &schedule->start_time_each_day,
				    clk.day_start);
				tmax = tmp1;
				goto done;
			}
		}
	} else if (duration) {
		if (time - duration >= clk.day_start) {
			/*
			 * check if subtracting duration will
			 * actually change the day. if not,
			 * then there's no way we're in_range.
			 */
			goto next_day;
		}
		clock_fill_details(&clk, time - duration);
		if (!sched_check_mask(schedule->months_of_year, clk.month) ||
		    !sched_check_day(schedule, &clk)) {
			goto next_day;
		}
		sched_determine_range(schedule, time - duration,
		    range_start, range_end, 0);
		if (time <= *range_end) {
			return 0;
		}
		clock_fill_details(&clk, time);
	}
	goto next_day;

no_more_events:
	*range_start = 0;
	*range_end = 0;

	return in_range;

next_day:
	in_range = 0;
	tmin = clk.day_end + 1;
	tmax = max_tmax;
	goto find_next_event;
}

/*
 * Returns the minimum val that's greater than time.
 */
static u32 sched_get_minimum(u32 time, u32 val1, u32 val2)
{
	if (val1 >= time && val2 >= time) {
		return val1 < val2 ? val1 : val2;
	} else if (val1 >= time) {
		return val1;
	} else if (val2 >= time) {
		return val2;
	}

	return 0;
}

/*
 * Given a schedule, and the current time, this function will:
 * Case A: Time falls within the schedule interval. range_start
 * is set to the begininng of the interval and range_end is the end.
 * Case B: Time is before the scheduled interval. range_start
 * will be set to the beginning of the next interval.
 * Case C: No more events in the schedule after time. range_start
 * and range_end will both be set to 0.
 * This function will take into account intervals.
 */
static int sched_determine_range(const struct schedule *schedule, u32 time,
	u32 *range_start, u32 *range_end, u8 toplevel)
{
	struct clock_info clk;
	u8 in_range;
	u32 little_range_start;
	u32 little_range_end;
	u32 real_time = time;
	u32 utc_range_start1;
	u32 utc_range_start2;
	u32 utc_range_end1;
	u32 utc_range_end2;
	u32 orig_range_start;
	u32 orig_range_end;
	u32 tmp;

	/* Get the big picture range first */
	if (!time || time == MAX_U32) {
		goto no_more_events;
	}
	if (toplevel && !schedule->is_utc) {
		real_time = clock_local(&time);
	}
	in_range = sched_determine_big_range(schedule, real_time,
	    &orig_range_start, &orig_range_end, toplevel);

local_to_utc:
	if ((!orig_range_start && !orig_range_end) ||
	    orig_range_start == MAX_U32) {
		goto no_more_events;
	}

	if (toplevel && !schedule->is_utc) {
		if (daylight_info.valid && !daylight_info.active) {
			/* Accounts for spring-forward */
			*range_start = clock_local_to_utc(orig_range_start, 1);
			if (!schedule->interval && schedule->duration) {
				*range_end = clock_local_to_utc(orig_range_end,
				    2);
				if (*range_start >= *range_end) {
					goto find_next_spring_forward;
				}
				if (time < *range_start) {
					in_range = 0;
				}
				goto determine_little_range;
			}
			*range_end = clock_local_to_utc(orig_range_end, 0);
			utc_range_start2 =
			    clock_local_to_utc(orig_range_start, 0);
			if (*range_start > *range_end ||
			    (*range_start != utc_range_start2)) {
				in_range = 0;
				sched_determine_big_range(schedule,
				    *range_start, &orig_range_start,
				    &orig_range_end, toplevel);
				goto local_to_utc;
			} else if (*range_start == *range_end &&
			    orig_range_end - orig_range_start) {
find_next_spring_forward:
				in_range = 0;
				sched_determine_big_range(schedule,
				    *range_start + 1, &orig_range_start,
				    &orig_range_end, toplevel);
				goto local_to_utc;
			}
			goto determine_little_range;
		}
		/* Accounts for fallback */
		utc_range_start1 = clock_local_to_utc(orig_range_start, 0);
		utc_range_start2 = clock_local_to_utc(orig_range_start, 1);
		utc_range_end1 = clock_local_to_utc(orig_range_end, 0);
		utc_range_end2 = clock_local_to_utc(orig_range_end, 1);
		if (!in_range) {
			if (time >= utc_range_start1) {
				in_range = 1;
				goto in_range;
			}
			*range_start = sched_get_minimum(time,
			    utc_range_start1, utc_range_start2);
		} else {
in_range:
			*range_start = utc_range_start1;
		}
		if (!schedule->interval && schedule->duration) {
			/* Account for durations across FB */
			*range_end = *range_start +
			    orig_range_end - orig_range_start;
			if (in_range && time > *range_end) {
				in_range = 0;
				if (!daylight_info.valid ||
				    !daylight_info.active) {
					/* shouldn't happen */
					goto no_more_events;
				}
				real_time = time + DAYLIGHT_OFFSET;
				time = daylight_info.change + DAYLIGHT_OFFSET;
				sched_determine_big_range(schedule, real_time,
				    &orig_range_start, &orig_range_end,
				    toplevel);
				goto local_to_utc;
			}
		} else {
			*range_end = sched_get_minimum(time, utc_range_end1,
			    utc_range_end2);
		}
	} else {
		*range_start = orig_range_start;
		*range_end = orig_range_end;
	}
determine_little_range:
	if ((!*range_start && !*range_end) || *range_start == MAX_U32 ||
	    !in_range || !schedule->interval) {
		return 0;
	}
	/* Need to find the little interval from the big interval */
	tmp = ((time - *range_start) / schedule->interval) * schedule->interval;
	if (tmp >= MAX_U32 - *range_start) {
		goto no_more_events;
	}
	little_range_start = *range_start + tmp;
	if (little_range_start > *range_end) {
		/* should not happen, bad schedule */
		goto no_more_events;
	}
calc_end_and_return:
	if (little_range_start == *range_end && schedule->duration) {
		/*
		 * for instance, every hour on Monday for 20 mins,
		 * we don't want an event to occur at Tuesday midnight
		 */
		goto calc_next_interval;
	}
	/*
	 * its ok if little_range_end is > *range_end.
	 * we just care that the start time falls within the schedule.
	 * unless end_time_each_day is specified in which case the duration
	 * is chopped off.
	 */
	if (schedule->duration > MAX_U32 - little_range_start) {
		little_range_end = MAX_U32;
	} else {
		little_range_end = little_range_start + schedule->duration;
	}
	if (little_range_end > *range_end && schedule->end_time_each_day.spec) {
		clock_fill_details(&clk, little_range_end);
		tmp = sched_reltime_get(&schedule->end_time_each_day,
		    clk.day_start);
		tmp = tmp - clk.day_start;	/* end time for this day */
		if (clk.secs_since_midnight > tmp) {
			little_range_end -= clk.secs_since_midnight -
			    tmp;
		}
	}
	if (little_range_end < little_range_start) {
		goto calc_next_interval;
	}
	if (time <= little_range_end) {
		*range_start = little_range_start;
		*range_end = little_range_end;
		return 0;
	}
calc_next_interval:
	if (schedule->interval >= MAX_U32 - little_range_start) {
		goto no_more_events;
	}
	little_range_start += schedule->interval;
	if (little_range_start > *range_end) {
		/* no valid little intervals left in this big interval */
		/* find the next big interval */
		if (!toplevel || orig_range_end == MAX_U32) {
			goto no_more_events;
		}
		in_range = 0;
		real_time = orig_range_end + 1;
		time = *range_end + 1;
		sched_determine_big_range(schedule, real_time,
		    &orig_range_start, &orig_range_end, toplevel);
		goto local_to_utc;
	}
	goto calc_end_and_return;

no_more_events:
	*range_start = 0;
	*range_end = 0;

	return 0;
}

/*
 * Takes a schedtlv structure and fills a schedule structure with
 * the information.
 * Uses the current UTC time to calculate when the next event will
 * occur in UTC time. Returns this value.
 * If it returns 0 or MAX_U32, no more events are set to occur for
 * this schedule.
 * Calls the action_handler for any actions that should occur now.
 */
u32 sched_evaluate(struct sched_prop *schedtlv, u32 time)
{
	struct ayla_tlv *tlvs = (struct ayla_tlv *)schedtlv->tlvs;
	enum ada_err err;

	err = ada_sched_eval(tlvs, schedtlv->len, &time, sched_action_pend);
	switch (err) {
	case AE_OK:
		/* Success */
		break;
	case AE_INVAL_VAL:
		/* Invalid schedule; disable to avoid repeat calls */
		schedtlv->len = 0;
		return 0;
	case AE_INVAL_STATE:
		/*
		 * Not enough time info (UTC or timezone) to evaluate
		 * schedule. For now, fail silently, as schedule will be
		 * re-evaluated if UTC or timezone is received later.
		 */
		return 0;
	default:
		return 0;
	}
	return time;
}

/*
 * Evaluate a schedule.
 *
 * See description in <ada/sched.h>
 */
enum ada_err ada_sched_eval(const struct ayla_tlv *atlv_in, size_t len,
	u32 *timep, void (*action_handler)(const struct ayla_tlv *tlv))
{
	const struct ayla_tlv *atlv = atlv_in;
	const u8 *tlvs = (u8 *)atlv_in;
	u32 time;
	u32 value;
	u8 consumed = 0;
	struct schedule sched;
	u8 end_needed = 0;
	u32 range_start = 0;
	u32 range_end = 0;
	u8 range_calculated = 0;
	enum sched_time_ref ref = SCHED_REF_ABS;
	int err = 0;
	enum ayla_tlv_type type = ATLV_INVALID;

	if (len <= sizeof(*atlv) ||
	    atlv->type != ATLV_VERSION ||
	    tlv_u32_get(&value, atlv) ||
	    value > SCHED_LIB_VERSION) {
		SCHED_LOGF(LOG_ERR, "bad sched ver");
		return AE_INVAL_VAL;
	}
	time = *timep;

	consumed += atlv->len + sizeof(struct ayla_tlv);
	sched_init_schedule(&sched);
	while (consumed < len) {
		atlv = (const struct ayla_tlv *)(tlvs + consumed);
		switch (atlv->type) {
		case ATLV_AND:
			break;
		case ATLV_DISABLE:
			return AE_INVAL_VAL;
		case ATLV_UTC:
			sched.is_utc = 1;
			break;
		case ATLV_REF:
			err = tlv_u32_get(&value, atlv);
			if (err) {
				break;
			}
			err = sched_ref_validity_test(value);
			if (err) {
				if (err < 0) {
					break;
				}
				/* wait for required solar table */
				*timep = MAX_U32;
				return AE_OK;
			}
			ref = (enum sched_time_ref)value;
			break;
		case ATLV_STARTDATE:
			err = tlv_u32_get(&sched.start_date, atlv);
			break;
		case ATLV_ENDDATE:
			err = tlv_u32_get(&sched.end_date, atlv);
			break;
		case ATLV_DAYSOFMON:
			err = tlv_u32_get(&sched.days_of_month, atlv);
			break;
		case ATLV_DAYSOFWK:
			err = tlv_u8_get(&sched.days_of_week, atlv);
			break;
		case ATLV_DAYOCOFMO:
			err = tlv_u8_get(&sched.day_occur_in_month, atlv);
			break;
		case ATLV_MOOFYR:
			err = tlv_u16_get(&sched.months_of_year, atlv);
			break;
		case ATLV_STTIMEEACHDAY:
			err = sched_reltime_set(&sched.start_time_each_day,
			    atlv, ref, sched.is_utc);
			break;
		case ATLV_ENDTIMEEACHDAY:
			err = sched_reltime_set(&sched.end_time_each_day,
			     atlv, ref, sched.is_utc);
			break;
		case ATLV_DURATION:
			err = tlv_u32_get(&sched.duration, atlv);
			break;
		case ATLV_INTERVAL:
			err = tlv_u32_get(&sched.interval, atlv);
			break;
		case ATLV_ATEND:
			end_needed = 1;
			/* fall through */
		case ATLV_ATSTART:
		case ATLV_INRANGE:
			type = (enum ayla_tlv_type)atlv->type;
			if (!time) {
				SCHED_LOGF(LOG_WARN, "cur time not known");
				return AE_INVAL_STATE;
			}
			if (!sched.is_utc && !timezone_info.valid) {
				SCHED_LOGF(LOG_WARN, "timezone not known");
				return AE_INVAL_STATE;
			}
			if (range_calculated) {
				/* ranges only need to be calculated once */
				break;
			}
			if (sched.interval && sched.duration &&
			    sched.interval <= sched.duration) {
				/* i.e. do something for 15 mins every 5 mins */
				sched.interval += sched.duration;
			}
			if (sched_determine_range(&sched, time, &range_start,
			    &range_end, 1)) {
				SCHED_LOGF(LOG_WARN, "range calc err");
				err = -1;
			}
			range_calculated = 1;
			break;
		case ATLV_SETPROP:
			if ((type == ATLV_ATSTART && time == range_start) ||
			    (type == ATLV_ATEND && time == range_end) ||
			    (type == ATLV_INRANGE && time >= range_start &&
			    time < range_end)) {
				action_handler(atlv);
			}
			break;
		default:
			SCHED_LOGF(LOG_WARN, "unknown sched tlv = %x",
			    atlv->type);
			return AE_INVAL_VAL;
		}
		if (err) {
			SCHED_LOGF(LOG_WARN, "sched value err tlv 0x%x",
			    atlv->type);
			return AE_INVAL_VAL;
		}
		consumed += atlv->len + sizeof(struct ayla_tlv);
	}
	if (!range_start && !range_end) {
		*timep = 0;	/* TODO is this an error */
		return AE_OK;
	} else if (time < range_start) {
		*timep = range_start;
		return AE_OK;
	}
	if (!end_needed && range_end != MAX_U32) {
		time = range_end + 1;
		goto determine_next_event;
	}
	if (time < range_end) {
		*timep = range_end;
	} else if (time == range_end && time < MAX_U32) {
		/* find the next interval */
		time = range_end + 1;
determine_next_event:
		sched_determine_range(&sched, time,
		    &range_start, &range_end, 1);
		if (range_start && range_start < MAX_U32 &&
		    range_start < time) {
			SCHED_LOGF(LOG_ERR, "sched_eval err");
			return AE_INVAL_VAL;
		}
		*timep = range_start;
	} else {
		*timep = MAX_U32;
	}
	return AE_OK;
}

#ifdef SCHED_TEST
/*
 * Print schedule for testing.
 */
int sched_eval_print(const struct sched_prop *schedtlv)
{
	const u8 *tlvs = &schedtlv->tlvs[0];
	struct ayla_tlv *atlv = (struct ayla_tlv *)(tlvs);
	struct schedule sched;
	u32 value;
	u8 consumed = 0;
	enum sched_time_ref ref = SCHED_REF_ABS;
	int err = 0;

	if (atlv->type != ATLV_VERSION ||
	    tlv_u32_get(&value, atlv) ||
	    value > SCHED_LIB_VERSION) {
		SCHED_LOGF(LOG_WARN, "bad sched ver %u", value);
	}
	consumed += atlv->len + sizeof(struct ayla_tlv);
	sched_init_schedule(&sched);
	while (consumed < schedtlv->len) {
		atlv = (struct ayla_tlv *)(tlvs + consumed);
		switch (atlv->type) {
		case ATLV_AND:
			printf("TLV AND\n");
			break;
		case ATLV_DISABLE:
			printf("TLV disable\n");
			return 0;
		case ATLV_UTC:
			printf("TLV UTC\n");
			sched.is_utc = 1;
			break;
		case ATLV_REF:
			err = tlv_u32_get(&value, atlv);
			if (!err) {
				printf("TLV ref %lu\n", value);
				ref = value;
			}
			break;
		case ATLV_STARTDATE:
			err = tlv_u32_get(&sched.start_date, atlv);
			printf("TLV start_date %lu %s\n",
			    sched.start_date, err ? "err" : "");
			break;
		case ATLV_ENDDATE:
			err = tlv_u32_get(&sched.end_date, atlv);
			printf("TLV end_date %lu %s\n",
			    sched.end_date, err ? "err" : "");
			break;
		case ATLV_DAYSOFMON:
			err = tlv_u32_get(&sched.days_of_month, atlv);
			printf("TLV days_of_month %lx %s\n",
			    sched.days_of_month, err ? "err" : "");
			break;
		case ATLV_DAYSOFWK:
			err = tlv_u8_get(&sched.days_of_week, atlv);
			printf("TLV days_of_week %x %s\n",
			    sched.days_of_week, err ? "err" : "");
			break;
		case ATLV_DAYOCOFMO:
			err = tlv_u8_get(&sched.day_occur_in_month, atlv);
			printf("TLV occur_in_month %x %s\n",
			    sched.day_occur_in_month, err ? "err" : "");
			break;
		case ATLV_MOOFYR:
			err = tlv_u16_get(&sched.months_of_year, atlv);
			printf("TLV months_of_year %x %s\n",
			    sched.months_of_year, err ? "err" : "");
			break;
		case ATLV_STTIMEEACHDAY:
			err = sched_reltime_set(&sched.start_time_each_day,
			    atlv, ref, sched.is_utc);
			if (err) {
				printf("TLV start time each day - err");
				break;
			}
			sched_reltime_print(&sched.start_time_each_day,
			    "start_time_each_day");
			break;
		case ATLV_ENDTIMEEACHDAY:
			err = sched_reltime_set(&sched.end_time_each_day,
			    atlv, ref, sched.is_utc);
			if (err) {
				printf("TLV end time each day - err");
				break;
			}
			sched_reltime_print(&sched.end_time_each_day,
			    "end_time_each_day");
			break;
		case ATLV_DURATION:
			err = tlv_u32_get(&sched.duration, atlv);
			printf("TLV duration %lu %s\n",
			    sched.duration, err ? "err" : "");
			break;
		case ATLV_INTERVAL:
			err = tlv_u32_get(&sched.interval, atlv);
			printf("TLV interval %lu %s\n",
			    sched.interval, err ? "err" : "");
			break;
		case ATLV_ATEND:
			printf("TLV at end\n");
			break;
		case ATLV_ATSTART:
			printf("TLV at start\n");
			break;
		case ATLV_INRANGE:
			printf("TLV in range\n");
			break;
		case ATLV_SETPROP:
			printf("TLV set prop\n");
			break;
		default:
			printf("warning: unknown sched TLV = %x", atlv->type);
			return 0;
		}
		if (err) {
			printf("warning: sched value err");
			return 0;
		}
		consumed += atlv->len + sizeof(struct ayla_tlv);
	}
	return 0;
}
#endif /* SCHED_TEST */
