#ifndef __IOTALINK_WS2812_H__
#define __IOTALINK_WS2812_H__



#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "scm_timer.h"


#include <ayla/log.h>

#define MS_TO_TICKS(ms) ((uint32_t)(((uint32_t)(ms) * osKernelGetTickFreq()) / (uint32_t)1000))



/**
 * The color mode of the LED strip.
 */
typedef enum light_color_mode
{
    light_color_mode_grb, /**< GRB mode. */
    light_color_mode_bgr, /**< BGR mode. */
    light_color_mode_brg, /**< cdt test mode. */
}light_color_mode_e;


typedef struct
{
	u16 sh; 	//色域	/* hsv参数 */
	u16 ss;    //饱和度
	u16 sv;	//亮度
}MAGIC_HSV_T;

typedef struct
{
	u16 r; 	//色域	/* rgb参数 */
	u16 g;    //饱和度
	u16 b;	//亮度
}MAGIC_RGB_T;

typedef struct 
{
    u16 magicunit;                //场景号 1 
    u8 magicspeed;				 //流动速度	3/4
	u8	 music_unit;   				//音乐模式2

		
    u8 magicallcnt;//计算得出	      //所有单元组数
    MAGIC_HSV_T    magic_hsv[80];//8----
    MAGIC_RGB_T    magic_rgb[80];
	
}MAGIC_SCENE_DATA_T;



#define iotalink_delay_100ns()    __asm("NOP");__asm("NOP");  __asm("NOP"); __asm("NOP") ;__asm("NOP"); __asm("NOP");  __asm("NOP"); __asm("NOP") ; __asm("NOP") ; __asm("NOP") ; __asm("NOP") ; __asm("NOP") ; __asm("NOP");  __asm("NOP"); 


#define   hsv_h_t   unsigned short   /*!< HSV模型 H的数据类型 */
#define   hsv_s_t   unsigned short   /*!< HSV模型 S的数据类型 */
#define   hsv_v_t   unsigned short   /*!< HSV模型 V的数据类型 */
#define   color_t   unsigned char   /*!< RGB模型 RGB值的数据类型 */




int iotalink_magic_init(void);
void iotalink_light_driver_init(void);

void rgb_to_hsv(unsigned char R, unsigned char G, unsigned char B, unsigned short *H, unsigned char *S, unsigned char *V);
void  hsv_to_rgb(unsigned char *R, unsigned char *G, unsigned char *B, unsigned short H, unsigned char S, unsigned char V);
void rgb_value_sync(void);//RGB数值输出
void  singleColor(unsigned short sh,unsigned char ss,unsigned char sv);

// 灯珠控制函数和数组
extern u8 rgb_colorful_values[77+1][3];  // 灯珠颜色缓冲区
void rgb_colorful_buffer_clean(void);      // 清空缓冲区
void rgb_colorful_buffer_set(unsigned char NUM, color_t R, color_t G, color_t B);  // 设置单个灯珠




/**************************************************** pwm ***********************************************************/

/*
#ifdef CONFIG_USE_TIMER0_PWM
		
		pinmap(15, "timer.0", "pwm0",  0),
		pinmap(16, "timer.0", "pwm1",  0),
		pinmap(17, "timer.0", "pwm2",  0),
		pinmap(18, "timer.0", "pwm3",  0),
#endif
	
#ifdef CONFIG_USE_TIMER1_PWM	
		pinmap(19, "timer.1", "pwm0",  0),
		pinmap(20, "timer.1", "pwm1",  0),
#endif

  
  typedef enum
  {
	  LED_PWM_CH_0,
	  LED_PWM_CH_1,
	  LED_PWM_CH_2,
	  LED_PWM_CH_3,
	  LED_PWM_CH_MAX,
  }E_LED_PWM_CHANNEL;
	  
#define LED_PWM_CYCLE 1000

#define USE_PWM_NUM 2

*/

/**************************************************** pwm  end ***********************************************************/

#endif//__IOTALINK_WS2812_H__
