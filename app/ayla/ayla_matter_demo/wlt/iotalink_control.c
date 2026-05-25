


#include "iotalink.h"
#include "scm_flash.h"
#include "queue.h"


bool MUSIC_LOCAL_MODE;	
bool LOCAL_MAGIC_MODE =1;	
// 倒计时定时器
osTimerAttr_t countdown_timer_attr;
static osTimerId_t countdown_timer;
// 自动模式定时器
osTimerAttr_t auto_timer_attr;
static osTimerId_t auto_timer;
// 5s断电记忆
osTimerAttr_t flash_timer_attr;
static osTimerId_t flash_timer;
//睡眠倒计时定时器
osTimerAttr_t sleep_timer_attr;
static osTimerId_t sleep_timer;

static u8  auto_flag =0;


// 	云端或面板控制参数
light_ctrl_data_t sg_light_ctrl_data;



//
SleepStatus_t sleep_data;

int iotalink_sleep_data_init(void)
{

	//睡眠灯默认值
		sleep_data.sleep_enable =0; 	   // 睡眠开关
		sleep_data.duration = 10;		   // 总时长（分钟 1~240）
		sleep_data.brightness =10; 	   // 亮度 0~100
		sleep_data.color_r =255; 		   // R
		sleep_data.color_g =255;		   // G
		sleep_data.color_b =255;		   // B
		sleep_data.volume=5;			   // 音量 0~10
		//sleep_data.music_id=0;		   // 音乐编号 0~9
		//sleep_data.play_ctrl=0;		   // 播放控制
}


//仅仅第一次初始化写入，后面从flash读取
int iotalink_light_ctrl_data_init(void)
{

	printf("==============iotalink_light_ctrl_data_init===========\n");
	
	sg_light_ctrl_data.switch_status = 1;

#if 1
	//sg_light_ctrl_data.mode = SCENE_MODE;

	sg_light_ctrl_data.mode =WHITE_MODE;

	sg_light_ctrl_data.magicunit = 1;
	sg_light_ctrl_data.custome_unit= 1;//
	
	sg_light_ctrl_data.musicunit = 1;

	sg_light_ctrl_data.bright = 100;
	sg_light_ctrl_data.temper = 50;
	sg_light_ctrl_data.sensitivity = 50;
	sg_light_ctrl_data.speed = 50;
	sg_light_ctrl_data.color=0x00ff0000;
#else
	sg_light_ctrl_data.mode = MUSIC_MODE;
	sg_light_ctrl_data.musicunit = 0;
#endif
	sg_light_ctrl_data.target_val.red  =100;
	sg_light_ctrl_data.target_val.green = 100;
	sg_light_ctrl_data.target_val.blue =100;



	iotalink_sleep_data_init();


	iotalink_light_ctrl_process();

}



void iotalink_control_timer_init( void )
{
	iotalink_sleep_data_init();

	//定时器初始化
	countdown_timer = osTimerNew(countdown_timer_cb, osTimerOnce, NULL, &countdown_timer_attr);
	auto_timer = osTimerNew(auto_timer_cb, osTimerPeriodic, NULL, &auto_timer_attr);
	flash_timer = osTimerNew(flash_timer_cb, osTimerOnce, NULL, &flash_timer_attr);
	sleep_timer = osTimerNew(sleep_timer_cb, osTimerPeriodic, NULL, &sleep_timer_attr);
	//消息线程初始化
	auto_flash_operation_init();

}


LIGHT_MODE_E light_mode_get()
{

	return  sg_light_ctrl_data.mode;

}

u8 light_bright_get( void )
{
	return sg_light_ctrl_data.bright;
}


void light_mode_set(LIGHT_MODE_E mode)
{

	if(mode>=MODE_MAX)return;
	sg_light_ctrl_data.mode = mode;

	my_printf("light_mode_set==%d \n",mode);
	light_mode_update(mode);

}

void light_power_set(bool on_off)
{
	
	sg_light_ctrl_data.switch_status = on_off;
	
    light_power_update(on_off);
	
}
void light_bright_set(u8 bright)
{

	if(bright<=0 || bright>100) bright =1;//限制最低亮度
	
	sg_light_ctrl_data.bright = bright;

	my_printf("light_bright_set %d\r\n", bright);	

	light_brightness_update(bright);
	
}

void light_temper_set(u8 temper)
{
	sg_light_ctrl_data.temper = temper;
	
	my_printf("light_temper_set==%d \n",temper);
	
	light_temperature_update(temper);	

}

void cwrgb_target_val_set (unsigned short cold, unsigned short warm,unsigned short red, unsigned short green,unsigned short blue)
{
	sg_light_ctrl_data.target_val.white = cold;	
	sg_light_ctrl_data.target_val.warm  = warm;
	sg_light_ctrl_data.target_val.red   = red;
	sg_light_ctrl_data.target_val.green = green;
	sg_light_ctrl_data.target_val.blue  = blue;
}

void light_color_set(u32 color)
{
	sg_light_ctrl_data.color = color;
	light_color_update(color);
}

void light_magicunit_set(u16 magicunit)
{

	//if(SCENE_MODE != sg_light_ctrl_data.mode)		
	if(0 ==sg_light_ctrl_data.switch_status)
	return;
	
	sg_light_ctrl_data.magicunit = magicunit;
	
	light_scene_update(magicunit);

	my_printf("light_magicunit_set==%d \n",magicunit);	

}

void light_custome_unit_set(u16 custome_unit)
{

	//if(SCENE_MODE != sg_light_ctrl_data.mode)		
	if(0 ==sg_light_ctrl_data.switch_status)
	return;
	
	sg_light_ctrl_data.custome_unit = custome_unit;
	
	light_custome_unit_update(custome_unit);
	my_printf("light_custome_unit_set==%d \n",custome_unit);	

}


// 0-100 幻彩速度处理	
void light_speed_set(u16 speed)
{
	extern unsigned char magic_rate;
	
	if(speed>100)return;
	
// 分阶梯处理

	sg_light_ctrl_data.speed = speed;

	if(speed<20) magic_rate=(250-speed);
	else if (speed<80)magic_rate=(105-speed);
	else magic_rate=(103-speed);

	my_printf("light_speed_set==%d magic_rate %d \n",speed,magic_rate);	


	light_speed_update(speed);

}

void light_musicunit_set(u8 musicunit)
{
	if(0 ==sg_light_ctrl_data.switch_status)
	return;
	
	sg_light_ctrl_data.musicunit = musicunit;

	light_music_update(musicunit);
}



void light_sensitivity_set(u8 sensitivity)
{
	

	sg_light_ctrl_data.sensitivity = sensitivity;

	extern u8 MUSIC_SENSITIVITY ; // 用于律动灵敏度调整

	MUSIC_SENSITIVITY = 500 - (100-sensitivity)*4; // 用于律动灵敏度调整0-100--> 500 -100


	printf("light_sensitivity_set==%d  MUSIC_SENSITIVITY %d \n",sensitivity,MUSIC_SENSITIVITY);

	light_sensitivity_update(sensitivity);

}

void light_rgb_sequence_set(LIGHT_RGB_SEQUENCE_E rgb_sequence)
{
	

	sg_light_ctrl_data.rgb_sequence = rgb_sequence;

	light_rgb_sequence_update(rgb_sequence);
	
}


//----------------------------------- 解析&&处理 ---------------------------------------------------------------/



void light_ctrl_data_calculate_cw(u16 temperatue,  bright_data_t *result)
{

	result->white =   temperatue ;
	result->warm  = 100 - temperatue;

}
void light_ctrl_data_calculate_rgb(u32 color,  bright_data_t *result)
{

	result->red   =  (color>>16) & 0xFF;
	result->green  = (color>>8) & 0xFF;
	result->blue = color & 0xFF;

}


void iotalink_light_ctrl_process(void)
{

	if(auto_flag==0)	osTimerStop(auto_timer) ;

	rgb_colorful_buffer_clean();
	rgb_value_sync();
	wlt_led_pwm_set_duty(0,0);


	LOCAL_MAGIC_MODE =0;
	MUSIC_LOCAL_MODE =0;

//5s自动保存
	osTimerStart(flash_timer, MS_TO_TICKS(5000)) ;

	/*******************>>> 开灯 <<<****************************/
	if (sg_light_ctrl_data.switch_status) 
	{
		switch (sg_light_ctrl_data.mode)
		{
			case WHITE_MODE :


				light_ctrl_data_calculate_cw(sg_light_ctrl_data.temper,&sg_light_ctrl_data.target_val);
				printf(" c %d  w %d \n",sg_light_ctrl_data.target_val.white,sg_light_ctrl_data.target_val.warm);
				wlt_light_set_rgbcw(sg_light_ctrl_data.target_val.white,sg_light_ctrl_data.target_val.warm,0,0,0);

				
				break;
			case COLOR_MODE :


			    light_ctrl_data_calculate_rgb(sg_light_ctrl_data.color, &sg_light_ctrl_data.target_val);
				printf("-------------> rgb %d  %d %d \n",sg_light_ctrl_data.target_val.red,sg_light_ctrl_data.target_val.green,sg_light_ctrl_data.target_val.blue);
				wlt_light_set_rgbcw(0,0,sg_light_ctrl_data.target_val.red,sg_light_ctrl_data.target_val.green,sg_light_ctrl_data.target_val.blue);

			
				break;
			case SCENE_MODE :
			case CUSTOME_MODE:
				LOCAL_MAGIC_MODE =1;
				iotalink_magic_process_init();			
				break;
			case MUSIC_MODE :
				MUSIC_LOCAL_MODE = 1;
				break;
			case MATTER_MODE://临时显示
				wlt_light_set_rgbcw(sg_light_ctrl_data.target_val.white,sg_light_ctrl_data.target_val.warm,sg_light_ctrl_data.target_val.red,sg_light_ctrl_data.target_val.green,sg_light_ctrl_data.target_val.blue);
				break;
			case SLEEP_MODE ://睡眠灯
				wlt_light_set_rgbcw(0,0,sleep_data.color_r ,sleep_data.color_g, sleep_data.color_b  );
				break;

			default:
				
				break;
		}
	}
	else	//onoff ctrl state -- turn off
	{

		sg_light_ctrl_data.switch_status = 0;
		MUSIC_LOCAL_MODE = 0;
		LOCAL_MAGIC_MODE = 0;
		wlt_sleep_cancel_deal();
	}
}


void wlt_ble_app_control(u8 *buf, u16 length)
{
	printf("wlt_ble_app_control recv : \n");
	set_auto_flag(0);

	for (int i = 0; i < length; i++) 
	{
		printf("%02x ", buf[i]);
	}
	printf("\n");

	if(buf[0]== 0x1D) //头
	{

		u8 cmd = buf[1];
		switch(cmd)
		{
			case 1://亮度
			
				light_bright_set(buf[3]);

				break;
			case 2://速度
			
				light_speed_set(buf[2]);

				break;		
			case 3://
				//经典/场景 buf[2]1/2
				//模式号 buf[3]212/33
				if(buf[2] ==1) //
				{
					light_mode_set(CUSTOME_MODE);		
					light_custome_unit_set( buf[3] );

				}
				else if(buf[2] ==2)
				{
					light_mode_set(SCENE_MODE);
					light_magicunit_set( buf[3] );
		
				}
		
				break;	
			case 4://开关 buf[3]

				if(buf[3])
				light_power_set(1);
				else
				light_power_set(0);	
				
				break;
			case 5://  Byte2: 控制灯珠类。
				/* 0 保留；
				1 表示设置 RGB 彩灯颜色值； 
				2 表示设置 W 白灯色值；
				3 表示设置 CT 黄灯色值；
				4 表示设置 WCT 白灯和黄灯色值；
				Byte3: 
				Byte2 == 1 时表示红色值   0-255；
				Byte2 == 2 时表示白灯色值 0-100；
				Byte2 == 3 时表示黄灯色值 0-100；
				Byte2 == 4 时表示白灯色值 0-100；
				Byte4: 
				Byte2 == 4 时表示黄灯色值 0-100*/

				if(buf[2] ==1) //RGB
				{
		
					light_mode_set(COLOR_MODE);
					u32 rgb_color = buf[3]<<16|buf[4]<<8|buf[5];

					light_color_set(rgb_color);
					


				}
				else
				{
					light_mode_set(WHITE_MODE);
					//light_ctrl_data_calculate_cw();
					light_bright_set(buf[3]);
				}	 
				break;	
			case  6://律动效果
				light_mode_set(MUSIC_MODE);
				light_musicunit_set(buf[2]);//1-6

				break;
			case  7://灵敏度
				light_sensitivity_set(buf[2]);
				break;

//-------------------------------------- 0x17-1c ---------------------------------------------------------------------
			case 0x17://设置睡眠颜色和亮度
			
				light_mode_set(SLEEP_MODE); 

				sleep_data.brightness = buf[3];

				sleep_data.color_r   = buf[4];
				sleep_data.color_g   = buf[5];
				sleep_data.color_b  = buf[6];

				break;		
			case 0x18://控制串口 0x1D	0x18	0x01	0～10	0～9	0x00/0x01	0xff	0xff	0xD1

				sleep_data.volume    = buf[3];
				sleep_data.music_id  = buf[4];
				sleep_data.play_ctrl = buf[5];
				wlt_control_music(sleep_data.volume,sleep_data.music_id,sleep_data.play_ctrl);
				
				break;
			case 0x19://睡眠时长1～240
			
				sleep_data.duration = buf[3];	
				sleep_data.duration_sec = buf[3]*60;
				
				duration_sec_to_hhmmss(sleep_data.duration_sec,sleep_data.countdown_hour, sleep_data.countdown_min , sleep_data.countdown_sec);

			
				break;
				
			case 0x1a://一键睡眠	 控制串口 模式输出 
			
#if 0

				scm_gpio_write(6 , 0);
				wlt_ms_delay(200);
				scm_gpio_write(6 , 1);


				light_mode_set(COLOR_MODE);
				cwrgb_target_val_set (0, 0,10,0,50);

				return;
#else

				scm_gpio_write(6 , 0);
				wlt_ms_delay(200);
				scm_gpio_write(6 , 1);

				sleep_data.sleep_enable =buf[3];
				sleep_data.play_ctrl = buf[3];
				
				if(buf[3])wlt_sleep_start_deal();	
				else 
				{
	
					wlt_sleep_cancel_deal();
				}
			
				printf(" brightness %d rgb %d %d %d  \n",sleep_data.brightness,sleep_data.color_r,sleep_data.color_g ,sleep_data.color_b );
				
				break;
				
#endif
			case 0x1c://睡眠状态获取
			
				if(buf[2])	wlt_sleep_data_ble_up();
				
				break;
			
			default :
			
				break;


		}
		
		iotalink_light_ctrl_process();

	}
	else if(buf[0]==0x55&&buf[1]==0xAA)// 雷达配置
	{
		iotalink_send_data(buf,length);
	}	
	
}
//-------------------------------------------- beacon 遥控 ----------------------------------------------------//
// 键值宏定义

#define HYD_KEY_R1_L    	0
#define HYD_KEY_R1_M    	7
#define HYD_KEY_R1_R    	10
#define HYD_KEY_R2_L    	3
#define HYD_KEY_R2_M    	6
#define HYD_KEY_R2_R    	13
#define HYD_KEY_R3_L    	2
#define HYD_KEY_R3_M    	9
#define HYD_KEY_R3_R    	12
#define HYD_KEY_R4_L    	5
#define HYD_KEY_R4_R    	15
#define HYD_KEY_R5_M    	8
#define HYD_KEY_R6_L    	4
#define HYD_KEY_R6_R    	14

#define OFF_1H 	(60*60*1000)
#define OFF_2H  (120*60*1000)
#define OFF_3H  (240*60*1000)
#define OFF_4H   (320*60*1000)


// 初始化颜色数据
u32 color_list[] = {
   0xFF0000 ,//红
   0xFFA500,//  {"橙 (Orange)", 
   0xFFFF00,// {"黄 (Yellow)",  
   0x00FF00, //  {"绿 (Green)",  
   0x00FFFF,  // {"青 (Cyan)",
   0x0000FF,/// {"蓝 (Blue)",  
   0x800080, // {"紫 (Violet)"
   0xFFFFFF} ; // {"白 (White)",
u8 write_list[]={0,50,100};



void countdown_timer_cb(void *arv)
{

	my_printf("countdown_timer_cb");
	osTimerStop(countdown_timer);

	light_power_set(0);
	iotalink_light_ctrl_process();	
}
// 引入 FreeRTOS 核心头文件
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

// 操作参数结构体
typedef struct {
    uint8_t op_type;        // 操作类型：1=auto操作，2=flash操作
} op_param_t;

// 原生事件标志组
#define FLASH_OP_EVENT    (1UL << 0)  // Flash操作事件标志
EventGroupHandle_t xFlashEventGroup;  // FreeRTOS 事件标志组句柄

//
#define FLASH_THREAD_STACK_SIZE  1024
QueueHandle_t xFlashMsgQueue;         // FreeRTOS 消息队列句柄


void iotalink_auto_flash_operation_task(void *arg)
{
    op_param_t op_param;
    BaseType_t xQueueStatus;          // FreeRTOS 队列操作返回值
    EventBits_t uxEventFlags;         // FreeRTOS 事件标志返回值

    while (1)
    {
        // 参数说明：
        // 1. 事件标志组句柄 2. 等待的标志 3. 等待后自动清除标志 4. 仅匹配一个标志 5. 永久阻塞
        uxEventFlags = xEventGroupWaitBits( xFlashEventGroup,FLASH_OP_EVENT, pdTRUE,  pdFALSE, portMAX_DELAY );

        if ((uxEventFlags & FLASH_OP_EVENT) != 0)
        {
            // 从消息队列取出操作参数（非阻塞）
            xQueueStatus = xQueueReceive( xFlashMsgQueue, &op_param,0);

            if (xQueueStatus == pdPASS)  // FreeRTOS 队列成功返回 pdPASS
            {
                switch (op_param.op_type)
                {
                    case 1: // auto 操作

					    my_printf("AUTO  \n");
						#if 1
                        if( sg_light_ctrl_data.switch_status == 0 ) break;
                        light_mode_set(SCENE_MODE);
                        sg_light_ctrl_data.magicunit++;
                        if(sg_light_ctrl_data.magicunit > 33) sg_light_ctrl_data.magicunit = 1;
                        my_printf("---sg_light_ctrl_data.magicunit==%d \n", sg_light_ctrl_data.magicunit);
                        light_magicunit_set(sg_light_ctrl_data.magicunit);
                        iotalink_light_ctrl_process();
                      
						#endif
						break;
                    case 2: // flash 操作
                    
                    	taskENTER_CRITICAL();
                        scm_partition_erase(FLASH_PARTITION_TMP, 0, 4096);
                        scm_partition_write(FLASH_PARTITION_TMP, 0, &sg_light_ctrl_data, sizeof(sg_light_ctrl_data));
					    taskEXIT_CRITICAL();

						my_printf("scm_partition_write OK \n");
						 
                        break;

                    default:
                        break;
                }
            }
            memset(&op_param, 0, sizeof(op_param));
        }
    }
}

// 5. 初始化函数改为 FreeRTOS 原生 API
void auto_flash_operation_init(void)
{
    // 创建 FreeRTOS 事件标志组
    xFlashEventGroup = xEventGroupCreate();
    if (xFlashEventGroup == NULL)
    {
        // 初始化失败处理
        while(1);
    }

    // 创建  消息队列（参数：队列深度、每个元素大小、队列属性）
    xFlashMsgQueue = xQueueCreate(4, sizeof(op_param_t));
    if (xFlashMsgQueue == NULL)
    {
        while(1);
    }

	osThreadAttr_t attr = {
	.name 		= "iotalnk__auto_flash_thread",
	.stack_size = FLASH_THREAD_STACK_SIZE,
	.priority 	= osPriorityNormal,    //osPriorityNormal  = 24
	};
	osThreadNew(iotalink_auto_flash_operation_task, NULL, &attr);
}

// 6. auto_timer_cb 定时器回调
void auto_timer_cb(void *arg)
{
	my_printf("auto_timer_cb \n");
	
#if 1 
    op_param_t op_param = {
        .op_type = 1,  // auto_timer_cb 操作
    };
    // 软件定时器→服务线程上下文→用xQueueSend（非FromISR）
    BaseType_t xQueueStatus = xQueueSend(  xFlashMsgQueue, &op_param,0  );// 非阻塞发送

    if (xQueueStatus != pdPASS)
    {
        my_printf("auto_timer_cb: queue full! \n"); // 调试：队列满会导致数据丢失
        return;
    }

    // 发送事件标志（普通版本）
    xEventGroupSetBits(xFlashEventGroup, FLASH_OP_EVENT);

#else
	if( sg_light_ctrl_data.switch_status == 0 ) break;
	light_mode_set(SCENE_MODE);
	sg_light_ctrl_data.magicunit++;
	if(sg_light_ctrl_data.magicunit > 33) sg_light_ctrl_data.magicunit = 1;
	my_printf("---sg_light_ctrl_data.magicunit==%d \n", sg_light_ctrl_data.magicunit);
	light_magicunit_set(sg_light_ctrl_data.magicunit);
	iotalink_light_ctrl_process();

#endif
}

// flash_timer_cb 定时器回调
void flash_timer_cb(void *arg)
{
    my_printf("flash_timer_cb \n");

    op_param_t op_param = {
        .op_type = 2,  // flash 操作
    };
    BaseType_t xQueueStatus = xQueueSend(  xFlashMsgQueue, &op_param,0  );// 非阻塞发送
    if (xQueueStatus != pdPASS)
    {
        my_printf("flash_timer_cb: queue full! \n"); // 调试：队列满会导致数据丢失
        return;
    }

    // 发送事件标志（普通版本）
    xEventGroupSetBits(xFlashEventGroup, FLASH_OP_EVENT);

}


void set_auto_flag(u8 state)
{
	auto_flag = state;
}


void wlt_ble_remote_control(u8 keyvalue)
{

	static u8  color_i =0;
	static u8  write_i =0;

	switch(keyvalue)
	{
		case HYD_KEY_R1_L://开关	
			light_power_set(!sg_light_ctrl_data.switch_status);
			//iotalink_light_ctrl_process();

			if(auto_flag)wlt_ble_remote_control(HYD_KEY_R1_M);
			break;
		case HYD_KEY_R1_M://AUTO
		
			auto_flag =1;
			sg_light_ctrl_data.magicunit =0;
			//先默认20s切一次
			auto_timer_cb(NULL);
			osTimerStart(auto_timer, MS_TO_TICKS(20000)) ;
			
			break;
		case HYD_KEY_R2_L://1H  倒计时关？？？
			if(	sg_light_ctrl_data.switch_status ==0) return;
		//	osTimerStart(countdown_timer,   MS_TO_TICKS(6*1000)) ;
			osTimerStart(countdown_timer,   MS_TO_TICKS(OFF_1H)) ;
			//倒计时提醒
			light_power_set(0);
			iotalink_light_ctrl_process();
			wlt_light_set_rgbcw(0,0,0,0,0);
			wlt_ms_delay(100);
			light_power_set(1);
			//iotalink_light_ctrl_process();	
			break;
		case HYD_KEY_R2_M://2H
			if(	sg_light_ctrl_data.switch_status ==0) return;
		//	osTimerStart(countdown_timer,   MS_TO_TICKS(12*1000)) ;
			osTimerStart(countdown_timer,   MS_TO_TICKS(OFF_2H)) ;
						//倒计时提醒
			light_power_set(0);
			iotalink_light_ctrl_process();
			wlt_light_set_rgbcw(0,0,0,0,0);
			wlt_ms_delay(100);
			light_power_set(1);
			//iotalink_light_ctrl_process();	
			break;
		case HYD_KEY_R3_L://3H
		
			if(	sg_light_ctrl_data.switch_status ==0) return;
		//	osTimerStart(countdown_timer,   MS_TO_TICKS(18*1000)) ;
			osTimerStart(countdown_timer,   MS_TO_TICKS(OFF_3H)) ;
			//倒计时提醒
			light_power_set(0);
			iotalink_light_ctrl_process();
			wlt_light_set_rgbcw(0,0,0,0,0);
			wlt_ms_delay(100);
			light_power_set(1);
			//iotalink_light_ctrl_process();	
			break;
		case HYD_KEY_R3_M://4H
		
			if(	sg_light_ctrl_data.switch_status ==0) return;	
			//osTimerOnce
		//	osTimerStart(countdown_timer,   MS_TO_TICKS(24*1000)) ;
			osTimerStart(countdown_timer,   MS_TO_TICKS(OFF_4H)) ;
			//倒计时提醒
			light_power_set(0);
			iotalink_light_ctrl_process();
			wlt_light_set_rgbcw(0,0,0,0,0);
			wlt_ms_delay(100);
			light_power_set(1);
			//iotalink_light_ctrl_process();	

		
		break;
		case HYD_KEY_R1_R://MODE

			if(	sg_light_ctrl_data.switch_status ==0) return;

			auto_flag=0;
			
			light_mode_set(SCENE_MODE);

			sg_light_ctrl_data.magicunit++;
			if(sg_light_ctrl_data.magicunit>33)sg_light_ctrl_data.magicunit =1; 	
			my_printf("sg_light_ctrl_data.magicunit==%d \n",sg_light_ctrl_data.magicunit);
			light_magicunit_set(sg_light_ctrl_data.magicunit);
			iotalink_light_ctrl_process();
		break;
		case HYD_KEY_R2_R://MUSIC
		
			if(	sg_light_ctrl_data.switch_status ==0) return;
			auto_flag=0;
			light_mode_set(MUSIC_MODE);
			sg_light_ctrl_data.musicunit++;
			if(sg_light_ctrl_data.musicunit>6)sg_light_ctrl_data.musicunit =1;
			
			//iotalink_light_ctrl_process();
			my_printf("sg_light_ctrl_data.musicunit==%d \n",sg_light_ctrl_data.musicunit);
		break;	
		case HYD_KEY_R3_R://W
			if( sg_light_ctrl_data.switch_status ==0) return;

			auto_flag=0;
			write_i++;
			if(write_i>=3)write_i =0;
			light_mode_set(WHITE_MODE);
			light_temper_set(write_list[write_i]);
			//iotalink_light_ctrl_process();
			break;	

		case HYD_KEY_R5_M://COLOR
			if( sg_light_ctrl_data.switch_status ==0) return;

			auto_flag=0;
			color_i++;
			if(color_i>=8)color_i =0;
			light_mode_set(COLOR_MODE);
			light_color_set(color_list[color_i]);
			//iotalink_light_ctrl_process();
			break;
		case HYD_KEY_R4_L://亮度+

			if(	sg_light_ctrl_data.switch_status ==0) return;
				
		 	sg_light_ctrl_data.bright+=10;
		 	if(sg_light_ctrl_data.bright>=100)
		 	{
				sg_light_ctrl_data.bright =100;

				//阈值提醒
				light_power_set(0);
				iotalink_light_ctrl_process();
				
				wlt_light_set_rgbcw(0,0,100,100,100);
				wlt_ms_delay(100);
				light_power_set(1);
				//iotalink_light_ctrl_process();


			}
			
			break;
		case HYD_KEY_R6_L://亮度-

			if(	sg_light_ctrl_data.switch_status ==0) return;
			
			sg_light_ctrl_data.bright-=10;
		 	if(sg_light_ctrl_data.bright>100)
			{
				//阈值提醒
				light_power_set(0);
				iotalink_light_ctrl_process();
				
				wlt_light_set_rgbcw(0,0,100,100,100);
				wlt_ms_delay(100);
				light_power_set(1);
				//iotalink_light_ctrl_process();
				sg_light_ctrl_data.bright =9;
		 	}

			
		break;
		case HYD_KEY_R4_R://速度+

			if(	sg_light_ctrl_data.switch_status ==0) return;
			
			sg_light_ctrl_data.speed+=10;
			if(sg_light_ctrl_data.speed>=100)
			{
				sg_light_ctrl_data.speed =100;
		
				//阈值提醒
				light_power_set(0);
				iotalink_light_ctrl_process();
				
				wlt_light_set_rgbcw(0,0,100,100,100);
				wlt_ms_delay(100);
				light_power_set(1);
				//iotalink_light_ctrl_process();

			}
			light_speed_set(sg_light_ctrl_data.speed);
			
		break;
		case HYD_KEY_R6_R://速度-

			if(	sg_light_ctrl_data.switch_status ==0) return;
				
			sg_light_ctrl_data.speed-=10;
			if(sg_light_ctrl_data.speed>100)
			{
				//阈值提醒
				light_power_set(0);
				iotalink_light_ctrl_process();
				
				wlt_light_set_rgbcw(0,0,100,100,100);
				wlt_ms_delay(100);
				light_power_set(1);
				iotalink_light_ctrl_process();
				
				sg_light_ctrl_data.speed =9;
			}
			light_speed_set(sg_light_ctrl_data.speed);
		break;

		default :
			break;

	}

	iotalink_light_ctrl_process();
}


//-------------------------------------------------睡眠灯相关----------------------------------------------------------------//
/**
* @brief  
* @retval 睡眠模式亮度
*/
u8 light_sleep_bright_get( void )
{
	return sleep_data.brightness;
}


/**
 * @brief  秒数 转换为 时:分:秒
 * @param  total_sec  输入：总秒数
 * @param  out_hour   输出：小时
 * @param  out_min    输出：分钟
 * @param  out_sec    输出：秒
 * @retval 无
 */
void duration_sec_to_hhmmss(uint32_t total_sec,  uint8_t *out_hour,uint8_t *out_min,uint8_t *out_sec)
{
    // 计算小时：1小时=3600秒
    *out_hour = total_sec / 3600;
    // 计算剩余秒数
    uint32_t remain = total_sec % 3600;
    // 计算分钟
    *out_min = remain / 60;  
    // 剩余就是秒
    *out_sec = remain % 60;
}

//上报sleep_data
/*
睡眠状态上传（状态改变主动上传或者App获取时上传；
开启一键睡眠后，每隔一秒上传状态，同步倒计时给App*/
void wlt_sleep_data_ble_up( void  )
{
//	0x1D	0x1c	0x02	0x00/0x01	1～240 0～23	0～59	0～59	0～100 0～255 0～255 0～255 0～10	0～9	0x00/0x01	0 xff	0xD1
	//						    	 一键睡眠	时长	时	分       秒	 亮度	红    绿 蓝 	  音量 编号 	 放停
	u8 sleep_ble[17]={0x1D,0x1c,0x02,0x01,   120,  1, 20,   5,   80, 255,255,255, 5, 3,     1, 0xff, 0xD1};
	// ==============================================
	// 把 sleep_data 填充到 sleep_ble 发送数组
	// ==============================================
	sleep_ble[3]  = sleep_data.sleep_enable; 	   // 睡眠开关
	sleep_ble[4]  = sleep_data.duration; 		   // 总时长（分钟 1~240）
	sleep_ble[5]  = sleep_data.countdown_hour;	   // 倒计时-时
	sleep_ble[6]  = sleep_data.countdown_min;	   // 倒计时-分
	sleep_ble[7]  = sleep_data.countdown_sec;	   // 倒计时-秒
	sleep_ble[8]  = sleep_data.brightness;		   // 亮度 0~100
	sleep_ble[9]  = sleep_data.color_r;			   // R
	sleep_ble[10] = sleep_data.color_g; 		   // G
	sleep_ble[11] = sleep_data.color_b; 		   // B
	sleep_ble[12] = sleep_data.volume;			   // 音量 0~10
	sleep_ble[13] = sleep_data.music_id;		   // 音乐编号 0~9
	sleep_ble[14] = sleep_data.play_ctrl;		   // 播放控制
//	hex_dump("sleep_ble",sleep_ble,17);

	wlt_radar_notify(sleep_ble,sizeof(sleep_ble));


}
// 一键睡眠 需提前有时长和颜色
void wlt_sleep_start_deal(void)
{
	//1s定时器  
	osTimerStart(sleep_timer, MS_TO_TICKS(1000)) ;

// 	时长 颜色用 默认值或断电前最后配置的数据
	sleep_data.duration_sec = sleep_data.duration*60;

	//printf("wlt_sleep_start_deal : duration_sec %d \n",sleep_data.duration_sec);

// 串口2控制 音乐模组
	wlt_control_music(sleep_data.volume,sleep_data.music_id,1);
	
	light_power_set(1);
	light_mode_set(SLEEP_MODE);	
	iotalink_light_ctrl_process();	
}
// 睡眠倒计时结束处理
void wlt_sleep_end_deal(void)
{	
	//关闭灯光音乐等
	light_power_set(0);
	iotalink_light_ctrl_process();
	//关音乐
	//wlt_control_music(sleep_data.volume,sleep_data.music_id,0);
	//sleep_data.sleep_enable =0;
	//wlt_sleep_data_ble_up();

}
// 睡眠取消
void wlt_sleep_cancel_deal(void)
{
	osTimerStop(sleep_timer);	
	//只关音乐
	wlt_control_music(sleep_data.volume,sleep_data.music_id,0);
	sleep_data.sleep_enable =0;
	wlt_sleep_data_ble_up();

}

// 睡眠倒计时处理
void sleep_timer_cb(void *arg)
{
	printf("sleep_data.duration_sec %d \n",sleep_data.duration_sec);
	
	if(sleep_data.duration_sec>0)
	{
		sleep_data.duration_sec--;	
		

		duration_sec_to_hhmmss(sleep_data.duration_sec,&sleep_data.countdown_hour, &sleep_data.countdown_min , &sleep_data.countdown_sec);	
		//每s上报剩余时间
		wlt_sleep_data_ble_up();
	}
	else if(sleep_data.duration_sec==0)//结束睡眠
	{		
		wlt_sleep_end_deal();
	}

}

