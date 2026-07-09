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

#include <ayla/utypes.h>
#include <adb/adb.h>
#include <adb/adb_ayla_svc.h>
#include <adb/adb_conn_svc.h>
#include <adb/adb_wifi_cfg_svc.h>
#include <adb/adb_ota_svc.h>

#include "app_int.h"
#include "app_event.h"

#include "wise_task_wdt.h"

#include "color_format.h"
#include "led_widget.h"
#include "lighting_mgr.h"
#include "lighting_ctrl.h"

#ifdef AYLA_ADA_SERVICE_ENABLE
#include "ftm.h"
#include "bp5758d.h"
#endif
#include "build.h"

#include "scm_flash.h"
#include "scm_gpio.h"
#include "scm_fs.h"
#include "wise_wifi.h"
#include "ping.h"

#include "iotalink.h"

/* wpas notifies the scan through event callbacks, and after obtaining the scan results, the data will be erased */
static uint8_t g_scan_source = 0;

uint8_t demo_get_scan_source(void)
{
    return g_scan_source;
}

void demo_set_scan_source(uint8_t src)
{
    g_scan_source = src;
}

static bool g_initial_scan_enable = true;

void demo_set_inital_scan(bool enable)
{
    g_initial_scan_enable = enable;
}

#define OTA_APP_VER     "1.0.1"
int demo_get_app_version()
{
    int major, minor, patch;
    if (sscanf(OTA_APP_VER, "%d.%d.%d", &major, &minor, &patch) != 3) {
        return -1;
    }
    return major * 100 + minor * 10 + patch;
}

uint8_t ip_up;
uint8_t demo_get_ip_state()
{
    return ip_up;
}

#define DEMO_EXTERNAL_PING_HOST "8.8.8.8"

int demo_log_external_connectivity(void)
{
    int ret = ping_check_external_connectivity();

    log_put(LOG_INFO "External network %s via ping %s",
        ret == 0 ? "reachable" : "unreachable",
        DEMO_EXTERNAL_PING_HOST);
    return ret == 0 ? 1 : 0;
}

#define DEMO_ENDPOINT_LIGHTING	1

#define DEMO_SYNC_RETRY_MAX	10

/* Attributes */
//static char version[] = "ADA-" ADA_VERSION BUILD_NAME "-" SDK_VERSION;
static char version[] = APP_NAME " " BUILD_STRING;
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

#define	msecs_to_ticks(ms)      (((ms)*1000)/osKernelGetTickFreq())
#ifdef AYLA_ADA_SERVICE_ENABLE
/* ayla cloud connect timer */
#define AC_DELAY_TIMER_MSECS    (10 * 1000)
static osTimerId_t ac_conn_timer;
#endif
/* matter all farbrics remove timer, reboot after 4 secs. */
#define MF_REMOVE_DELAY_TIMER_MSECS    (4 * 1000)
static osTimerId_t mf_remove_timer;

/*****************************《 Template 》**************************************/
#include "iotalink_control.h"
// wlt_add
// static u8 power;

static u16 big_mode;//大模式

//全局亮度
// static u32 brightness;

// 动态场景相关
static u16 scene;
static u32 speed;
// 白光

static u32 temperature;

// 彩光
// static u32 color;

//律动
static u32 music;
static u32 sensitivity;

//自定义

static u32 custome;

//线序
static u32 rgb_sequence;


//-------------------------------用于上报----------------------------------------------------//
void light_mode_update(LIGHT_MODE_E locol_mode)
{
	big_mode = locol_mode;
	demo_send_prop("big_mode"); 
}
void light_power_update(bool on_off)
{
	power = on_off;
	demo_send_prop("Power"); 
}


void light_brightness_update(uint32_t value)
{
    brightness = value;
    demo_send_prop("Brightness"); 
}

void light_scene_update(uint16_t value)
{
    scene = value;
    demo_send_prop("Scene"); 
}

void light_custome_unit_update(uint16_t value)
{
    custome = value;
    demo_send_prop("custome"); 
}

void light_speed_update(uint32_t value)
{
    speed = value;
    demo_send_prop("Speed"); 
}

void light_temperature_update(uint32_t value)
{
    temperature = value;
    demo_send_prop("Temperature"); 
}

void light_color_update(uint32_t value)
{
    color_select = value;
    demo_send_prop("Color"); 
}

void light_music_update(uint32_t value)
{
    music = value;
    demo_send_prop("Music"); 
}

void light_sensitivity_update(uint32_t value)
{
    sensitivity = value;
    demo_send_prop("Sensitivity"); 
}

void light_rgb_sequence_update(uint32_t value)
{
    rgb_sequence = value;
    demo_send_prop("RGB_Sequence"); 
}


//------------------------------- end ----------------------------------------------------//

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
static bool demo_sprop_is_ready(void)
{
    return (ada_sprop_dest_mask & NODES_ADS) ? true : false;
}

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
        log_put(LOG_INFO "bulb reset by %s", evt->light_event.action == kLightAction_Mode2 ? "mode change" : "on");
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

#ifdef AYLA_ADA_SERVICE_ENABLE
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

        err = adm_write_boolean(DEMO_ENDPOINT_LIGHTING, ADM_ON_OFF_CID,
                ADM_ON_OFF_AID, power);
        if (err) {
            log_put(LOG_WARN "%s: matter write err %d", __func__, err);
        } else {
            log_put(LOG_DEBUG "%s: %s to matter %d OK", __func__, sprop->name, power);
        }
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

        err = adm_write_u8(DEMO_ENDPOINT_LIGHTING, ADM_LEVEL_CONTROL_CID,
            ADM_CURRENT_LEVEL_AID, level);
        if (err) {
            log_put(LOG_WARN "%s: matter write err %d", __func__, err);
        } else {
            log_put(LOG_DEBUG "%s: %s to matter %d OK", __func__, sprop->name, level);
        }
    } else if (!strcmp(sprop->name, "color_temp")) {
        u16 mireds = demo_convert_temp_to_mireds(color_temp);

        demo_post_light_event(kLightAction_Temp, (uint32_t)color_temp);

        err = adm_write_u16(DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID,
            ADM_COLOR_CONTROL_COLOR_TEMPERATURE_AID, mireds);
        if (err) {
            log_put(LOG_WARN "%s: matter write err %d", __func__, err);
        } else {
            log_put(LOG_DEBUG "%s: %s to matter %d OK", __func__, sprop->name, color_temp);
        }
    } else if (!strcmp(sprop->name, "color_select")) {
        u8 r = color_select >> 16;
        u8 g = color_select >> 8;
        u8 b = color_select >> 0;

        demo_post_light_event(kLightAction_Color, (uint32_t)color_select);

        hsv = RgbToHsv(r, g, b);
        xy = RgbToXy(r, g, b);

        err = adm_write_u16(DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID,
            ADM_COLOR_CONTROL_CURRENT_HUE_AID, hsv.h);
        if (err) {
            log_put(LOG_WARN "%s: matter write err %d", __func__, err);
        } else {
            log_put(LOG_DEBUG "%s: CurrentHue to matter %d OK", __func__, hsv.h);
        }

        err = adm_write_u16(DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID,
            ADM_COLOR_CONTROL_CURRENT_SATURATION_AID, hsv.s);
        if (err) {
            log_put(LOG_WARN "%s: matter write err %d", __func__, err);
        } else {
            log_put(LOG_DEBUG "%s: CurrentSaturation to matter %d OK", __func__, hsv.s);
        }

        err = adm_write_u16(DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID,
            ADM_COLOR_CONTROL_CURRENT_X_AID, xy.x);
        if (err) {
            log_put(LOG_WARN "%s: matter write err %d", __func__, err);
        } else {
            log_put(LOG_DEBUG "%s: CurrentX to matter %d OK", __func__, xy.x);
        }

        err = adm_write_u16(DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID,
            ADM_COLOR_CONTROL_CURRENT_Y_AID, xy.y);
        if (err) {
            log_put(LOG_WARN "%s: matter write err %d", __func__, err);
        } else {
            log_put(LOG_DEBUG "%s: CurrentY to matter %d OK", __func__, xy.y);
        }

        if (!strncmp(mode, "color", sizeof(mode))) {
            err = adm_write_u8(DEMO_ENDPOINT_LIGHTING, ADM_LEVEL_CONTROL_CID,
                ADM_CURRENT_LEVEL_AID, hsv.v);
            if (err) {
                log_put(LOG_WARN "%s: matter write err %d", __func__, err);
            } else {
                log_put(LOG_DEBUG "%s: CurrentLevel to matter %d OK", __func__, hsv.v);
            }
        }
    }

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

    if (!strcmp(sprop->name, "mode")) 
	{
        if (!strncmp(buf, "white", len))
		{
            color = 0;
        } else if (!strncmp(buf, "color", len)) 
		{
            color = 1;
        } else 
		{
            return AE_INVAL_VAL;
        }

        err = adm_write_enum8(DEMO_ENDPOINT_LIGHTING, ADM_COLOR_CONTROL_CID,
                ADM_COLOR_CONTROL_COLOR_MODE_AID, color ? 0 : 2); /* XXX: XY ? */
        if (err) {
            log_put(LOG_WARN "%s: matter write err %d", __func__, err);
        } else {
            log_put(LOG_DEBUG "%s: %s to matter %d OK", __func__, sprop->name, color ? 0 : 2);
        }

        demo_post_light_event(kLightAction_Mode2, (uint32_t)color);
    }

	log_put(LOG_DEBUG "%s: %s %s", __func__, sprop->name, (const char *)sprop->val);

	return AE_OK;
}

/*
 * Demo set function for command property.
 */
static enum ada_err demo_cmd_set(struct ada_sprop *sprop, const void *buf,
	size_t len)
{
	if (sprop->type != ATLV_UTF8) {
		return AE_INVAL_TYPE;
	}

	if (len > sizeof(_log) - 1) {
		len = sizeof(_log) - 1;
	}
	memcpy(_log, buf, len);
	_log[len] = '\0';
	ada_sprop_send_by_name("log");
	return AE_OK;
}


/*****************************************************属性接收处理******************************************************************/
static enum ada_err wlt_attributes_set(struct ada_sprop *sprop,const void *buf, size_t len)
{
	int ret = 0;
	static unsigned int ack_count;
	
	
	
	//printf("%s: %d  %s \r\n",__func__,sprop->type, sprop->name);

	if (sprop->type == ATLV_BOOL) //bool
	{
		u32 val = *(u32 *)buf;
		if (val > 1)
		{
			return AE_INVAL_VAL;
		}
		if (sprop->val_len != sizeof(u8))
		{
			return AE_LEN;
		}
		*(u8 *)sprop->val = val;		
		if (sprop->val == &power)
		{
			light_power_set(power);
		}
		printf("[bool]%s: %s set to %u\r\n",__func__, sprop->name, *(u8 *)sprop->val);
	}

	else if (sprop->type == ATLV_INT || sprop->type == ATLV_CENTS) //整形
	{

	 	
  
		s32 val = *(s32 *)buf;

		switch (sprop->val_len)
		{
			case 1:
				if (val < MIN_S8 || val > MAX_S8) {
					return AE_INVAL_VAL;
				}
				*(s8 *)sprop->val = val;
				break;
			case 2:
				if (val < MIN_S16 || val > MAX_S16) {
					return AE_INVAL_VAL;
				}
				*(s16 *)sprop->val = val;
				break;
			case 4:
				*(s32 *)sprop->val = *(s32 *)buf;
				break;
		}
	
	
		 if(sprop->val == &big_mode)
		{
			light_mode_set(big_mode);		
		//	demo_send_prop("mode");	
			my_printf("[int]%s: set to %d\r\n", sprop->name, big_mode);	
		}
		else if(sprop->val == &brightness)
		{
			light_bright_set(brightness);				
		}
		else if(sprop->val == &scene)
		{
			light_magicunit_set(scene);			
			my_printf("[int]%s: set to %d\r\n", sprop->name, scene);
					
		}
		else if(sprop->val == &custome)
		{

			light_custome_unit_set(custome);

		}
		else if(sprop->val == &speed)
		{
			light_speed_set(speed);
		}
		else if(sprop->val == &temperature)//
		{
			light_temper_set(temperature);	
		}
		else if(sprop->val == &color_select)
		{
			light_color_set(color_select);				
		}
		else if(sprop->val == &music)
		{
			light_musicunit_set(music);				
		}
		else if(sprop->val == &sensitivity)
		{
			light_sensitivity_set(sensitivity); 			
		}
		else if(sprop->val == &rgb_sequence)
		{
			light_rgb_sequence_set(rgb_sequence); 			
		}


	}

	if (sprop->ack_id)
	{
		ada_sprop_send_ack(sprop, 0, ++ack_count);
	}
	
	if(sprop->val != &brightness && sprop->val != &speed) 
	{
		if( sprop->val != &power) set_auto_flag(0);//除开关亮度速度 先把AUTO置0
		
		iotalink_light_ctrl_process();	
	}

		

	return AE_OK;
}


static struct ada_sprop demo_props[] = {
	/*
	 * version properties
	 * oem_host_version is the template version and must be sent first.
	 */
	{ "oem_host_version", ATLV_UTF8,template_version, sizeof(template_version),ada_sprop_get_string, NULL},
	{ "version", ATLV_UTF8, &version[0], sizeof(version),ada_sprop_get_string, NULL},
	{ "factory_name", ATLV_UTF8,factory_name, sizeof(factory_name),ada_sprop_get_string, NULL},
	{ "device_id", ATLV_UTF8, &device_id[0], sizeof(device_id),ada_sprop_get_string, NULL},
//	{ "purchase_order", ATLV_UTF8,purchase_order, sizeof(purchase_order),ada_sprop_get_string, NULL},
//	{ "led_driver", ATLV_UTF8, &led_driver[0], sizeof(led_driver),ada_sprop_get_string, NULL},
//	{ "mode", ATLV_UTF8, mode, sizeof(mode),ada_sprop_get_string, demo_string_set},
//	{ "device_rename", ATLV_UTF8, &device_rename[0], sizeof(device_rename),ada_sprop_get_string, demo_string_set},

	/*
	 * boolean properties.
	 */
	{ "alexa_enabled", ATLV_BOOL, &alexa_enabled, sizeof(alexa_enabled),ada_sprop_get_bool, demo_bool_set},
	{ "google_enable", ATLV_BOOL, &google_enable, sizeof(google_enable),ada_sprop_get_bool, demo_bool_set },
	{ "local_voice_enable", ATLV_BOOL, &local_voice_enable, sizeof(local_voice_enable),ada_sprop_get_bool, demo_bool_set },


	//{ "power", ATLV_BOOL, &power, sizeof(power),ada_sprop_get_bool, demo_bool_set },

	/*
	 * Integer properties.
	 */
#if 0
//	{ "brightness", ATLV_INT, &brightness, sizeof(brightness),ada_sprop_get_int, demo_int_set },
//	{ "color_bright", ATLV_INT, &color_bright, sizeof(color_bright),ada_sprop_get_int, demo_int_set },
//	{ "color_saturation", ATLV_INT, &color_saturation, sizeof(color_saturationada_sprop_get_int, demo_int_set },
//	{ "color_select", ATLV_INT, &color_select, sizeof(color_select),ada_sprop_get_int, demo_int_set },
//	{ "color_temp", ATLV_INT, &color_temp, sizeof(color_temp),ada_sprop_get_int, demo_int_set },
//	{ "wifi_rssi", ATLV_INT, &wifi_rssi, sizeof(wifi_rssi),ada_sprop_get_int, demo_int_set },
#endif
	/*
	 * String properties.
	 */
	{ "cmd", ATLV_UTF8, "", 0, ada_sprop_get_string, demo_cmd_set },
	{ "log", ATLV_UTF8, _log, sizeof(_log), ada_sprop_get_string, NULL },


	//wlt_add 开关
	{ "Power", ATLV_BOOL, &power, sizeof(power),ada_sprop_get_bool, wlt_attributes_set },

//	{ "Mode", ATLV_INT, &mode, sizeof(mode),ada_sprop_get_int, wlt_attributes_set },
//和原matter冲突暂换
	{ "big_mode", ATLV_INT, &big_mode, sizeof(big_mode),ada_sprop_get_int, wlt_attributes_set },

	{ "custome", ATLV_INT, &custome, sizeof(custome),ada_sprop_get_int, wlt_attributes_set },

	{ "Brightness", ATLV_INT, &brightness, sizeof(brightness),ada_sprop_get_int, wlt_attributes_set },

	{ "Temperature", ATLV_INT, &temperature, sizeof(temperature),ada_sprop_get_int, wlt_attributes_set },

	{ "Color", ATLV_INT, &color_select, sizeof(color_select),ada_sprop_get_int, wlt_attributes_set },

	{ "Scene", ATLV_INT,&scene, sizeof(scene),ada_sprop_get_int, wlt_attributes_set },

	{ "Speed", ATLV_INT, &speed, sizeof(speed),ada_sprop_get_int, wlt_attributes_set },

	{ "Music", ATLV_INT, &music, sizeof(music),ada_sprop_get_int, wlt_attributes_set },
	
	{ "Sensitivity", ATLV_INT, &sensitivity, sizeof(sensitivity),ada_sprop_get_int, wlt_attributes_set },
};

static void prop_send_by_name(const char *name)
{
	enum ada_err err;

	err = ada_sprop_send_by_name(name);
	if (err) {
		log_put(LOG_ERR "demo: %s: send of %s: err %d",
				__func__, name, err);
	}
}
#endif
void demo_send_prop(const char *name)
{
#ifdef AYLA_BATCH_PROP_SUPPORT
	/*
	 * If batch hold is enabled, batch changes for selected from-device
	 * properties.
	 */
	if (node_batch_hold && (
	    !strcmp(name, "log") ||
	    !strcmp(name, "output") ||
	    !strcmp(name, "decimal_out"))) {
		demo_put_prop_to_batch(NULL, name);
	} else {
		prop_send_by_name(name);
	}
#else
#ifdef AYLA_ADA_SERVICE_ENABLE
	prop_send_by_name(name);
#endif
#endif
}

static void demo_lc_completed(bool timeout)
{
#if 0
    struct app_event evt;

    evt.type = kEventType_Light;
    evt.light_event.action = kLightAction_On;

    app_event_post(&evt);
	
    if (timeout) {
        onboarding = false;
    }
#else
	iotalink_light_ctrl_data_init();

#endif
}

static void demo_lc_do_sync(u32 timeout)
{
    u16 mireds;
	enum ada_err err;
#ifdef AYLA_ADA_SERVICE_ENABLE
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
        log_put(LOG_DEBUG "%s: send brightness  %ld OK", __func__,
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
#else
    /* set initial values to sync up */
    power = 1;
    strncpy(mode, "white", sizeof(mode));
    brightness = color_bright = 87;
    color_temp = 50;
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

#define INITIAL_WIFI_SCAN_RETRY_COUNT  3
static int8_t initial_wifi_scan_rounds_left;

static void demo_start_initial_wifi_scan(void)
{
    wifi_scan_config_t wif_scan_config;
    wise_err_t scan_err;

    log_put(LOG_INFO "Start initial wifi scan, rounds left after this: %u", initial_wifi_scan_rounds_left);
    adb_bt_scan_cancel_wrap();

    demo_set_scan_source(1);

    memset(&wif_scan_config, 0, sizeof(wifi_scan_config_t));
    wif_scan_config.scan_type            = WIFI_SCAN_TYPE_ACTIVE;
    wif_scan_config.scan_time.active.min = 105;
    wif_scan_config.scan_time.active.max = 130;
    scan_err = wise_wifi_scan_start(&wif_scan_config, true, WIFI_IF_STA);
    if (scan_err != WISE_OK) {
        log_put(LOG_ERR "Initial wifi scan start failed: %d", scan_err);
        initial_wifi_scan_rounds_left = 0;
        adb_bt_scan_start_wrap();
        return;
    }

    adb_bt_scan_start_wrap();
}

#define BUTTON_LONG_PRESSED_FLASH_STATE    (0xAA)

static void demo_lc_run_events(struct app_event *events,
        size_t event_count, int repeat, u32 timeout,
        void (*callback)(bool timeout))
{
    size_t i;

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < event_count; i++) {
        lighting_ctrl_add_event(&events[i]);
    }
    lighting_ctrl_run(repeat, timeout, callback);
}

static void demo_lc_start_commissioning(void)
{
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

    if (onboarding) {
        return;
    }

    demo_lc_run_events(evt, ARRAY_LEN(evt), -1, 60000 /* 1 min. */,
        demo_lc_completed);
    onboarding = true;
}

static void demo_lc_complete_commissioning(void)
{
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

    demo_lc_run_events(evt, ARRAY_LEN(evt), 1, 0, demo_lc_sync);
    onboarding = false;
}

static void demo_matter_event_cb(enum adm_event_id id)
{
	uint8_t state = 0;

	log_put(LOG_DEBUG2 "%s %d", __func__, id);

	switch (id) {
    case ADM_EVENT_INITIALIZED:
    {
        /*
         * For Some Conner Cases, we need to set light manager ON.
         * One Case: Home APP still on color set page, but device already reboots,
         * cached cmds cannot set color mode becasue light manager is OFF after rebooting.
         */
        log_put(LOG_INFO "Set light manager state to ON.");
        demo_post_light_event(kLightAction_On, 1);

        if (g_initial_scan_enable)
        {
            /* Start initial wifi scan */
            log_put(LOG_INFO "Start a initial wifi scan");
            initial_wifi_scan_rounds_left = INITIAL_WIFI_SCAN_RETRY_COUNT;
            demo_start_initial_wifi_scan();
        }
        else
            log_put(LOG_INFO "Cancel a initial wifi scan");

        break;
    }
    case ADM_EVENT_WIFI_SCAN_DONE:
        initial_wifi_scan_rounds_left--;
        if (g_initial_scan_enable && demo_get_scan_source() == 1 && initial_wifi_scan_rounds_left > 0) {
            // log_put(LOG_INFO "Inital scan done trigger scan again");
            demo_start_initial_wifi_scan();
        }
		break;
	case ADM_EVENT_IPV4_UP:
        ip_up = 1;
#ifdef AYLA_ADA_SERVICE_ENABLE
        if (ac_conn_timer) {
            log_put(LOG_INFO "IPV4 up, notify ADA after %d s!", AC_DELAY_TIMER_MSECS/1000);
            osTimerStart(ac_conn_timer, msecs_to_ticks(AC_DELAY_TIMER_MSECS));
        } else {
            log_put(LOG_INFO "IPV4 up, notify ADA at once!");
            ada_client_ip_up();
            ada_client_health_check_en();
        }
#endif
		break;
	case ADM_EVENT_IPV4_DOWN:
        ip_up = 0;
#ifdef AYLA_ADA_SERVICE_ENABLE
		ada_client_ip_down();
#endif
		break;
	case ADM_EVENT_COMMISSIONING_SESSION_STARTED:
	case ADM_EVENT_COMMISSIONING_WINDOW_OPENED:
#ifdef BUTTON_LONG_PRESSED_FLASH_STATE
    {
        scm_partition_read(FLASH_PARTITION_TMP, 0, &state, sizeof(state));
        log_put(LOG_INFO "button long pressed flash state: 0x%02x\n", state);
        if (state == BUTTON_LONG_PRESSED_FLASH_STATE)
        {
            scm_partition_erase(FLASH_PARTITION_TMP, 0, 4096);
            iotalink_button_state_set(0xff);
            demo_lc_start_commissioning();
        }
    }
#else
        demo_lc_start_commissioning();
#endif
        break;
	case ADM_EVENT_COMMISSIONING_SESSION_STOPPED:
	case ADM_EVENT_COMMISSIONING_WINDOW_CLOSED:
        break;
	case ADM_EVENT_COMMISSIONING_COMPLETE:
        demo_lc_complete_commissioning();
        break;
    case ADM_EVENT_ALL_FABRIC_REMOVED:
    {
#if 1
        /*
        * Note: For Iphone Alexa APP Conner Case - Reboot Device.
        * Google Home & Apple Home can use WLAN ways to add device again, but Alexa can not.
        * Reboot after 3-5 secs, because matter 'remove' device need some time to delete 
        * old farbric stored files.
        */
        if (mf_remove_timer) {
            log_put(LOG_INFO "All matter farbrics are removed, reboot after %d s!", MF_REMOVE_DELAY_TIMER_MSECS/1000);
            osTimerStart(mf_remove_timer, msecs_to_ticks(MF_REMOVE_DELAY_TIMER_MSECS));
        }
#endif
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

    if (demo_sprop_is_ready()) {
        err = ada_sprop_send_by_name("power");
        if (err) {
            log_put(LOG_ERR "%s: send power: err %d",
                __func__, err);
        }
    }

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

	if (type != ZCL_INT8U_ATTRIBUTE_TYPE) {
		log_put(LOG_ERR "%s: invalid type %u", __func__, type);
		return AE_INVAL_TYPE;
	}

	if ((size != 1) || (value == NULL)) {
		log_put(LOG_ERR "%s: invalid size %u or value data %p",
		    __func__, size, value);
		return AE_INVAL_VAL;
	}

    log_put(LOG_INFO "%s level_control %u", __func__, *value);

    if (*value <= 1) 
	{
        /* XXX: what should we do? It seems to be the best to ignore it.
         */
        log_put(LOG_WARN "%s: level_control %d ignored", __func__, *value);
        return AE_OK;
    }

    /* XXX: why do they have to differ? */
    brightness = color_bright = (int)demo_convert_range(*value, 0, 254, 0, 100);

    if (demo_sprop_is_ready()) {
        err = ada_sprop_send_by_name("brightness");
        if (err) {
            log_put(LOG_ERR "%s: send brightness: err %d",
                __func__, err);
        }
        err = ada_sprop_send_by_name("color_bright");
        if (err) {
            log_put(LOG_ERR "%s: send color_bright: err %d",
                __func__, err);
        }
    }

    demo_post_light_event(kLightAction_Level, *value);

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
        if (demo_sprop_is_ready()) {
            err = ada_sprop_send_by_name("color_select");
            if (err) {
                log_put(LOG_ERR "%s: send color_select: err %d",
                    __func__, err);
            }
            /* XXX: update color_saturation ? */
        }

        demo_post_light_event(kLightAction_Color, val);

        log_put(LOG_INFO "New XY color: %u|%u", xy.x, xy.y);
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

        /* Workround: We get hsv.v wrong value rarely. */
        if (hsv.v < 2) {
            hsv.v = (u8)demo_convert_range((brightness > 5 ? brightness: 5), 0, 100, 0, 254);
        }
        rgb = HsvToRgb(hsv);
        val = (0 << 24 | rgb.r << 16 | rgb.g << 8 | rgb.b);

        color_select = (int)val;
        if (attribute == ADM_COLOR_CONTROL_CURRENT_SATURATION_AID) {
            color_saturation = demo_convert_range(hsv.s, 0, 254, 0, 100);
        }

        if (demo_sprop_is_ready()) {
            err = ada_sprop_send_by_name("color_select");
            if (err) {
                log_put(LOG_ERR "%s: send color_select: err %d",
                    __func__, err);
            }
            if (attribute == ADM_COLOR_CONTROL_CURRENT_SATURATION_AID) {
                err = ada_sprop_send_by_name("color_saturation");
                if (err) {
                    log_put(LOG_ERR "%s: send color_saturation: err %d",
                        __func__, err);
                }
            }
        }

        demo_post_light_event(kLightAction_Color, val);

        log_put(LOG_INFO "New HSV color: %u|%u|%u, RGB: %u|%u|%u", hsv.h, hsv.s, hsv.v, rgb.r, rgb.g, rgb.b);
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

        if (demo_sprop_is_ready()) {
            err = ada_sprop_send_by_name("color_temp");
            if (err) {
                log_put(LOG_ERR "%s: send color_temp: err %d",
                    __func__, err);
            }
        }

        demo_post_light_event(kLightAction_Temp, (uint32_t)temp);

        log_put(LOG_INFO "%s: mireds %u, temp %d", __func__, mireds, temp);
    }
    /* Color mode */
    else if (attribute == ADM_COLOR_CONTROL_COLOR_MODE_AID)
    {
        if (((*value == 0 || *value == 1) && strcmp(mode, "color"))
                || (*value == 2 && strcmp(mode, "white"))) {
            strncpy(mode, *value == 2 ? "white" : "color", sizeof(mode));
            if (demo_sprop_is_ready()) {
                err = ada_sprop_send_by_name("mode");
                if (err) {
                    log_put(LOG_ERR "%s: send mode: err %d",
                        __func__, err);
                }
            }
        }

        demo_post_light_event(kLightAction_Mode2, 1);

        log_put(LOG_INFO "%s: color mode: %u", __func__, *value);
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

#ifdef AYLA_ADA_SERVICE_ENABLE
static int demo_run_ftm(void)
{
    struct app_event evt;

    if (!ftm_is_enabled()) {
        return 0;
    }

    evt.type = kEventType_Install;
    evt.install_event.callback = ftm_run;
    evt.install_event.arg = 0;

    app_event_post(&evt);

    return 1;
}

static void ac_timer_handle(void *arg)
{
#if 0
    /* Use ADA Client Task to handle cb */
    client_callback_pend(&ac_conn_cb);
#else
    ada_client_ip_up();
    ada_client_health_check_en();
#endif
}
#endif

static void mf_remove_handle(void *arg)
{
#ifdef AYLA_ADA_SERVICE_ENABLE
    /* Only reboot */
    ada_conf_reset(0);
#else
    wise_restart();
#endif
}

void demo_init(void)
{
#ifdef AYLA_ADA_SERVICE_ENABLE
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	int rc;
#endif
#endif

#if 0
    /* Use your own IC init apis */
    bp5758d_init();
    bp5758d_set_rgbcw_channel(0, 0, 0, 156, 44);
#endif

    app_event_init();
    app_event_install_handler(kEventType_Light, demo_evt_handler);
    app_event_install_handler(kEventType_Install, demo_evt_handler);

    lighting_mgr_init(LightMgr());
    lighting_ctrl_init();

#ifdef AYLA_ADA_SERVICE_ENABLE
    ftm_init();
    if (demo_run_ftm()) {
        /* This is FTM mode.
         * Skip everything else and go directly to the event loop.
         */
        return;
    }
#endif

	adm_init();
    init_ayla_clusters();
	adm_event_cb_register(demo_matter_event_cb);
	adm_start(demo_test_cert_declaration,sizeof(demo_test_cert_declaration));

    adb_hyd_svc_register(NULL);
    adb_ayla_svc_register(NULL);
    adb_wifi_cfg_svc_register(NULL);
    adb_conn_svc_register(NULL);
    adb_ota_svc_register(NULL);

#ifdef AYLA_ADA_SERVICE_ENABLE
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
#endif

	adm_attribute_change_cb_register(&demo_on_off_cb_entry);
	adm_attribute_change_cb_register(&demo_level_control_cb_entry);
	adm_attribute_change_cb_register(&demo_color_control_cb_entry);

#ifdef AYLA_ADA_SERVICE_ENABLE
	ada_sprop_mgr_register("demo_matter", demo_props, ARRAY_LEN(demo_props));

    if (!ac_conn_timer) {
        ac_conn_timer = osTimerNew(ac_timer_handle, osTimerOnce, NULL, NULL);
        // callback_init(&ac_conn_cb, ayla_client_ip_up_cb, NULL);
    }
#endif
    if (!mf_remove_timer) {
        mf_remove_timer = osTimerNew(mf_remove_handle, osTimerOnce, NULL, NULL);
    }
}

#define BUTTON_SHORT_PRESSED_PERIOD_MIN 10
#define BUTTON_SHORT_PRESSED_PERIOD_MAX 800
#define BUTTON_LONG_PRESSED_PERIOD 3000

void demo_button_toggle(unsigned long pressed, unsigned long released)
{
    uint8_t state = BUTTON_LONG_PRESSED_FLASH_STATE;
    if (pressed && ((released - pressed) > BUTTON_SHORT_PRESSED_PERIOD_MIN) &&
        ((released - pressed) < BUTTON_SHORT_PRESSED_PERIOD_MAX))
    {
        log_put("Button short pressed");
        /* Turn on/off the light by short press */
        light_power_set(!power);
        iotalink_light_ctrl_process();
    }
    if (pressed && ((released - pressed) > BUTTON_LONG_PRESSED_PERIOD))
    {
        log_put(LOG_INFO "Button long pressed");
        /* Set long press flash state */
        // todo: use FS instead of other flash partition
        scm_partition_erase(FLASH_PARTITION_TMP, 0, 4096);
        scm_partition_write(FLASH_PARTITION_TMP, 0, &state, sizeof(state));
        /* It's not recommended to do so */
        iotalink_button_state_set(state);
        /* Set the light flashing to indicate the network commission state */
        onboarding = false;
        demo_lc_start_commissioning();
        osDelay(MS_TO_TICKS(2000));
#ifdef AYLA_ADA_SERVICE_ENABLE
        ada_conf_reset(2);
#else
        scm_fs_clear_all_config_value(NULL);
        osDelay(MS_TO_TICKS(1000));
        wise_restart();
#endif
    }
}

void demo_idle(void)
{
    struct app_event event;

#ifdef AYLA_ADA_SERVICE_ENABLE
    ada_sprop_send_to_by_name("oem_host_version", NODES_ALL);
    ada_sprop_send_to_by_name("version", NODES_ALL);
#endif

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
