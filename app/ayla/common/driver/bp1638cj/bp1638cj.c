#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "scm_timer.h"
#include "scm_gpio.h"
#include "wise_err.h"
#include "bp1638cj.h"

#define COLOR_LEVEL_MAX			0xFF
#define KHZ                     1000
#define MHZ                     1000000
#define PWM_FREQUENCY           (5 * KHZ) // 5kHz
#define PWM_CYCLE_PERIOD_IN_US  (1*MHZ / PWM_FREQUENCY) // 200us

#define BP1638CJ_MAX_PIN			5

#if 0
#define INVALID_ADDR			0xFF
#define BP1638CJ_ADDR_SLEEP   	0x80  //10 00 0110: Sleep mode bits set (OUT1-5 enable setup selected, ignored by chip)
#define BP1638CJ_ADDR_SETUP   	0x90  //10 01 0000: OUT1-5 enable/disable setup - used during init
#define BP1638CJ_ADDR_OUT1_CR 	0x91  //10 01 0001: OUT1 current range
#define BP1638CJ_ADDR_OUT2_CR 	0x92  //10 01 0010: OUT2 current range
#define BP1638CJ_ADDR_OUT3_CR 	0x93  //10 01 0011: OUT3 current range
#define BP1638CJ_ADDR_OUT4_CR 	0x94  //10 01 0100: OUT4 current range
#define BP1638CJ_ADDR_OUT5_CR 	0x95  //10 01 0101: OUT5 current range
#define BP1638CJ_ADDR_OUT1_GL 	0x96  //10 01 0110: OUT1 gray-scale level
#define BP1638CJ_ADDR_OUT2_GL 	0x98  //10 01 1000: OUT2 gray-scale level
#define BP1638CJ_ADDR_OUT3_GL 	0x9A  //10 01 1010: OUT3 gray-scale level
#define BP1638CJ_ADDR_OUT4_GL 	0x9C  //10 01 1100: OUT4 gray-scale level
#define BP1638CJ_ADDR_OUT5_GL 	0x9E  //10 01 1110: OUT5 gray-scale level
#endif
#define BP1638CJ_ENABLE_ALL_OUT 	0x1F
#define BP1638CJ_DISABLE_ALL_OUT	0x00

struct scm_pwm_channel {
    enum scm_timer_idx timer_idx;
    enum scm_timer_ch  ch_idx;
};
struct bp1638cj_ctx {
#if 0
	uint8_t rgb_current;
	uint8_t cw_current;
	uint8_t rgb_set_current;
	uint8_t cw_set_current;
#endif
	struct scm_pwm_channel mapping[BP1638CJ_MAX_PIN];
	bool sleep_mode;
};

static struct bp1638cj_ctx *g_ctx = NULL;

static int check_initialized(void)
{
	if (!g_ctx) {
		printf("Not initialized\n");
		return -1;
	}

	return 0;
}
#if 0
static uint8_t get_mapping_addr(enum bp1638cj_channel channel)
{
	uint8_t addr[] = {
		BP1638CJ_ADDR_OUT1_GL,
		BP1638CJ_ADDR_OUT2_GL,
		BP1638CJ_ADDR_OUT3_GL,
		BP1638CJ_ADDR_OUT4_GL,
		BP1638CJ_ADDR_OUT5_GL
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
#endif
static void bp1638cj_register_channel(enum bp1638cj_channel channel, enum scm_timer_idx timer_idx, enum scm_timer_ch ch_idx)
{
	g_ctx->mapping[channel].timer_idx = timer_idx;
	g_ctx->mapping[channel].ch_idx = ch_idx;
}

/* 
 * high, low : duration of high/low level unit: us 
 */
static int send_pwm(enum bp1638cj_channel channel, uint16_t high, uint16_t low)
{
	int ret = 0;
	struct scm_timer_cfg cfg;
    enum scm_timer_idx timer_idx = g_ctx->mapping[channel].timer_idx;
    enum scm_timer_ch ch_idx = g_ctx->mapping[channel].ch_idx;

    /*
     * LED mapping to a GPIO pin can be specified by the board configuration
     * For example, the default on SCM2010 EVB, TIMER0-CHANNEL0 is mapped to GPIO 15
     */

	/* you can further adjust PWM parameters to change LED brightness.
	 * for example, by modifying the high and low values through the corresponding function calls
	 */

	/* Configure as PWM mode */
	cfg.mode = SCM_TIMER_MODE_PWM;
	cfg.intr_en = 0;  			/* Disable interrupts */
	cfg.data.pwm.high = high;	/* Duration of high level (unit: us) */
	cfg.data.pwm.low = low;	    /* Duration of low level (unit: us) */
	cfg.data.pwm.park = (high == 0 ? 0 : 1);	/* Park value, set to 1/0, only take effect when timer stop */
        
	/* Call the TIMER driver configuration function */
	ret = scm_timer_configure(timer_idx, ch_idx, &cfg, NULL, NULL);
	if (ret) {
		printf("TIMER PWM configure error = %x\n", ret);
	} else if (high == 0 || low == 0) {
        /* Stop the TIMER, let .park decide on/off */
        scm_timer_stop(timer_idx, ch_idx);
    } else {
		/* Start the TIMER */
		scm_timer_start(timer_idx, ch_idx);
	}

    return ret;
}


int bp1638cj_init(void)
{
    int ret = 0;

	if (g_ctx) {
		printf("Already initialized\n");
		return -1;
	}

	g_ctx = malloc(sizeof(struct bp1638cj_ctx));
	if (!g_ctx) {
		printf("Not enough resource\n");
		return -1;
	}

    memset(g_ctx, 0, sizeof(*g_ctx));

	/*
	 * Configure channel-to-PWM mapping according to different LED module hardware
	 * This configuration corresponds to the SkyLighting LED module
	 */
	bp1638cj_register_channel(BP1638CJ_CHANNEL_R, SCM_TIMER_IDX_0, SCM_TIMER_CH_0); // PWM0
	bp1638cj_register_channel(BP1638CJ_CHANNEL_G, SCM_TIMER_IDX_0, SCM_TIMER_CH_1); // PWM1
	bp1638cj_register_channel(BP1638CJ_CHANNEL_B, SCM_TIMER_IDX_0, SCM_TIMER_CH_2); // PWM2
	bp1638cj_register_channel(BP1638CJ_CHANNEL_T, SCM_TIMER_IDX_0, SCM_TIMER_CH_3); // PWM3
	bp1638cj_register_channel(BP1638CJ_CHANNEL_L, SCM_TIMER_IDX_1, SCM_TIMER_CH_1); // PWM5

    // default : 100% warm white color 
    
    ret = bp1638cj_set_rgbcw_channel(100, 100, 100, 100, 100);
    return ret;
}

int bp1638cj_deinit(void)
{
	if (check_initialized() < 0) {
		return -1;
	}

	bp1638cj_set_rgbcw_channel(0, 0, 0, 0, 0);
	bp1638cj_set_standby(true);

	free(g_ctx);
	g_ctx = NULL;

	return 0;
}

int bp1638cj_shutdown(void)
{
    return bp1638cj_set_rgbcw_channel(0, 0, 0, 0, 0);
}

int bp1638cj_set_standby(bool enable)
{
	int ret = 0;

	if (check_initialized() < 0) {
		return -1;
	}

    /* FIXME: How to simulate standby w/ PWM ? 
     *        PWM auto keep parking (ie: standby mode) if all '0%' already
     */
	if (enable) {
		// DISABLE_ALL_OUT;
		g_ctx->sleep_mode = true;
        ret = bp1638cj_set_rgbcw_channel(0, 0, 0, 0, 0);
	} else {
		// ENABLE_ALL_OUT;
		g_ctx->sleep_mode = false;
        ret = bp1638cj_set_rgbcw_channel(100, 100, 100, 100, 100);
	}

	if (ret) {
		printf("%s/ standby fail\n", enable ? "Enable" : "Disable");
	}

	return ret;
}

int bp1638cj_get_max_level()
{
    return COLOR_LEVEL_MAX;
}

int bp1638cj_get_min_level()
{
    return 0;
}

inline static uint32_t value_to_pwm_high(uint16_t value)
{
    uint32_t ratio = ((value * 100 + 1) / (bp1638cj_get_max_level()-bp1638cj_get_min_level())); // round up
    return (ratio * PWM_CYCLE_PERIOD_IN_US) / 100; 
}


#if 0
int bp1638cj_get_current_rgb_current()
{
    return g_ctx->rgb_set_current;
}

int bp1638cj_get_current_cw_current()
{
    return g_ctx->cw_set_current;
}

int bp1638cj_set_current(uint8_t rgb_current, uint8_t cw_current)
{
	uint8_t value[6];
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp1638cj_set_standby(false);
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

	value[0] = BP1638CJ_ADDR_OUT1_CR;
	value[1] = g_ctx->rgb_current;
	value[2] = g_ctx->rgb_current;
	value[3] = g_ctx->rgb_current;
	value[4] = g_ctx->cw_current;
	value[5] = g_ctx->cw_current;

	return send_pwm(value, 6);
}
#endif
int bp1638cj_set_channel(enum bp1638cj_channel channel, uint16_t ch_value)
{

	int ret;
    uint16_t high, low;
    
	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp1638cj_set_standby(false);
		if (ret) {
			printf("set normal mode failed : %d\n", ret);
			return ret;
		}
	}
    high = value_to_pwm_high(ch_value);
    low = PWM_CYCLE_PERIOD_IN_US - high;


	return send_pwm(channel, high, low);
}

#if 0
int bp1638cj_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
	uint8_t value[7] = { 0, };
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp1638cj_set_standby(false);
		if (ret) {
			printf("set normal mode failed : %d\n", ret);
			return ret;
		}
	}

    value[0] = BP1638CJ_ADDR_OUT1_GL;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_R] * 2 + 0] = value_r & 0x1F;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_R] * 2 + 1] = value_r >> 5;

	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_G] * 2 + 0] = value_g & 0x1F;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_G] * 2 + 1] = value_g >> 5;

	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_B] * 2 + 0] = value_b & 0x1F;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_B] * 2 + 1] = value_b >> 5;

	return send_pwm(value, 7);
}

int bp1638cj_set_cw_channel(uint16_t value_c, uint16_t value_w)
{
	uint8_t value[5] = { 0, };
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp1638cj_set_standby(false);
		if (ret) {
			printf("set normal mode failed : %d\n", ret);
			return ret;
		}
	}

    value[0] = BP1638CJ_ADDR_OUT4_GL;
	value[1 + (g_ctx->mapping_addr[BP1638CJ_CHANNEL_W] - BP1638CJ_PIN_OUT4) * 2 + 0] = value_w & 0x1F;
	value[1 + (g_ctx->mapping_addr[BP1638CJ_CHANNEL_W] - BP1638CJ_PIN_OUT4) * 2 + 1] = value_w >> 5;

	value[1 + (g_ctx->mapping_addr[BP1638CJ_CHANNEL_C] - BP1638CJ_PIN_OUT4) * 2 + 0] = value_c & 0x1F;
	value[1 + (g_ctx->mapping_addr[BP1638CJ_CHANNEL_C] - BP1638CJ_PIN_OUT4) * 2 + 1] = value_c >> 5;

	return send_pwm(value, 5);
}
#endif

int bp1638cj_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_t, uint16_t value_l)
{
	int ret;

	if (check_initialized() < 0) {
		return -1;
	}

	if (g_ctx->sleep_mode) {
		ret = bp1638cj_set_standby(false);
		if (ret) {
			return ret;
		}
	}

    bp1638cj_set_channel(BP1638CJ_CHANNEL_R, value_r);
    bp1638cj_set_channel(BP1638CJ_CHANNEL_G, value_g);
    bp1638cj_set_channel(BP1638CJ_CHANNEL_B, value_b);
    bp1638cj_set_channel(BP1638CJ_CHANNEL_T, value_t);
    bp1638cj_set_channel(BP1638CJ_CHANNEL_L, value_l);

#if 0
	printf("[%s, %d] r: %d, g: %d, b: %d, c: %d, w: %d\n", __func__, __LINE__,
            value_r, value_g, value_b, value_c, value_w);

    value[0] = BP1638CJ_ADDR_OUT1_GL;

	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_R] * 2 + 0] = value_r & 0x1F;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_R] * 2 + 1] = value_r >> 5;

	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_G] * 2 + 0] = value_g & 0x1F;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_G] * 2 + 1] = value_g >> 5;

	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_B] * 2 + 0] = value_b & 0x1F;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_B] * 2 + 1] = value_b >> 5;

	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_W] * 2 + 0] = value_w & 0x1F;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_W] * 2 + 1] = value_w >> 5;

	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_C] * 2 + 0] = value_c & 0x1F;
	value[1 + g_ctx->mapping_addr[BP1638CJ_CHANNEL_C] * 2 + 1] = value_c >> 5;

	ret = send_pwm(value, 11);
	if (ret) {
		return ret;
	}
#endif

	if (value_r == 0 && value_g == 0 && value_b == 0 &&
	    value_t == 0 && value_l == 0) {
		ret = bp1638cj_set_standby(true);
	}

	return ret;
}
