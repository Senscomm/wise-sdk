#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "scm_i2c.h"
#include "bp5758d.h"


#define INVALID_ADDR			0xFF

#define BP5758D_MAX_PIN			5

#define BP5758D_ADDR_SLEEP   	0x80  //10 00 0110: Sleep mode bits set (OUT1-5 enable setup selected, ignored by chip)
#define BP5758D_ADDR_SETUP   	0x90  //10 01 0000: OUT1-5 enable/disable setup - used during init
#define BP5758D_ADDR_OUT1_CR 	0x91  //10 01 0001: OUT1 current range
#define BP5758D_ADDR_OUT2_CR 	0x92  //10 01 0010: OUT2 current range
#define BP5758D_ADDR_OUT3_CR 	0x93  //10 01 0011: OUT3 current range
#define BP5758D_ADDR_OUT4_CR 	0x94  //10 01 0100: OUT4 current range
#define BP5758D_ADDR_OUT5_CR 	0x95  //10 01 0101: OUT5 current range
#define BP5758D_ADDR_OUT1_GL 	0x96  //10 01 0110: OUT1 gray-scale level
#define BP5758D_ADDR_OUT2_GL 	0x98  //10 01 1000: OUT2 gray-scale level
#define BP5758D_ADDR_OUT3_GL 	0x9A  //10 01 1010: OUT3 gray-scale level
#define BP5758D_ADDR_OUT4_GL 	0x9C  //10 01 1100: OUT4 gray-scale level
#define BP5758D_ADDR_OUT5_GL 	0x9E  //10 01 1110: OUT5 gray-scale level

#define BP5758D_ENABLE_ALL_OUT 	0x1F
#define BP5758D_DISABLE_ALL_OUT	0x00

struct bp5758d_ctx {
	uint8_t rgb_current;
	uint8_t cw_current;
	uint8_t rgb_set_current;
	uint8_t cw_set_current;
	uint8_t mapping_addr[BP5758D_MAX_PIN];
	bool sleep_mode;
};

struct bp5758d_ctx *g_ctx = NULL;

static int check_initialized(void)
{
	if (!g_ctx) {
		printf("Not initialized\n");
		return -1;
	}

	return 0;
}

static uint8_t get_mapping_addr(enum bp5758d_channel channel)
{
	uint8_t addr[] = {
		BP5758D_ADDR_OUT1_GL,
		BP5758D_ADDR_OUT2_GL,
		BP5758D_ADDR_OUT3_GL,
		BP5758D_ADDR_OUT4_GL,
		BP5758D_ADDR_OUT5_GL
	};

	return addr[g_ctx->mapping_addr[channel]];
}

static int convert_current_value(uint8_t *dst, uint8_t src)
{
	if (src > 90) {
		return -1;
	}

	if (src >= 64) {
		uint8_t tmp = src;

		tmp -= 62;
		src = tmp | 0x60;
	}

	*dst = src;

	return 0;
}

static void bp5758d_register_channel(enum bp5758d_channel channel, enum bp5758d_out_pin pin)
{
	g_ctx->mapping_addr[channel] = pin;
}

static int send_i2c(uint8_t *value, uint32_t len)
{
	int ret = 0;
	
#ifdef CONFIG_API_I2C
		ret = scm_i2c_master_tx(SCM_I2C_IDX_GPIO, 0, value, len, 0);
#endif


	return ret;
}

int bp5758d_init(void)
{
	struct scm_i2c_cfg cfg = {0,};
	uint8_t value[17];

	if (g_ctx) {
		printf("Already initialized\n");
		return -1;
	}

	g_ctx = malloc(sizeof(struct bp5758d_ctx));
	if (!g_ctx) {
		printf("Not enough resource\n");
		return -1;
	}

    memset(g_ctx, 0, sizeof(*g_ctx));

	/*
	 * Configure channel-to-pin mapping according to different LED module hardware
	 * This configuration corresponds to the SkyLighting LED module
	 */
	bp5758d_register_channel(BP5758D_CHANNEL_B, BP5758D_PIN_OUT3);
	bp5758d_register_channel(BP5758D_CHANNEL_G, BP5758D_PIN_OUT2);
	bp5758d_register_channel(BP5758D_CHANNEL_R, BP5758D_PIN_OUT1);
	bp5758d_register_channel(BP5758D_CHANNEL_W, BP5758D_PIN_OUT4);
	bp5758d_register_channel(BP5758D_CHANNEL_C, BP5758D_PIN_OUT5);

	memset(value, 0, 17);

	value[0] = BP5758D_ADDR_SETUP;
	value[1] = BP5758D_ENABLE_ALL_OUT;
	value[2] = 14;
	value[3] = 14;
	value[4] = 14;
	value[5] = 14;
	value[6] = 14;

    g_ctx->rgb_set_current = 14;
    g_ctx->cw_set_current = 14;

#ifdef CONFIG_API_I2C
		scm_i2c_init(SCM_I2C_IDX_GPIO);
		cfg.skip_address = 1;
		scm_i2c_configure(SCM_I2C_IDX_GPIO, &cfg, NULL, NULL);
#else
		(void)cfg;
#endif

	return send_i2c(value, sizeof(value));
}

int bp5758d_deinit(void)
{
	if (check_initialized() < 0) {
		return -1;
	}

//	bp5758d_set_rgbcw_channel(0, 0, 0, 0, 0);
	bp5758d_set_standby(true);

	free(g_ctx);
	g_ctx = NULL;

	return 0;
}

int bp5758d_shutdown(void)
{
    uint8_t value[11] = { 0 };

    value[0] = BP5758D_ADDR_SETUP;
	return send_i2c(value, sizeof(value));
}

int bp5758d_set_standby(bool enable)
{
	uint8_t value[2];
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (enable) {
        value[0] = BP5758D_ADDR_SETUP;
		value[1] = BP5758D_DISABLE_ALL_OUT;

		ret = send_i2c(value, 2);
		if (!ret) {
            value[0] = BP5758D_ADDR_SLEEP;
			ret = send_i2c(value, 1);
		}
		if (!ret) {
			g_ctx->sleep_mode = true;
		}
	} else {
        value[0] = BP5758D_ADDR_SETUP;
		value[1] = BP5758D_ENABLE_ALL_OUT;

		ret = send_i2c(value, 2);
		if (!ret) {
			g_ctx->sleep_mode = false;
		}
	}

	if (ret) {
		// printf("%s/ standby fail\n", enable ? "Enable" : "Disable");
	}

	return ret;
}

int bp5758d_get_max_level()
{
    return 1023;
}

int bp5758d_get_min_level()
{
    return 0;
}

int bp5758d_get_current_rgb_current()
{
    return g_ctx->rgb_set_current;
}

int bp5758d_get_current_cw_current()
{
    return g_ctx->cw_set_current;
}

int bp5758d_set_current(uint8_t rgb_current, uint8_t cw_current)
{
	uint8_t value[6];
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp5758d_set_standby(false);
		if (ret) {
			return ret;
		}
	}

	if (g_ctx->rgb_current != rgb_current) {
		ret = convert_current_value(&g_ctx->rgb_current, rgb_current);
		if (ret) {
			printf("Invalid RGB current\n");
			return -1;
		}
        g_ctx->rgb_set_current = rgb_current;
	}

	if (g_ctx->cw_current != cw_current) {
		ret = convert_current_value(&g_ctx->cw_current, cw_current);
		if (ret) {
			printf("Invalid CW current\n");
			return -1;
		}
        g_ctx->cw_set_current = cw_current;
	}

	value[0] = BP5758D_ADDR_OUT1_CR;
	value[1] = g_ctx->rgb_current;
	value[2] = g_ctx->rgb_current;
	value[3] = g_ctx->rgb_current;
	value[4] = g_ctx->cw_current;
	value[5] = g_ctx->cw_current;

	return send_i2c(value, 6);
}

int bp5758d_set_channel(enum bp5758d_channel channel, uint16_t ch_value)
{
	uint8_t value[3] = { 0, };
	uint8_t addr;
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp5758d_set_standby(false);
		if (ret) {
			printf("set normal mode failed : %d\n", ret);
			return ret;
		}
	}

	value[1] = (ch_value & 0x1F);
	value[2] = (ch_value >> 5);

	addr = get_mapping_addr(channel);
	value[0] = addr;

	return send_i2c(value, 3);
}

int bp5758d_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
	uint8_t value[7] = { 0, };
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp5758d_set_standby(false);
		if (ret) {
			printf("set normal mode failed : %d\n", ret);
			return ret;
		}
	}

    value[0] = BP5758D_ADDR_OUT1_GL;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_R] * 2 + 0] = value_r & 0x1F;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_R] * 2 + 1] = value_r >> 5;

	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_G] * 2 + 0] = value_g & 0x1F;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_G] * 2 + 1] = value_g >> 5;

	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_B] * 2 + 0] = value_b & 0x1F;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_B] * 2 + 1] = value_b >> 5;

	return send_i2c(value, 7);
}

int bp5758d_set_cw_channel(uint16_t value_c, uint16_t value_w)
{


	uint8_t value[5] = { 0, };
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp5758d_set_standby(false);
		if (ret) {
			printf("set normal mode failed : %d\n", ret);
			return ret;
		}
	}

    value[0] = BP5758D_ADDR_OUT4_GL;
	value[1 + (g_ctx->mapping_addr[BP5758D_CHANNEL_W] - BP5758D_PIN_OUT4) * 2 + 0] = value_w & 0x1F;
	value[1 + (g_ctx->mapping_addr[BP5758D_CHANNEL_W] - BP5758D_PIN_OUT4) * 2 + 1] = value_w >> 5;

	value[1 + (g_ctx->mapping_addr[BP5758D_CHANNEL_C] - BP5758D_PIN_OUT4) * 2 + 0] = value_c & 0x1F;
	value[1 + (g_ctx->mapping_addr[BP5758D_CHANNEL_C] - BP5758D_PIN_OUT4) * 2 + 1] = value_c >> 5;

	return send_i2c(value, 5);
}

int bp5758d_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w)
{


	printf("-----------> 1  set  rgbcw %d %d %d %d %d \n ",value_r,value_g,value_b ,value_c,value_w);	
	matter_wlt_light_set_rgbcw(value_c,value_w ,value_r,value_g,value_b);
	return 0;


	uint8_t value[11] = { 0, };
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp5758d_set_standby(false);
		if (ret) {
			return ret;
		}
	}

#if 1
	printf("[%s, %d] r: %d, g: %d, b: %d, c: %d, w: %d\n", __func__, __LINE__,
            value_r, value_g, value_b, value_c, value_w);
#endif

    value[0] = BP5758D_ADDR_OUT1_GL;

	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_R] * 2 + 0] = value_r & 0x1F;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_R] * 2 + 1] = value_r >> 5;

	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_G] * 2 + 0] = value_g & 0x1F;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_G] * 2 + 1] = value_g >> 5;

	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_B] * 2 + 0] = value_b & 0x1F;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_B] * 2 + 1] = value_b >> 5;

	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_W] * 2 + 0] = value_w & 0x1F;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_W] * 2 + 1] = value_w >> 5;

	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_C] * 2 + 0] = value_c & 0x1F;
	value[1 + g_ctx->mapping_addr[BP5758D_CHANNEL_C] * 2 + 1] = value_c >> 5;

	ret = send_i2c(value, 11);
	if (ret) {
		return ret;
	}

	if (value_r == 0 && value_g == 0 && value_b == 0 &&
	    value_c == 0 && value_w == 0) {
		ret = bp5758d_set_standby(true);
	}

	return ret;
}

#if 0
#include <cli.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( x[ 0 ] ) )
#endif


static int do_led_init(int argc, char *argv[])
{
	bp5758d_init();

	return CMD_RET_SUCCESS;
}

static int do_led_deinit(int argc, char *argv[])
{
	bp5758d_deinit();

	return CMD_RET_SUCCESS;
}

static int do_led_sleep(int argc, char *argv[])
{
	int enable;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	enable = atoi(argv[1]);
	if (enable) {
		bp5758d_set_standby(true);
	} else {
		bp5758d_set_standby(false);
	}

	return CMD_RET_SUCCESS;
}

static int do_led_set(int argc, char *argv[])
{
	int channel;
	int value;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	channel = atoi(argv[1]);
	value = atoi(argv[2]);

	if (channel > BP5758D_CHANNEL_W) {
		return CMD_RET_USAGE;
	}

	if (value> 1024) {
		return CMD_RET_USAGE;
	}

	bp5758d_set_channel(channel, value);

	return CMD_RET_SUCCESS;
}

static int do_led_cur(int argc, char *argv[])
{
	int rgb_cur;
	int cw_cur;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	rgb_cur = atoi(argv[1]);
	cw_cur = atoi(argv[2]);

	bp5758d_set_current(rgb_cur, cw_cur);

	return CMD_RET_SUCCESS;
}

static int do_led_rgb(int argc, char *argv[])
{
	int value_r;
	int value_g;
	int value_b;

	if (argc != 4) {
		return CMD_RET_USAGE;
	}

	value_r = atoi(argv[1]);
	value_g = atoi(argv[2]);
	value_b = atoi(argv[3]);

	if (value_r > 1024 || value_g > 1024 || value_b > 1024) {
		return CMD_RET_USAGE;
	}

	bp5758d_set_rgb_channel(value_r, value_g, value_b);

	return CMD_RET_SUCCESS;
}

static int do_led_cw(int argc, char *argv[])
{
	int value_c;
	int value_w;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	value_c = atoi(argv[1]);
	value_w = atoi(argv[2]);

	if (value_c > 1024 || value_w > 1024) {
		return CMD_RET_USAGE;
	}

	bp5758d_set_cw_channel(value_c, value_w);

	return CMD_RET_SUCCESS;
}

static int do_led_all(int argc, char *argv[])
{
	int value_r;
	int value_g;
	int value_b;
	int value_c;
	int value_w;

	if (argc != 6) {
		return CMD_RET_USAGE;
	}

	value_r = atoi(argv[1]);
	value_g = atoi(argv[2]);
	value_b = atoi(argv[3]);
	value_c = atoi(argv[4]);
	value_w = atoi(argv[5]);

	if (value_r > 1024 || value_g > 1024 || value_b > 1024 ||
	    value_c > 1024 || value_w > 1024) {
		return CMD_RET_USAGE;
	}

	bp5758d_set_rgbcw_channel(value_r, value_g, value_b, value_c, value_w);

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd led_cmd[] = {
	CMDENTRY(init, do_led_init, "", ""),
	CMDENTRY(deinit, do_led_deinit, "", ""),
	CMDENTRY(sleep, do_led_sleep, "", ""),
	CMDENTRY(set, do_led_set, "", ""),
	CMDENTRY(cur, do_led_cur, "", ""),
	CMDENTRY(rgb, do_led_rgb, "", ""),
	CMDENTRY(cw, do_led_cw, "", ""),
	CMDENTRY(all, do_led_all, "", ""),
};

static int do_led(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], led_cmd, ARRAY_SIZE(led_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(led, do_led,
		"test LED control",
		"led init" OR
		"led deinit" OR
		"led sleep <enable>" OR
		"led set <channel> <scale>" OR
		"led cur <rgb> <cw>" OR
		"led rgb <R scale> <G scale> <B scale>" OR
		"led cw <C scale> <W scale>" OR
		"led all <R scale> <G scale> <B scale> <C scale> <W scale>"
   );
#endif
