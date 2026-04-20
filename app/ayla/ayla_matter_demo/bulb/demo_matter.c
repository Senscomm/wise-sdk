/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
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

/* XXX: cmsis_os2 doesn't support xQueuePeek yet. */
#include <app-common/zap-generated/attribute-type.h>

#include <al/al_os_thread.h>
#include <ada/libada.h>
#include <ada/server_req.h>
#include <ada/sprop.h>
#include <adm/adm_csa_numbers.h>
#include <adm/adm.h>
#include <ayla/log.h>
#include <ayla/timer.h>

#include "app_int.h"
#include "app_event.h"

#include "wise_task_wdt.h"

#include "color_format.h"
#include "led_widget.h"
#include "lighting_mgr.h"
#include "lighting_ctrl.h"

// #include "bp5758d.h"
#include "bp1638cj.h"
#include "build.h"

#define DEMO_ENDPOINT_LIGHTING	1

#define DEMO_SYNC_RETRY_MAX	10

/* Attributes */
static char version[] = "ADA-" ADA_VERSION BUILD_NAME "-" SDK_VERSION;
char template_version[] = DEMO_TEMPLATE_VERSION;
static char factory_name[] = DEMO_FACTORY_NAME;
static char purchase_order[] = DEMO_PURCHASE_ORDER;
static char device_id[] = DEMO_DEVICE_ID;
static char led_driver[] = DEMO_LED_DRIVER;
static char device_rename[32];
static char mode[32];
static u8 alexa_enabled;
static u8 google_enable;
static u8 local_voice_enable;
static u8 power;
static int brightness;
static int color_bright;
static int color_saturation;
static int color_select;
static int color_temp;
static int wifi_rssi;
static char _log[1024];

/* Static local storage */
static HsvColor_t hsv;
static XyColor_t xy;

static bool onboarding;

/*
 * Matter Certification Declaration(s).
 *
 * TODO: This is current a test certification declaration from the Matter SDK.
 *
 * Before certification, this should be replace by provisional CDs issued by
 * the CSA. There will likely be one for each model.
 *
 * After certification and before production release, the final CDs issued by
 * the CSA must be used.
 */
static const uint8_t demo_test_cert_declaration[539] = {
	0x30, 0x82, 0x02, 0x17, 0x06, 0x09, 0x2a, 0x86,
	0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0,
	0x82, 0x02, 0x08, 0x30, 0x82, 0x02, 0x04, 0x02,
	0x01, 0x03, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x09,
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
	0x01, 0x30, 0x82, 0x01, 0x70, 0x06, 0x09, 0x2a,
	0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01,
	0xa0, 0x82, 0x01, 0x61, 0x04, 0x82, 0x01, 0x5d,
	0x15, 0x24, 0x00, 0x01, 0x25, 0x01, 0xf1, 0xff,
	0x36, 0x02, 0x05, 0x00, 0x80, 0x05, 0x01, 0x80,
	0x05, 0x02, 0x80, 0x05, 0x03, 0x80, 0x05, 0x04,
	0x80, 0x05, 0x05, 0x80, 0x05, 0x06, 0x80, 0x05,
	0x07, 0x80, 0x05, 0x08, 0x80, 0x05, 0x09, 0x80,
	0x05, 0x0a, 0x80, 0x05, 0x0b, 0x80, 0x05, 0x0c,
	0x80, 0x05, 0x0d, 0x80, 0x05, 0x0e, 0x80, 0x05,
	0x0f, 0x80, 0x05, 0x10, 0x80, 0x05, 0x11, 0x80,
	0x05, 0x12, 0x80, 0x05, 0x13, 0x80, 0x05, 0x14,
	0x80, 0x05, 0x15, 0x80, 0x05, 0x16, 0x80, 0x05,
	0x17, 0x80, 0x05, 0x18, 0x80, 0x05, 0x19, 0x80,
	0x05, 0x1a, 0x80, 0x05, 0x1b, 0x80, 0x05, 0x1c,
	0x80, 0x05, 0x1d, 0x80, 0x05, 0x1e, 0x80, 0x05,
	0x1f, 0x80, 0x05, 0x20, 0x80, 0x05, 0x21, 0x80,
	0x05, 0x22, 0x80, 0x05, 0x23, 0x80, 0x05, 0x24,
	0x80, 0x05, 0x25, 0x80, 0x05, 0x26, 0x80, 0x05,
	0x27, 0x80, 0x05, 0x28, 0x80, 0x05, 0x29, 0x80,
	0x05, 0x2a, 0x80, 0x05, 0x2b, 0x80, 0x05, 0x2c,
	0x80, 0x05, 0x2d, 0x80, 0x05, 0x2e, 0x80, 0x05,
	0x2f, 0x80, 0x05, 0x30, 0x80, 0x05, 0x31, 0x80,
	0x05, 0x32, 0x80, 0x05, 0x33, 0x80, 0x05, 0x34,
	0x80, 0x05, 0x35, 0x80, 0x05, 0x36, 0x80, 0x05,
	0x37, 0x80, 0x05, 0x38, 0x80, 0x05, 0x39, 0x80,
	0x05, 0x3a, 0x80, 0x05, 0x3b, 0x80, 0x05, 0x3c,
	0x80, 0x05, 0x3d, 0x80, 0x05, 0x3e, 0x80, 0x05,
	0x3f, 0x80, 0x05, 0x40, 0x80, 0x05, 0x41, 0x80,
	0x05, 0x42, 0x80, 0x05, 0x43, 0x80, 0x05, 0x44,
	0x80, 0x05, 0x45, 0x80, 0x05, 0x46, 0x80, 0x05,
	0x47, 0x80, 0x05, 0x48, 0x80, 0x05, 0x49, 0x80,
	0x05, 0x4a, 0x80, 0x05, 0x4b, 0x80, 0x05, 0x4c,
	0x80, 0x05, 0x4d, 0x80, 0x05, 0x4e, 0x80, 0x05,
	0x4f, 0x80, 0x05, 0x50, 0x80, 0x05, 0x51, 0x80,
	0x05, 0x52, 0x80, 0x05, 0x53, 0x80, 0x05, 0x54,
	0x80, 0x05, 0x55, 0x80, 0x05, 0x56, 0x80, 0x05,
	0x57, 0x80, 0x05, 0x58, 0x80, 0x05, 0x59, 0x80,
	0x05, 0x5a, 0x80, 0x05, 0x5b, 0x80, 0x05, 0x5c,
	0x80, 0x05, 0x5d, 0x80, 0x05, 0x5e, 0x80, 0x05,
	0x5f, 0x80, 0x05, 0x60, 0x80, 0x05, 0x61, 0x80,
	0x05, 0x62, 0x80, 0x05, 0x63, 0x80, 0x18, 0x24,
	0x03, 0x16, 0x2c, 0x04, 0x13, 0x43, 0x53, 0x41,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x53, 0x57, 0x43,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x2d, 0x30, 0x30,
	0x24, 0x05, 0x00, 0x24, 0x06, 0x00, 0x24, 0x07,
	0x01, 0x24, 0x08, 0x00, 0x18, 0x31, 0x7c, 0x30,
	0x7a, 0x02, 0x01, 0x03, 0x80, 0x14, 0xfe, 0x34,
	0x3f, 0x95, 0x99, 0x47, 0x76, 0x3b, 0x61, 0xee,
	0x45, 0x39, 0x13, 0x13, 0x38, 0x49, 0x4f, 0xe6,
	0x7d, 0x8e, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x30,
	0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
	0x04, 0x03, 0x02, 0x04, 0x46, 0x30, 0x44, 0x02,
	0x20, 0x4a, 0x12, 0xf8, 0xd4, 0x2f, 0x90, 0x23,
	0x5c, 0x05, 0xa7, 0x71, 0x21, 0xcb, 0xeb, 0xae,
	0x15, 0xd5, 0x90, 0x14, 0x65, 0x58, 0xe9, 0xc9,
	0xb4, 0x7a, 0x1a, 0x38, 0xf7, 0xa3, 0x6a, 0x7d,
	0xc5, 0x02, 0x20, 0x20, 0xa4, 0x74, 0x28, 0x97,
	0xc3, 0x0a, 0xed, 0xa0, 0xa5, 0x6b, 0x36, 0xe1,
	0x4e, 0xbb, 0xc8, 0x5b, 0xbd, 0xb7, 0x44, 0x93,
	0xf9, 0x93, 0x58, 0x1e, 0xb0, 0x44, 0x4e, 0xd6,
	0xca, 0x94, 0x0b
};

/* Check if sprop is ready and connected.
 *
 * This is to prevent Matter attribute default values from being queued
 * as echos to be sent to ADA server overriding stored values.
 * There is no obvious solution to filter out default values incoming right after
 * the Matter fabric is reconnected.
 * For now, we assume that default attribute values come before ADS is connected.
 */

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

#define KWW    (2100.0)
#define KCW    (7688.0)

static u16 demo_convert_temp_to_mireds(int temp)
{
    double K, T;

    T = (double)temp;
    K = KWW + (T * (KCW - KWW)) / 100;

    return (uint16_t)(1000000 / K);
}

static int demo_convert_mireds_to_temp(u16 mireds)
{
    double K, T;
    int t;

    K = (double)(1000000 / mireds);
    T = ((K - KWW) * 100) / (KCW - KWW);
    T = fmin(fmax(T, 0.0), 100.0);

    return (int)T;
}

static void demo_light_bulb_do_action(Action_t action, uint8_t *value,
        bool wait)
{
    bool initiated = lighting_mgr_initiate_action(LightMgr(), 0, action, value);
    while (initiated && wait && !lighting_mgr_is_light_on(LightMgr())) {
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

static void demo_evt_handler(struct app_event *evt)
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
        demo_light_bulb_do_action(OFF_ACTION, 0, false);
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


static void demo_lc_completed(bool timeout)
{
    struct app_event evt;

    evt.type = kEventType_Light;
    evt.light_event.action = kLightAction_Off;

    app_event_post(&evt);

    if (timeout) {
        onboarding = false;
    }
}

static void demo_lc_do_sync(u32 timeout)
{
    u16 mireds;
	enum ada_err err;

#if 0
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
#endif
    /* Synchronize to cluster attributes
     */

    err = adm_write_boolean(DEMO_ENDPOINT_LIGHTING, ADM_ON_OFF_CID,
            ADM_ON_OFF_AID, power);
    if (err) {
        log_put(LOG_WARN "%s: matter write err %d", __func__, err);
    } else {
        log_put(LOG_DEBUG "%s: power to matter %d OK", __func__, power);
    }

    err = adm_write_enum8(DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID,
            ADM_COLOR_CONTROL_COLOR_MODE_AID, 2);
    if (err) {
        log_put(LOG_WARN "%s: matter write err %d", __func__, err);
    } else {
        log_put(LOG_DEBUG "%s: mode to matter %d OK", __func__, 2);
    }

    err = adm_write_u8(DEMO_ENDPOINT_LIGHTING, ADM_LEVEL_CONTROL_CID,
        ADM_CURRENT_LEVEL_AID, 254);
    if (err) {
        log_put(LOG_WARN "%s: matter write err %d", __func__, err);
    } else {
        log_put(LOG_DEBUG "%s: brightness to matter %d OK", __func__, 254);
    }

    mireds = demo_convert_temp_to_mireds(color_temp);
    err = adm_write_u16(DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID,
        ADM_COLOR_CONTROL_COLOR_TEMPERATURE_AID, mireds);
    if (err) {
        log_put(LOG_WARN "%s: matter write err %d", __func__, err);
    } else {
        log_put(LOG_DEBUG "%s: color_temp to matter %d OK", __func__, mireds);
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

static void demo_matter_event_cb(enum adm_event_id id)
{
	log_put(LOG_DEBUG2 "%s %d", __func__, id);

    printf("ADM Event:%d\n", id);
	switch (id) {
	case ADM_EVENT_IPV4_UP:
		ada_client_ip_up();
		ada_client_health_check_en();
		break;

	case ADM_EVENT_IPV4_DOWN:
		ada_client_ip_down();
		break;
	case ADM_EVENT_COMMISSIONING_SESSION_STARTED:
	case ADM_EVENT_COMMISSIONING_WINDOW_OPENED:
    {
        // printf("[1]Do nothing now!!!!\n");
        // break;
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
            break;

        lighting_ctrl_terminate(false, false);
        for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
            lighting_ctrl_add_event(&evt[i]);
        }
        lighting_ctrl_run(-1, 60000/* 3 min. */, demo_lc_completed);

        onboarding = true;
    }
        break;
	case ADM_EVENT_COMMISSIONING_SESSION_STOPPED:
	case ADM_EVENT_COMMISSIONING_WINDOW_CLOSED:
        break;
	case ADM_EVENT_COMMISSIONING_COMPLETE:
    {
        // printf("[2]Do nothing now!!!!\n");
        // break;
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
        lighting_ctrl_terminate(false, false);
        for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
            lighting_ctrl_add_event(&evt[i]);
        }
        lighting_ctrl_run(1, 0, demo_lc_sync);

        onboarding = false;
        break;
    }
	default:
		break;
	}
}

static enum ada_err demo_on_off_cb(u8 post_change, u16 endpoint,
    u32 cluster, u32 attribute, u8 type, u16 size, u8 *value)
{
	enum ada_err err;

    printf("%s %d\n", __func__, __LINE__);
	if (type != ZCL_BOOLEAN_ATTRIBUTE_TYPE) {
		log_put(LOG_ERR "%s: invalid type %u", __func__, type);
		return AE_INVAL_TYPE;
	}

	if ((size != 1) || (value == NULL)) {
		log_put(LOG_ERR "%s: invalid size %u or value data %p",
		    __func__, size, value);
		return AE_INVAL_VAL;
	}

    power = *value;

    demo_post_light_event(*value ? kLightAction_On : kLightAction_Off, 0);

	log_put(LOG_INFO "%s on_off %u", __func__, *value);

	return AE_OK;
}

static struct adm_attribute_change_callback demo_on_off_cb_entry =
    ADM_ACCE_INIT(ADM_ACCE_POST_CHANGE,
	DEMO_ENDPOINT_LIGHTING, ADM_ON_OFF_CID, ADM_ON_OFF_AID,
	demo_on_off_cb);

static enum ada_err demo_level_control_cb(u8 post_change, u16 endpoint,
    u32 cluster, u32 attribute, u8 type, u16 size, u8 *value)
{
    enum ada_err err;

    printf("%s %d\n", __func__, __LINE__);
	if (type != ZCL_INT8U_ATTRIBUTE_TYPE) {
		log_put(LOG_ERR "%s: invalid type %u", __func__, type);
		return AE_INVAL_TYPE;
	}

	if ((size != 1) || (value == NULL)) {
		log_put(LOG_ERR "%s: invalid size %u or value data %p",
		    __func__, size, value);
		return AE_INVAL_VAL;
	}

    if (*value <= 1) {
        /* XXX: what should we do? It seems to be the best to ignore it.
         */
        log_put(LOG_WARN "%s: level_control %d ignored", __func__, *value);
        return AE_OK;
    }

    /* XXX: why do they have to differ? */
    brightness = color_bright = (int)demo_convert_range(*value, 0, 254, 0, 100);

    demo_post_light_event(kLightAction_Level, *value);

	log_put(LOG_DEBUG "%s level_control %u", __func__, *value);

	return AE_OK;
}

static struct adm_attribute_change_callback demo_level_control_cb_entry =
    ADM_ACCE_INIT(ADM_ACCE_POST_CHANGE,
    DEMO_ENDPOINT_LIGHTING, ADM_LEVEL_CONTROL_CID, ADM_CURRENT_LEVEL_AID,
	demo_level_control_cb);

static enum ada_err demo_color_control_cb(u8 post_change, u16 endpoint,
    u32 cluster, u32 attribute, u8 type, u16 size, u8 *value)
{
    enum ada_err err;
    RgbColor_t rgb;
    u32 val;

    printf("%s %d\n", __func__, __LINE__);

	if (value == NULL) {
		log_put(LOG_ERR "%s: invalid value data %p",
		    __func__, value);
		return AE_INVAL_VAL;
	}

    /* XY color space */
    if (attribute == ADM_COLOR_CONTROL_CURRENT_X_AID
            || attribute == ADM_COLOR_CONTROL_CURRENT_Y_AID) {
        uint8_t level;
        if (size != sizeof(uint16_t)) {
            log_put(LOG_ERR "Wrong length for ColorControl value: %d", size);
		    return AE_INVAL_VAL;
        }

        if (attribute == ADM_COLOR_CONTROL_CURRENT_X_AID) {
            xy.x = *(uint16_t *)(value);
        } else if (attribute == ADM_COLOR_CONTROL_CURRENT_Y_AID) {
            xy.y = *(uint16_t *)(value);
        }

        err = adm_read_attribute(DEMO_ENDPOINT_LIGHTING, ADM_LEVEL_CONTROL_CID,
                ADM_CURRENT_LEVEL_AID, &level, 1, 0);
        if (err) {
            log_put(LOG_ERR "%s: read attribute: err %d",
                __func__, err);
        }

        rgb = XYToRgb(level, xy.x, xy.y);
        val = (0 << 24 | rgb.r << 16 | rgb.g << 8 | rgb.b);

        color_select = (int)val;

        demo_post_light_event(kLightAction_Color, val);

        log_put(LOG_DEBUG "New XY color: %u|%u", xy.x, xy.y);
    }
    /* HSV color space */
    else if (attribute == ADM_COLOR_CONTROL_CURRENT_HUE_AID ||
             attribute == ADM_COLOR_CONTROL_CURRENT_SATURATION_AID)
    {
        if (size != sizeof(uint8_t)) {
            log_put(LOG_ERR "Wrong length for ColorControl value: %d", size);
            return AE_INVAL_VAL;
        }

        if (attribute == ADM_COLOR_CONTROL_CURRENT_HUE_AID) {
            hsv.h = *value;
        } else if (attribute == ADM_COLOR_CONTROL_CURRENT_SATURATION_AID) {
            hsv.s = *value;
        }

        err = adm_read_attribute(DEMO_ENDPOINT_LIGHTING, ADM_LEVEL_CONTROL_CID,
                ADM_CURRENT_LEVEL_AID, &hsv.v, 1, 0);
        if (err) {
            log_put(LOG_ERR "%s: read attribute: err %d",
                __func__, err);
        }

        rgb = HsvToRgb(hsv);
        val = (0 << 24 | rgb.r << 16 | rgb.g << 8 | rgb.b);

        color_select = (int)val;
        if (attribute == ADM_COLOR_CONTROL_CURRENT_SATURATION_AID) {
            color_saturation = demo_convert_range(hsv.s, 0, 254, 0, 100);
        }

        demo_post_light_event(kLightAction_Color, val);

        log_put(LOG_DEBUG "New HSV color: %u|%u|%u", hsv.h, hsv.s, hsv.v);
    }
    /* Color temperature */
    else if (attribute == ADM_COLOR_CONTROL_COLOR_TEMPERATURE_AID)
    {
        /* XXX: treat it as a (tunable-)white */
        u16 mireds;
        int temp;
        if (size != sizeof(uint16_t)) {
            log_put(LOG_ERR "Wrong length for ColorControl value: %d", size);
		    return AE_INVAL_VAL;
        }
        mireds = *(u16 *)(value);
        temp = demo_convert_mireds_to_temp(mireds);

        color_temp = temp;

        demo_post_light_event(kLightAction_Temp, (uint32_t)temp);

        log_put(LOG_DEBUG "%s: mireds %u, temp %d", __func__, mireds, temp);
    }
    /* Color mode */
    else if (attribute == ADM_COLOR_CONTROL_COLOR_MODE_AID)
    {
        if (((*value == 0 || *value == 1) && strcmp(mode, "color"))
                || (*value == 2 && strcmp(mode, "white"))) {
            strncpy(mode, *value == 2 ? "white" : "color", sizeof(mode));
        }

        demo_post_light_event(kLightAction_Mode2, 1);

        log_put(LOG_DEBUG "color mode: %u", *value);
    }
    else
    {
        log_put(LOG_WARN "%s: unhandled attritbute 0x%lx", __func__, attribute);
        return AE_NOT_FOUND;
    }

	return AE_OK;
}

static struct adm_attribute_change_callback demo_color_control_cb_entry =
    ADM_ACCE_INIT(ADM_ACCE_POST_CHANGE | ADM_ACCE_ANY_ATTRIBUTE,
    DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID, 0,
	demo_color_control_cb);

extern void MatterAylaBasePluginServerInitCallback();
extern void MatterAylaLocalControlPluginServerInitCallback();

static void init_ayla_clusters(void)
{
    MatterAylaBasePluginServerInitCallback();
    MatterAylaLocalControlPluginServerInitCallback();
}

void demo_init(void)
{
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	int rc;
#endif

    /* 替换成其他IC驱动 */
    // bp5758d_init();
	// bp5758d_set_rgbcw_channel(0, 0, 0, 156, 44);
    bp1638cj_init();
    bp1638cj_set_rgbcw_channel(0, 0, 0, 77, 44);

    app_event_init();
    app_event_install_handler(kEventType_Light, demo_evt_handler);
    app_event_install_handler(kEventType_Install, demo_evt_handler);

    lighting_mgr_init(LightMgr());
    lighting_ctrl_init();

	adm_init();
    init_ayla_clusters();
	adm_event_cb_register(demo_matter_event_cb);
	adm_start(demo_test_cert_declaration,
	    sizeof(demo_test_cert_declaration));

#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	/*
	 * Enable local control access.
	 */
	rc = ada_client_lc_up();
	if (rc) {
		log_put(LOG_ERR "ADA local control up failed");
		return;
	}
#endif

	adm_attribute_change_cb_register(&demo_on_off_cb_entry);
	adm_attribute_change_cb_register(&demo_level_control_cb_entry);
	adm_attribute_change_cb_register(&demo_color_control_cb_entry);
}

void demo_idle(void)
{
	struct app_event event;

    wise_task_wdt_add(NULL);

	while (1) {
        BaseType_t eventReceived = xQueueReceive(g_app_event_queue, &event, pdMS_TO_TICKS(10));
        while (eventReceived == pdTRUE) {
            app_event_dispatch(&event);
            eventReceived = xQueueReceive(g_app_event_queue, &event, 0);
        }
	    wise_task_wdt_reset(NULL);
	}
}

#if 1
// cli cmd control NVC
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <cli.h>

static int do_nvc_test(int argc, char *argv[])
{
    const char *format;
	int value_r, value_g, value_b;

	if (argc != 5) {
		return CMD_RET_USAGE;
	}

    format = argv[1];
	value_r = atoi(argv[2]);
	value_g = atoi(argv[3]);
	value_b = atoi(argv[4]);

	if (value_r > 255 || value_g > 255 || value_b > 255) {
		return CMD_RET_USAGE;
	}

    printf("Input V:[%d][%d][%d]\n", value_r, value_g, value_b);
    if (!strcmp(format, "color")) {
        bp1638cj_set_rgbcw_channel(value_r, value_g, value_b, 0, 0);
    } else if (!strcmp(format, "white")) {
        bp1638cj_set_rgbcw_channel(0, 0, 0, value_r, value_g);
    } else if (!strcmp(format, "onoff")) {
        // bp1638cj_set_standby(value_r);
        if (value_r != 0) {
            bp1638cj_set_rgbcw_channel(100, 100, 100, 0, 0);
        } else {
            bp1638cj_set_rgbcw_channel(0, 0, 0, 0, 0);
        }
    } else {
        return CMD_RET_USAGE;
    }

	return CMD_RET_SUCCESS;
}

CMD(nvc, do_nvc_test,
		"Test nvc Bulb control",
		"nvc <color|white|onoff> <r/c/on-off> <g/w/0> <b/0/0>"
   );
#endif