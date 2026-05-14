#ifndef _SM15633EH_H_
#define _SM15633EH_H_
// default on SCM2010 EVB, TIMER0-CHANNEL0 is mapped to GPIO 15
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "scm_timer.h"
#include "scm_gpio.h"
//#include "task.h"

#define SUPPORT_SM15633EH_LED_DRIVER

#define PWM_FREQUENCY  2000

#define PWM0_CHANNEL_ID				SCM_TIMER_IDX_0
#define WARM_PWM_CH			SCM_TIMER_CH_0
#define BLUE_PWM_CH			SCM_TIMER_CH_1
#define GREEN_PWM_CH			SCM_TIMER_CH_2
#define RED_PWM_CH			SCM_TIMER_CH_3

#define PWM1_CHANNEL_ID				SCM_TIMER_IDX_1
#define 	WHITE_PWM_CH		SCM_TIMER_CH_2

enum LED_PWM_CHANNEL
{
    LED_R_CHANNEL=0,//GPIO3
    LED_G_CHANNEL=1,//GPIO2
    LED_B_CHANNEL=2,//GPIO1
    LED_WHITE_CHANNEL=3,//GPIO6
    LED_WARM_CHANNEL=4,//GPIO15
};

typedef struct {
uint8_t rcolour;
uint8_t gcolour;
uint8_t bcolour;
uint8_t whitecolour;
uint8_t warmcolour;
}led_present_info_t;

#ifdef SUPPORT_SM15633EH_LED_DRIVER
int sm15633eh_pwm_init(uint8_t colour_select,uint8_t Rcolour,uint8_t Gcolour,uint8_t Bcolour,uint8_t Whitecolour,uint8_t Warmcolour);
int sm15633eh_pwm_start(uint8_t colour_select,bool onoff);
void sm15633eh_present_info_get(uint8_t *Rcolour,uint8_t *Gcolour,uint8_t *Bcolour,uint8_t *Whitecolour,uint8_t *Warmcolour);
#endif
#endif
