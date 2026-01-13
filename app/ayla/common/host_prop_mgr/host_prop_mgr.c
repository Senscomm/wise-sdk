#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "al/al_utypes.h"
#include "cmsis_os.h"

#include <ada/libada.h>
#include <ada/sprop.h>

#include <ayla/log.h>
#include <ayla/crc.h>
#include <ayla/callback.h>
#include "client_timer.h"

#include "host_prop_mgr.h"

/* update for every 5 secs */
#define NV_UPDATE_TIMER_MSECS (5 * 1000UL)
#define	msecs_to_ticks(ms)	(((ms)*1000)/osKernelGetTickFreq())
static osTimerId_t nv_update_timer = (osTimerId_t)NULL;

static struct prop_conf_metadata *g_prop_conf_table = NULL;
static struct ada_sprop *g_sprops_table = NULL;
static u8 g_sprop_table_size;
static struct callback prop_save_cb;

static void host_prop_connect_status(u8 mask);

static const struct prop_mgr demo_host_prop_mgr = {
    .name = "host_prop_mgr",
    .prop_meta_recv = NULL,
    .send_done = NULL,
    .get_val = NULL,
    .connect_status = host_prop_connect_status,
    .event = NULL,
};

static struct ada_sprop *host_sprop_lookup(const char *name)
{
    struct ada_sprop *sprop;

    for (int i = 0; i < g_sprop_table_size; i++) {
        sprop = g_sprops_table + i;
        if (!strcmp(sprop->name, name)) {
            return sprop;
        }
    }

    return NULL;
}

/* Sync nv storage input props from local to ADS */
static void host_props_nv_storage_sync(struct prop_conf_metadata *prop_conf_table)
{
    enum ada_err err;
    struct prop_conf_metadata *iter = prop_conf_table;
    struct ada_sprop *sprop;
    size_t offset = 0;

    while (iter) {
        if (iter->prop_name == NULL) {
            break;
        }

        iter->exist = ada_conf_get_item(&(iter->item)) > 0 ? 1 : 0;
        sprop = host_sprop_lookup(iter->prop_name);

        /* if file not exists, wait for next unsync operation */
        if (iter->exist && sprop && !sprop->send_req) {
            err = ada_prop_mgr_meta_set(iter->prop_name, iter->item.type, iter->item.val, 
                    iter->item.len, &offset, NODES_LOCAL, NULL, NULL, NULL);
            if (err) {
                log_put(LOG_WARN "%s: send nv prop %s: err %d", __func__, iter->prop_name, err);
            }
        } 
        iter++;
    }
}

static void host_prop_connect_status(u8 mask)
{
	if ((mask & NODES_ADS)) {
        log_put(LOG_WARN "%s: Sync local to ADS!!\n", __func__);
        host_props_nv_storage_sync(g_prop_conf_table);
		ada_prop_mgr_ready(&demo_host_prop_mgr);
	}
}

void host_prop_unsync_tag(struct ada_sprop *sprop, struct prop_conf_metadata *prop_conf_table)
{
    struct prop_conf_metadata *iter = prop_conf_table;

    while (iter) {
        if (iter->prop_name == NULL) {
            break;
        }
        if (!strcmp(iter->prop_name, sprop->name)) {
            /* Note: We use the prop val_len, itme.val_len may be larger when use BOOL */
            iter->tag = (memcmp(iter->item.val, sprop->val, sprop->val_len) == 0) ? 0 : 1;
            log_put(LOG_DEBUG "%s: %s %u", __func__, sprop->name, iter->tag);
            if (iter->tag) {
                memcpy(iter->item.val, sprop->val, sprop->val_len);
            }
            break;
        }

        iter++;
    }
}

void host_prop_conf_load(struct prop_conf_metadata *prop_conf_table)
{
    struct prop_conf_metadata *iter = prop_conf_table;

    while (iter) {
        if (iter->prop_name == NULL) {
            break;
        }
        iter->exist = ada_conf_get_item(&(iter->item)) > 0 ? 1 : 0;
        iter++;
    }
}

static void host_props_conf_save(void *arg)
{
    struct prop_conf_metadata *prop_conf_table = (struct prop_conf_metadata *)(arg);
    struct prop_conf_metadata *iter = prop_conf_table;

    while (iter) {
        if (iter->prop_name == NULL) {
            break;
        }
        if (iter->tag || !iter->exist) {
            iter->tag = 0;
            iter->exist = 1;

            conf_delete(iter->token);
            switch (iter->item.type) {
            case ATLV_INT:
            {
                log_put(LOG_DEBUG "%s: %ld, %d", __func__, *(s32 *)(iter->item.val), iter->item.len);
                conf_put_s32(iter->token, *(s32 *)(iter->item.val));
                break;
            }
            case ATLV_UTF8:
            {
                log_put(LOG_DEBUG "%s: %s, %d", __func__, (char *)(iter->item.val), iter->item.len);
                conf_put_str(iter->token, (char *)(iter->item.val));
                break;
            }
            case ATLV_BOOL:
            {
                log_put(LOG_DEBUG "%s: %d, %d", __func__, *(u8 *)(iter->item.val), iter->item.len);
                conf_put_u32(iter->token, *(u8 *)iter->item.val);
            }
            default :
                /* add if need */
                break;
            }
        }

        iter++;
    }

    conf_cd_parent();
}

static void host_props_save_cb(void *arg)
{
	conf_persist(CT_prop, host_props_conf_save, arg);
}

static void host_props_nv_update_tmr(void *arg)
{
    /* Use ADA Client Task to handle cb */
    client_callback_pend(&prop_save_cb);
}

void host_prop_mgr_init(struct prop_conf_metadata *prop_conf_table, struct ada_sprop *table, u8 size)
{
    if (!mfg_or_setup_mode_active()) {
        ada_prop_mgr_register(&demo_host_prop_mgr);
        if (nv_update_timer == NULL)
            nv_update_timer = osTimerNew(host_props_nv_update_tmr, osTimerPeriodic, NULL, NULL);

        g_prop_conf_table = prop_conf_table;
        g_sprops_table = table;
        g_sprop_table_size = size;

        host_prop_conf_load(prop_conf_table);
        /* The first call is to create missing files */
        host_props_save_cb(prop_conf_table);
        /*
        * Sync Local first, from nv to running.
        * Not sure whether to consider this situation or not.
        */
        // host_props_nv_storage_sync(prop_conf_table);

        osTimerStart(nv_update_timer, msecs_to_ticks(NV_UPDATE_TIMER_MSECS));
        callback_init(&prop_save_cb, host_props_save_cb, prop_conf_table);
    } else {
        log_put(LOG_WARN "%s: Do not support in mfg or setup mode.\n", __func__);
    }
}