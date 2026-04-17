#ifndef _BP1638CJ_H_
#define _BP1638CJ_H_

enum bp1638cj_channel {
	BP1638CJ_CHANNEL_R = 0,
	BP1638CJ_CHANNEL_G,
	BP1638CJ_CHANNEL_B,
	BP1638CJ_CHANNEL_T, // color Temperature
	BP1638CJ_CHANNEL_L, // brightness Level
};

enum bp1638cj_out_pin {
	BP1638CJ_PIN_OUT1 = 0,
	BP1638CJ_PIN_OUT2,
	BP1638CJ_PIN_OUT3,
	BP1638CJ_PIN_OUT4,
	BP1638CJ_PIN_OUT5,
};

int bp1638cj_init(void);
int bp1638cj_deinit(void);
int bp1638cj_set_standby(bool enable);
int bp1638cj_get_max_level();
int bp1638cj_get_min_level();
int bp1638cj_get_current_rgb_current();
int bp1638cj_get_current_cw_current();
int bp1638cj_set_current(uint8_t rgb_current, uint8_t cw_current);
int bp1638cj_set_channel(enum bp1638cj_channel channel, uint16_t ch_value);
int bp1638cj_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b);
int bp1638cj_set_cw_channel(uint16_t value_c, uint16_t value_w);
int bp1638cj_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w);

#endif //_BP1638CJ_H_