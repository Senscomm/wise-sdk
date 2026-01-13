/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <al/al_os_thread.h>
#include <ada/libada.h>
#include <ada/sprop.h>
#include <adw/wifi.h>
#ifdef AYLA_BLUETOOTH_SUPPORT
#include <adb/adb.h>
#include <adb/al_bt.h>
#endif
#include <ayla/log.h>
#include <ayla/crc.h>
#include <ayla/callback.h>
#include "client_timer.h"
#include "conf.h"
#include "demo.h"
#include "ada/batch.h"
#include "app_event.h"

#include "wise_task_wdt.h"

#include "color_format.h"
#include "led_widget.h"
#include "lighting_mgr.h"
#include "lighting_ctrl.h"

#include "bp5758d.h"
#include "build.h"
#include "power_cycle_reset.h"
#include "host_prop_mgr.h"

/*
 * The oem and oem_model strings determine the host name for the
 * Ayla device service and the device template on the service.
 *
 * If these are changed, the encrypted OEM secret must be re-encrypted
 * unless the oem_model was "*" (wild-card) when the oem_key was encrypted.
 */
char oem[] = DEMO_OEM_ID;
char oem_model[] = DEMO_OEM_MODEL;

/* Attributes */
static char version[] = "ADA-" ADA_VERSION BUILD_NAME "-" SDK_VERSION;
char template_version[] = DEMO_TEMPLATE_VERSION;
static char factory_name[] = DEMO_FACTORY_NAME;
static char purchase_order[] = DEMO_PURCHASE_ORDER;
static char device_id[] = DEMO_DEVICE_ID;
static char led_driver[] = DEMO_LED_DRIVER;
static char device_rename[32];
static char mode[32];
static u8 alexa_enabled __maybe_unused;
static u8 google_enable __maybe_unused;
static u8 local_voice_enable __maybe_unused;
static u8 power;
static int brightness;
static int color_bright;
static int color_saturation;
static int color_select;
static int color_temp;
static int wifi_rssi;

static bool onboarding;
static char *cmd_buf;

static struct {
    s32 brightness;
    s32 color_bright;
    s32 color_saturation;
    s32 color_select;
    s32 color_temp;
    char mode[32];
} prop_conf_val_map = {
    /* string must have no zero len value */
    .mode = "null"
};

static struct prop_conf_metadata prop_conf_table[] = {
    {
        .prop_name = "brightness",
        .token =CT_brightness, 
        .item = {
            .name = "prop/brightness", 
            .type = ATLV_INT, 
            .val = &(prop_conf_val_map.brightness),
            .len = sizeof(prop_conf_val_map.brightness)
        }
    },
    {
        .prop_name = "color_bright",
        .token =CT_color_bright, 
        .item = {
            .name = "prop/color_bright", 
            .type = ATLV_INT, 
            .val = &prop_conf_val_map.color_bright, 
            .len = sizeof(prop_conf_val_map.color_bright)
        }
    },
    {
        .prop_name = "color_saturation",
        .token =CT_color_saturation, 
        .item = {
            .name = "prop/color_saturation",
            .type = ATLV_INT, 
            .val = &prop_conf_val_map.color_saturation,
            .len = sizeof(prop_conf_val_map.color_saturation)
        }
    },
    {
        .prop_name = "color_select",
        .token =CT_color_select,
        .item = {
            .name = "prop/color_select",
            .type = ATLV_INT,
            .val = &prop_conf_val_map.color_select,
            .len = sizeof(prop_conf_val_map.color_select)
        }
    },
    {
        .prop_name = "color_temp",
        .token =CT_color_temp, 
        .item = {
            .name = "prop/color_temp",
            .type = ATLV_INT,
            .val = &prop_conf_val_map.color_temp,
            .len = sizeof(prop_conf_val_map.color_temp)
        }
    },
    {
        .prop_name = "mode",
        .token =CT_mode, 
        .item = {
            .name = "prop/mode",
            .type = ATLV_UTF8,
            .val = prop_conf_val_map.mode,
            .len = sizeof(prop_conf_val_map.mode)
        }
    },
    /* must be the last, we do not pass the table size */
    {
        .prop_name = NULL,
    }
};

/*
 * Range converter
 * To convert a value in [cmin, cmax] to [nmin, nmaz].
 */
static int demo_convert_range(int value, int cmin, int cmax, int nmin, int nmax)
{
    if (cmax == cmin || nmax == nmin) {
        /* Conversion not possible */
        return value;
    }

    return nmin + ((value - cmin) * (nmax - nmin)) / (cmax - cmin);
}

static void demo_light_bulb_do_action(Action_t action, uint8_t *value,
        bool wait)
{
    bool (*is_done)(struct lighting_mgr *);
    bool initiated;

    is_done = (action == OFF_ACTION) ? lighting_mgr_is_light_off :\
              lighting_mgr_is_light_on;
    initiated = lighting_mgr_initiate_action(LightMgr(), 0, action, value);
    while (initiated && wait && !is_done(LightMgr())) {
        al_os_thread_sleep(1);
    }
}

static void demo_set_light_bulb(void)
{
    u8 color = -1;
    u8 level, temp;
    RgbColor_t rgb;
    if (!strcmp(mode, "white")) {
        color = 0;
    } else if (!strcmp(mode, "color")) {
        color = 1;
    } else {
        log_put(LOG_ERR "invalid mode: %s", mode);
        return;
    }
    demo_light_bulb_do_action(MODE_ACTION, &color, true);
    if (!color) {
        level = (u8)demo_convert_range(brightness, 0, 100, 0, 254);
        temp = color_temp; /* no conversion needed */
        demo_light_bulb_do_action(LEVEL_ACTION, &level, level ? true : false);
        demo_light_bulb_do_action(TEMP_ACTION, &temp, true);
    } else {
        rgb.r = (((u32)color_select) >> 16) & 0xff;
        rgb.g = (((u32)color_select) >>  8) & 0xff;
        rgb.b = (((u32)color_select) >>  0) & 0xff;
        demo_light_bulb_do_action(COLOR_ACTION, (u8 *)&rgb, true);
    }
}

static void demo_light_evt_handler(struct app_event *evt)
{
    bool need_set = false;

    if (evt->type != kEventType_Light
            && evt->type != kEventType_Install)
        return;

    if (evt->type == kEventType_Install) {
        (*evt->install_event.callback)(evt->install_event.arg);
        return;
    }

    switch(evt->light_event.action) {
    case kLightAction_On:
    {
        u8 on_only = (u8)evt->light_event.value;
        demo_light_bulb_do_action(ON_ACTION, 0, true);
        need_set = on_only != 0 ? false : true;
        break;
    }
    case kLightAction_Mode2:
    {
        need_set = true;
        break;
    }
    case kLightAction_Mode:
    {
        u8 value = (u8)evt->light_event.value;
        demo_light_bulb_do_action(MODE_ACTION, &value, true);
        break;
    }
    case kLightAction_Off:
        demo_light_bulb_do_action(OFF_ACTION, 0, true);
        break;
	case kLightAction_Level:
    {
        u8 value = (u8)evt->light_event.value;
        demo_light_bulb_do_action(LEVEL_ACTION, &value, value ? true : false);
        break;
    }
	case kLightAction_Temp:
    {
        u8 value = (u8)evt->light_event.value;
        demo_light_bulb_do_action(TEMP_ACTION, &value, true);
        break;
    }
	case kLightAction_Color:
    {
        u32 value = evt->light_event.value;
        RgbColor_t rgb;
        rgb.r = (value >> 16) & 0xff;
        rgb.g = (value >>  8) & 0xff;
        rgb.b = (value >>  0) & 0xff;
        demo_light_bulb_do_action(COLOR_ACTION, (u8 *)&rgb, true);
        break;
    }
    default:
        break;
    }

    if (need_set) {
        demo_set_light_bulb();
    }
}

static void demo_post_light_event(uint8_t action, uint32_t value)
{
    struct app_event evt;

    if (onboarding) {
        /* Do not interrupt lighting_ctrl operations. */
        log_put(LOG_WARN "[%s] ignored during onboarding", __func__);
        return;
    }

    evt.type = kEventType_Light;
    evt.light_event.action = action;
    evt.light_event.actor = 0;
    evt.light_event.value = value;

    app_event_post(&evt);
}

/*
 * Demo set function for bool properties.
 */
static enum ada_err demo_bool_set(struct ada_sprop *sprop, const void *buf,
        size_t len)
{
	enum ada_err err;

	err = ada_sprop_set_bool(sprop, buf, len);
	if (err) {
		return err;
	}

    if (!strcmp(sprop->name, "power")) {
        demo_post_light_event(*(uint8_t *)sprop->val ? kLightAction_On
                : kLightAction_Off, 0);
    }

	log_put(LOG_DEBUG "%s: %s %u", __func__, sprop->name, *(uint8_t *)sprop->val);

	return AE_OK;
}

/*
 * Demo set function for integer and decimal properties.
 */
static enum ada_err demo_int_set(struct ada_sprop *sprop, const void *buf,
        size_t len)
{
	enum ada_err err;

	err = ada_sprop_set_int(sprop, buf, len);
	if (err) {
		return err;
	}

    if (!strcmp(sprop->name, "brightness")) {
        u8 level = (u8)demo_convert_range(brightness, 0, 100, 0, 254);
        demo_post_light_event(kLightAction_Level, (uint32_t)level);
    } else if (!strcmp(sprop->name, "color_temp")) {
        demo_post_light_event(kLightAction_Temp, (uint32_t)color_temp);
    } else if (!strcmp(sprop->name, "color_select")) {
        demo_post_light_event(kLightAction_Color, (uint32_t)color_select);
    }

    host_prop_unsync_tag(sprop, prop_conf_table);
	log_put(LOG_DEBUG "%s: %s %u", __func__, sprop->name, *(int *)sprop->val);

	return AE_OK;
}

/*
 * Demo set function for string properties.
 */
static enum ada_err demo_string_set(struct ada_sprop *sprop, const void *buf,
        size_t len)
{
	enum ada_err err;
    u8 color;

	err = ada_sprop_set_string(sprop, buf, len);
	if (err) {
		return err;
	}

    if (!strcmp(sprop->name, "mode")) {
        if (!strncmp(buf, "white", len)) {
            color = 0;
        } else if (!strncmp(buf, "color", len)) {
            color = 1;
        } else {
            return AE_INVAL_VAL;
        }

        demo_post_light_event(kLightAction_Mode2, (uint32_t)color);
    }

    host_prop_unsync_tag(sprop, prop_conf_table);
	log_put(LOG_DEBUG "%s: %s %s", __func__, sprop->name, (const char *)sprop->val);

	return AE_OK;
}

static enum ada_err demo_cmd_set(struct ada_sprop *sprop,
		const void *buf, size_t len);

static struct ada_sprop demo_props[] = {
	/*
	 * version properties
	 * oem_host_version is the template version and must be sent first.
	 */
	{ "oem_host_version", ATLV_UTF8,
		template_version, sizeof(template_version),
		ada_sprop_get_string, NULL},
	{ "version", ATLV_UTF8, &version[0], sizeof(version),
		ada_sprop_get_string, NULL},
	{ "factory_name", ATLV_UTF8,
		factory_name, sizeof(factory_name),
		ada_sprop_get_string, NULL},
	{ "device_id", ATLV_UTF8, &device_id[0], sizeof(device_id),
		ada_sprop_get_string, NULL},
	{ "purchase_order", ATLV_UTF8,
		purchase_order, sizeof(purchase_order),
		ada_sprop_get_string, NULL},
	{ "led_driver", ATLV_UTF8, &led_driver[0], sizeof(led_driver),
		ada_sprop_get_string, NULL},
	{ "mode", ATLV_UTF8, mode, sizeof(mode),
		ada_sprop_get_string, demo_string_set},
	{ "device_rename", ATLV_UTF8, &device_rename[0], sizeof(device_rename),
		ada_sprop_get_string, demo_string_set},

	/*
	 * boolean properties.
	 */
	{ "power", ATLV_BOOL, &power, sizeof(power),
		ada_sprop_get_bool, demo_bool_set },

	/*
	 * Integer properties.
	 */
	{ "brightness", ATLV_INT, &brightness, sizeof(brightness),
		ada_sprop_get_int, demo_int_set },
	{ "color_bright", ATLV_INT, &color_bright, sizeof(color_bright),
		ada_sprop_get_int, demo_int_set },
	{ "color_saturation", ATLV_INT, &color_saturation, sizeof(color_saturation),
		ada_sprop_get_int, demo_int_set },
	{ "color_select", ATLV_INT, &color_select, sizeof(color_select),
		ada_sprop_get_int, demo_int_set },
	{ "color_temp", ATLV_INT, &color_temp, sizeof(color_temp),
		ada_sprop_get_int, demo_int_set },
	{ "wifi_rssi", ATLV_INT, &wifi_rssi, sizeof(wifi_rssi),
		ada_sprop_get_int, demo_int_set },

	/*
	 * String properties.
	 */
	{ "cmd", ATLV_UTF8, "", 0, ada_sprop_get_string, demo_cmd_set },
	{ "log", ATLV_UTF8,
		 "", 0,	/* note value and len are modified by demo_cmd_log_set() */
		ada_sprop_get_string, NULL },

};

static struct ada_sprop *demo_sprop_lookup(const char *name)
{
	struct ada_sprop *sprop;

	for (sprop = demo_props; sprop < ARRAY_END(demo_props); sprop++) {
		if (!strcmp(sprop->name, name)) {
			return sprop;
		}
	}
	return NULL;
}

/*
 * Set log and cmd strings value and length.
 */
static void demo_sprop_set(const char *name, char *val, size_t len)
{
	struct ada_sprop *sprop;

	sprop = demo_sprop_lookup(name);
	if (sprop) {
		sprop->val = val;
		sprop->val_len = len;
	}
}

static void demo_cmd_log_set(char *val, size_t len)
{
	demo_sprop_set("cmd", val, len);
	demo_sprop_set("log", val, len);
}

/*
 * Send property with metadata.
 */
static void demo_send_prop_with_meta(const char *name)
{
	struct prop_dp_meta meta[] = {
		{ "time", "" },
		{ "key1", "val1" },
		{ "key2", "val2" },
	};

	clock_fmt(meta[0].value, sizeof(meta[0].value), clock_utc());
	ada_sprop_send_by_name_with_meta(name, meta, ARRAY_LEN(meta));
}

/*
 * Demo set function for command property.
 */
static enum ada_err demo_cmd_set(struct ada_sprop *sprop,
		const void *buf, size_t len)
{
	char *new_buf;

	if (sprop->type != ATLV_UTF8) {
		return AE_INVAL_TYPE;
	}
	new_buf = malloc(len + 1);
	if (!new_buf) {
		return AE_ALLOC;
	}

	memcpy(new_buf, buf, len);
	new_buf[len] = '\0';

	demo_cmd_log_set("", 0);
	free(cmd_buf);
	cmd_buf = new_buf;
	demo_cmd_log_set(new_buf, len);

	log_put(LOG_INFO "%s: cloud set %s to \"%s\"\r\n",
	    __func__, sprop->name, cmd_buf);
	demo_send_prop_with_meta("log");
	return AE_OK;
}


static void prop_send_by_name(const char *name)
{
	enum ada_err err;

	err = ada_sprop_send_by_name(name);
	if (err) {
		log_put(LOG_ERR "demo: %s: send of %s: err %d",
				__func__, name, err);
	}
}

static void demo_lc_completed(bool timeout)
{
    struct app_event evt;

    evt.type = kEventType_Light;
    evt.light_event.action = kLightAction_Off;

    app_event_post(&evt);
}

static void demo_lc_do_sync(u32 timeout)
{
	enum ada_err err;

    /* Synchronize to ADA properties
     */

    power = 1;
    err = ada_sprop_send_by_name("power");
    if (err) {
        log_put(LOG_WARN "%s: send power: err %d",
            __func__, err);
    }

    strncpy(mode, "white", sizeof(mode));
    err = ada_sprop_send_by_name("mode");
    if (err) {
        log_put(LOG_WARN "%s: send mode: err %d",
            __func__, err);
    }

    /* XXX: why do they have to differ? */
    brightness = color_bright = 100;
    err = ada_sprop_send_by_name("brightness");
    if (err) {
        log_put(LOG_WARN "%s: send brightness: err %d",
            __func__, err);
    } else {
        log_put(LOG_DEBUG "%s: send brightness  %d OK", __func__,
                brightness);
    }
    err = ada_sprop_send_by_name("color_bright");
    if (err) {
        log_put(LOG_WARN "%s: send color_bright: err %d",
            __func__, err);
    } else {
        log_put(LOG_DEBUG "%s: send color_bright  %d OK", __func__,
                color_bright);
    }

    color_temp = 78;
    err = ada_sprop_send_by_name("color_temp");
    if (err) {
        log_put(LOG_WARN "%s: send color_temp: err %d",
            __func__, err);
    } else {
        log_put(LOG_DEBUG "%s: send color_temp  %d OK", __func__,
                color_temp);
    }
}

static void demo_lc_sync(bool timeout)
{
    struct app_event evt;

    evt.type = kEventType_Install;
    evt.install_event.callback = demo_lc_do_sync;
    evt.install_event.arg = (timeout ? 1 : 0);

    app_event_post(&evt);
}

#ifdef AYLA_BLUETOOTH_SUPPORT
/*
 * Callback when the device should identify itself to the end user, for
 * example, briefly blinking an LED.
 */
void demo_identify_cb(void)
{
	printf("%s called\n", __func__);
}
#endif

#ifdef AYLA_WIFI_SUPPORT
/*
 * Event handler for updating link LED state based on Wi-Fi events.
 */
static void demo_wifi_event_handler(enum adw_wifi_event_id id,
    void *arg)
{
	switch (id) {
	case ADW_EVID_STA_DOWN:
		break;
	default:
		break;
	}
}
#endif

static void demo_client_event(void *arg, enum ada_err err)
{
	char *msg;

	switch (err) {
	case AE_OK:
		msg = "up";
		break;
	case AE_IN_PROGRESS:
		msg = "down";
		break;
	case AE_NOTCONN:
		msg = "not connected";
		break;
	default:
		msg = NULL;
	}

	if (msg) {
		log_put(LOG_INFO "%s: ADS %s\n", __func__, msg);
	} else {
		log_put(LOG_WARN "%s: err %d\n", __func__, err);
	}
}

/*
 * Initialize property manager.
 */
static void demo_start(void)
{
#ifdef AYLA_WIFI_SUPPORT
	adw_wifi_event_register(demo_wifi_event_handler, NULL);
#endif
	ada_client_event_register(demo_client_event, NULL);

	ada_sprop_mgr_register("demo", demo_props, ARRAY_LEN(demo_props));

}

static void demo_onboarding_started(void)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 127,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 78,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 500,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Off,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 500,
            }
        },
    };

    if (onboarding)
        return;

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(-1, 180000/* 3 min. */, demo_lc_completed);

    onboarding = true;
}

static void demo_onboarding_done(void)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 254,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 78,
            }
        }
    };

    if (!onboarding)
        return;

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(1, 0, demo_lc_sync);

    onboarding = false;
}

static void demo_idle(void)
{
	struct app_event event;

    host_prop_mgr_init(prop_conf_table, demo_props, ARRAY_LEN(demo_props));

	wise_task_wdt_add(NULL);

	while (1) {
        BaseType_t eventReceived = xQueueReceive(g_app_event_queue, &event, pdMS_TO_TICKS(10));
        while (eventReceived == pdTRUE) {
            app_event_dispatch(&event);
            eventReceived = xQueueReceive(g_app_event_queue, &event, 0);
        }

        if (!demo_cloud_has_started()) {
            if (demo_bt_is_provisioning()) {
                demo_onboarding_started();
            }
        } else if (onboarding) {
            demo_onboarding_done();

            log_put(LOG_INFO "%s: cloud connection started\r\n", __func__);

            prop_send_by_name("oem_host_version");
            prop_send_by_name("version");

        }

	    wise_task_wdt_reset(NULL);
	}
}

void app_main()
{
	bp5758d_init();
	bp5758d_set_rgbcw_channel(0, 0, 0, 156, 44);

    check_power_cycle_count();

    app_event_init();
    app_event_install_handler(kEventType_Light, demo_light_evt_handler);
    app_event_install_handler(kEventType_Install, demo_light_evt_handler);

    lighting_mgr_init(LightMgr());
    lighting_ctrl_init();

	if (demo_init()) {
	    demo_start();
    }
	demo_idle();
}
