/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <ayla/conf.h>
#include <platform/pfm_ada_thread.h>

#include "wise_task_wdt.h"

#define PFM_ADA_MODEL	"AY008ESP1"		/* Integrated Agent model */

#define PFM_TASK_WATCHDOG_TIMEOUT		15 /* 15s */

/*
 * Initialize task watchdog for the ADA thread.
 * The task watchdog must be previously enabled by sdkconfig or by the
 * host application.
 */
static void pfm_ada_task_watchdog_init(void)
{
	wise_task_wdt_init(PFM_TASK_WATCHDOG_TIMEOUT);

	wise_task_wdt_add(NULL);
}

void al_ada_watchdog_timer_reset(void)
{
	wise_task_wdt_reset(NULL);
}

void pfm_ada_thread_init_pfm(void)
{
	int rc;

	pfm_ada_task_watchdog_init();

	/*
	 * Set model as Integrated Agent, if not already set by the host app.
	 */
	if (!conf_sys_model[0]) {
		rc = snprintf(conf_sys_model, CONF_MODEL_MAX, PFM_ADA_MODEL);
		ASSERT(rc < CONF_MODEL_MAX);
	}
}
