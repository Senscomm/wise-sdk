/*
 * Copyright 2013 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_SCHED_H__
#define __AYLA_SCHED_H__

#define SCHED_NAME_LEN	28		/* max name length including NUL */

#ifdef AYLA_SCHED_TLV_LEN
#define SCHED_TLV_LEN	AYLA_SCHED_TLV_LEN /* max length of schedule value */
#else
#define SCHED_TLV_LEN	255		/* max length of schedule value */
#endif /* SCHED_TLV_LEN */

/*
 * Macro to make logging easier
 */
#define SCHED_LOGF(_level, _format, ...) \
	sched_log(_level "%s: " _format, __func__, ##__VA_ARGS__)

/*
 * Evaluate a schedule.
 *
 * Arguments are a schedule tlv bundle pointer and a length,
 * a pointer to UTC time, and an action handler function pointer.
 *
 * The supplied action handler is called for any actions that should
 * take place at that time.
 *
 * Then it determines when the next action should take place and sets the
 * time pointed to by timep to the time of the next action.
 * If there is no next action, the value will be set to MAX_U32.
 *
 * Returns AE_INVAL_VAL if the schedule is invalid or disabled.
 * Returns AE_INVAL_STATE if there is not enough data
 * to evaluate the schedule (e.g., time zone information and/or solar table).
 */
enum ada_err ada_sched_eval(const struct ayla_tlv *atlv_in, size_t len,
	u32 *timep, void (*action_handler)(const struct ayla_tlv *tlv));

/*
 * Run through all schedules. Fire events as time progresses
 * to current utc time. Determine the next future event and
 * setup a timer to re-run at that time.
 */
void sched_run_all(void);

#ifdef SCHED_TEST
/*
 * Handle a set_prop action inside a schedule
 */
int sched_prop_set(const char *name, const void *val_ptr,
		enum ayla_tlv_type type, u8 src);
#else
/*
 * Handle a schedule property from service using the module library.
 */
int sched_prop_set(const char *name, const void *val_ptr, size_t val_len);
#endif /* SCHED_TEST */

/*
 * Logging for sched
 */
void sched_log(const char *fmt, ...);

/*
 * Initialize schedules and allocate space.
 */
enum ada_err ada_sched_init(unsigned int count);

/*
 * Initialize schedules with dynamic naming.
 */
enum ada_err ada_sched_dynamic_init(unsigned int count);

/*
 * Turn on schedule handling.
 */
enum ada_err ada_sched_enable(void);

/*
 * Set name of schedule.
 * The passed-in name is not referenced after this function returns.
 */
enum ada_err ada_sched_set_name(unsigned int index, const char *name);

/*
 * Get name and value for schedule.
 * Fills in the name pointer, the value to be persisted, and its length.
 */
enum ada_err ada_sched_get_index(unsigned int idx, char **name,
				void *tlvs, size_t *tlv_len);

/*
 * Set the value for a schedule by index.
 * This sets the value of the schedule, e.g., after reloaded from flash.
 */
enum ada_err ada_sched_set_index(unsigned int idx, const void *buf, size_t len);

/*
 * Set the value for a schedule by name.
 */
enum ada_err ada_sched_set(const char *name, const void *buf, size_t len);

/*
 * Persist schedule values as required.
 * Supplied by platform.
 * The schedule values can be fetched with ada_sched_get_index();
 */
void adap_sched_conf_persist(void);

/*
 * Save last run time to NVRAM.
 * Supplied by platform.
 * This allows the schedules to be more efficient if power has not been lost.
 * Use RAM if no NVRAM is provided.
 */
void adap_sched_run_time_write(u32);

/*
 * Read last run time for schedules from NVRAM.
 * Supplied by platform.
 * Use RAM if no NVRAM is provided.  Return 0 if the value hasn't been saved.
 */
u32 adap_sched_run_time_read(void);

/*
 * CLI Interfaced for sched
 */
void sched_cli(int argc, char **argv);

/*
 * Reads the schedule action and fires it.
 */
void sched_set_prop(const struct ayla_tlv *atlv, u8 tot_len);

/*
 * Get the number of seconds until the next scheduled event
 */
enum ada_err ada_sched_get_next_event(u32 *timep, const char **namep);

/*
 * Register a handler to be called whenever the time of the next scheduled
 * event changes, passing the number of seconds until the next event. The
 * handler will be called whenever:
 *
 * - the schedule configuration changes, or
 * - a schedule runs and the next event time changes
 */
enum ada_err ada_sched_event_register(void (*fn)(u32, const char *, void *),
    void *arg);

/*
 * Remove a schedule event time handler
 */
enum ada_err ada_sched_event_unregister(
    void (*fn)(u32, const char *, void *));

#ifdef SERVER_DEV_PAGES
/*
 * Run through sched1 and generate a debug string to send to service.
 * See Google Doc on Schedule Property Testing.
 */
void sched_generate_debug(long start_time);

/*
 * Send the debug string to service.
 */
void sched_send_debug(void);

#endif /* SERVER_DEV_PAGES */

#endif /* __AYLA_SCHED_H__ */
