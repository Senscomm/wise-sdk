#include "iotalink.h"


#define button1 	18

#define button_cnt 1

#define SHORT_PRESS_TIME_MS  40   //短按最长时间  20次*10ms


 // 全局函数指针声明
//int (*button_event_handle)(unsigned char, unsigned int, unsigned char) = NULL;

typedef struct {
    u8 current_level; /*!< 当前检测的电平 位域1*/
    bool last_level:1;    /*!< 上次检测的电平 */
    unsigned long long int last_time; /*!< 最后一次触发事件时的时间点 */
    unsigned long long int btn_time_count;

	unsigned int  uiKeyTimeCnt1; //按键去抖动延时计数器
	unsigned char ucKeyTouchCnt1; //按键按下的次数记录
	unsigned int  uiKeyIntervalCnt1; //按键间隔的时间计数器
	unsigned char ucKeyLock1; //按键触发后自锁的变量标志
}button_t;

button_t button[button_cnt] = {0};

void button_detection_cb(void *data )
{
	//bk_printf("======button_detection_cb===========");
	unsigned long long int time = 0;
	unsigned char i = 0;
 
		
	scm_gpio_read(button1,&button[0].current_level);


	//printf("button[0].current_level :%d  \n",button[0].current_level);

	for(i=0;i<button_cnt;i++)
	{
	
		 button[i].btn_time_count++;
		 
		 time = 20 * (button[i].btn_time_count - button[i].last_time);//按下时间 20加一点

		
	//	printf("time :%llu  \n", button[i].btn_time_count - button[i].last_time); 
		
		 if (button[i].current_level==1 )//按键没按     一直刷新
		 {
			button[i].last_time = button[i].btn_time_count;
		 }
//****************>>>>>>>按键0短按及双击<<<<<<<************************//
		if(button[i].current_level==1)
		{
			 button[i].ucKeyLock1=0; //按键自锁标志清零
			 button[i].uiKeyTimeCnt1=0;//按键去抖动延时计数器清零。
			 
             if(button[i].ucKeyTouchCnt1>0) //之前已经有按键触发过一次，再来一次就构成双击            1-》2
             {
                 button[i].uiKeyIntervalCnt1++; //按键间隔的时间计数器累加

				 
				//	printf("uiKeyIntervalCnt1 :%d  \n",  button[i].uiKeyIntervalCnt1);
			
                 if(button[i].uiKeyIntervalCnt1>SHORT_PRESS_TIME_MS) //超过最大允许的间隔时间   //超过500ms没有再次按下触发
                 {
				 
                    button[i].uiKeyIntervalCnt1=0; //时间计数器清零
                    button[i].ucKeyTouchCnt1=0; //清零按键的按下的次数
                     
                    /** 短按释放处理 */

					iotalink_button_event_handle(i,time, false);
					//if (button_event_handle)
					{
						//button_event_handle(i,time, false);
					}					
                 }
				 
             }
			 if (time>=500)//长按释放处理
			 {
				//printf("long release time:% ",time,);
			 }
					
		}
		 
		else if(button[i].ucKeyLock1==0)//else 按键0按下，且是第一次被按下  
		{
		  button[i].uiKeyTimeCnt1++; //累加定时中断次数
		  if(button[i].uiKeyTimeCnt1>3)//超过50ms
		  {
			 button[i].uiKeyTimeCnt1=0;//累加定时中断次数置零
			 button[i].ucKeyLock1=1;	//自锁按键置位,避免一直触发
			 button[i].uiKeyIntervalCnt1=0; //按键有效间隔的时间计数器清零
			 button[i].ucKeyTouchCnt1++;//连续按下次数0-》1
			//bk_printf (" ucKeyTouchCnt1 %d \n",ucKeyTouchCnt1); 
			 if(button[i].ucKeyTouchCnt1>1)  //连续被按了两次以上
			 {
				 //button_event_handle(i,time, true); 
				 	iotalink_button_event_handle(i,time, true);
				 button[i].ucKeyTouchCnt1=0;	//统计按键次数清零						 
			 }
		  }	
		}

//*********   >>>>> 长按  <<<<  *********//
	 	if (button[i].current_level == 0 && (0 == (time%500))&&(time>=400))
		{

			button[i].ucKeyTouchCnt1=0;//进入长按统计按键次数清零	，防止释放后判断短按
			/** 按键按下 */
			//if (button_event_handle)
			{
				//button_event_handle(i, time, false);
				iotalink_button_event_handle(i,time, false);
			}								
		}
	}
}
extern light_ctrl_data_t sg_light_ctrl_data;

/***********************>>>>>>按键事件实际处理<<<<<<<*****************************************/
int iotalink_button_event_handle(unsigned char btn, unsigned int press_time, unsigned char pressed)
{


	printf ("--------------> %d %d %d\n",btn,press_time,pressed);
#if 1
	extern void demo_button_toggle(unsigned long pressed, unsigned long released);
	demo_button_toggle(1,press_time+1);
#else
	bool _switch = sg_light_ctrl_data.switch_status;	
	u8 mode =  sg_light_ctrl_data.mode;
	/************************>>双击开关灯<<********************************/
	if (pressed==1)
	{
		light_power_set(!_switch);
		iotalink_light_ctrl_process();

		return;
	}


	
	if(press_time<200)
	{
		
		
		switch(sg_light_ctrl_data.mode)
		{
			case WHITE_MODE:
				
				break;
				
			case COLOR_MODE:
				break;
			case SCENE_MODE:

						sg_light_ctrl_data.magicunit++;
				if(sg_light_ctrl_data.magicunit>33)sg_light_ctrl_data.magicunit =1;		
				printf("sg_light_ctrl_data.magicunit==%d \n",sg_light_ctrl_data.magicunit);
				light_magicunit_set(sg_light_ctrl_data.magicunit);
				iotalink_light_ctrl_process();
			case CUSTOME_MODE:

				sg_light_ctrl_data.magicunit++;
					if(sg_light_ctrl_data.magicunit>233)sg_light_ctrl_data.magicunit =1; 	
					printf("sg_light_ctrl_data.magicunit==%d \n",sg_light_ctrl_data.magicunit);
					light_magicunit_set(sg_light_ctrl_data.magicunit);
					iotalink_light_ctrl_process();

	
				
				break;
			case MUSIC_MODE:

				sg_light_ctrl_data.musicunit++;
				if(sg_light_ctrl_data.musicunit>=5)sg_light_ctrl_data.musicunit =0;
				
				iotalink_light_ctrl_process();
				
				my_printf("sg_light_ctrl_data.musicunit==%d \n",sg_light_ctrl_data.musicunit);

				break;

			default:
				
				break;

		}

	}
	else if(press_time==500)
	{

		my_printf(" 模式   \n");
		
		mode++;
		mode = mode>=MODE_MAX ? 0: mode ;

		light_mode_set(mode);
		iotalink_light_ctrl_process();
		

	}
	else if(press_time>=3000)
	{
		//conf_reset_factory();
		printf("long \n");
		ada_conf_reset(1);
		//wise_restart();
	}
#endif

}



void iotalink_button_process(void*argv)
{

	while (1)
    {
		button_detection_cb(NULL);

		//svTaskDelay(1000);//10---》20ms
		osDelay(MS_TO_TICKS(20));

    }

}

//void button_init(int(*event_handle)(unsigned char, unsigned int, bool))
void button_init()
{

    printf("====button_init====\n");
	
  	scm_gpio_configure(button1,SCM_GPIO_PROP_INPUT_PULL_UP);
	scm_gpio_write(button1 , 1);
	
	//button_event_handle = event_handle;
	//printf("button_event_handle addr: %p\n", button_event_handle);
}

void iotalink_button_init(void )
{
//	button_init(iotalink_button_event_handle);
	button_init();
	//xTaskCreate(iotalink_button_process, "iotalink_button_process",512, NULL, 7, NULL);
	osThreadAttr_t attr = {
		.name 		= "button_task",
		.stack_size = 1024,
		.priority 	= osPriorityNormal,    //osPriorityNormal        = 24,osPriorityRealtime,//osPriorityLowcon
	};
	/* run the demo in a new thread to allow further CLI */
	osThreadNew(iotalink_button_process, NULL, &attr);
	

}

