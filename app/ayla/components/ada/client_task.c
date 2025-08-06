/*
 * Copyright 2014-2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/callback.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/timer.h>
#include <ayla/http.h>
#include <ayla/tlv.h>
#include <ayla/conf.h>
#include <ayla/nameval.h>

#include <al/al_ada_thread.h>
#include <al/al_os_lock.h>
#include <al/al_os_thread.h>
#include <al/al_clock.h>

#include <ada/libada.h>
#include <ada/client.h>
#include <ada/client_ota.h>
#include <ada/server_req.h>
#include <ada/log_page.h>
#include <ada/task_label.h>

#include "client_req.h"
#include "client_int.h"
#include "client_lock.h"
#include "client_timer.h"
#include "metrics_int.h"

#define ADA_THREAD_NAME	"ADA"
#define CLIENT_PDA_MODEL "AY008PWB1"	/* Portable Agent model */

static struct al_thread *client_task;

static struct callback_queue client_task_queue;
static struct timer_head client_timers;
static struct timer client_watchdog_timer;
static struct al_lock *client_mutex;
static struct al_lock *client_callback_lock;

static const char *client_mutex_func USED;
static int client_mutex_line USED;
static void *client_mutex_owner USED;
u8 client_locked;			/* for debug only */
static struct callback client_timer_callback;

void client_lock_stamp(const char *func, int line)
{
	ASSERT(client_locked);
	client_mutex_func = func;
	client_mutex_line = line;
}

void client_lock_int(const char *func, int line)
{
	void *thread = al_os_thread_self();

	ASSERT(client_mutex_owner != thread);

	al_os_lock_lock(client_mutex);

	ASSERT(!client_locked);
	ASSERT(client_mutex_owner == NULL);
	client_mutex_owner = thread;
	client_locked = 1;
	client_mutex_func = func;
	client_mutex_line = line;
}

void client_unlock_int(const char *func, int line)
{
	void *thread = al_os_thread_self();

	client_mutex_func = func;
	client_mutex_line = line;
	ASSERT(client_mutex_owner == thread);
	client_mutex_owner = NULL;
	client_locked = 0;

	al_os_lock_unlock(client_mutex);
}

static void client_timer_cb(void *arg)
{
}

void client_timer_set(struct timer *timer, unsigned long ms)
{
	ASSERT(client_locked);
	timer_set(&client_timers, timer, ms);
	client_callback_pend(&client_timer_callback);	/* wakeup client task */
}

void client_timer_cancel(struct timer *timer)
{
	ASSERT(client_locked);
	timer_cancel(&client_timers, timer);
}

static void client_watchdog_timeout(struct timer *timer)
{
	if (!ada_client_health_check()) {
		al_ada_watchdog_timer_reset();
	}
	client_timer_set(timer, AL_ADA_WATCHDOG_TIMER);
}

/*
 * Client thread main idle loop.
 */
static void client_idle(struct al_thread *thread, void *arg)
{
	struct callback *ac;
	int max_wait;
	int rc;

	al_ada_thread_init(thread);
	client_task = thread;
	log_thread_id_set(TASK_LABEL_CLIENT);
#ifdef AYLA_LOG_SNAPSHOTS
	log_snap_status();
#endif
	if (!conf_sys_model[0]) {
		rc = snprintf(conf_sys_model, CONF_MODEL_MAX, CLIENT_PDA_MODEL);
		ASSERT(rc < CONF_MODEL_MAX);
	}

	client_lock();
	timer_handler_init(&client_watchdog_timer, client_watchdog_timeout);
	client_timer_set(&client_watchdog_timer, AL_ADA_WATCHDOG_TIMER);

	while (!al_os_thread_get_exit_flag(thread)) {
		max_wait = timer_advance(&client_timers);
		log_buf_new_event();
		while (1) {
			al_os_lock_lock(client_callback_lock);
			ac = callback_dequeue(&client_task_queue);
			al_os_lock_unlock(client_callback_lock);
			if (!ac) {
				break;
			}
			ASSERT(ac->pending);
			ac->pending = 0;
			if (ac->impl_priv) {
				client_unlock();
				ac->func(ac->arg);
				client_lock();
			} else {
				ac->func(ac->arg);
			}
			max_wait = 0;
		}
		client_unlock();
		al_ada_poll(max_wait);
		client_lock();
	}
#ifndef AYLA_ADA_MAIN_THREAD_SUPPORT
	ASSERT_NOTREACHED();
#endif
}

/*
 * Pend a callback for the client thread to handle.
 * This may be called from any thread, and doesn't need to hold client_lock.
 */
void client_callback_pend(struct callback *cb)
{
	al_os_lock_lock(client_callback_lock);
	callback_enqueue(&client_task_queue, cb);
	al_os_lock_unlock(client_callback_lock);
	al_ada_wakeup();
}

/*
 * Pend a callback for the client thread to handle.
 * This may be called from any thread, and doesn't need to hold client_lock.
 * The client lock will not be held when the handler function is called.
 */
void ada_callback_pend(struct callback *cb)
{
	if (!cb || cb->pending) {
		return;
	}
	cb->impl_priv = 1;
	client_callback_pend(cb);
}

#ifdef AYLA_ADA_MAIN_THREAD_SUPPORT
/*
 * Start the client task loop.
 * For PDA this is done in the main thread of the app after the host app
 * has initialized everything.
 */
void ada_main_loop(void)
{
	client_idle(al_os_thread_self(), NULL);
}
#endif

int ada_init(void)
{
	log_mask_init_min((enum log_mask)BIT(LOG_SEV_INFO), LOG_DEFAULT);
	log_thread_id_set(TASK_LABEL_MAIN);	/* calling from main thread */
	log_buf_init();

	callback_init(&client_timer_callback, client_timer_cb, NULL);

	client_mutex = al_os_lock_create();
	if (client_mutex == NULL) {
		printf("lock creation failed\n");
	}
	ASSERT(client_mutex);
	client_callback_lock = al_os_lock_create();
	ASSERT(client_callback_lock);

	al_clock_init();
	client_init();
	ada_conf_load();
	oem_conf_load();
	client_conf_load();
	log_conf_load();
	log_page_mods_init();
	log_page_get_init();
	log_page_snaps_init();
#ifdef AYLA_METRICS_SUPPORT
	metrics_conf_load();
#endif
	conf_commit();

	if (conf_setup_mode) {
		log_info(LOG_EXPECTED "setup mode");
	}

#ifndef AYLA_ADA_MAIN_THREAD_SUPPORT
	client_task = al_os_thread_create(ADA_THREAD_NAME, NULL,
	    PFM_STACKSIZE_ADA, al_os_thread_pri_normal, client_idle, NULL);
	ASSERT(client_task);
#endif

#ifdef AYLA_METRICS_SUPPORT
	client_metrics_init();
#endif
	return 0;
}
