#ifndef _BP5758D_H_
#define _BP5758D_H_

enum bp5758d_channel {
	BP5758D_CHANNEL_R = 0,
	BP5758D_CHANNEL_G,
	BP5758D_CHANNEL_B,
	BP5758D_CHANNEL_C,
	BP5758D_CHANNEL_W,
};

enum bp5758d_out_pin {
	BP5758D_PIN_OUT1 = 0,
	BP5758D_PIN_OUT2,
	BP5758D_PIN_OUT3,
	BP5758D_PIN_OUT4,
	BP5758D_PIN_OUT5,
};

int bp5758d_init(void);

int bp5758d_deinit(void);

int bp5758d_set_standby(bool enable);

int bp5758d_set_channel(enum bp5758d_channel channel, uint16_t ch_value);

int bp5758d_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b);

int bp5758d_set_cw_channel(uint16_t value_c, uint16_t value_w);

int bp5758d_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w);

#endif //_BP5758D_H_
