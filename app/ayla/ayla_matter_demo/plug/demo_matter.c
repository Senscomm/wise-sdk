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

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <timers.h>

/* XXX: cmsis_os2 doesn't support xQueuePeek yet. */

#include <app-common/zap-generated/attribute-type.h>

#include <ada/libada.h>
#include <ada/server_req.h>
#include <ada/sprop.h>
#include <adm/adm_csa_numbers.h>
#include <adm/adm.h>
#include <ayla/log.h>
#include <ayla/timer.h>

#include "app_int.h"

#include "scm_gpio.h"
#include "gpio_types.h"
#include "build.h"
#include "host_prop_mgr.h"

static struct {
	u8 blue_led;
    u8 outlet;
} prop_conf_val_map;

static struct prop_conf_metadata prop_conf_table[] = {
    {
        .prop_name = "night_mode",
        .token =CT_night_mode, 
        .item = {
            .name = "prop/night_mode", 
            .type = ATLV_BOOL, 
            .val = &(prop_conf_val_map.blue_led),
            .len = sizeof(u32)
        }
    },
    {
        .prop_name = "outlet1",
        .token =CT_outlet1, 
        .item = {
            .name = "prop/outlet1", 
            .type = ATLV_BOOL, 
            .val = &prop_conf_val_map.outlet, 
            .len = sizeof(u32)
        }
    },
    /* must be the last, we do not pass the table size */
    {
        .prop_name = NULL,
    }
};

#define GPIO_OUTPUT_PIN_SEL	\
    (BIT64(GPIO_WIFI_LED) | BIT64(GPIO_POWER_LED) | BIT64(GPIO_LINK_LED) | BIT64(GPIO_RELAY_OUT))

#define GPIO_INPUT_PIN_SEL	BIT64(GPIO_BOOT_BUTTON)

#define DEMO_ENDPOINT_SWITCH	1

#define DEMO_SYNC_RETRY_MAX	10

#define BUTTON_SHORT_PRESSED_PERIOD_MIN 200
#define BUTTON_SHORT_PRESSED_PERIOD_MAX 2000
#define BUTTON_LONG_PRESSED_PERIOD 5000

#define LED_INDICATOR_BLINK_PERIOD 500

enum demo_queue_event {
	DEMO_FROM_MATTER_ON,
	DEMO_FROM_MATTER_OFF,
	DEMO_TO_MATTER_ON,
	DEMO_TO_MATTER_OFF,
	DEMO_NONE
};

static char version[] = "ADA-" ADA_VERSION BUILD_NAME "-" SDK_VERSION;
char template_version[] = DEMO_TEMPLATE_VERSION;
static u8 blue_led;
static u8 green_led;
static u8 outlet;
static char factory_name[] = DEMO_FACTORY_NAME;
static char purchase_order[] = DEMO_PURCHASE_ORDER;
static char device_id[] = DEMO_DEVICE_ID;
static char device_rename[32];
static u8 alexa_enabled;
static u8 google_enable;
static u8 local_voice_enable;
static int wifi_rssi;
static char log[1024];

static xQueueHandle demo_evt_queue;

TimerHandle_t g_led_indicator_timer = NULL;

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

typedef int gpio_num_t;

int gpio_get_level(int gpio)
{
	//printf("GPIO: get pin=%d\n", gpio);
	uint8_t value;

	if (scm_gpio_read((uint32_t)gpio, &value) != WISE_OK) {
		printf("Error reading GPIO level\n");
		return -1;
	}
	return (int)value;
}

void gpio_set_level(int gpio, u8 level)
{
	if (scm_gpio_write((uint32_t)gpio, level) != WISE_OK) {
		printf("Error setting GPIO level\n");
	}
	return;
}

void gpio_config(gpio_config_t *config)
{
	for (int pin = 0; pin < 64; pin++) {
		if (config->pin_bit_mask & (1ULL << pin)) {
			enum scm_gpio_property property = SCM_GPIO_PROP_INPUT;

			if (config->mode == GPIO_MODE_OUTPUT) {
				property = SCM_GPIO_PROP_OUTPUT;
			} else if (config->pull_up_en) {
				property = SCM_GPIO_PROP_INPUT_PULL_UP;
			} else if (config->pull_down_en) {
				property = SCM_GPIO_PROP_INPUT_PULL_DOWN;
			}

			if (scm_gpio_configure(pin, property) != WISE_OK) {
				printf("Error configuring GPIO pin %d\n", pin);
			}
		}
	}
	return;
}

static void set_led(gpio_num_t gpio_num, u8 on)
{
	/*
	 * LEDs are active low.
	 *
	 * GPIO_pin--resistor--LED--VCC
	 */
	if (on) {
		gpio_set_level(gpio_num, 0);
	} else {
		gpio_set_level(gpio_num, 1);
	}
}

static void set_outlet(gpio_num_t gpio_num, u8 on)
{
	if (on) {
		gpio_set_level(gpio_num, 1);
		printf("Outlet ON\n");
	} else {
		gpio_set_level(gpio_num, 0);
		printf("Outlet OFF\n");
	}
}

static enum ada_err demo_sync_to_matter(u8 on_off)
{
	enum ada_err err;
	err = adm_write_boolean(DEMO_ENDPOINT_SWITCH, ADM_ON_OFF_CID,
	    ADM_ON_OFF_AID, on_off);
	if (err) {
		log_put(LOG_DEBUG "demo: %s: matter write err %d",
		    __func__, err);
	} else {
		log_put(LOG_INFO "%s: To matter %d OK", __func__, on_off);
	}
	return err;
}

static enum ada_err demo_sync_to_cloud(u8 on_off)
{
	enum ada_err err = AE_OK;
	log_put(LOG_DEBUG "%s: From matter %d", __func__, on_off);
	if (outlet != on_off) {
		log_put(LOG_INFO "%s: Set outlet to %d",
		    __func__, on_off);
		outlet = on_off;
		err = ada_sprop_send_by_name("outlet1");
		if (err) {
			log_put(LOG_ERR "%s: send outlet1: err %d",
			    __func__, err);
		}
	}
	return err;
}

static void demo_write_notify_event(u8 on_off)
{
	uint32_t event = DEMO_NONE;
	if (on_off) {
		event = DEMO_TO_MATTER_ON;
	} else {
		event = DEMO_TO_MATTER_OFF;
	}
	xQueueSend(demo_evt_queue, &event, 10);
}

/*
 * Demo set function for bool properties.
 */
static enum ada_err demo_led_set(struct ada_sprop *sprop,
		const void *buf, size_t len)
{
	enum ada_err ret;

	ret = ada_sprop_set_bool(sprop, buf, len);
	if (ret) {
		return ret;
	}
    if (sprop->val == &blue_led) {
		set_led(GPIO_WIFI_LED, blue_led);
	}

	host_prop_unsync_tag(sprop, prop_conf_table);
	log_put(LOG_INFO "%s on_off %u", __func__, blue_led);

	demo_write_notify_event(blue_led);

	return AE_OK;
}

static enum ada_err demo_outlet_set(struct ada_sprop *sprop,
		const void *buf, size_t len)
{
	enum ada_err ret;

	ret = ada_sprop_set_bool(sprop, buf, len);
	if (ret) {
		return ret;
	}
    if (sprop->val == &outlet) {
		set_outlet(GPIO_RELAY_OUT, outlet);
	}

	host_prop_unsync_tag(sprop, prop_conf_table);
	log_put(LOG_INFO "%s on_off %u", __func__, outlet);

	demo_write_notify_event(outlet);

	return AE_OK;
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

	log_put(LOG_DEBUG "%s: %s %u", __func__, sprop->name, *(uint8_t *)sprop->val);

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

	if (len > sizeof(log) - 1) {
		len = sizeof(log) - 1;
	}
	memcpy(log, buf, len);
	log[len] = '\0';
	ada_sprop_send_by_name("log");
	return AE_OK;
}

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
	{ "device_rename", ATLV_UTF8, &device_rename[0], sizeof(device_rename),
		ada_sprop_get_string, demo_string_set},
	/*
	 * boolean properties. It associates with a LED and a button.
	 */
	{ "alexa_enabled", ATLV_BOOL, &alexa_enabled, sizeof(alexa_enabled),
		ada_sprop_get_bool, demo_bool_set},
	{ "google_enable", ATLV_BOOL, &google_enable, sizeof(google_enable),
		ada_sprop_get_bool, demo_bool_set },
	{ "local_voice_enable", ATLV_BOOL, &local_voice_enable, sizeof(local_voice_enable),
		ada_sprop_get_bool, demo_bool_set },
	{ "night_mode", ATLV_BOOL, &blue_led, sizeof(blue_led),
		ada_sprop_get_bool, demo_led_set },
	{ "outlet1", ATLV_BOOL, &outlet, sizeof(outlet),
		ada_sprop_get_bool, demo_outlet_set },

	/*
	 * Integer properties.
	 */
	{ "wifi_rssi", ATLV_INT, &wifi_rssi, sizeof(wifi_rssi),
		ada_sprop_get_int, NULL },
	/*
	 * String properties.
	 */
	{ "cmd", ATLV_UTF8, "", 0, ada_sprop_get_string, demo_cmd_set },
	{ "log", ATLV_UTF8, log, sizeof(log),
		ada_sprop_get_string, NULL },
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

void led_indicator_cancel_timer(void)
{
	if (g_led_indicator_timer == NULL)
		return;

	if (xTimerStop(g_led_indicator_timer, 0) == pdFAIL) {
		log_put(LOG_WARN "app timer stop failed!");
	}
}

void led_indicator_start_timer(uint32_t timeout_ms)
{
	if (g_led_indicator_timer == NULL)
		return;

	if (xTimerIsTimerActive(g_led_indicator_timer)) {
		log_put(LOG_WARN "app timer already started!");
		led_indicator_cancel_timer();
	}

	// timer is not active, change its period to required value (== restart).
	// FreeRTOS- Block for a maximum of 100 ticks if the change period command
	// cannot immediately be sent to the timer command queue.
	if (xTimerChangePeriod(g_led_indicator_timer, (timeout_ms / portTICK_PERIOD_MS), 100) != pdPASS) {
		log_put(LOG_ERR "led_indicator timer start() failed");
	}
}

void led_indicator_timer_cb(TimerHandle_t xTimer)
{
	static u8 state = 0;

	set_led(GPIO_WIFI_LED, state);
	state = 1- state;
	led_indicator_start_timer(LED_INDICATOR_BLINK_PERIOD);
}

static void demo_led_indicator_init(void)
{
	g_led_indicator_timer = xTimerCreate("indicatorTmr", LED_INDICATOR_BLINK_PERIOD, false, NULL, led_indicator_timer_cb);
	if (g_led_indicator_timer == NULL) {
		log_put(LOG_ERR "led_indicator timer init failed");
	}
}

static void demo_matter_event_cb(enum adm_event_id id)
{
	log_put(LOG_DEBUG "%s %d", __func__, id);
	switch (id) {
	case ADM_EVENT_IPV4_UP:
		ada_client_ip_up();
		ada_client_health_check_en();
		led_indicator_cancel_timer();
		set_led(GPIO_WIFI_LED, 1); /* turn on WiFi LED to indicate connection up */
		break;

	case ADM_EVENT_IPV4_DOWN:
		ada_client_ip_down();
		set_led(GPIO_WIFI_LED, 0); /* turn off WiFi LED to indicate connection down */
		break;

	case ADM_EVENT_COMMISSIONING_SESSION_STARTED:
	case ADM_EVENT_COMMISSIONING_WINDOW_OPENED:
		led_indicator_start_timer(LED_INDICATOR_BLINK_PERIOD);
		break;
	case ADM_EVENT_COMMISSIONING_SESSION_STOPPED:
	case ADM_EVENT_COMMISSIONING_WINDOW_CLOSED:
		led_indicator_cancel_timer();
		set_led(GPIO_WIFI_LED, 0);
		break;
	default:
		break;
	}
}

static void demo_gpio_init(void)
{
	log_put(LOG_INFO "%s: Init completed", __func__);
}

static enum ada_err demo_on_off_cb(u8 post_change, u16 endpoint,
    u32 cluster, u32 attribute, u8 type, u16 size, u8 *value)
{
	uint32_t event;

	if (type != ZCL_BOOLEAN_ATTRIBUTE_TYPE) {
		log_put(LOG_INFO "%s: invalid type %u", __func__, type);
		return AE_INVAL_TYPE;
	}

	if ((size != 1) || (value == NULL)) {
		log_put(LOG_INFO "%s: invalid size %u or value data %p",
		    __func__, size, value);
		return AE_INVAL_VAL;
	}

	outlet = *value;
	set_outlet(GPIO_RELAY_OUT, outlet);
	log_put(LOG_INFO "%s on_off %u", __func__, *value);

	if (*value) {
		event = DEMO_FROM_MATTER_ON;
	} else {
		event = DEMO_FROM_MATTER_OFF;
	}
	xQueueSend(demo_evt_queue, &event, 10);

	return AE_OK;
}

static struct adm_attribute_change_callback demo_on_off_cb_entry =
    ADM_ACCE_INIT(ADM_ACCE_POST_CHANGE,
	DEMO_ENDPOINT_SWITCH, ADM_ON_OFF_CID, ADM_ON_OFF_AID,
	demo_on_off_cb);

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

	/* create a queue to handle gpio event from isr */
	demo_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	AYLA_ASSERT(demo_evt_queue != NULL);

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

	demo_gpio_init();
	demo_led_indicator_init();
	adm_attribute_change_cb_register(&demo_on_off_cb_entry);

	ada_sprop_mgr_register("demo_matter",
	    demo_props, ARRAY_LEN(demo_props));

}

static void demo_button_toggle(unsigned long pressed, unsigned long released)
{
	if (pressed && ((released - pressed) > BUTTON_SHORT_PRESSED_PERIOD_MIN) && ((released - pressed) < BUTTON_SHORT_PRESSED_PERIOD_MAX)) {
		log_put(LOG_INFO "Button short pressed");
		outlet = !outlet;
		set_outlet(GPIO_RELAY_OUT, outlet);
		demo_write_notify_event(outlet);
		prop_send_by_name("outlet1");
	}

	if (pressed && ((released - pressed) > BUTTON_LONG_PRESSED_PERIOD)) {
		log_put(LOG_INFO "Button long pressed");
		/* Trigger factory reset */
		ada_conf_reset(1);
	}
}

void demo_idle(void)
{
	static unsigned long button_pressed;
	static unsigned long button_released;
	uint32_t event;
	enum ada_err err;
	uint32_t retry_count = 0;
	gpio_config_t io_conf;

	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;

	gpio_config(&io_conf);

	prop_send_by_name("oem_host_version");
	prop_send_by_name("version");

	/* start with all LEDs to off but power led to on */
	set_led(GPIO_WIFI_LED, 0);
	set_led(GPIO_POWER_LED, 1);
	set_led(GPIO_LINK_LED, 0);

	scm_gpio_configure(GPIO_BOOT_BUTTON, SCM_GPIO_PROP_INPUT);

	host_prop_mgr_init(prop_conf_table, demo_props, ARRAY_LEN(demo_props));

	while (1) {
		if (gpio_get_level(GPIO_BOOT_BUTTON) == 0) {
			if (button_pressed == 0) {
				button_pressed = time_now();
				log_put(LOG_DEBUG "Button pressed");
			}
		} else {
			if (button_pressed) {
				button_released = time_now();
				log_put(LOG_DEBUG "Button released");
				demo_button_toggle(button_pressed,
				    button_released);
				button_pressed = 0;
				button_released = 0;
			}
		}

		if (xQueuePeek(demo_evt_queue, &event, pdMS_TO_TICKS(10))) {
			switch (event) {
			case DEMO_FROM_MATTER_ON:
				err = demo_sync_to_cloud(1);
				if (err == AE_NOT_FOUND) {
					/* Not found prop, should drop it */
					err = AE_OK;
				}
				break;
			case DEMO_FROM_MATTER_OFF:
				err = demo_sync_to_cloud(0);
				if (err == AE_NOT_FOUND) {
					/* Not found prop, should drop it */
					err = AE_OK;
				}
				break;
			case DEMO_TO_MATTER_ON:
				err = demo_sync_to_matter(1);
				break;
			case DEMO_TO_MATTER_OFF:
				err = demo_sync_to_matter(0);
				break;
			default:
				log_put(LOG_DEBUG "%s: Ignore event %lu",
				    __func__, event);
				err = AE_OK;
				break;
			}
			if (err == AE_OK) {
				retry_count = 0;
				xQueueReceive(demo_evt_queue, &event,
				    portMAX_DELAY);
			} else if (retry_count >= DEMO_SYNC_RETRY_MAX) {
				log_put(LOG_WARN "%s: Drop event %ld",
				    __func__, event);
				retry_count = 0;
				xQueueReceive(demo_evt_queue, &event,
				    portMAX_DELAY);
			} else {
				log_put(LOG_DEBUG "%s: handle event %lu error",
				    __func__, event);
				retry_count++;
				vTaskDelay(pdMS_TO_TICKS(50));
			}
		}
	}
}
