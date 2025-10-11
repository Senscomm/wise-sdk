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
#include "bp5758d.h"
#include "gpio_types.h"

#define GPIO_OUTPUT_PIN_SEL	\
    (BIT64(GPIO_BLUE_LED) | BIT64(GPIO_GREEN_LED) | BIT64(GPIO_LINK_LED))

#define GPIO_INPUT_PIN_SEL	BIT64(GPIO_BOOT_BUTTON)

#define DEMO_ENDPOINT_SWITCH	1

#define DEMO_SYNC_RETRY_MAX	10

enum demo_queue_event {
	DEMO_FROM_MATTER_ON,
	DEMO_FROM_MATTER_OFF,
	DEMO_TO_MATTER_ON,
	DEMO_TO_MATTER_OFF,
	DEMO_NONE
};

static char version[] = APP_NAME " " BUILD_STRING;
char template_version[] = DEMO_TEMPLATE_VERSION;
static u8 boot_button;
static u8 blue_led;
static u8 green_led;
static int input;
static int output;
static int decimal_in;
static int decimal_out;
static char log[1024];

static xQueueHandle demo_evt_queue;

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
	printf("GPIO: set pin=%d, level=%d\n", gpio, level);
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
		if (GPIO_BLUE_LED == gpio_num) {
			printf("Blue set ON\n");
			bp5758d_set_channel(BP5758D_CHANNEL_B, 20);
		} else if (GPIO_GREEN_LED == gpio_num) {
			printf("Green set ON\n");
			bp5758d_set_channel(BP5758D_CHANNEL_G, 20);
		}

		gpio_set_level(gpio_num, 0);
	} else {
		if (GPIO_BLUE_LED == gpio_num) {
			printf("Blue set OFF\n");
			bp5758d_set_channel(BP5758D_CHANNEL_B, 0);
		} else if (GPIO_GREEN_LED == gpio_num) {
			printf("Green set OFF\n");
			bp5758d_set_channel(BP5758D_CHANNEL_G, 0);
		}

		gpio_set_level(gpio_num, 1);
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
	if (blue_led != on_off) {
		log_put(LOG_INFO "%s: Set Blue LED to %d",
		    __func__, on_off);
		blue_led = on_off;
		gpio_set_level(GPIO_LED, on_off);
		bp5758d_set_channel(BP5758D_CHANNEL_B, on_off ? 20 : 0);
		err = ada_sprop_send_by_name("Blue_LED");
		if (err) {
			log_put(LOG_ERR "%s: send blue_led: err %d",
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
		set_led(GPIO_BLUE_LED, blue_led);
	} else {
		set_led(GPIO_GREEN_LED, green_led);
	}

	log_put(LOG_INFO "%s on_off %u", __func__, blue_led);

	demo_write_notify_event(blue_led);

	return AE_OK;
}

/*
 * Demo set function for integer and decimal properties.
 */
static enum ada_err demo_int_set(struct ada_sprop *sprop,
		const void *buf, size_t len)
{
	enum ada_err ret;

	ret = ada_sprop_set_int(sprop, buf, len);
	if (ret) {
		return ret;
	}
	if (sprop->val == &input) {
		output = input;
		ada_sprop_send_by_name("output");
	} else if (sprop->val == &decimal_in) {
		decimal_out = decimal_in;
		ada_sprop_send_by_name("decimal_out");
	}
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
	/*
	 * boolean properties. It associates with a LED and a button.
	 */
	{ "Blue_button", ATLV_BOOL, &boot_button, sizeof(boot_button),
		ada_sprop_get_bool, NULL},
	{ "Blue_LED", ATLV_BOOL, &blue_led, sizeof(blue_led),
		ada_sprop_get_bool, demo_led_set },
	{ "Green_LED", ATLV_BOOL, &green_led, sizeof(green_led),
		ada_sprop_get_bool, demo_led_set },
	/*
	 * Integer properties.
	 */
	{ "input", ATLV_INT, &input, sizeof(input),
		ada_sprop_get_int, demo_int_set },
	{ "output", ATLV_INT, &output, sizeof(output),
		ada_sprop_get_int, NULL },
	/*
	 * Decimal properties.
	 */
	{ "decimal_in", ATLV_CENTS, &decimal_in, sizeof(decimal_in),
		ada_sprop_get_int, demo_int_set },
	{ "decimal_out", ATLV_CENTS, &decimal_out, sizeof(decimal_out),
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

static void demo_matter_event_cb(enum adm_event_id id)
{
	log_put(LOG_DEBUG "%s %d", __func__, id);
	switch (id) {
	case ADM_EVENT_IPV4_UP:
		ada_client_ip_up();
		ada_client_health_check_en();
		break;

	case ADM_EVENT_IPV4_DOWN:
		ada_client_ip_down();
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

	log_put(LOG_DEBUG "%s on_off %u", __func__, *value);

	if (*value) {
		event = DEMO_FROM_MATTER_ON;
	} else {
		event = DEMO_FROM_MATTER_OFF;
	}
	xQueueSend(demo_evt_queue, &event, 10);

	return AE_OK;
}

static const struct adm_attribute_change_callback demo_on_off_cb_entry =
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
	adm_attribute_change_cb_register(&demo_on_off_cb_entry);

	ada_sprop_mgr_register("demo_matter",
	    demo_props, ARRAY_LEN(demo_props));

}

static void demo_button_toggle(unsigned long pressed, unsigned long released)
{
	if (pressed && ((released - pressed) > 50)) {
		log_put(LOG_INFO "Button pressed more than 50ms");
		demo_write_notify_event(!blue_led);
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

	/* start with all LEDs to off */
	set_led(GPIO_BLUE_LED, 0);
	set_led(GPIO_GREEN_LED, 0);
	set_led(GPIO_LINK_LED, 0);

	while (1) {
#ifdef __no_stub__
		if (gpio_get_level(GPIO_BOOT_BUTTON) == 0) {
#else /* __no_stub__ */
		if (1) {
#endif /* __no_stub__ */
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
