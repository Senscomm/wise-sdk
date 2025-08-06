/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/timer.h>
#include <ada/err.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include <platform/pfm_ada_thread.h>
#include <al/al_log.h>
#include <al/al_persist.h>
#include "pfm_log_core.h"

/*
 * State of log summary.
 */
#define LOG_CORE_STATE_CONF "sys/error/status"	/* config item */
#define LOG_CORE_START		1	/* starting summary */

#define LOG_CORE_TIMEOUT	15000	/* interval between health checks, ms */

static struct timer log_core_sum_timer;
static u32 log_core_sum_state;
static u32 log_core_sum_hash;

static int log_core_sum_state_save(void)
{
	if (al_persist_data_write(AL_PERSIST_FACTORY, LOG_CORE_STATE_CONF,
	    &log_core_sum_state, sizeof(log_core_sum_state))) {
		log_put(LOG_ERR "log_core_sum: save error");
		return -1;
	}
	return 0;
}

/*
 * Check if it is time to summarize crashdump.
 *
 * Wait until connectivity is up and a command has been fetched to
 * reduce risk of a crash loop which might occur due to bugs in the
 * core dump summary code.
 */
static void log_core_sum_timeout(struct timer *timer)
{
	int rc;

	if (ada_client_health_check()) {
		pfm_timer_set(timer, LOG_CORE_TIMEOUT);
		return;
	}
	rc = pfm_log_core_hash(&log_core_sum_hash);
	if (rc) {
		if (rc < 0) {
			log_put(LOG_ERR "core: error during hash");
		}
		return;
	}
	if (log_core_sum_hash == log_core_sum_state) {
		log_put(LOG_DEBUG "core: already summarized");
		return;
	}
	log_core_sum_state = LOG_CORE_START;
	if (log_core_sum_state_save()) {
		return;
	}
	log_put(LOG_INFO "core: summarizing earlier coredump for log-snap");
	al_log_core_dump_info();
	log_save();
	log_core_sum_state = log_core_sum_hash;
	log_core_sum_state_save();
}

/*
 * Event when client status changes.
 * Called with client_lock held.
 */
static void log_core_sum_event(void *arg, enum ada_err status)
{
	static u8 done;

	if (status != AE_OK || done) {
		return;
	}
	done = 1;
	timer_handler_init(&log_core_sum_timer, log_core_sum_timeout);
	pfm_timer_set(&log_core_sum_timer, LOG_CORE_TIMEOUT);
}

/*
 * Initialize log core subsystem.
 */
void pfm_log_core_summarize(void)
{
	u32 state;

	if (!pfm_log_core_is_present()) {
		return;
	}
	if (al_persist_data_read(AL_PERSIST_FACTORY, LOG_CORE_STATE_CONF,
	    &state, sizeof(state)) != sizeof(state)) {
		state = 0;
	}
	if (state == LOG_CORE_START) {
		return;			/* previous core save failed */
	}
	log_core_sum_state = state;
	ada_client_event_register(log_core_sum_event, NULL);
}
