/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

/*
 * Ayla device agent demo of a simple lights and buttons evaluation board
 * using the "simple" property manager.
 *
 * The property names are chosen to be compatible with the Ayla Control
 * App.  E.g., the LED property is Blue_LED even though the color is yellow.
 * Button1 sends the Blue_button property, even though the button is white.
 *
 * This demo also includes a batch sending example:
 *	(1). Put about 60 data-points into a batch and send the batch to cloud
 *	     when blue button is pressed.
 *	(2). Property of "node_batch_hold" can be set in cloud or mobile app
 *	     Aura. When this property's value is 1, auto batching is enabled.
 *	     Any other property will be put into a batch when it is changed.
 *	     When the batch is full, it will be sent to cloud automatically.
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <ada/libada.h>
#include <ada/sprop.h>
#include <adw/wifi.h>
#ifdef AYLA_BLUETOOTH_SUPPORT
#include <adb/adb.h>
#include <adb/al_bt.h>
#endif
#include <ayla/log.h>
#include <ayla/crc.h>
#include "conf.h"
#include "demo.h"
#include "ada/batch.h"
#ifdef GPIO_RGB_LED
#include "driver/rmt.h"
#include "led_strip.h"
#endif

#include <cmsis_os.h>
#include "scm_gpio.h"

#include "bp5758d.h"

#define APP_VER         "2.0"
#define APP_NAME        "ayla_ledevb_demo"

#define BUILD_STRING	APP_VER " "  __DATE__ " " __TIME__

#define GPIO_OUTPUT_PIN_SEL	\
    (BIT64(GPIO_BLUE_LED) | BIT64(GPIO_GREEN_LED) | BIT64(GPIO_LINK_LED))

#define GPIO_INPUT_PIN_SEL	BIT64(GPIO_BOOT_BUTTON)

#define BATCH_SIZE	9
#define BATCH_DATA_MAX	1200	/* adjust if batched property list changed */

#define MS_TO_TICKS(ms) ((uint32_t)(((uint32_t)(ms) * osKernelGetTickFreq()) / (uint32_t)1000))

const char mod_sw_build[] = BUILD_STRING;
const char mod_sw_version[] = APP_NAME " " BUILD_STRING;

/*
 * The oem and oem_model strings determine the host name for the
 * Ayla device service and the device template on the service.
 *
 * If these are changed, the encrypted OEM secret must be re-encrypted
 * unless the oem_model was "*" (wild-card) when the oem_key was encrypted.
 */
char oem[] = DEMO_OEM_ID;
char oem_model[] = DEMO_OEM_MODEL;
char template_version[] = DEMO_TEMPLATE_VERSION;

static u8 boot_button;
static u8 blue_led;
static u8 green_led;
static int input;
static int output;
static int decimal_in;
static int decimal_out;
static char *cmd_buf;
#ifdef AYLA_FILE_PROP_SUPPORT
static int stream_up_len;
static u8 stream_down_patt_match;
static u32 stream_down_patt_match_len;
static struct file_dp stream_down_state;
static struct file_dp stream_up_state;
#endif
static char version[] = APP_NAME " " BUILD_STRING;

static osTimerId_t indicator_led_timer;
static u8 indicator_led;
static int c_status;
#define LED_CONTROL_SHORT_TIMEOUT		10	 //10ms
#define NOTCONN_LED_CONTROL_TIMEOUT		1000 //1s   red led blink
#define IN_PROGESS_LED_CONTROL_TIMEOUT	500  //0.5s red led blink

#ifdef AYLA_BATCH_PROP_SUPPORT
static u8 node_batch_hold; /* This property can be changed in Ayla Developer
			    * Suite or in mobile app Aura.
			    *   0 -- do not put data-point into batch buffer;
			    *   1 -- put data-point into batch, send the batch
			    *	  to cloud on batch-buffer full.
			    */
static struct batch_ctx *batch;
#endif

static u8 link_up;

#ifdef GPIO_RGB_LED
static led_strip_t *led_strip;
#ifdef GPIO_RGB_LED2
static led_strip_t *led_strip2;
#endif
#endif

static enum ada_err demo_led_set(struct ada_sprop *, const void *, size_t);
static enum ada_err demo_int_set(struct ada_sprop *, const void *, size_t);
static enum ada_err demo_cmd_set(struct ada_sprop *, const void *, size_t);

#ifdef AYLA_BATCH_PROP_SUPPORT
static enum ada_err demo_node_batch_hold_set(struct ada_sprop *sprop,
			const void *val, size_t val_len);
#endif

#ifdef AYLA_FILE_PROP_SUPPORT
static enum ada_err demo_stream_down_begin(struct ada_sprop *,
		const void *, size_t);
#endif

#ifdef DEMO_MSG_PROP
#define DEMO_MSG_LOOP_BUF_LEN	4096
static struct ada_sprop *demo_msg_loop_back;	/* property owning loopback */
static int demo_msg_total_len;
static char msg_cmd_buf[] = "";
static enum ada_err demo_msg_prop_handler(struct ada_sprop *prop,
			const void *msgbuf, size_t len);
static enum ada_err demo_msg_prop_in_begin(struct ada_sprop *,
			const void *, size_t);
static u8 *demo_msg_loop_buf;
static u32 demo_msg_crc;
static size_t demo_msg_rx_len;
#endif


#include "gpio_types.h"

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

#ifdef GPIO_RGB_LED
static void demo_rgb_led_update(void)
{
	u8 red = 0;
	u8 green = 0;
	u8 blue = 0;

	/*
	 * Note: The LED is exceptionally bright. Setting the color channels
	 * to the lowest possible value, 1, makes it easiest on the eyes.
	 */
	if (!link_up) {
		red = 1;
	} else {
		if (blue_led && green_led) {
			/* use pink when both are on since yellow is status */
			red = 1;
			blue = 1;
		} else if (blue_led) {
			blue = 1;
		} else if (green_led) {
			green = 1;
		}
	}

	led_strip->set_pixel(led_strip, 0, red, green, blue);
	led_strip->refresh(led_strip, 100);

#ifdef GPIO_RGB_LED2
	led_strip2->set_pixel(led_strip2, 0, red, green, blue);
	led_strip2->refresh(led_strip2, 100);
#endif
}
#endif

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
	 * boolean properties
	 */
	{ "Blue_button", ATLV_BOOL, &boot_button, sizeof(boot_button),
		ada_sprop_get_bool, NULL},
	{ "Blue_LED", ATLV_BOOL, &blue_led, sizeof(blue_led),
		ada_sprop_get_bool, demo_led_set },
	{ "Green_LED", ATLV_BOOL, &green_led, sizeof(green_led),
		ada_sprop_get_bool, demo_led_set },
	/*
	 * string properties
	 */
	{ "cmd", ATLV_UTF8, "", 0, ada_sprop_get_string, demo_cmd_set },
	{ "log", ATLV_UTF8,
		 "", 0,	/* note value and len are modified by demo_log_set() */
		ada_sprop_get_string, NULL },
	/*
	 * integer properties
	 */
	{ "input", ATLV_INT, &input, sizeof(input),
		ada_sprop_get_int, demo_int_set },
	{ "output", ATLV_INT, &output, sizeof(output),
		ada_sprop_get_int, NULL },
	/*
	 * decimal properties
	 */
	{ "decimal_in", ATLV_CENTS, &decimal_in, sizeof(decimal_in),
		ada_sprop_get_int, demo_int_set },
	{ "decimal_out", ATLV_CENTS, &decimal_out, sizeof(decimal_out),
		ada_sprop_get_int, NULL },
#ifdef AYLA_BATCH_PROP_SUPPORT
	{ "node_batch_hold", ATLV_BOOL, &node_batch_hold,
		sizeof(node_batch_hold),
		ada_sprop_get_bool, demo_node_batch_hold_set },
#endif
#ifdef AYLA_FILE_PROP_SUPPORT
	/*
	 * file upload properties
	 */
	{ "stream_up_len", ATLV_INT, &stream_up_len, sizeof(stream_up_len),
		ada_sprop_get_int, demo_int_set },
	{ "stream_up", ATLV_LOC, &stream_up_state, 0, NULL, NULL },

	/*
	 * file download properties
	 */
	{ "stream_down", ATLV_LOC, &stream_down_state, 0, NULL,
		demo_stream_down_begin },
	{ "stream_down_len", ATLV_INT, &stream_down_state.next_off,
		sizeof(stream_down_state.next_off), ada_sprop_get_int, NULL },
	{ "stream_down_match_len", ATLV_INT, &stream_down_patt_match_len,
		sizeof(stream_down_patt_match_len), ada_sprop_get_int, NULL },
#endif
#ifdef DEMO_MSG_PROP
	/*
	 * String property to start sending message property datapoints.
	 * The value is a command string, which is always sent back as empty.
	 */
	{ "message_start", ATLV_UTF8, msg_cmd_buf, sizeof(msg_cmd_buf),
		ada_sprop_get_string, demo_msg_prop_handler},

	{ "string_out", ATLV_MSG_UTF8, NULL, 0, NULL, NULL},
	{ "string_in", ATLV_MSG_UTF8, NULL, 0, NULL, demo_msg_prop_in_begin},

	{ "json_out", ATLV_MSG_JSON, NULL, 0, NULL, NULL},
	{ "json_in", ATLV_MSG_JSON, NULL, 0, NULL, demo_msg_prop_in_begin},

	{ "binary_out", ATLV_MSG_BIN, NULL, 0, NULL, NULL},
	{ "binary_in", ATLV_MSG_BIN, NULL, 0, NULL, demo_msg_prop_in_begin},
#endif
};

#ifdef AYLA_BATCH_PROP_SUPPORT
static void demo_batch_send_done(size_t size_sent)
{
	printf("\r\nSent batch json size = %u.\r\n", size_sent);
}

static void demo_batch_error_cb(int batch_id, int status)
{
	printf("batch_id = %d, status = %d.\n", batch_id, status);
}


static void demo_put_prop_to_batch(struct ada_sprop *sprop,
		const char *prop_name)
{
	int ret;
	int retry;
	struct clock_time ct;
	s64 stamp;

	if (!sprop && !prop_name) {
		printf("%s() params error.\r\n", __func__);
		return;
	}
	if (!node_batch_hold || !batch) {
		return;
	}

	clock_get(&ct);
	stamp = ((s64)ct.ct_sec) * 1000 + ct.ct_usec / 1000;
	printf("time_stamp = %lld\r\n", stamp);
	for (retry = 0; retry < 2; retry++) {
		if (sprop) {
			ret = ada_batch_add_prop(batch, sprop, stamp);
		} else {
			ret = ada_batch_add_prop_by_name(batch,
				prop_name, stamp);
		}

		if (ret > 0) {
			printf("ada_batch_add_prop(%s): batch_id = %d.\r\n",
			    sprop ? sprop->name : prop_name, ret);
			break;
		} else if (ret == AE_BUF) {
			/* batch buffer is full, send the batch to cloud */
			ada_batch_send(batch, demo_batch_send_done);
			continue;
		} else {
			printf("ada_batch_add_prop(%s) err = %d.\r\n",
			    sprop ? sprop->name : prop_name, ret);
			break;
		}
	}
}
#endif

static void prop_send_by_name(const char *name)
{
	enum ada_err err;

	err = ada_sprop_send_by_name(name);
	if (err) {
		printf("demo: %s: send of %s: err %d\r\n",
				__func__, name, err);
	}
}

/*
 * Send property to cloud or put it into batch.
 */
static void demo_send_prop(const char *name)
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
	prop_send_by_name(name);
#endif
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

#ifdef AYLA_FILE_PROP_SUPPORT
static void demo_file_start_send(const char *name, size_t len)
{
	struct prop_dp_meta meta[] = {
		{ "time", "" },
		{ "key1", "val1" },
		{ "key2", "val2" },
	};
	enum ada_err err;

	clock_fmt(meta[0].value, sizeof(meta[0].value), clock_utc());
	err = ada_sprop_file_start_send_with_meta(name, len,
	    meta, ARRAY_LEN(meta));
	if (err) {
		log_put(LOG_ERR "%s: err %d", __func__, err);
	}
}
#endif

/*
 * Demo set function for bool properties.
 */
static enum ada_err demo_led_set(struct ada_sprop *sprop,
		const void *buf, size_t len)
{
	int ret = 0;

	ret = ada_sprop_set_bool(sprop, buf, len);
	if (ret) {
		return ret;
	}
	if (sprop->val == &blue_led) {
		set_led(GPIO_BLUE_LED, blue_led);
	} else {
		set_led(GPIO_GREEN_LED, green_led);
	}
#ifdef GPIO_RGB_LED
	demo_rgb_led_update();
#endif
	printf("%s: %s set to %u\r\n",
		__func__, sprop->name, *(u8 *)sprop->val);

	return AE_OK;
}

/*
 * Demo set function for integer and decimal properties.
 */
static enum ada_err demo_int_set(struct ada_sprop *sprop,
		const void *buf, size_t len)
{
	int ret;
	struct prop_dp_meta *meta;
	unsigned int i;
	static unsigned int ack_count;

	ret = ada_sprop_set_int(sprop, buf, len);
	if (ret) {
		return ret;
	}

	if (sprop->val == &input) {
		printf("%s: %s set to %d\r\n",
			__func__, sprop->name, input);
		output = input;
		demo_send_prop_with_meta("output");
	} else if (sprop->val == &decimal_in) {
		printf("%s: %s set to %d\r\n",
			__func__, sprop->name, decimal_in);
		decimal_out = decimal_in;
		demo_send_prop("decimal_out");
#ifdef AYLA_FILE_PROP_SUPPORT
	} else if (sprop->val == &stream_up_len) {
		printf("%s: %s set to %d\r\n",
			__func__, sprop->name, stream_up_len);
		/* start file upload and clear stream_up_len if its not zero */
		if (stream_up_len) {
			demo_file_start_send("stream_up", stream_up_len);
			stream_up_len = 0;
			prop_send_by_name("stream_up_len");
		}
#endif
	} else {
		return AE_NOT_FOUND;
	}
	if (sprop->ack_id) {
		ada_sprop_send_ack(sprop, 0, ++ack_count);
	}
	meta = sprop->metadata;
	if (meta) {
		for (i = 0; i < PROP_MAX_DPMETA; i++) {
			if (!meta->key[0]) {
				break;
			}
			printf("%s: %s metadata %u \"%s\"=\"%s\"\n",
			    __func__, sprop->name, i, meta->key, meta->value);
			meta++;
		}
	}
	return AE_OK;
}

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

	printf("%s: cloud set %s to \"%s\"\r\n",
	    __func__, sprop->name, cmd_buf);
	demo_send_prop_with_meta("log");
	return AE_OK;
}

#ifdef AYLA_BATCH_PROP_SUPPORT
static enum ada_err demo_node_batch_hold_set(struct ada_sprop *sprop,
			const void *buf, size_t len)
{
	enum ada_err err;
	u8 old_val;

	old_val = node_batch_hold ? 1 : 0;
	err = ada_sprop_set_bool(sprop, buf, len);
	if (err != AE_OK) {
		printf("ada_sprop_set_bool() = %d.\r\n", err);
		return err;
	}
	printf("%s: %s set to %u\r\n",
		__func__, sprop->name, node_batch_hold);
	node_batch_hold = node_batch_hold ? 1 : 0;

	if (node_batch_hold == old_val) {
		/* no change */
		return AE_OK;
	}
	if (node_batch_hold) {
		if (!batch) {
			/* create a batch buffer. */
			batch = ada_batch_create(BATCH_SIZE, BATCH_DATA_MAX);
			ada_batch_set_err_cb(demo_batch_error_cb);
		} else {
			/* discard content in the batch buffer. */
			ada_batch_discard(batch);
		}
	} else {
		/* before the batch buffer is destroyed, send its content
		 * to cloud.
		 */
		err = ada_batch_send(batch, demo_batch_send_done);
		if (err != AE_OK) {
			printf("ada_batch_send() = %d.\r\n", err);
		}
		ada_batch_destroy(batch);
		batch = NULL;
	}
	return AE_OK;
}
#endif

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

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Demo function to begin stream download.
 */
static enum ada_err demo_stream_down_begin(struct ada_sprop *sprop,
		const void *buf, size_t len)
{
	return ada_sprop_file_start_recv(sprop->name, buf, len, 0);
}


#define TEST_PATT_BASE 0x11223344

/*
 * Return the test pattern byte value for a given offset.
 * This is a four-byte counting pattern, in big-endian byte order.
 * The starting bytes are in TEST_PATT_BASE.
 */
static u8 test_patt(size_t off)
{
	u32 patt;

	patt = off / 4;
	patt += TEST_PATT_BASE;
	off &= 3;
	off = 24 - off * 8;
	return (patt >> off) & 0xff;
}

/*
 * Accept new value for stream_down property.
 * This just accumulates the length and tests for matching the test pattern.
 */
static enum ada_err test_patt_set(struct ada_sprop *sprop, size_t off,
		void *buf, size_t len, u8 eof)
{
	struct file_dp *dp;
	u8 *bp;
	enum ada_err err = AE_OK;

	dp = sprop->val;
	if (off == 0) {
		stream_down_patt_match = 1;
		stream_down_patt_match_len = 0;
	}
	if (dp->next_off != off) {
		stream_down_patt_match = 0;
		return AE_INVAL_OFF;
	}

	dp->next_off = off + len;

	if (stream_down_patt_match) {
		for (bp = buf; len > 0; len--, bp++, off++) {
			if (*bp != test_patt(off)) {
				stream_down_patt_match = 0;
				break;
			}
			stream_down_patt_match_len = off + 1;
		}
	}

	if (eof) {
		prop_send_by_name("stream_down_len");
		prop_send_by_name("stream_down_match_len");
	}
	return err;
}


/*
 * Return test_pattern for testing sending large datapoints to the service.
 * The offset and length is given.
 * The data pattern is a 16-bit counting pattern for now.
 */
static size_t test_patt_get(struct ada_sprop *sprop, size_t off,
		void *buf, size_t len)
{
	u8 *bp;
	int out_len = 0;

	bp = buf;
	for (out_len = 0; out_len < len; out_len++) {
		*bp++ = test_patt(off++);
	}
	return out_len;
}
#endif

#ifdef DEMO_MSG_PROP

/*
 * Fill buffer with specified pattern for string message prop.
 * Test pattern is like "0a0b0c0d ... 0z1a1b1c ... 1z2a2b2c ..."
 */
static size_t demo_string_test_patt(void *buf, size_t len, size_t off)
{
	u8 *bp = buf;
	unsigned int out_len = 0;
	unsigned int msg_index = 0;

	u8 iter;
	bp = buf;
	iter = off / 52;
	msg_index = (off % 52) / 2;

	for (out_len = 0; out_len < len; out_len++) {
		if (out_len % 2 == 1) {
			*bp++ = (msg_index % 26) + 'a';
			if (msg_index % 26 == 25) {
				iter++;
			}
			msg_index++;
		} else {
			*bp++ = (iter % 10) + '0';
		}
	}
	return out_len;
}

/*
 * Get a test pattern for a JSON string with the specified length and offset.
 */
static size_t demo_json_test_patt(void *buf, size_t len, size_t off)
{
	u8 *bp = buf;
	const char head[] = "{\"val\":\"";
	const char tail[] = "\"}";
	size_t head_len = sizeof(head) - 1;
	size_t tail_len = sizeof(tail) - 1;
	u8 *src;
	size_t tlen;
	size_t body_end = demo_msg_total_len - tail_len;
	size_t rlen;

	/*
	 * For very short patterns, give digits only.
	 */
	if (demo_msg_total_len < head_len + tail_len) {
		for (rlen = len; rlen; rlen--) {
			*bp++ = '0' + off % 10;
		}
		return len;
	}

	/*
	 * Loop for three sections, selecting head, body, tail or done.
	 */
	for (rlen = len; rlen && off < demo_msg_total_len; rlen -= tlen) {
		if (off < head_len) {
			tlen = head_len - off;
			src = (u8 *)head + off;
		} else if (off < body_end) {
			src = NULL;
			tlen = body_end - off;
		} else {
			tlen = demo_msg_total_len - off;
			src = (u8 *)tail + off - body_end;
		}

		/*
		 * limit copy to size of remaining buffer.
		 */
		if (tlen > rlen) {
			tlen = rlen;
		}

		/*
		 * Copy from head or tail. Use test pattern if src is NULL.
		 * Use string test pattern for value of JSON string.
		 */
		if (src) {
			memcpy(bp, src, tlen);
		} else {
			demo_string_test_patt(bp, tlen, off - head_len);
		}
		bp += tlen;
		off += tlen;
	}
	return len - rlen;
}

/*
 * Fill buffer with specified pattern for binary message prop.
 */
static size_t demo_binary_test_patt(void *buf, size_t len, size_t off)
{
	unsigned int i;
	u8 patt;
	u8 *cp = buf;

	patt = (u8)off;
	for (i = 0; i < len; i++) {
		cp[i] = patt++;
	}
	return len;
}

/*
 * Demo function to begin string download.
 *
 * buf is a pointer to the location string
 * len is the length of the location string
 */
static enum ada_err demo_msg_prop_in_begin(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	log_info("%s: %s, loc:%s len:%zu", __func__, sprop->name,
	    (char *)buf, len);
	return ada_sprop_file_start_recv(sprop->name, buf, len, 0);
}

/*
 * Send corresponding "from device" message property for a
 *  "to device" message property
 */
static void demo_msg_start_send(struct ada_sprop *sprop, size_t size)
{
	const char *name;
	enum ada_err err;

	if (!strcmp(sprop->name, "json_in")) {
		name = "json_out";
	} else if (!strcmp(sprop->name, "string_in")) {
		name = "string_out";
	} else if (!strcmp(sprop->name, "binary_in")) {
		name = "binary_out";
	} else {
		name = "";
		log_put(LOG_ERR "invalid msg prop for test: %s", sprop->name);
		return;
	}

	sprop = demo_sprop_lookup(name);
	if (!sprop) {
		log_put(LOG_ERR "invalid loopback prop for test: %s", name);
		return;
	}

	demo_msg_loop_back = sprop;	/* transfer ownership of loopback */
	log_info("%s : Sending %zu bytes to %s", __func__, size, name);
	err = ada_sprop_file_start_send(name, size);
	if (err) {
		log_put(LOG_ERR "%s: %s send err %d", __func__, name, err);
	}
}

/*
 * Accept new value for a message property.
 * This accumulates the length and performs a CRC.
 * If the size fits in the loopback buffer, it is sent back on the
 * corresponding input property.
 */
static enum ada_err demo_msg_prop_test_set(struct ada_sprop *sprop,
	size_t off, void *buf, size_t len, u8 eof)
{
	enum ada_err err = AE_OK;

	log_put(LOG_DEBUG "%s: %s offset %zu length %zu",
	    __func__, sprop->name, off, len);

	if (off == 0) {
		demo_msg_crc = CRC32_INIT;
		demo_msg_rx_len = 0;
		if (!demo_msg_loop_back) {
			free(demo_msg_loop_buf);
			demo_msg_loop_buf = malloc(DEMO_MSG_LOOP_BUF_LEN);
			if (demo_msg_loop_buf) {
				demo_msg_loop_back = sprop;
			} else {
				/* proceed without loopback */
				log_put(LOG_ERR
				     "%s: alloc of loopback buf failed",
				    __func__);
			}
		}
	}
	if (demo_msg_loop_buf && demo_msg_loop_back == sprop) {
		if (off + len <= DEMO_MSG_LOOP_BUF_LEN) {
			memcpy(demo_msg_loop_buf + off, buf, len);
		} else {
			free(demo_msg_loop_buf);
			demo_msg_loop_buf = NULL;
			demo_msg_loop_back = NULL;
		}
	}
	demo_msg_crc = crc32(buf, len, demo_msg_crc);
	demo_msg_rx_len += len;

	if (eof) {
		log_put(LOG_INFO "%s: %s end. received %zu bytes crc %#lx",
		    __func__, sprop->name, demo_msg_rx_len, demo_msg_crc);
		if (sprop->ack_id) {
			log_put(LOG_DEBUG "%s: %s sending ack %d",
			    __func__, sprop->name, demo_msg_rx_len);
			ada_sprop_send_ack(sprop, 0, demo_msg_rx_len);
		}
	}
	return err;
}

/*
 * Callback indicating transfer of message property has ended.
 * For received properties, if the length fits in the buffer, loop it back.
 */
static void demo_msg_prop_result(struct ada_sprop *sprop, enum ada_err err)
{
	log_put(LOG_DEBUG "%s: prop %s result err %d",
	    __func__, sprop->name, err);
	if (sprop == demo_msg_loop_back) {
		if (err) {
			free(demo_msg_loop_buf);
			demo_msg_loop_buf = NULL;
			demo_msg_loop_back = NULL;
		} else {
			demo_msg_start_send(sprop, demo_msg_rx_len);
		}
	}
}

/*
 * Return test_pattern for testing sending large datapoints to the service.
 * The offset and length is given.
 * The data pattern is a 16-bit counting pattern for now.
 */
static size_t demo_msg_prop_test_patt_get(struct ada_sprop *sprop, size_t off,
	void *buf, size_t len)
{
	int out_len = 0;

	log_info("%s: prop:%s, length:%zu", __func__, sprop->name, len);

	if (demo_msg_loop_back == sprop && demo_msg_loop_buf) {
		if (off > demo_msg_rx_len) {
			return 0;
		}
		if (off + len > demo_msg_rx_len) {
			log_put(LOG_ERR "%s: %s: "
			    "get out of range off %zu len %zu rx len %zu",
			    __func__, sprop->name, off, len, demo_msg_rx_len);
			len = demo_msg_rx_len - off;
		}
		if (off + len > DEMO_MSG_LOOP_BUF_LEN) {
			log_put(LOG_ERR "%s: %s: "
			    "get out of range off %zu len %zu buf len %zu",
			    __func__, sprop->name, off, len,
			    DEMO_MSG_LOOP_BUF_LEN);
			len = DEMO_MSG_LOOP_BUF_LEN - off;
		}
		memcpy(buf, demo_msg_loop_buf + off, len);
		if (off + len == demo_msg_rx_len) {
			free(demo_msg_loop_buf);
			demo_msg_loop_buf = NULL;
			demo_msg_loop_back = NULL;
		}
		return len;
	}
	if (!strcmp(sprop->name, "json_out")) {
		out_len = demo_json_test_patt(buf, len, off);
	} else if (!strcmp(sprop->name, "string_out")) {
		out_len = demo_string_test_patt(buf, len, off);
	} else if (!strcmp(sprop->name, "binary_out")) {
		out_len = demo_binary_test_patt(buf, len, off);
	} else {
		log_info("unrecognized pattern name %s", sprop->name);
	}
	return out_len;
}

/*
 * Set property "messsage_start" requesting sending message item up.
 * Sets itself back to empty.
 * Commands are space separated as: <len> [<prop>]
 * Where <len> is the length in bytes to send.
 * And <prop> is the property name, defaulting to "json_out".
 */
static enum ada_err demo_msg_prop_handler(struct ada_sprop *prop,
			const void *msgbuf, size_t len)
{
	char buf[80];
	char *errptr;
	char *name;
	size_t out_len;
	enum ada_err err = AE_OK;

	/*
	 * Copy value to buffer, possibly truncating.
	 */
	if (len > sizeof(buf) - 1) {
		len = sizeof(buf) - 1;
	}
	memcpy(buf, msgbuf, len);
	buf[len] = '\0';

	if (!len) {
		return AE_OK;		/* no command */
	}
	log_info("setting %s to \"%s\"", prop->name, buf);

	out_len = strtoul(buf, &errptr, 0);
	demo_msg_total_len = out_len;
	if (errptr == buf || (*errptr != '\0' && *errptr != ' ')) {
		log_put(LOG_WARN "%s: invalid value for %s = \"%s\"",
		     __func__, prop->name, buf);
		err = AE_INVAL_VAL;
	} else {
		name = errptr;
		while (*name != '\0' && *name == ' ') {
			name++;
		}
		if ((!strcmp(name, "json_out")) ||
		    (!strcmp(name, "string_out")) ||
		    (!strcmp(name, "binary_out"))) {
			log_info("sending msg %s (%zu bytes)", name, out_len);
			demo_msg_loop_back = NULL;
			demo_file_start_send(name, out_len);
		} else {
			log_put(LOG_WARN "%s: unrecognized pattern name %s",
			    __func__, name);
		}
	}

	/*
	 * Send message_start property back empty so command isn't repeated.
	 */
	ada_sprop_send(prop);
	return err;
}

static void demo_sprop_file_init(const char *name,
	size_t (*get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len),
	enum ada_err (*set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof))
{
	enum ada_err err;

	err = ada_sprop_file_alloc(name, get, set, demo_msg_prop_result,
	    SPROP_DEF_FILECHUNK_SIZE);
	if (err) {
		log_put(LOG_ERR "%s: init of %s failed err %d",
		    __func__, name, err);
	}
}

static void demo_msg_props_init(void)
{
	demo_sprop_file_init("string_out", demo_msg_prop_test_patt_get, NULL);
	demo_sprop_file_init("json_out", demo_msg_prop_test_patt_get, NULL);
	demo_sprop_file_init("binary_out", demo_msg_prop_test_patt_get, NULL);
	demo_sprop_file_init("string_in", NULL, demo_msg_prop_test_set);
	demo_sprop_file_init("json_in", NULL, demo_msg_prop_test_set);
	demo_sprop_file_init("binary_in", NULL, demo_msg_prop_test_set);
}
#endif /* DEMO_MSG_PROP */

static void demo_indicator_toggle(int timeout)
{
	if (osTimerIsRunning(indicator_led_timer)) {
		osTimerStop(indicator_led_timer);
	}

	if (timeout) {
		while (1) {
			if (osTimerStart(indicator_led_timer, MS_TO_TICKS(timeout)) != osErrorResource) {
				break;
			}
		}
	}
}

#ifdef AYLA_WIFI_SUPPORT
/*
 * Event handler for updating link LED state based on Wi-Fi events.
 */
static void demo_ledevb_wifi_event_handler(enum adw_wifi_event_id id,
    void *arg)
{
	switch (id) {
	case ADW_EVID_STA_DOWN:
		set_led(GPIO_LINK_LED, 0);
		c_status = AE_NOTCONN;
		demo_indicator_toggle(LED_CONTROL_SHORT_TIMEOUT);
#ifdef GPIO_RGB_LED
		demo_rgb_led_update();
#endif
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
		conf_connected = 1;
		link_up = 1;
		msg = "up";
		set_led(GPIO_LINK_LED, 1);

		c_status = AE_OK;
		demo_indicator_toggle(LED_CONTROL_SHORT_TIMEOUT);
		break;
	case AE_IN_PROGRESS:
		link_up = 0;
		msg = "down";
		set_led(GPIO_LINK_LED, 0);

		c_status = AE_IN_PROGRESS;
		demo_indicator_toggle(LED_CONTROL_SHORT_TIMEOUT);

		break;
	case AE_NOTCONN:
		link_up = 0;
		msg = "not connected";
		set_led(GPIO_LINK_LED, 0);

		c_status = AE_NOTCONN;
		demo_indicator_toggle(LED_CONTROL_SHORT_TIMEOUT);
		break;
	default:
		msg = NULL;
	}

	if (msg) {
#ifdef GPIO_RGB_LED
		demo_rgb_led_update();
#endif
		printf("%s: ADS %s\n", __func__, msg);
	} else {
		printf("%s: err %d\n", __func__, err);
	}
}

/*
 * Initialize property manager.
 */
void demo_init(void)
{
#ifdef AYLA_WIFI_SUPPORT
	adw_wifi_event_register(demo_ledevb_wifi_event_handler, NULL);
#endif
	ada_client_event_register(demo_client_event, NULL);
#ifdef AYLA_FILE_PROP_SUPPORT
	ada_sprop_file_init(&stream_up_state, test_patt_get, NULL,
			SPROP_DEF_FILECHUNK_SIZE);
	ada_sprop_file_init(&stream_down_state, NULL, test_patt_set,
			SPROP_DEF_FILECHUNK_SIZE);
#endif

	ada_sprop_mgr_register("ledevb", demo_props, ARRAY_LEN(demo_props));

#ifdef DEMO_MSG_PROP
	demo_msg_props_init();
#endif
}

void demo_idle(void)
{
	struct {
		int gpio;
		int val;
		uint32_t start_timer;
	} button_info;
	int tmp;
	gpio_config_t io_conf;
	uint8_t flag = 0;
#ifdef AYLA_BATCH_PROP_SUPPORT
	int i;
	struct batch_ctx *handle;
	int rc;
	int cnt;
#endif

	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);

	/* start with all LEDs to off */
	set_led(GPIO_BLUE_LED, 0);
	set_led(GPIO_GREEN_LED, 0);
	set_led(GPIO_LINK_LED, 0);

	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	gpio_config(&io_conf);

#ifdef GPIO_RGB_LED
	led_strip = led_strip_init(RMT_CHANNEL_0, GPIO_RGB_LED, 1);
#ifdef GPIO_RGB_LED2
	led_strip2 = led_strip_init(RMT_CHANNEL_1, GPIO_RGB_LED2, 1);
#endif
	demo_rgb_led_update();
#endif
	button_info.gpio = GPIO_BOOT_BUTTON;
	button_info.val = !gpio_get_level(button_info.gpio);
	button_info.start_timer = 0;

	/* Wait for cloud connection to come up for the 1st time. */
	while (!demo_cloud_has_started()) {
		osDelay(MS_TO_TICKS(100));
	}
	printf("%s: cloud connection started\r\n", __func__);

	prop_send_by_name("oem_host_version");
	prop_send_by_name("version");

	while (1) {
		osDelay(MS_TO_TICKS(100));

		/* When the button is pressed, zero is returned. */
		tmp = !gpio_get_level(button_info.gpio);
		if (tmp == button_info.val) {
			if (flag) {
				if ((osKernelGetTickCount() -
				    button_info.start_timer) >=
				    MS_TO_TICKS(5000)) {
					flag = 0;
					client_reg_window_start();
				}
			}
			continue;
		}
		if (tmp == 0) {
			button_info.start_timer = osKernelGetTickCount();
			flag = 1;
		} else if (tmp == 1) {
			if ((osKernelGetTickCount() - button_info.start_timer) <
			    MS_TO_TICKS(5000)) {
				flag = 0;
			}
		}
		button_info.val = tmp;
		boot_button = button_info.val;
		printf("%s: boot button %s\r\n", __func__, button_info.val ?
		    "DOWN(1)" : "UP(0)");
		prop_send_by_name("Blue_button");

		if (boot_button == 0) {
#ifdef AYLA_BLUETOOTH_SUPPORT
			if (!demo_bt_is_provisioning()) {
				adb_pairing_mode_set(ADB_PM_NO_PASSKEY, 60);
			}
#endif
			continue;
		}
#ifdef AYLA_BATCH_PROP_SUPPORT
		printf("Send batch data points:\r\n");
		handle = ada_batch_create(BATCH_SIZE, BATCH_DATA_MAX);
		if (!handle) {
			continue;
		}
		for (cnt = 0, i = 0; i < BATCH_SIZE / 3; i++) {
			rc = ada_batch_add_prop_by_name(handle, "output", 0);
			if (rc <= 0) {
				break;
			}
			cnt++;
			rc = ada_batch_add_prop_by_name(handle,
				"decimal_out", 0);
			if (rc <= 0) {
				break;
			}
			cnt++;
			rc = ada_batch_add_prop_by_name(handle, "log", 0);
			if (rc <= 0) {
				break;
			}
			cnt++;
		}
		printf("BATCH datapoints send = %d.\r\n", cnt);
		ada_batch_send(handle, demo_batch_send_done);
		ada_batch_destroy(handle);
		handle = NULL;
#endif
	}
}

static void demo_led_cb(void *arg)
{
	int timeout;

	if (c_status == AE_OK) {
		//printf("LED OFF (%d)\n", c_status);
		indicator_led = 0;
		bp5758d_set_channel(BP5758D_CHANNEL_R,  0);
		return;
	}

	if (c_status == AE_NOTCONN) {
		timeout = NOTCONN_LED_CONTROL_TIMEOUT;
	} else {
		timeout = IN_PROGESS_LED_CONTROL_TIMEOUT;
	}

	if (indicator_led) {
		//printf("LED OFF (%d)\n", c_status);
		indicator_led = 0;
		bp5758d_set_channel(BP5758D_CHANNEL_R,  0);
	} else {
		//printf("LED ON  (%d)\n", c_status);
		indicator_led = 1;
		bp5758d_set_channel(BP5758D_CHANNEL_R,  20);
	}


	if (timeout) {
		while (1) {
			if (osTimerStart(indicator_led_timer, MS_TO_TICKS(timeout)) != osErrorResource) {
				break;
			}
		}
	}

}

void app_main()
{
	osTimerAttr_t timer_attr;

	demo_start();

	bp5758d_init();

	bp5758d_set_rgbcw_channel(0, 0, 0, 0, 0);

	memset(&timer_attr, 0, sizeof(timer_attr));
	timer_attr.name = "led";
	indicator_led_timer = osTimerNew(demo_led_cb, osTimerOnce, NULL, &timer_attr);

	indicator_led = 0;
	c_status = AE_NOTCONN;
	demo_indicator_toggle(LED_CONTROL_SHORT_TIMEOUT);

	demo_idle();
}
