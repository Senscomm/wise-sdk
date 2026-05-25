#ifndef _IOTALINK_CONTROL_H
#define _IOTALINK_CONTROL_H




#include <lwip/sys.h>
#include "wise_wifi.h"
#include "wise_wifi_types.h"
#include "wise_system.h"

#include "scm_gpio.h"
#include "scm_timer.h"
#include "scm_adc.h"



typedef enum LIGHT_MODE{
 WHITE_MODE = 0,
 COLOR_MODE    ,  
// STATIC_MODE   ,  
 SCENE_MODE    , //预设
 MUSIC_MODE    , //本地音乐
 CUSTOME_MODE  ,//自定义4 
 MATTER_MODE  ,//暂时使用 避免同步问题 
 SLEEP_MODE ,//睡眠灯6
 MODE_MAX   =7 
} LIGHT_MODE_E;

typedef enum RGB_SEQUENCE{
    RGB = 0,
 	RBG,
 	BRG,
 	BGR,
 	GRB,
 	GBR
} LIGHT_RGB_SEQUENCE_E;

typedef struct {
    u16 red;  
    u16 green;
    u16 blue;
}color_rgb_t;

typedef struct {
    u16 red;
    u16 green;
    u16 blue;
    u16 white;
    u16 warm;
}bright_data_t;

typedef struct __attribute__((packed)){

	u8 matter_state;

    u8 switch_status;             
    LIGHT_MODE_E mode;
    u8 bright;
	
    u8 temper;
	
	u32 color;

	u8 magicunit;//scene
	u8  speed;//music
	u16 custome_unit;
	
	u8  musicunit;//music
	u8  sensitivity;//灵敏度

	u8 rgb_sequence;
//  color_rgb_st color;
//  color_origin_t color_origin;
//  u32 countdown;

// 存放白彩时解析的数据
	bright_data_t target_val;

}light_ctrl_data_t;

//--------------------------------------------------------------------------------------------
// 睡眠灯
typedef struct {
    // 模式开关
    uint8_t  sleep_enable;      // 睡眠模式：0=关闭 1=开启
    // 时间相关
    uint8_t  duration;          // 总时长：1~240 分钟 
	uint16_t  duration_sec;
    uint8_t  countdown_hour;    // 倒计时-时
    uint8_t  countdown_min;     // 倒计时-分
    uint8_t  countdown_sec;     // 倒计时-秒

    // 灯光颜色 + 亮度
    uint8_t  brightness;        // 亮度 0~100
    uint8_t  color_r;           // 红 0~255
    uint8_t  color_g;           // 绿 0~255
    uint8_t  color_b;           // 蓝 0~255

    // 音乐 + 音量
    uint8_t  volume;            // 音量 0~10
    uint8_t  music_id;          // 音乐编号 0~9
    uint8_t  play_ctrl;         // 播放控制：0=暂停 1=播放

    // 血氧（设备上报）
  //  uint8_t  blood_oxygen;      // 血氧值
} SleepStatus_t;

void duration_sec_to_hhmmss(uint32_t total_sec,  uint8_t *out_hour,uint8_t *out_min,uint8_t *out_sec);

u8 light_sleep_bright_get( void );

void wlt_sleep_data_ble_up( void  );
// 一键睡眠开
void wlt_sleep_start_deal(void);

// 一键睡眠关
void wlt_sleep_end_deal(void);
// 睡眠取消
void wlt_sleep_cancel_deal(void);

//--------------------------------------------------------------------------------------------




//仅仅第一次初始化写入，后面从flash读取
int iotalink_light_ctrl_data_init(void);


LIGHT_MODE_E light_mode_get(void);

u8 light_bright_get( void);


void light_mode_set(LIGHT_MODE_E mode);
void light_power_set(bool on_off);
void light_speed_set(u16 speed);
void light_magicunit_set(u16 magicunit);
void light_sensitivity_set(u8 sensitivity);
void light_musicunit_set(u8 musicunit);
void light_bright_set(u8 bright);
void light_temper_set(u8 temper);
void light_color_set(u32 color);
void light_rgb_sequence_set(LIGHT_RGB_SEQUENCE_E rgb_sequence);

void light_custome_unit_update(uint16_t value);


void light_mode_update(LIGHT_MODE_E locol_mode);
void light_power_update(bool on_off);
void light_brightness_update(uint32_t value);
void light_scene_update(uint16_t value);
void light_speed_update(uint32_t value);
void light_temperature_update(uint32_t value);
void light_color_update(uint32_t value);
void light_music_update(uint32_t value);
void light_sensitivity_update(uint32_t value);
void light_rgb_sequence_update(uint32_t value);


//--------------------------------------------------------------------------------------------





void iotalink_light_ctrl_process(void);

void iotalink_control_timer_init( void );

//定时器回调
void countdown_timer_cb(void *arv);
void auto_timer_cb(void *arv);
void flash_timer_cb(void *arv);
void sleep_timer_cb(void *arg);


void auto_flash_operation_init(void);



void wlt_ble_remote_control(u8 keyvalue);

void set_auto_flag(u8 state);


#endif

