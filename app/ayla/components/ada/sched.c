/*
 * Copyright 2013-2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 * Schedule analyzer and interpreter.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/tlv.h>
#include <ayla/tlv_access.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/endian.h>
#include <ayla/conf.h>
#include <ayla/callback.h>
#include <ayla/clock.h>
#include <ayla/timer.h>
#include <al/al_os_mem.h>
#include <ada/err.h>
#include <ada/prop.h>
#include <ada/ada_conf.h>
#include <ada/sched.h>
#include <ada/prop_mgr.h>
#include "client_timer.h"
#include "client_lock.h"
#include "schedeval.h"

#define SCHED_CONF_VALUE "sched/n/%u/value"	/* conf path for saving TLVs */
#define SCHED_CONF_NAME "sched/n/%u/prop"	/* conf path for saving name */
#define SCHED_CONF_NAME_LEN	20

/*
 * Action to be performed after all schedules evaluated up to the current time.
 * This element is used only during the call to sched_evaluate.
 */
struct sched_action {
	const struct ayla_tlv *atlv;	/* TLV containing prop and value */
};

struct sched_event_handler {
	void (*handler)(u32 next_event, const char *next_sched, void *arg);
	void *arg;
	u32 last_utc;
	struct sched_event_handler *next;
};

struct sched_event_callback {
	struct callback cb;
	void (*handler)(u32 next_event, const char *next_sched, void *arg);
	void *arg;
	u32 next_utc;
	const char *next_sched;
};

/*
 * State for schedule subsystem.
 */
struct sched_state {
	struct sched_prop *mod_scheds;
	u32 nsched;          /* length of mod_scheds array */
	struct sched_action *actions;
	u32 action_table_ct; /* number of actions in table */
	u32 action_pend_ct;  /* number of actions pending */
	u32 action_en_ct;    /* number of actions in enabled schedules */
	u32 run_time;	     /* virtual run time */
	u8 next_fire_sched;  /* index of schedule we expect to fire */
	u8 save_needed:1;    /* persist schedule values */
	u8 enabled:1;
	u8 dynamic:1;		/* schedules are assigned names dynamically */
	struct timer timer;
	struct sched_event_handler *event_head;
};

static struct sched_state sched_state;

static void sched_timeout(struct timer *);
static enum conf_error sched_conf_get(int src,
		enum conf_token *token, size_t len);
static enum conf_error sched_conf_set(int src,
		enum conf_token *token, size_t len, struct ayla_tlv *tlv);
static void sched_conf_export(void);
static void sched_conf_commit(int from_ui);

static const struct conf_entry sched_conf_entry = {
	.token = CT_sched,
	.get = sched_conf_get,
	.set = sched_conf_set,
	.export = sched_conf_export,
	.commit = sched_conf_commit,
};

/*
 * Logging for sched
 */
void sched_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_SCHED, fmt, args);
	ADA_VA_END(args);
}

/*
 * Call function for each allocated sched structure, including empty ones.
 * End the loop and return if the function returns non-NULL.
 */
static void *sched_iterate(void *(*func)(struct sched_prop *sched, void *arg),
		void *arg)
{
	struct sched_state *st = &sched_state;
	struct sched_prop *sched;
	unsigned int i;
	void *rv;

	sched = st->mod_scheds;
	if (!sched) {
		return 0;
	}
	for (i = 0; i < st->nsched; i++, sched++) {
		rv = func(sched, arg);
		if (rv) {
			return rv;
		}
	}
	return 0;
}

static struct sched_prop *sched_prop_by_index(unsigned int index)
{
	struct sched_state *st = &sched_state;

	if (index < st->nsched) {
		return &st->mod_scheds[index];
	}
	return NULL;
}

static void *sched_prop_match(struct sched_prop *sched, void *name_to_match)
{
	if (!strcmp((char *)name_to_match, sched->name)) {
		return sched;
	}
	return NULL;
}

static struct sched_prop *sched_prop_lookup(const char *name)
{
	return sched_iterate(sched_prop_match, (void *)name);
}

/*
 * Allocate schedule entry, which is already known to not exist.
 */
static struct sched_prop *sched_prop_create(const char *name)
{
	struct sched_prop *sched;
	size_t name_len;

	sched_log(LOG_DEBUG "%s: creating sched %s", __func__, name);
	sched = sched_prop_lookup("");	/* find empty slot */
	if (!sched) {
		sched_log(LOG_WARN "%s: no empty slot for %s", __func__, name);
		return NULL;
	}
	name_len = strlen(name) + 1;
	if (name_len > sizeof(sched->name)) {
		sched_log(LOG_ERR "%s: name too long: \"%s\"", __func__, name);
		return NULL;
	}
	memcpy(sched->name, name, name_len);
	sched->len = 0;
	return sched;
}

/*
 * Count the number of actions in a schedule.
 */
static u32 sched_action_count(const u8 *tlvs, size_t len)
{
	const struct ayla_tlv *tlv = (const struct ayla_tlv *)tlvs;
	int count = 0;

	while (len) {
		if (len < sizeof(*tlv) || len < sizeof(*tlv) + tlv->len) {
			return count;
		}
		switch (tlv->type) {
		case ATLV_SETPROP:
			count++;
			break;
		case ATLV_DISABLE:
			return 0;
		default:
			break;
		}
		len -= sizeof(*tlv) + tlv->len;
		tlv = (struct ayla_tlv *)((char *)(tlv + 1) + tlv->len);
	}
	return count;
}

/*
 * Reset all pending actions.
 */
static void sched_actions_reset(struct sched_state *st)
{
	struct sched_action *new;

	st->action_pend_ct = 0;
	if (st->action_table_ct < st->action_en_ct) {
		new = al_os_mem_alloc(sizeof(*new) * st->action_en_ct);
		if (!new) {
			SCHED_LOGF(LOG_ERR, "%s: alloc failed");
			return;
		}
		al_os_mem_free(st->actions);
		st->actions = new;
		st->action_table_ct = st->action_en_ct;
	}
	if (st->actions) {
		memset(st->actions, 0,
		    sizeof(*st->actions) * st->action_table_ct);
	}
}

/*
 * Pend an action.
 * If already pending, move it to the end, so the actions are chronological.
 */
void sched_action_pend(const struct ayla_tlv *atlv)
{
	struct sched_state *st = &sched_state;
	const struct ayla_tlv *name;
	const struct ayla_tlv *new_name;
	struct sched_action *action;
	unsigned int i;

	new_name = TLV_VAL(atlv);
	if (new_name->type != ATLV_NAME) {
		sched_log(LOG_ERR "action with no name TLV");
		return;
	}

	/*
	 * Look for action already pending with same prop name.
	 */
	for (i = 0; i < st->action_pend_ct; i++) {
		action = &st->actions[i];
		name = TLV_VAL(action->atlv);
		if (name->len == new_name->len &&
		    !memcmp(TLV_VAL(name), TLV_VAL(new_name),
		    name->len)) {
			memmove(action, action + 1,
			    (st->action_pend_ct - (i + 1)) *
			    sizeof(*st->actions));
			i = st->action_pend_ct - 1;
			break;
		}
	}
	if (i < st->action_table_ct) {
		sched_log(LOG_DEBUG2 "action pend %u", i);
		st->actions[i].atlv = atlv;
		st->action_pend_ct = i + 1;
	}
}

/*
 * Act on all pending actions and then clear them.
 */
static void sched_actions_run(struct sched_state *st)
{
	const struct ayla_tlv *atlv;
	int i;

	for (i = 0; i < st->action_pend_ct; i++) {
		sched_log(LOG_DEBUG2 "action run %u", i);
		atlv = st->actions[i].atlv;
		sched_set_prop(atlv + 1, atlv->len);
	}
	st->action_pend_ct = 0;
}

/*
 * Set schedule from configuration.
 */
static enum ada_err sched_set(struct sched_prop *sched,
				const void *tlvs, size_t len)
{
	struct sched_state *st = &sched_state;
	u32 actions;

	if (len > sizeof(sched->tlvs)) {
		sched_log(LOG_ERR "%s(): schedule %s val_len(%u) > %u",
		    __func__, sched->name, len, sizeof(sched->tlvs));
		return AE_LEN;
	}

	/*
	 * Subtract the actions in the old schedule.
	 * Then add the actions in the new schedule.
	 */
	actions = sched_action_count(sched->tlvs, sched->len);
	ASSERT(actions <= st->action_en_ct);
	st->action_en_ct -= actions;

	actions = sched_action_count(tlvs, len);
	if (actions) {
		memcpy(sched->tlvs, tlvs, len);
		st->action_en_ct += actions;
		sched->len = len;
		sched->updated = 1;
	} else {
		if (st->dynamic) {
			sched->name[0] = '\0';
		}
		sched->len = 0;
	}
	return AE_OK;
}

/*
 * Handle a schedule property from service using the module library.
 */
static enum ada_err sched_prop_set_int(const char *name, const void *val_ptr,
				size_t val_len)
{
	struct sched_state *st = &sched_state;
	struct sched_prop *sched;
	enum ada_err err;

	sched_log(LOG_DEBUG "%s: setting sched %s len %zu",
	    __func__, name, val_len);
	sched = sched_prop_lookup(name);
	if (!sched) {
		if (!st->dynamic) {
			sched_log(LOG_DEBUG "%s: name not found: %s",
			    __func__, name);
			return AE_NOT_FOUND;
		}
		if (!val_len) {
			sched_log(LOG_ERR "%s: empty value: %s",
			    __func__, name);
			return AE_INVAL_VAL;
		}
		sched = sched_prop_create(name);
		if (!sched) {
			return AE_ALLOC;
		}
	}
	err = sched_set(sched, val_ptr, val_len);
	sched_log(LOG_DEBUG "%s: set sched %s len %u rc %d",
	    __func__, name, sched->len, err);	/* XXX */
	return err;
}

/*
 * Handle a schedule property from service using the module library.
 */
int sched_prop_set(const char *name, const void *val_ptr, size_t val_len)
{
	if (sched_prop_set_int(name, val_ptr, val_len)) {
		return -1;
	}
	return 0;
}

/*
 * Set a timeout to re-evaluate all schedules after one is set
 * or after a possible time change.
 */
static void sched_time_update(void)
{
	struct sched_state *st = &sched_state;

	if (!st->nsched) {
		return;		/* not yet initialized */
	}
	client_timer_set(&st->timer, 0);
}

static enum ada_err sched_prop_recv(const char *name, enum ayla_tlv_type type,
			const void *val, size_t val_len, size_t *off,
			u8 src, void *req_arg)
{
	struct sched_state *st = &sched_state;
	enum ada_err err;

	if (type != ATLV_SCHED) {
		return AE_NOT_FOUND;
	}

	err = sched_prop_set_int(name, val, val_len);
	if (err) {
		return err;
	}
	st->save_needed = 1;
	sched_time_update();
	return AE_OK;
}

static void sched_prop_send_done(enum prop_cb_status status,
			u8 fail_mask, void *req_arg)
{
}

static enum ada_err sched_prop_get_val(const char *name,
		enum ada_err (*send_cb)(struct prop *, void *arg, enum ada_err),
		void *arg)
{
	if (!sched_prop_lookup(name)) {
		return AE_NOT_FOUND;
	}
	return AE_INVAL_VAL;
}

/*
 * At start of GET of all schedules, clear the updated flag.
 */
static void *sched_prop_get_start(struct sched_prop *sched, void *arg)
{
	sched->updated = 0;
	return NULL;
}

/*
 * At end of GET of all schedules, delete a schedule if it wasn't updated.
 */
static void *sched_prop_get_end(struct sched_prop *sched, void *arg)
{
	struct sched_state *st = &sched_state;

	if (sched->updated || !sched->name[0] || !sched->len) {
		return NULL;
	}
	sched_log(LOG_INFO "deleting old schedule \"%s\"", sched->name);
	sched->name[0] = '\0';
	sched->len = 0;
	st->save_needed = 1;
	return NULL;
}

static void sched_prop_event(enum prop_mgr_event event, const void *arg)
{
	struct sched_state *st = &sched_state;

	switch (event) {
	case PME_TIME:
		sched_time_update();
		break;
	case PME_GET_INPUT_START:
		sched_log(LOG_DEBUG2 "get input start");
		sched_iterate(sched_prop_get_start, NULL);
		break;
	case PME_GET_INPUT_END:
		sched_log(LOG_DEBUG2 "get input end");
		sched_iterate(sched_prop_get_end, NULL);
		if (st->save_needed) {
			sched_time_update();
		}
		break;
	default:
		break;
	}
}

static const struct prop_mgr sched_prop_mgr = {
	.name = "sched",
	.prop_recv = sched_prop_recv,
	.send_done = sched_prop_send_done,
	.get_val = sched_prop_get_val,
	.event = sched_prop_event,
};

enum ada_err ada_sched_enable(void)
{
	struct sched_state *st = &sched_state;

	if (!st->enabled) {
		st->enabled = 1;
		ada_prop_mgr_register(&sched_prop_mgr);
		ada_prop_mgr_ready(&sched_prop_mgr);
		sched_log(LOG_DEBUG "schedules enabled. count %u", st->nsched);
		client_lock();
		sched_time_update();
		client_unlock();
	}
	return AE_OK;
}

static void sched_conf_load(void)
{
	struct sched_state *st = &sched_state;
	struct sched_prop *sched;
	unsigned int i;
	char name[SCHED_CONF_NAME_LEN];
	int len;
	u8 tlvs[SCHED_TLV_LEN];
	ssize_t rc;

	sched = st->mod_scheds;
	for (i = 0; i < st->nsched; i++, sched++) {
		len = snprintf(name, sizeof(name), SCHED_CONF_NAME, i);
		ASSERT(len < sizeof(name));
		rc = conf_persist_get(name, sched->name, sizeof(sched->name));
		if (rc < 0 && rc != AE_NOT_FOUND) {
			sched_log(LOG_ERR "%s: get name %u failed err %d",
			    __func__, i, rc);
		}

		len = snprintf(name, sizeof(name), SCHED_CONF_VALUE, i);
		ASSERT(len < sizeof(name));
		rc = conf_persist_get(name, tlvs, sizeof(tlvs));
		if (rc < 0) {
			if (rc != AE_NOT_FOUND) {
				sched_log(LOG_ERR
				    "%s: get val %u failed err %d",
				    __func__, i, rc);
			}
			rc = 0;
		}
		sched_set(sched, tlvs, rc);
	}
}

/*
 * Initialize and allocate storage for schedules.
 */
enum ada_err ada_sched_init(unsigned int count)
{
	struct sched_state *st = &sched_state;

	if (st->mod_scheds) {
		return AE_BUSY;
	}
	st->mod_scheds = al_os_mem_calloc(count * sizeof(*st->mod_scheds));
	if (!st->mod_scheds) {
		return AE_ALLOC;
	}
	st->nsched = count;
	timer_handler_init(&st->timer, sched_timeout);
	sched_conf_load();
	conf_table_entry_add(&sched_conf_entry);
	return AE_OK;
}

/*
 * Enable dynamic allocation and ADA handling of schedule persistence.
 */
enum ada_err ada_sched_dynamic_init(unsigned int count)
{
	struct sched_state *st = &sched_state;
	enum ada_err err;

	err = ada_sched_init(count);
	if (err) {
		return err;
	}
	st->dynamic = 1;
	return AE_OK;
}

/*
 * Set name for schedule.
 * The passed-in name is not referenced after this function returns.
 */
enum ada_err ada_sched_set_name(unsigned int index, const char *name)
{
	struct sched_state *st = &sched_state;
	struct sched_prop *sched;

	if (index >= st->nsched) {
		return AE_INVAL_STATE;
	}
	sched = &st->mod_scheds[index];
	strncpy(sched->name, name, sizeof(sched->name) - 1);
	return AE_OK;
}

/*
 * Get name and value for schedule.
 * Fills in the name pointer, the value to be persisted, and its length.
 * The value will be given as zero-length if the schedule is disabled or
 * if there are no actions.
 */
enum ada_err ada_sched_get_index(unsigned int index, char **name,
				void *tlvs, size_t *lenp)
{
	struct sched_state *st = &sched_state;
	struct sched_prop *sched;

	if (index >= st->nsched) {
		return AE_INVAL_STATE;
	}
	sched = st->mod_scheds;
	if (!sched) {
		return AE_NOT_FOUND;
	}
	sched += index;
	*name = sched->name;
	if (*lenp < sched->len) {
		return AE_LEN;
	}
	if (!sched_action_count(sched->tlvs, sched->len)) {
		*lenp = 0;
		return AE_OK;
	}
	memcpy(tlvs, sched->tlvs, sched->len);
	*lenp = sched->len;
	return AE_OK;
}

/*
 * Set the value for a schedule by index.
 * This sets the value of the schedule, e.g., after reloaded from flash.
 */
enum ada_err ada_sched_set_index(unsigned int index,
				const void *tlvs, size_t len)
{
	struct sched_state *st = &sched_state;
	struct sched_prop *sched;

	if (index >= st->nsched) {
		return AE_NOT_FOUND;
	}
	sched = &st->mod_scheds[index];
	return sched_set(sched, tlvs, len);
}

enum ada_err ada_sched_set(const char *name, const void *tlvs, size_t len)
{
	struct sched_prop *sched;

	sched = sched_prop_lookup(name);
	if (!sched) {
		return AE_NOT_FOUND;
	}
	return sched_set(sched, tlvs, len);
}

static enum conf_error sched_conf_get(int src,
		enum conf_token *token, size_t len)
{
	u8 tlvs[SCHED_TLV_LEN];
	size_t tlvs_len;
	char *name;
	enum ada_err err;

	switch (token[0]) {
	case CT_n:
		if (len != 3) {
			return CONF_ERR_PATH;
		}
		tlvs_len = sizeof(tlvs);
		err = ada_sched_get_index(token[1], &name, tlvs, &tlvs_len);
		if (err) {
			return CONF_ERR_PATH;
		}
		switch (token[2]) {
		case CT_prop:
			conf_resp_str(name);
			break;
		case CT_value:
			conf_resp(ATLV_SCHED, tlvs, tlvs_len);
			break;
		default:
			return CONF_ERR_PATH;
		}
		break;
	default:
		return CONF_ERR_PATH;
	}
	return CONF_ERR_NONE;
}

static enum conf_error sched_conf_set(int src,
		enum conf_token *token, size_t len, struct ayla_tlv *tlv)
{
	struct sched_prop *sched;
	u8 tlvs[SCHED_TLV_LEN];
	size_t tlvs_len;

	switch (token[0]) {
	case CT_n:
		if (len != 3) {
			return CONF_ERR_PATH;
		}
		sched = sched_prop_by_index(token[1]);
		if (!sched) {
			return CONF_ERR_PATH;
		}
		switch (token[2]) {
		case CT_prop:
			sched->name[0] = '\0';
			conf_get(tlv, ATLV_UTF8,
			    sched->name, sizeof(sched->name) - 1);
			break;
		case CT_value:
			tlvs_len = conf_get(tlv, ATLV_SCHED,
			    tlvs, sizeof(tlvs));
			sched_set(sched, tlvs, tlvs_len);
			break;
		default:
			return CONF_ERR_PATH;
		}
		break;
	default:
		return CONF_ERR_PATH;
	}
	return CONF_ERR_NONE;
}

/*
 * Save the schedule info to flash. All names and values.
 */
static void sched_conf_save(void *arg)
{
	struct sched_state *st = &sched_state;
	unsigned int i;
	u8 tlvs[SCHED_TLV_LEN];
	size_t tlvs_len;
	char *name;

	conf_cd(CT_n);
	for (i = 0; i < st->nsched; i++) {
		conf_cd_table(i);
		tlvs_len = sizeof(tlvs);
		if (ada_sched_get_index(i, &name, tlvs, &tlvs_len) ||
		    !tlvs_len) {
			conf_delete(CT_prop);
			conf_delete(CT_value);
		} else {
			conf_put_str(CT_prop, name);
			conf_put(CT_value, ATLV_SCHED, tlvs, tlvs_len);
		}
		conf_cd_parent();
	}
	conf_cd_parent();
}

static void sched_conf_export(void)
{
	sched_conf_save(NULL);
}

static void sched_conf_commit(int from_ui)
{
	client_lock();
	sched_time_update();
	client_unlock();
}

/*
 * Reads the schedule action and fires it.
 */
void sched_set_prop(const struct ayla_tlv *atlv, u8 tot_len)
{
	struct prop_recvd recvd;
	const struct ayla_tlv *prop;
	enum ayla_tlv_type type;
	union {
		s32 v;
		char s[SCHED_TLV_LEN + 1];
	} val;
	int cur_len;
	int ret;

	memset(&recvd, 0, sizeof(recvd));
	prop = atlv;
	if (prop->type != ATLV_NAME) {
		SCHED_LOGF(LOG_WARN, "missing name");
		return;
	}
	if (prop->len >= sizeof(recvd.name)) {
		SCHED_LOGF(LOG_WARN, "invalid prop name");
		return;
	}
	memcpy(recvd.name, prop + 1, prop->len);
	recvd.name[prop->len] = '\0';
	cur_len = prop->len;

	prop = (struct ayla_tlv *)((u8 *)prop + prop->len +
	    sizeof(struct ayla_tlv));
	type = (enum ayla_tlv_type)prop->type;
	if (type != ATLV_INT && type != ATLV_UINT && type != ATLV_BOOL
	    && type != ATLV_CENTS && type != ATLV_UTF8) {
		SCHED_LOGF(LOG_WARN, "(name=%s, type=%d, len=%d) type "
		    "not supported", recvd.name, type, prop->len);
		return;
	}
	if (cur_len + prop->len != tot_len - 2 * sizeof(struct ayla_tlv)) {
		SCHED_LOGF(LOG_WARN, "(name=%s, type=%d, len=%d) length error",
		    recvd.name, type, prop->len);
		return;
	}
	if (type == ATLV_UTF8) {
		if (prop->len >= (sizeof(val.s) - 1)) {
			SCHED_LOGF(LOG_WARN, "(name=%s, type=%d, len=%d) "
			    "string too long", recvd.name, type, prop->len);
		}
		memcpy(val.s, prop + 1, prop->len);
		val.s[prop->len] = '\0';
		ret = 0;
		cur_len = prop->len;
	} else {
		val.v = 0;
		ret = tlv_s32_get(&val.v, prop);
		cur_len = sizeof(val.v);
	}
	if (ret) {
		SCHED_LOGF(LOG_WARN, "(name=%s, type=%d, len=%d) value error",
		    recvd.name, type, prop->len);
		return;
	}
	if (type == ATLV_UTF8) {
		sched_log(LOG_DEBUG "action setting \"%s\" = \"%s\"",
		    recvd.name, val.s);
	} else {
		sched_log(LOG_DEBUG "action setting \"%s\" = %ld",
		    recvd.name, val.v);
	}
	ada_prop_mgr_set(recvd.name, type, &val, cur_len,
	    &recvd.offset, NODES_SCHED, NULL);

	/*
	 * Perhaps this function should echo the datapoint to ADS and LAN.
	 * For now, we rely on each property manager to perform the echo.
	 */
}

static void sched_show_all(void)
{
	struct sched_state *st = &sched_state;
	int i;

	printcli("schedules %sabled", st->nsched ? "en" : "dis");
	for (i = 0; i < st->nsched; i++) {
		printcli("%d : %s", i,
		    st->mod_scheds[i].name);
	}
}

/*
 * CLI Interfaced for sched
 */
void sched_cli(int argc, char **argv)
{
	struct sched_prop *sched;
	unsigned long i;
	char *n;
	char *errptr;

	if (argc == 1) {
		sched_show_all();
		return;
	}
	if (!mfg_or_setup_mode_ok()) {
		return;
	}
	if (argc != 4) {
		goto usage;
	}
	argv++;
	n = *argv++;
	argc -= 2;
	i = strtoul(n, &errptr, 10);
	if (*errptr != '\0') {
		printcli("bad sched # \"%s\"", n);
		return;
	}
	sched = sched_prop_by_index(i);
	if (!sched) {
		printcli("bad sched # \"%s\"", n);
		return;
	}
	if (strcmp(*argv++, "name")) {
		goto usage;
	}
	if (!prop_name_valid(*argv)) {
		printcli("bad sched name");
		return;
	}
	strncpy(sched->name, *argv, sizeof(sched->name) - 1);
	return;

usage:
	printcli("usage:");
	printcli("sched <schedule #> name <schedule name>");
}

static void sched_action_none(const struct ayla_tlv *atlv)
{
	return;
}

enum ada_err ada_sched_get_next_event(u32 *timep, const char **namep)
{
	struct sched_state *st = &sched_state;
	u32 utc_time = clock_utc();
	u32 earliest_event;
	char *earliest_name;
	u8 start;
	int i;
	enum ada_err err;

	client_lock();
	if (!st->enabled || !st->mod_scheds || clock_source() <= CS_DEF) {
		return AE_INVAL_VAL;
	}

	earliest_event = MAX_U32;
	earliest_name = NULL;
	start = st->next_fire_sched;
	i = start;
	do {
		struct sched_prop *schedtlv;
		struct ayla_tlv *tlvs = (struct ayla_tlv *)schedtlv->tlvs;
		u32 next_event;

		ASSERT(i < st->nsched);
		schedtlv = &sched_state.mod_scheds[i];
		if (schedtlv->name[0] == '\0' || !schedtlv->len) {
			goto move_on;
		}

		tlvs = (struct ayla_tlv *)schedtlv->tlvs;
		next_event = utc_time;
		err = ada_sched_eval(tlvs, schedtlv->len, &next_event,
		    sched_action_none);
		if ((err) || next_event == 0 || next_event == MAX_U32) {
			goto move_on;
		}
		if (next_event < earliest_event) {
			earliest_event = next_event;
			earliest_name = schedtlv->name;
		}

move_on:
		i++;
		if (i >= st->nsched) {
			i = 0;
		}
	} while (i != start);
	client_unlock();

	if (!earliest_event || earliest_event == MAX_U32) {
		/* no events left for any of the schedules */
		err = AE_INVAL_VAL;
		goto finish;
	}

	if (earliest_event < utc_time) {
		*timep = 0;
	} else {
		*timep = earliest_event - utc_time;
	}
	*namep = earliest_name;
	err = AE_OK;
finish:
	return err;
}

static void sched_event_cb(void *arg)
{
	struct sched_event_callback *cb;

	cb = (struct sched_event_callback *)arg;
	cb->handler(cb->next_utc, cb->next_sched, cb->arg);
	al_os_mem_free(cb);
}

enum ada_err ada_sched_event_register(void (*fn)(u32, const char *, void *),
    void *arg)
{
	struct sched_state *st = &sched_state;
	struct sched_event_handler *hp;
	struct sched_event_callback *cb;
	const char *next_sched;
	u32 next_utc;

	hp = al_os_mem_calloc(sizeof(*hp));
	if (hp == NULL) {
		return AE_ALLOC;
	}
	hp->arg = arg;
	hp->handler = fn;
	hp->last_utc = MAX_U32;
	client_lock();
	hp->next = st->event_head;
	st->event_head = hp;
	client_unlock();

	if (ada_sched_get_next_event(&next_utc, &next_sched) == AE_OK) {
		cb = al_os_mem_calloc(sizeof(*cb));
		if (cb != NULL) {
			cb->next_sched = next_sched;
			cb->next_utc = next_utc;
			cb->handler = fn;
			cb->arg = arg;
			callback_init(&cb->cb, sched_event_cb, cb);
			client_callback_pend(&cb->cb);
		}
	}

	return AE_OK;
}

enum ada_err ada_sched_event_unregister(
    void (*fn)(u32, const char *, void *))
{
	struct sched_state *st = &sched_state;
	struct sched_event_handler *hp;
	struct sched_event_handler **hpp;

	client_lock();
	hpp = &(st->event_head);
	while ((*hpp) && (*hpp)->handler != fn) {
		hpp = &((*hpp)->next);
	}
	if (*hpp) {
		hp = (*hpp);
		*hpp = hp->next;
		al_os_mem_free(hp);
	}
	client_unlock();
	return AE_OK;
}

static void sched_notify_next_event(u32 next_utc, char *next_sched)
{
	struct sched_state *st = &sched_state;
	struct sched_event_handler *hp;
	u32 utc_time = clock_utc();

	ASSERT(client_locked);

	/*
	 * Set time to max if no events scheduled.
	 */
	if (!next_utc) {
		next_utc = MAX_U32;
	}

	/*
	 * If next event time is in the past, return to allow
	 * the schedule timer to expire again.
	 */
	if (next_utc <= utc_time) {
		return;
	}

	/*
	 * Notify interested parties if the next event time
	 * has changed (for them)
	 */
	hp = st->event_head;
	while (hp) {
		if (next_utc != hp->last_utc) {
			hp->last_utc = next_utc;
			if (next_utc == MAX_U32) {
				(*hp->handler)(next_utc,
				    NULL, hp->arg);
			} else {
				(*hp->handler)((next_utc - utc_time),
				    next_sched, hp->arg);
			}
		}
		hp = hp->next;
	}
}

/*
 * A scheduled event is due to fire.
 */
static void sched_timeout(struct timer *arg)
{
	struct sched_state *st = &sched_state;

	if (st->save_needed) {
		st->save_needed = 0;
		conf_persist(CT_sched, sched_conf_save, NULL);
	}
	sched_run_all();
}

static void sched_debug_time(const char *msg, u32 time)
{
	char buf[40];

	if (log_mod_sev_is_enabled(MOD_LOG_SCHED, LOG_SEV_DEBUG2)) {
		if (time == MAX_U32) {
			snprintf(buf, sizeof(buf), "never");
		} else {
			clock_fmt(buf, sizeof(buf), time);
		}
		sched_log(LOG_DEBUG2 "run: %s %s", msg, buf);
	}
}

/*
 * Run through all schedules. Fire events as time progresses
 * to current utc time. Determine the next future event and
 * setup a timer to re-run at that time.
 */
void sched_run_all(void)
{
	struct sched_state *st = &sched_state;
	u32 utc_time = clock_utc();
	u32 next_event;
	u32 earliest_event;
	char *earliest_name;
	u32 bb_run_time = adap_sched_run_time_read();
	u8 start;
	int i;

	/* Determine if time has been set. If not, then don't run schedules */
	client_timer_cancel(&st->timer);
	if (!st->enabled || !st->mod_scheds || clock_source() <= CS_DEF) {
		return;
	}
	sched_actions_reset(st);
	if (!bb_run_time || bb_run_time > utc_time ||
	    bb_run_time < CLOCK_START) {
		/* the stored bb_run_time can't be trusted */
		/* start from the current utc time */
		bb_run_time = utc_time;
		st->next_fire_sched = 0;
	}
	st->run_time = bb_run_time;
	if (st->next_fire_sched >= st->nsched) {
		st->next_fire_sched = 0;
	}
	sched_debug_time("last run", st->run_time);
run_schedules:
	earliest_event = MAX_U32;
	earliest_name = NULL;
	start = sched_state.next_fire_sched;
	i = start;
	do {
		ASSERT(i < st->nsched);
		if (sched_state.mod_scheds[i].name[0] == '\0' ||
		    !sched_state.mod_scheds[i].len) {
			goto move_on;
		}
		sched_log(LOG_DEBUG2 "eval sched %s",
		    sched_state.mod_scheds[i].name);
		next_event = sched_evaluate(&sched_state.mod_scheds[i],
		    sched_state.run_time);
		sched_debug_time("next event", next_event);
		if (!next_event || next_event == MAX_U32) {
			/* no more events to fire for this schedule */
			goto move_on;
		}
		if (next_event < earliest_event) {
			sched_state.next_fire_sched = i;
			earliest_event = next_event;
			earliest_name = sched_state.mod_scheds[i].name;
		}
move_on:
		i++;
		if (i >= st->nsched) {
			i = 0;
		}
	} while (i != start);

	sched_debug_time("earliest event", earliest_event);

	utc_time = clock_utc();

	if (!earliest_event || earliest_event == MAX_U32) {
		/* no events left for any of the schedules */
		goto finish;
	}

	/*
	 * The earliest event will be after the run time.
	 * If the earliest event is before the current time, run again.
	 */
	if (earliest_event <= utc_time) {
		st->run_time = earliest_event;
		goto run_schedules;
	}

	sched_log(LOG_DEBUG2 "run: timeout in %lu s",
	    earliest_event - utc_time);
	client_timer_set(&st->timer, (earliest_event - utc_time) * 1000);

finish:
	st->run_time = utc_time + 1;
	adap_sched_run_time_write(st->run_time);
	sched_actions_run(st);

	sched_notify_next_event(earliest_event, earliest_name);
}
