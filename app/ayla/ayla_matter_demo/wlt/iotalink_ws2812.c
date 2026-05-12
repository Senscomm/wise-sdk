#include "iotalink_ws2812.h"
#include <lwip/sys.h>
#include "wise_wifi.h"
#include "wise_wifi_types.h"
#include "wise_system.h"
#include "scm_gpio.h"
#include "scm_timer.h"
#include "iotalink_control.h"

// PWM通道枚举（与 wlt_ws2812_spi.c 中定义一致）
typedef enum
{
	LED_PWM_CH_0,
	LED_PWM_CH_1,
	LED_PWM_CH_2,
	LED_PWM_CH_3,
	LED_PWM_CH_MAX,
}E_LED_PWM_CHANNEL;

// 声明 wlt_led_pwm_set_duty 函数（定义在 wlt_ws2812_spi.c）
extern void wlt_led_pwm_set_duty(E_LED_PWM_CHANNEL ch, u8 duty);


#define magic_color_gpio  24  //焕彩接口 

light_color_mode_e ws_light_color_mode;

//实际输出数组数据 后面可改成动态申请！！！！
u8 rgb_colorful_values[100+1][3] = { 0 };

u16  RGB_LED_NUM = 12;//灯珠数量--》app下发可调
//#define  RGB_LED_NUM  10 	//灯珠数量固定

u16  RGB_LED_NUM_INCREASE= 30;//灯珠数量--》app下发可调 至少30

unsigned char magic_rate=50;
unsigned char default_magic_rate=5;

unsigned char r, g, b; 
unsigned short hue;

static unsigned char sr[90]={0}; //颜色参数可调节
static unsigned char sg[90]={0};
static unsigned char sb[90]={0};

static unsigned char sr_bk[90]={0}; //颜色参数可调节
static unsigned char sg_bk[90]={0};
static unsigned char sb_bk[90]={0};
 
static unsigned short sh[90]={0};
static unsigned char  ss[90]={0};
static unsigned char  sv[90]={0};
//解决同类型不同模式重置初始化
static unsigned short  mode_i;
static unsigned short  mode_j;
static unsigned short  mode_k;

extern bool LOCAL_MAGIC_MODE;

//幻彩场景结构
MAGIC_SCENE_DATA_T MAGIC_SCENE_DATA={0};//幻彩场景结构
MAGIC_RGB_T  magic_rgb_relax[80]={0};

// 场景重置标记
static bool scene_reset_flag = false;

// 	云端或面板控制参数
extern light_ctrl_data_t sg_light_ctrl_data;


void iotalink_ws2812_gpio_init(void)
{
	printf("-----iotalink_ws2812_gpio_init!-------\n");
	scm_gpio_configure(magic_color_gpio,SCM_GPIO_PROP_OUTPUT);
	scm_gpio_write(magic_color_gpio,0);

}

#define  SDA_L 	 (*( (volatile unsigned int *) (0xF0700014)))&= (unsigned int)~(1<<magic_color_gpio)		 
#define  SDA_H 	( *( (volatile unsigned int *) (0xF0700014)))|= (1<<magic_color_gpio)


 void rgb_send_package(unsigned char R, unsigned char G, unsigned char B)
 {
	 unsigned int led_color = 0;
	 unsigned char i = 24;//先发高位 
	 
	led_color = (unsigned int)G << 16;
	led_color |= (unsigned int)R << 8;
	led_color |= (unsigned int)B;    
	
	 while (i)
	 {
		i--;
		if ((led_color >> i) & 0x1)
        {
			 SDA_H;//电平翻转180ns
			 iotalink_delay_100ns();		 
			 iotalink_delay_100ns();
			 iotalink_delay_100ns();
			 iotalink_delay_100ns(); 
			 iotalink_delay_100ns();
			 iotalink_delay_100ns();
			 iotalink_delay_100ns(); 
			 iotalink_delay_100ns();		 
			 SDA_L; 
			 iotalink_delay_100ns(); 
			 iotalink_delay_100ns();	
		 }
		 else
		 {
			  SDA_H;
			  iotalink_delay_100ns();
			  iotalink_delay_100ns();			  
			  SDA_L; 
			  iotalink_delay_100ns();
			  iotalink_delay_100ns();
			  iotalink_delay_100ns();
			  iotalink_delay_100ns();
			  iotalink_delay_100ns();
			  iotalink_delay_100ns();
			  iotalink_delay_100ns();
			  iotalink_delay_100ns(); 
		 }	
	 }
 }
//#include "scm_irq.h"


//发送测试
void rgb_send_test (void)
{ 
	 int flags;
	{	
	 rgb_send_package(0xFF,0x00,0x00);
	 rgb_send_package(0xFF,0x00,0x00);
	 rgb_send_package(0xFF,0x00,0x00);
	 rgb_send_package(0xFF,0x00,0x00);
	 rgb_send_package(0xFF,0x00,0x00);
	 rgb_send_package(0xFF,0x00,0x00);
	
	 wlt_ms_delay(500);


		
	 printf("---change----\n");
	
	 rgb_send_package(0x0,0xFF,0x00);
	 rgb_send_package(0x0,0xFF,0x00);
	 rgb_send_package(0x0,0xFF,0x00);
	 rgb_send_package(0x0,0xFF,0x00);
	 rgb_send_package(0x0,0xFF,0x00);
	 rgb_send_package(0x0,0xFF,0x00);
	
	wlt_ms_delay(500);

	}
}




void iotalink_light_driver_init(void)
{

	iotalink_ws2812_gpio_init();
	//pwm
	//wlt_pwm_timer_init();
	//rgb_send_test();
	iotalink_magic_init();

}
//灭整灯
void rgb_colorful_buffer_clean(void)
{
	  unsigned short led = 0;
	  for (led = 0; led < RGB_LED_NUM; led++)
	  {
		  rgb_colorful_values[led][0] = 0;
		  rgb_colorful_values[led][1] = 0;
		  rgb_colorful_values[led][2] = 0;
	  }
}


void  singleColor(unsigned short sh,unsigned char ss,unsigned char sv)
{
	unsigned short led = 0;
	 unsigned char r, g, b;
	 hsv_to_rgb(&r, &g, &b, sh, ss, sv);
	for (led = 0; led < RGB_LED_NUM; led++)
	{
		rgb_colorful_values[led][0] = r;
		rgb_colorful_values[led][1] = g;
		rgb_colorful_values[led][2] = b;
	}


}
void  rgbsingleColor(unsigned char r,unsigned short g,unsigned char b)
{
	unsigned short led = 0;
	for (led = 0; led < RGB_LED_NUM; led++)
	{
		rgb_colorful_values[led][0] = r;
		rgb_colorful_values[led][1] = g;
		rgb_colorful_values[led][2] = b;
	}


}

 //设置第 Num 个灯珠
 void rgb_colorful_buffer_set(unsigned char NUM ,color_t R, color_t G, color_t B )
{
	rgb_colorful_values[NUM][0] = R;
	rgb_colorful_values[NUM][1] = G;
	rgb_colorful_values[NUM][2] = B; 
}

 
  //设置第 Num 个灯珠
 void rgb_magic_rgb_relax_set(unsigned char NUM ,color_t R, color_t G, color_t B )
 {
	  magic_rgb_relax[NUM].r= R;
	  magic_rgb_relax[NUM].g=  G;
	  magic_rgb_relax[NUM].b = B; 
 }

 void  index_hsv_Move_Back (u8 x ,u8 y ,u16 sx,u8 sy ,u8 sz)
 {
 
	 unsigned char j;
	 for (j = y-1; j>x ; j--)
	 {			 
		 sh[j]=sh[j-1];
		 ss[j]=ss[j-1];
		 sv[j]=sv[j-1];
	 }
	 sh[x]=sx;
	 ss[x]=sy;
	 sv[x]=sz;
 }

 void  index_hsv_Move_Pre (u8 x ,u8 y ,u16 sx,u8 sy ,u8 sz)
 {

	 int  j;
	 
	 for (j = x; j< y-1; j++)
	 {			 
		 sh[j] =sh[j+1] ;
		 ss[j] =ss[j+1] ;
		 sv[j] =sv[j+1] ;
	 }
	 sh[y-1]=sx;
	 ss[y-1]=sy;
	 sv[y-1]=sz;


 }

  void  Move_Mun_Pre (int num)
{
	int RGB_LED_NUM= num;

	unsigned char temp[3]={0};
	unsigned char j;
	temp[0]=rgb_colorful_values[0][0];
	temp[1]=rgb_colorful_values[0][1];
	temp[2]=rgb_colorful_values[0][2];
	
	for (j = 0; j<RGB_LED_NUM-1 ; j++)
		{
			
			rgb_colorful_values[j][0] =rgb_colorful_values[j+1][0] ;
			rgb_colorful_values[j][1] =rgb_colorful_values[j+1][1] ;
			rgb_colorful_values[j][2] =rgb_colorful_values[j+1][2] ;
		}
	rgb_colorful_values[RGB_LED_NUM-1][0]=temp[0];
	rgb_colorful_values[RGB_LED_NUM-1][1]=temp[1];
	rgb_colorful_values[RGB_LED_NUM-1][2]=temp[2];
}
  void  Move_Mun_Back  (int num)
{
	int RGB_LED_NUM= num;
	
	unsigned char temp[3]={0};
	unsigned char j;
	temp[0]=rgb_colorful_values[RGB_LED_NUM-1][0];
	temp[1]=rgb_colorful_values[RGB_LED_NUM-1][1];
	temp[2]=rgb_colorful_values[RGB_LED_NUM-1][2];
	
	for (j = RGB_LED_NUM-1; j>0 ; j--)
	{
		
		rgb_colorful_values[j][0] =rgb_colorful_values[j-1][0] ;
		rgb_colorful_values[j][1] =rgb_colorful_values[j-1][1] ;
		rgb_colorful_values[j][2] =rgb_colorful_values[j-1][2] ;
	}
	rgb_colorful_values[0][0]=temp[0];
	rgb_colorful_values[0][1]=temp[1];
	rgb_colorful_values[0][2]=temp[2];
}
 void  Move_Pre ()
 {

	unsigned char temp[3]={0};
	unsigned char j;
	temp[0]=rgb_colorful_values[0][0];
	temp[1]=rgb_colorful_values[0][1];
	temp[2]=rgb_colorful_values[0][2];
	
	for (j = 0; j<RGB_LED_NUM-1 ; j++)
		{
			
			rgb_colorful_values[j][0] =rgb_colorful_values[j+1][0] ;
			rgb_colorful_values[j][1] =rgb_colorful_values[j+1][1] ;
			rgb_colorful_values[j][2] =rgb_colorful_values[j+1][2] ;
		}
	rgb_colorful_values[RGB_LED_NUM-1][0]=temp[0];
	rgb_colorful_values[RGB_LED_NUM-1][1]=temp[1];
	rgb_colorful_values[RGB_LED_NUM-1][2]=temp[2];
}
//数组逐次循环后移
void  Move_Back (void)
{
//	int RGB_LED_NUM= RGB_LED_NUM_INCREASE;
	
	unsigned char temp[3]={0};
	unsigned char j;
	temp[0]=rgb_colorful_values[RGB_LED_NUM-1][0];
	temp[1]=rgb_colorful_values[RGB_LED_NUM-1][1];
	temp[2]=rgb_colorful_values[RGB_LED_NUM-1][2];
	
	for (j = RGB_LED_NUM-1; j>0 ; j--)
	{
		
		rgb_colorful_values[j][0] =rgb_colorful_values[j-1][0] ;
		rgb_colorful_values[j][1] =rgb_colorful_values[j-1][1] ;
		rgb_colorful_values[j][2] =rgb_colorful_values[j-1][2] ;
	}
	rgb_colorful_values[0][0]=temp[0];
	rgb_colorful_values[0][1]=temp[1];
	rgb_colorful_values[0][2]=temp[2];
}

 //在  下标x后 插入某值rgb    ，其他值后移
void  index_colorful_buffer_Move_Back (u8 x ,u8 y ,u8 r,u8 g ,u8 b)
{

	unsigned char j;

	for (j = y-1; j>x ; j--)
	{			
		rgb_colorful_values[j][0]=rgb_colorful_values[j-1][0];
		rgb_colorful_values[j][1]=rgb_colorful_values[j-1][1];
		rgb_colorful_values[j][2]=rgb_colorful_values[j-1][2];
	}
	rgb_colorful_values[x][0]=r;
	rgb_colorful_values[x][1]=g;
	rgb_colorful_values[x][2]=b;
}

void  index_colorful_buffer_Move_Pre (u8 x ,u8 y ,u8 r,u8 g ,u8 b)
{

	unsigned char j;
	
	for (j = x; j< y-1; j++)
	{			
		rgb_colorful_values[j][0]=rgb_colorful_values[j+1][0];
		rgb_colorful_values[j][1]=rgb_colorful_values[j+1][1];
		rgb_colorful_values[j][2]=rgb_colorful_values[j+1][2];

	}
	rgb_colorful_values[y-1][0]=r;
	rgb_colorful_values[y-1][1]=g;
	rgb_colorful_values[y-1][2]=b;

}
//在  下标x后 插入某值sx，其他值后移
void  index_rgb_Move_Back (u8 x ,u8 y ,u8 sx,u8 sy ,u8 sz)
{

	unsigned char j;
	for (j = y-1; j>x ; j--)
	{			
		sr[j]=sr[j-1];
		sg[j]=sg[j-1];
		sb[j]=sb[j-1];
	}
	sr[x]=sx;
	sg[x]=sy;
	sb[x]=sz;
}

void  index_rgb_Move_Pre (u8 x ,u8 y ,u8 sx,u8 sy ,u8 sz)
{

	unsigned char j;
	
	for (j = x; j< y-1; j++)
	{			
			sr[j] =sr[j+1] ;
			sg[j] =sg[j+1] ;
			sb[j] =sb[j+1] ;
	}
	sr[y-1]=sx;
	sg[y-1]=sy;
	sb[y-1]=sz;
}

//数组逐次循环前移
void  Array_Color_Move_Pre (u8 x ,u8 y,u8* array)
{
	unsigned char temp=0;
	unsigned char j;
	temp=*(array+x);
	for (j = x; j< y-1 ; j++)
	{
		*(array+j)=*(array+j+1);
	}
	*(array+y-1)=temp;;
}


//数组逐次循环后移
void  Array_Color_Move_Back (u8 x ,u8 y,u8 * array)
{
	unsigned char temp=0;
	unsigned char j;
	temp=*(array+y-1) ;
	for (j = y-1; j>x ; j--)
	{
			
		*(array+j) =*(array+j-1) ;			
	}
	*(array+x)=temp;
}
//跳变RGG 数据输出 
 void iotalink_write_rgb_buffer(void )
 {
	u8 i=0;
	 for (i = 0; i<RGB_LED_NUM ; i++)
	 {			
		 rgb_colorful_buffer_set(i ,sr[i], sg[i], sb[i]); //写入当前点rgb	  
	 }	
	 rgb_value_sync(); //输出数据	
}
 void rgb_buffer_clean(void)
  {
	   unsigned short led = 0;
	   for (led = 0; led < RGB_LED_NUM; led++)
	   {
		   sr[led]=0;
		   sg[led]=0;
		   sb[led]=0;
	   }
  }

 void rgb_bk_buffer_clean(void)
 {
	  unsigned short led = 0;
	  for (led = 0; led < RGB_LED_NUM; led++)
	  {
		  sr_bk[led] =0;
		  sg_bk[led]=0;
	      sb_bk[led] = 0;
	  }
 }
 void iotalink_write_rgb_bk_buffer(void )
 {
	u8 i=0;
	 for (i = 0; i<RGB_LED_NUM ; i++)
	 {			
		 rgb_colorful_buffer_set(i ,sr_bk[i], sg_bk[i], sb_bk[i]); //写入当前点rgb	    
	 }	
	 rgb_value_sync(); //输出数据	
 }

 
int wlt_light_tool_get_abs_value( int value)
{
    return (value > 0 ? value : -value);
}


//呼吸调用
void  iotalink_hsv_breath(void)
{
	unsigned int i = 0 ;
   u8  sv_temp=0; 
   static u8 magicunit_bak=0;
   while(sv[0]!=sv_temp)//渐渐变亮
   {   
   
		for (i = 0; i<RGB_LED_NUM ; i++)   
		{
			if(sg_light_ctrl_data.mode == CUSTOME_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.custome_unit; 
					 return ;
				}
			}
			else if(sg_light_ctrl_data.mode == SCENE_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.magicunit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.magicunit; 
					return ;
				}
			}
			else return;

			hsv_to_rgb(&r, &g, &b, sh[i], ss[i], sv_temp);		
			rgb_colorful_buffer_set(i , r,	g, b ); 		
		}
		rgb_value_sync(); 
		sv_temp++;
		if (sv<20&&sv>10)wlt_ms_delay(3);//非线性加点延时更平滑,高了会有闪烁感
		if (sv<=10)wlt_ms_delay(5); 
		wlt_ms_delay(magic_rate);		//延时调速+-+-------
   }
   while(0!=sv_temp)//渐渐变暗
   {
		for (i = 0; i<RGB_LED_NUM ; i++)   
		{
			if(sg_light_ctrl_data.mode == CUSTOME_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.custome_unit; 
					 return ;
				}
			}
			else if(sg_light_ctrl_data.mode == SCENE_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.magicunit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.magicunit; 
					return ;
				}
			}
			hsv_to_rgb(&r, &g, &b, sh[i], ss[i], sv_temp);		
			rgb_colorful_buffer_set(i , r,	g, b ); 			
		}
		rgb_value_sync(); 
		sv_temp--;
		if (sv<20&&sv>10)wlt_ms_delay(3);//非线性加点延时更平滑,高了会有闪烁感
		if (sv<=10)wlt_ms_delay(5);
		wlt_ms_delay(magic_rate);		//延时调速+-+-------   
   }
}

//用于渐变RGB 数据控制	1
static void iotalink_rgb_relax(void)
{
   static s32 diff_red, diff_green, diff_blue;
   static  s32 uiGain ;
   uiGain=2;//(100-magic_rate)/15+1
   static s32 diffRed ,diffGreen,diffBlue;
   u8 i,j,k ;  
   static u8 magicunit_bak = 0;
   
   do//11变化
   {		   
			for (k=0;k<RGB_LED_NUM;k++)
			{
				if(sg_light_ctrl_data.mode == CUSTOME_MODE)
				{
					if (magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
					{
						 magicunit_bak = sg_light_ctrl_data.custome_unit; 
						 return ;
					}
				}
				else if(sg_light_ctrl_data.mode == SCENE_MODE)
				{
					if (magicunit_bak != sg_light_ctrl_data.magicunit||LOCAL_MAGIC_MODE==0)
					{
						 magicunit_bak = sg_light_ctrl_data.magicunit; 
						return ;
					}
				}
			   else return;


			   //实时rgb 差值
				diff_red   = sr[k] - sr_bk[k];
				diff_green = sg[k] - sg_bk[k];
				diff_blue  = sb[k] - sb_bk[k];				
			   if(diff_red != 0)
			   {
				   
				   if(wlt_light_tool_get_abs_value(diff_red) < uiGain){
						sr_bk[k] += diff_red;
				   }else{
					   if(diff_red < 0){
							sr_bk[k] -= uiGain;
					   }else{
							sr_bk[k] += uiGain;
					   }
				   }
			   }   
			   
			   if(diff_green != 0)
			   {
				   if(wlt_light_tool_get_abs_value(diff_green) < uiGain){
					   sg_bk[k] += diff_green;
				   }else{
					   if(diff_green < 0){
						   sg_bk[k] -= uiGain;
					   }else{
						   sg_bk[k] += uiGain;
					   }
				   }
			   }   
			   if(diff_blue != 0){
				   if(wlt_light_tool_get_abs_value(diff_blue) < uiGain){
						   sb_bk[k] += diff_blue;
				   }else{
					   if(diff_blue < 0){
						   sb_bk[k] -= uiGain;
					   }else{
						   sb_bk[k] += uiGain;
					   }
				   }
			   }
			}
			if(sg_light_ctrl_data.switch_status== 0)
			{
				///rgb_buffer_clean();
				diff_red   = 0;
				diff_green = 0;
				diff_blue  = 0;
			   iotalink_write_rgb_bk_buffer();
				break;//关灯不及时问题
			}
		   wlt_ms_delay(magic_rate+50);
		   iotalink_write_rgb_bk_buffer();					
   }while((diff_green != 0)||(diff_red != 0)||(diff_blue!= 0));
}


//每次过渡输出
 static int  iotalink_rgb_relax_1(u8 step)
{
	 //  u8 step=40;
	   static  s32 gain_red,gain_green,gain_blue ;
	   static s32 diffRed ,diffGreen,diffBlue;
	   u8 i=0,j,k=0 ;  
	   static u8 magicunit_bak = 0;
	   for (i=1;i<step;i++)
	   {
			if(sg_light_ctrl_data.mode == CUSTOME_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.custome_unit; 
					return ;
				}
			}
			else if(sg_light_ctrl_data.mode == SCENE_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.magicunit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.magicunit; 
					return ;
				}
			}
			else return;

			for (k=0;k<RGB_LED_NUM;k++)
			{
				   //实际rgb差值
					diffRed   = sr[k] - sr_bk[k];
					diffGreen = sg[k] - sg_bk[k];
					diffBlue  = sb[k] - sb_bk[k];  
					
					//步长
					gain_red=diffRed/step;
					gain_green=diffGreen/step;
					gain_blue=diffBlue/step; 
					
					//目标数值	
					rgb_colorful_values[k][0]=sr_bk[k]+gain_red*i;					
					rgb_colorful_values[k][1]=sg_bk[k]+gain_green*i;
					rgb_colorful_values[k][2]=sb_bk[k]+gain_blue*i;
					//关灯
					if((LOCAL_MAGIC_MODE==false))
					{
							k=RGB_LED_NUM;
							return ;
					}  
			}//magic_rate/10+20
			wlt_ms_delay(magic_rate*0.2+10);
			rgb_value_sync(); //输出数据
			
	   }   
	   memcpy(sr_bk,sr,sizeof(sr));
	   memcpy(sg_bk,sg,sizeof(sg));
	   memcpy(sb_bk,sb,sizeof(sb));
}



//每次过渡输出 自定义里
 static int  iotalink_rgb_relax_2(u8 step)
{
	 //  u8 step=40;
	   static  s32 gain_red,gain_green,gain_blue ;
	   static s32 diffRed ,diffGreen,diffBlue;
	   u8 i=0,j,k=0 ;  
	   static u8 magicunit_bak = 0;
	   for (i=1;i<step;i++)
	   {
			if(sg_light_ctrl_data.mode == CUSTOME_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.custome_unit; 
					 return ;
				}
			}
			else if(sg_light_ctrl_data.mode == SCENE_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.magicunit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.magicunit; 
					return ;
				}
			}
			for (k=0;k<RGB_LED_NUM;k++)
			{
				   //实际rgb差值
					diffRed   = sr[k] - sr_bk[k];
					diffGreen = sg[k] - sg_bk[k];
					diffBlue  = sb[k] - sb_bk[k];  
					
					//步长
					gain_red=diffRed/step;
					gain_green=diffGreen/step;
					gain_blue=diffBlue/step; 
					
					//目标数值	
					rgb_colorful_values[k][0]=sr_bk[k]+gain_red*i;					
					rgb_colorful_values[k][1]=sg_bk[k]+gain_green*i;
					rgb_colorful_values[k][2]=sb_bk[k]+gain_blue*i;
					//关灯
					if((LOCAL_MAGIC_MODE==false))
					{
							k=RGB_LED_NUM;
							return ;
					}  
			}//magic_rate/10+20
			wlt_ms_delay(20/(102-magic_rate)+10);
			rgb_value_sync(); //输出数据
			
	   }   
	   memcpy(sr_bk,sr,sizeof(sr));
	   memcpy(sg_bk,sg,sizeof(sg));
	   memcpy(sb_bk,sb,sizeof(sb));
}

//颜色分段
/*
 hsv_h_t s     			颜色设置
 unsigned char * sh		颜色输出到sh[]
unsigned char x 		灯珠段起点
unsigned char y   		灯珠段终点
*/
void  Color_Section( unsigned char x,unsigned char y, hsv_h_t s,unsigned char * sh)
{
	 unsigned int i = 0 ;
	  unsigned char r, g, b;  
	  		sh+=x;//指针偏移
	 for (i = x; i<y ; i++)
		{			
		  *sh=s;
		   sh++;					 
		}					
}



//解析字符串 构建颜色组
/*
num :颜色组
step:过渡分级
*/
void iotalink_color_transition(u8 num, u8 step , u8 *pSceneData,   MAGIC_SCENE_DATA_T  *pCtrlParam)
{
	 u16 usBrightness = 0, usTempature = 0;
	 u16 usValH = 0, usValS = 0, usValV = 0, usValT;
	 u16 usValR = 0, usValG = 0, usValB = 0;
	 mode_i =2;//第3个颜色
	 mode_k =RGB_LED_NUM;
	 
	 color_rgb_t RGBValue;
	 u16 i =0,k;
	 
	 static  s32 uiGain_red  ,uiGain_green,uiGain_blue ;
	 
	 static s32 diffRed ,diffGreen,diffBlue;	  
	 pCtrlParam->magicallcnt =num*step;//增加后的颜色分组	
	 
	 usValV=pSceneData[3];
	 
	 usValV = usValV == 0 ? 1: usValV; //这个面板1%会下发 0 
	 
//	 if(MUSIC_LOCAL_MODE)usValV =100;
	for (i=0; i<num;i++)
	{
		usValH = pSceneData[5 + 4*i]*255+pSceneData[6 + 4*i];
		usValS = pSceneData[7 + 4*i];
		usValT = pSceneData[8 + 4*i];		

//		usValV =usValT;

//		usValV = usValV == 0 ? 100: usValV;
	
	

		pCtrlParam->magic_hsv[i].sh = usValH;
		pCtrlParam->magic_hsv[i].ss =  usValS;
		pCtrlParam->magic_hsv[i].sv = usValV ;	
		hsv_to_rgb(&usValR, &usValG, &usValB,usValH, usValS, usValV);//解析幻彩rgb(0-255)
		pCtrlParam->magic_rgb[i].r = usValR;
		pCtrlParam->magic_rgb[i].g =  usValG;
		pCtrlParam->magic_rgb[i].b= usValB; 				
		//printf("1======[%d] h:%d s:%d v:%d ======\n",i,pCtrlParam->magic_hsv[i].sh,pCtrlParam->magic_hsv[i].ss ,pCtrlParam->magic_hsv[i].sv );
		printf("1======[%d] r:%d g:%d b:%d ======\n",i,pCtrlParam->magic_rgb[i].r,pCtrlParam->magic_rgb[i].g ,pCtrlParam->magic_rgb[i].b );
	}	
	//场景2 不需要过渡
		if(sg_light_ctrl_data.mode ==CUSTOME_MODE) iotalink_234_mode_mode_init();
//		if(MAGIC_SCENE_DATA.magicunit==12)
		if(pSceneData[0]==0)		
		{
			pCtrlParam->magicallcnt =num;//增加后的颜色分组	
			 return ;
		}
		pCtrlParam->magic_rgb[num].r=pCtrlParam->magic_rgb[0].r;
		pCtrlParam->magic_rgb[num].g=pCtrlParam->magic_rgb[0].g;
		pCtrlParam->magic_rgb[num].b=pCtrlParam->magic_rgb[0].b;
		for (i= 0; i<num; i++)
		{	
				//实际rgb 差值
				 diffRed   = pCtrlParam->magic_rgb[i+1].r - pCtrlParam->magic_rgb[i].r;
				 diffGreen = pCtrlParam->magic_rgb[i+1].g - pCtrlParam->magic_rgb[i].g;
				 diffBlue  = pCtrlParam->magic_rgb[i+1].b - pCtrlParam->magic_rgb[i].b;
				 //实时差值
  		     	uiGain_red	 = diffRed/step;
				uiGain_green = diffGreen/step;
				uiGain_blue  = diffBlue/step;
				for (k=0;k<step;k++)//固定step个数据        60/15
				{	
					magic_rgb_relax[i*step+k].r =pCtrlParam->magic_rgb[i].r+uiGain_red*k;
					magic_rgb_relax[i*step+k].g = pCtrlParam->magic_rgb[i].g+uiGain_green*k;
					magic_rgb_relax[i*step+k].b = pCtrlParam->magic_rgb[i].b+uiGain_blue*k;						
				}		
		}
		for (i= 0; i<pCtrlParam->magicallcnt; i++)
		{
			pCtrlParam->magic_rgb[i].r=magic_rgb_relax[i].r;
			pCtrlParam->magic_rgb[i].g=magic_rgb_relax[i].g;
			pCtrlParam->magic_rgb[i].b=magic_rgb_relax[i].b; 	
			printf("2======[%d] r:%d g:%d b:%d ======\n",i,pCtrlParam->magic_rgb[i].r,pCtrlParam->magic_rgb[i].g ,pCtrlParam->magic_rgb[i].b );
		}
}

/*公版静态模式 :
*
*	i:	app可自定义15组自定义颜色
*	j:loop : 每个颜色连续占的灯珠数(（过渡色在解析里加）)
*/
void  iotalink_magic_scene_static(void)
{
	// 原: loop=(RGB_LED_NUM+5)/MAGIC_SCENE_DATA.magicallcnt+1,
	// 改：loop=RGB_LED_NUM/MAGIC_SCENE_DATA.magicallcnt
	
   unsigned char i=0,j=0 ,loop=(RGB_LED_NUM)/(MAGIC_SCENE_DATA.magicallcnt),num=0, num_tep=0; 

	for (i= 0; i<MAGIC_SCENE_DATA.magicallcnt; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
	{
	   for (j= 0; j<loop;j++)
	   {		   
			rgb_colorful_buffer_set(num,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);  
			num++;//灯珠点
		   // if (num>=RGB_LED_NUM)break;
	   }
	   //if (num>=RGB_LED_NUM)break;
	}
	num_tep=num;
	for (i= num; i<RGB_LED_NUM_INCREASE; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
	{
		rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i-num_tep].r,MAGIC_SCENE_DATA.magic_rgb[i-num_tep].g,MAGIC_SCENE_DATA.magic_rgb[i-num_tep].b);    
	}	   
	rgb_value_sync();//输出
}

/***************************************************************************************/

//整灯同步呼吸测试
void  colorful_test4(void)
{
	unsigned int i = 0 ;
	static bool flag = 0;
	static hsv_h_t sh = 340;
	static hsv_s_t ss = 100;
	static hsv_v_t sv = 100;
	unsigned char r, g, b;
	if (flag == 0)
	{
		for (i = 0; i<RGB_LED_NUM ; i++)
		{
			hsv_to_rgb(&r, &g, &b, sh, ss, sv);		 	
			rgb_colorful_buffer_set(i , r,  g, b );	
		}
		rgb_value_sync(); 
		sv += 1;
	    if (sv > 80)//亮度80开始减弱
	    {
	       flag = 1;
	    }
		if (sv<20&&sv>10)wlt_ms_delay(3);//非线性加点延时更平滑,高了会有闪烁感
		if (sv<=10)wlt_ms_delay(5);
	}
	if (flag == 1)
	{
		 for (i = 0; i<RGB_LED_NUM ; i++)
		 {
			hsv_to_rgb(&r, &g, &b, sh, ss, sv);		 	
			rgb_colorful_buffer_set(i , r,  g, b );	
		 }
		 rgb_value_sync(); 
		sv -= 1;
	    if (sv < 2)//亮度1开始增强
	    {
	       flag = 0;
	    }
		if (sv<20&&sv>10)wlt_ms_delay(3);
		if (sv<=20)wlt_ms_delay(5);
	}
	//暖色
	if (sv == 2 )
		{
			sh+=5;
			if(sh>360)sh=340;
		}		
	wlt_ms_delay(4*default_magic_rate);			//延时调速+-+-------
}

//分 N==2  段 叠 加(掉落累加)
/*
j；  分段 RGB_LED_NUM/N
interval :  间隔   RGB_LED_NUM/N

*/
void colorful_test2_2(void)
{
	unsigned int i = 0,j=0;
	static hsv_h_t sh1 = 100;
	static  hsv_s_t ss = 100;
	static  hsv_v_t sv = 100;
	static  unsigned char num =1; //默认分2段RGB_LED_NUM/2
	u8 interval =RGB_LED_NUM/2;//奇数
	 unsigned char r, g, b;
	 hsv_to_rgb(&r, &g, &b, sh1, ss, sv);
		for (i =0 ; i<num; i++)
		{	
			//if (LOCAL_MAGIC_MODE!=true)return ;
			for (j=0;j<2;j++)//分2段
			{	
				rgb_colorful_buffer_set(i+interval*j , r,  g, b );//前一位置亮10=RGB_LED_NUM/5
				
			}				
			rgb_value_sync();
			wlt_ms_delay(default_magic_rate);
			if (LOCAL_MAGIC_MODE!=true){printf ("colorful_test2_2:\n"); return ;}
			if (i<num)
			{	
				for (j=0;j<2;j++)//分2段	
				{	
					rgb_colorful_buffer_set(i+interval*j , 0,  0, 0 );//前一位置灭
					
				}
			}			
			rgb_value_sync();		
			wlt_ms_delay(default_magic_rate); 		
		}
		num--;//叠加递减
		if (num==0)//一轮结束 颜色切换
		{
			num = interval+RGB_LED_NUM%2;
			sh1+=60;
			if(sh1>=360)sh1=0;
			printf ("6sh1:%d\n",sh1);
			rgb_colorful_buffer_clean();	
		}
		for (j=0;j<2;j++)//分2段	
		{	
			rgb_colorful_buffer_set(num+interval*j , r,  g, b );//保持最后位置亮  num= 	 9 8 7 ....
		} 			
		wlt_ms_delay(default_magic_rate);
		
}


//跑马灯初始化
void  colorful_paoma_init( void  )
{
	 unsigned int i = 0;
	 unsigned char r, g, b;
	/* static hsv_v_t sv[] = {0,  0,  3  ,10 ,30, 50, 65, 80 , 
							100,80,65, 50 ,30, 10  ,  5,   0,
							0,  3  ,10 ,30, 50, 65, 80 , 100 ,
							80,65, 50 ,40 ,30, 10    ,7,   5,      0,
							0,  5  ,10 ,30,40, 50, 65, 80 ,      100,
							80,65, 50 ,30, 10    ,7,  5,  3,						
					};//亮度变化模式2(呼吸)*/
	static hsv_h_t sh = 0;
	static hsv_s_t ss = 100;
	static hsv_v_t sv = 100;
					
	  rgb_colorful_buffer_clean();
	  
	  //初始状态
	 for (i = 0; i<RGB_LED_NUM ; i++)	 	
	 {
		  if ((i%3)==0) {  rgb_colorful_buffer_set(i , 0,  0, 0 );}
		  if ((i<3&&i>0)||(i<12&&i>9)||(i<21&&i>18)||(i<30&&i>27)||(i<39&&i>36)||(i<48&&i>45))  	{ hsv_to_rgb(&r, &g, &b, 240, ss, sv);rgb_colorful_buffer_set(i , r,  g, b );}			  
		  if ((i<6&&i>3)||(i<15&&i>12)||(i<24&&i>21)||(i<33&&i>30)||(i<42&&i>39)) { hsv_to_rgb(&r, &g, &b, 120, ss, sv);rgb_colorful_buffer_set(i , r,  g, b );}
		  if ((i<9&&i>6)||(i<18&&i>15)||(i<27&&i>24)||(i<36&&i>33)||(i<45&&i>42)||i==49||i==0){hsv_to_rgb(&r, &g, &b, 0, ss, sv);rgb_colorful_buffer_set(i , r,  g, b );}
			  
	 }
	 rgb_value_sync(); 
	
}


void  iotalink_mode_caihong_init( void  )
{
	  unsigned int i ;
       hue=0;  	  
	  for(i = 0 ; i<RGB_LED_NUM ; i++)
	  {					 		  
		  hsv_to_rgb(&r, &g, &b, hue, 100, MAGIC_SCENE_DATA.magic_hsv[0].sv);//亮度可调
		  index_rgb_Move_Pre(0,RGB_LED_NUM, r,g,b);//写入sr sbsg

		  rgb_magic_rgb_relax_set(i, r,g,b);
		  hue+=3;		 		 	 
	  }	
	  iotalink_write_rgb_buffer();//初始化输出
 
}


void  iotalink_mode_caihong_sync( void  )
{
  if(hue>=360 ) hue=0 ; 

  hsv_to_rgb(&r, &g, &b, hue, 100, MAGIC_SCENE_DATA.magic_hsv[0].sv);//用全局的那个sh
  index_rgb_Move_Pre(0,RGB_LED_NUM, r,g, b);
  iotalink_write_rgb_buffer();//初始化输出  
  hue+=3;
 	

}

  
//幻彩变化方式	
typedef enum
{
		MAGIC_SCENE_STATIC =0,//静态	   
		MAGIC_SCENE_RELAX ,//淡入淡出
		MAGIC_SCENE_JUMP,//跳变
		MAGIC_SCENE_BREATHE,//呼吸
		MAGIC_SCENE_TWINKLE,//闪烁
		MAGIC_SCENE_WATER=10,
		MAGIC_SCENE_RAINBOW,
		MAGIC_SCENE_MAX,
}MAGIC_CHANGE_MODE_E;


//全段 或整段 
void  iotalink_magic_scene_relax(int segment ,int    local_change_mode)
{

	unsigned char i,j ,led, num;  

	static u8 magicunit_bak = 0;

	 num=RGB_LED_NUM/segment;//默认分段分成5段
	   // num = RGB_LED_NUM ; //全段

   for (i= 0; i<MAGIC_SCENE_DATA.magicallcnt; i++)//颜色组
   {
	   
	
	   for (j= 0; j<num ; j++)//分5区，40/2/5 == 每组4个
	   {
			if(sg_light_ctrl_data.mode == CUSTOME_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.custome_unit; 
					 return ;
				}
			}
			else if(sg_light_ctrl_data.mode == SCENE_MODE)
			{
				if (magicunit_bak != sg_light_ctrl_data.magicunit||LOCAL_MAGIC_MODE==0)
				{
					 magicunit_bak = sg_light_ctrl_data.magicunit; 
					return ;
				}
			}
	
		   switch(local_change_mode)
		   {
			   case MAGIC_SCENE_RELAX://协同颜色亮度渐变	   
				   index_rgb_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);
				   break;
			   case MAGIC_SCENE_JUMP://跳变
			   
				   index_rgb_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[i].r, MAGIC_SCENE_DATA.magic_rgb[i].g, MAGIC_SCENE_DATA.magic_rgb[i].b);
				   break;
			   case MAGIC_SCENE_TWINKLE://闪烁
			   case MAGIC_SCENE_BREATHE://呼吸
				   index_hsv_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_hsv[i].sh, MAGIC_SCENE_DATA.magic_hsv[i].ss, MAGIC_SCENE_DATA.magic_hsv[i].sv);
				   break;
			   
		   }
	   }
	//   if (magic_init_en)
	   {
		   switch(local_change_mode)
		   {
			   case MAGIC_SCENE_RELAX://协同颜色亮度渐变			   
				   iotalink_rgb_relax();
				   break;		   
			   case MAGIC_SCENE_JUMP://跳变
				   iotalink_write_rgb_buffer();
				   //iotalink_write_hsv_buffer();
				   wlt_ms_delay(magic_rate*25);
				   break;
				   
			   case MAGIC_SCENE_BREATHE:
				   iotalink_hsv_breath();
				   
				   break;
			   case MAGIC_SCENE_TWINKLE://闪烁(跳变中间加灭灯)
				   iotalink_write_hsv_buffer();
				   wlt_ms_delay(magic_rate*18);
				   singleColor(0, 0, 0);
				   rgb_value_sync();
				   wlt_ms_delay(magic_rate*18);
				   break;
			   default:
				   break;
		   }
	   }
   
   }													   

}



//---------------------好易达场景 先放到200 +场景下标里测试 -------------------------------------/

//场景1：日出
void wlt_mode_sunup(void)
{
	static u32 step_count = 0;
	u16 current_hue;
	u8 current_saturation, current_value;
	u32 progress;
	int i;
	
	// 检查是否是新下发的指令
	if (scene_reset_flag) {
		step_count = 0;
		scene_reset_flag = false;
	}
	
	// 计算当前进度 (0-100%)，总步数100步，约10秒
	progress = step_count;
	if (progress > 100) {
		progress = 100;
		// 达到最亮后静止，不再循环
	}
	
	// 颜色变化：红色(0°) -> 黄色(60°) -> 白色(饱和度0)
	// 前50%进度：红色->黄色 (色相0->60)
	// 后50%进度：黄色->白色 (饱和度100->0)
	if (progress <= 50) {
		current_hue = (progress * 60) / 50; // 0 -> 60
		current_saturation = 100;
	} else {
		current_hue = 60;
		current_saturation = 100 - ((progress - 50) * 100) / 50; // 100 -> 0
	}
	
	// 亮度变化：10% -> 100%
	current_value = 10 + (progress * 90) / 100; // 10 -> 100
	
	// 将所有LED设置为相同颜色
	for (i = 0; i < RGB_LED_NUM; i++) {
		hsv_to_rgb(&sr[i], &sg[i], &sb[i], current_hue, current_saturation, current_value);
		rgb_colorful_buffer_set(i, sr[i], sg[i], sb[i]);
	}
	
	// 同步输出RGB值
	rgb_value_sync();
	
	// 延时约100ms，总时间10秒（100步 * 100ms = 10秒）
	iotalink_rgb_relax_1(10);
	
	// 增加步数（达到100后不再重置，保持最亮状态）
	if (step_count < 100) {
		step_count++;
	}
}

//场景2：日落
void wlt_mode_sundown(void)
{
	static u32 step_count = 0;
	u16 current_hue;
	u8 current_saturation, current_value;
	u32 progress;
	int i;
	
	// 检查是否是新下发的指令
	if (scene_reset_flag) {
		step_count = 0;
		scene_reset_flag = false;
	}
	
	// 计算当前进度 (0-100%)，总步数100步，约10秒
	progress = step_count;
	if (progress > 100) {
		progress = 100;
		// 达到最暗后静止，不再循环
	}
	
	// 颜色变化：白色(饱和度0) -> 黄色(60°) -> 红色(0°)
	// 前50%进度：白色->黄色 (饱和度0->100)
	// 后50%进度：黄色->红色 (色相60->0)
	if (progress <= 50) {
		current_hue = 60;
		current_saturation = (progress * 100) / 50; // 0 -> 100
	} else {
		current_hue = 60 - ((progress - 50) * 60) / 50; // 60 -> 0
		current_saturation = 100;
	}
	
	// 亮度变化：100% -> 10%
	current_value = 100 - (progress * 90) / 100; // 100 -> 10
	
	// 将所有LED设置为相同颜色
	for (i = 0; i < RGB_LED_NUM; i++) {
		hsv_to_rgb(&sr[i], &sg[i], &sb[i], current_hue, current_saturation, current_value);
		rgb_colorful_buffer_set(i, sr[i], sg[i], sb[i]);
	}
	
	// 同步输出RGB值
	rgb_value_sync();
	
	// 延时约100ms，总时间10秒（100步 * 100ms = 10秒）
	iotalink_rgb_relax_1(10);
	
	// 增加步数（达到100后不再重置，保持最暗状态）
	if (step_count < 100) {
		step_count++;
	}
}


//场景3：生日
void wlt_birthday(void)
{
    static u32 step_count = 0;
    static bool is_initialized = false;     // 标记是否已初始化选中的灯珠
    static bool is_transition = false;      // 标记是否进入颜色过渡阶段
    static u8 selected_leds[8] = {0};       // 存储选中的8颗灯珠索引
    static u16 start_hues[8] = {0};         // 存储初始颜色色相
    u16 current_hue;
    u8 current_saturation = 100;
    u8 current_value = 100;
    u32 progress;
    int i, j;
    
    // 检查是否是新下发的指令，需要重置状态
    if (scene_reset_flag) {
        step_count = 0;
        is_initialized = false;
        is_transition = false;
        scene_reset_flag = false;
    }
    
    // 计算当前进度 (0-100)，总步数100步，约10秒（每步100ms）
    progress = step_count;
    
    // 第一阶段（0-19步）：随机选择8颗灯珠并设置随机颜色，保持2秒
    if (!is_initialized) {
        // 只在第一次初始化时随机选择灯珠
        if (step_count == 0) {
            // 生成0-11的随机序列
            u8 temp_leds[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
            
            // Fisher-Yates 洗牌算法打乱顺序
            for (i = 11; i > 0; i--) {
                j = rand() % (i + 1);
                u8 temp = temp_leds[i];
                temp_leds[i] = temp_leds[j];
                temp_leds[j] = temp;
            }
            
            // 选择前8颗作为本次显示的灯珠，并设置随机颜色
            for (i = 0; i < 8; i++) {
                selected_leds[i] = temp_leds[i];
                start_hues[i] = rand() % 360;  // 随机色相 0-359
                
                // 设置选中灯珠的颜色
                hsv_to_rgb(&sr[selected_leds[i]], &sg[selected_leds[i]], &sb[selected_leds[i]], 
                          start_hues[i], current_saturation, current_value);
                rgb_colorful_buffer_set(selected_leds[i], sr[selected_leds[i]], sg[selected_leds[i]], sb[selected_leds[i]]);
            }
            
            // 关闭其他4颗灯珠
            for (i = 0; i < RGB_LED_NUM; i++) {
                bool is_selected = false;
                for (j = 0; j < 8; j++) {
                    if (i == selected_leds[j]) {
                        is_selected = true;
                        break;
                    }
                }
                if (!is_selected) {
                    rgb_colorful_buffer_set(i, 0, 0, 0);
                }
            }
            
            is_initialized = true;
        } else {
            // 在保持阶段（0-19步），保持颜色不变，只同步输出
            rgb_value_sync();
            iotalink_rgb_relax_1(10);
            step_count++;
            return;
        }
    }
    
    // 第二阶段（20-99步）：8颗灯珠颜色逐渐过渡到绿色，持续8秒
    if (!is_transition && step_count >= 20) {
        is_transition = true;
    }
    
    if (is_transition) {
        const u16 target_hue = 120;  // 绿色的色相
        
        // 计算过渡进度 (0-80步)
        u32 transition_step = step_count - 20;
        if (transition_step > 80) {
            transition_step = 80;
        }
        
        // 对8颗选中的灯珠进行颜色过渡
        for (i = 0; i < 8; i++) {
            // 计算当前色相：从起始色相线性过渡到120（绿色）
            int hue_diff = target_hue - start_hues[i];
            
            // 处理色相差值的环绕问题（确保走最短路径）
            if (hue_diff > 180) {
                hue_diff -= 360;
            } else if (hue_diff < -180) {
                hue_diff += 360;
            }
            
            // 线性插值计算当前色相
            int interpolated_hue = start_hues[i] + (hue_diff * (int)transition_step) / 80;
            
            // 确保色相值在0-359范围内
            while (interpolated_hue < 0) {
                interpolated_hue += 360;
            }
            while (interpolated_hue >= 360) {
                interpolated_hue -= 360;
            }
            
            current_hue = (u16)interpolated_hue;
            
            // 更新RGB值
            hsv_to_rgb(&sr[selected_leds[i]], &sg[selected_leds[i]], &sb[selected_leds[i]], 
                      current_hue, current_saturation, current_value);
            rgb_colorful_buffer_set(selected_leds[i], sr[selected_leds[i]], sg[selected_leds[i]], sb[selected_leds[i]]);
        }
    }
    
    // 同步输出RGB值
    rgb_value_sync();
    
    // 延时约100ms
    iotalink_rgb_relax_1(10);
    
    // 增加步数
    step_count++;
    
    // 完成一个周期（100步 = 10秒）后重置
    if (step_count >= 100) {
        step_count = 0;
        is_initialized = false;
        is_transition = false;
    }
}


//场景4：烛光
void wlt_candlelight(void)
{
    static u32 step_count = 0;
    static bool increasing = true;
    u16 current_hue;
    u8 current_saturation, current_value;
    u32 progress;
    int i;
    
    // 黄色固定值
    current_hue = 60;
    current_saturation = 100;
    
    // 控制亮度变化方向
    if (increasing) {
        // 亮度从10%增加到100%
        current_value = 10 + (step_count * 90) / 50; // 10 -> 100
        if (step_count >= 50) {
            increasing = false;
        }
    } else {
        // 亮度从100%减少到10%
        current_value = 100 - ((step_count - 50) * 90) / 50; // 100 -> 10
        if (step_count >= 100) {
            increasing = true;
            step_count = 0;
        }
    }
    
    // 将所有LED设置为相同颜色
    for (i = 0; i < RGB_LED_NUM; i++) {
        hsv_to_rgb(&sr[i], &sg[i], &sb[i], current_hue, current_saturation, current_value);
        rgb_colorful_buffer_set(i, sr[i], sg[i], sb[i]);
    }
    
    // 同步输出RGB值
    rgb_value_sync();
    
    // 延时约100ms，总时间10秒（100步 * 100ms = 10秒）
    iotalink_rgb_relax_1(10);
    
    // 增加步数
    step_count++;
}


//场景5： 烟花
void  colorful_firework(void)
{

	int RGB_LED_NUM=13*2;
	
	unsigned char i ,j,loop;  

     hsv_s_t ss = 100;	
	 hsv_v_t sv =100;
	 static char  num[30]={  0,1,5,6,7,8,
							 9,11,12,14,16,18,
							 19,20,21,22,25,22,
							 27,28,29,31,32,33,
						 35,36,37,40,42,44,
					 };

	 hsv_s_t sh1= 0;
	 	hsv_s_t sh2[30]={0,    25,   50,  0,   100,
					125,  150, 175, 200 , 225, 
					250 , 275, 300, 325,   350,
					240,  100,   155,   0,  75,
					125,  150, 175, 200 , 225,
						240,  100,   155,   0,  75,
					};

//烟花发射-----》《-----
	 singleColor(0, 0, 0);//清空  
	 rgb_value_sync();	
	// bk_printf("==========num:%d=\n",RGB_LED_NUM);
	for (i = 0; i<RGB_LED_NUM ; i++)
	{
	if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit!=5)return;
	   	sh1+=10;
		if (i>=RGB_LED_NUM/2) sv =0;//后半段0
		hsv_to_rgb(&r, &g, &b, sh1, ss, sv);	//转化
		index_colorful_buffer_Move_Pre(RGB_LED_NUM/2,RGB_LED_NUM, r,g, b );
		index_colorful_buffer_Move_Back(0,RGB_LED_NUM/2, r,	g, b );
		rgb_value_sync();
		wlt_ms_delay(default_magic_rate*12);
	}
//爆炸    @#@#@**@**#**@#*@#**#@
//随机亮点及颜色DISCO

	for (loop = 0; loop<15; loop++)
	{
		for (i =0 ; i <sizeof(num); i++)
		{
			if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit!=5)return;

			hsv_to_rgb(&r, &g, &b,sh2[i],100, 100);	//转化	sh[i]	
			rgb_colorful_buffer_set(num[i], r,	g, b ); //写入当前点rgb	
			//下一次随机亮点及颜色			
			num[i] = rand()%RGB_LED_NUM;
			sh2[i] = rand()%360; 		
	  	}
		 rgb_value_sync();//闪一次
	 	 wlt_ms_delay(default_magic_rate*20);
		 singleColor(0, 0, 0);//清空		
	}
	
}


//场景6：聚会
void  wlt_juhui(void)
{
    static u32 last_change_time = 0;
    u16 random_hue;
    u8 saturation = 100; // 最大饱和度
    u8 brightness = 100; // 最大亮度
    int i;
    
    // 检查是否是新下发的指令
    if (scene_reset_flag) {
        last_change_time = 0;
        scene_reset_flag = false;
    }
    
    // 获取当前时间
    u32 current_time = sys_jiffies();
    
    // 每2秒切换一次颜色
    if (current_time - last_change_time >= 200 || last_change_time == 0) {
        // 为每颗灯珠随机生成颜色
        for (i = 0; i < RGB_LED_NUM; i++) {
            // 生成随机色相 (0-359°)
            random_hue = rand() % 360;
            
            // 转换为RGB并设置到对应灯珠
            hsv_to_rgb(&sr[i], &sg[i], &sb[i], random_hue, saturation, brightness);
            rgb_colorful_buffer_set(i, sr[i], sg[i], sb[i]);
        }
        
        // 更新最后一次颜色变化时间
        last_change_time = current_time;
    }
    
    // 同步输出RGB值
    rgb_value_sync();
    
    // 延时约100ms（与其他场景保持一致的循环间隔）
    iotalink_rgb_relax_1(10);
}


//场景7：约会
void  wlt_yuehui(void)
{
    static u32 step_count = 0;
    u16 current_hue = 0;
    u8 current_saturation;
    u8 current_brightness = 100; // 最大亮度
    u32 progress;
    int i;
    
    // 检查是否是新下发的指令
    if (scene_reset_flag) {
        step_count = 0;
        scene_reset_flag = false;
    }
    
    // 计算当前进度 (0-240步，总时间12秒)
    progress = step_count;
    if (progress >= 240) {
        step_count = 0;
        progress = 0;
    }
    
    // 颜色过渡逻辑：白色->红色->白色
    // 白色：饱和度0；红色：色相0°，饱和度100
    // 时间分配：红色时间长(8秒)，白色时间短(2秒×2段)
    
    if (progress <= 40) {
        // 第一阶段：白色->红色 (0-40步，2秒)
        // 快速从白色过渡到红色
        current_saturation = (progress * 100) / 40; // 0 -> 100
        current_hue = 0; // 固定红色色相
    } else if (progress <= 200) {
        // 第二阶段：保持红色 (41-200步，8秒)
        // 红色持续时间较长
        current_saturation = 100; // 最大饱和度
        current_hue = 0; // 红色色相
    } else {
        // 第三阶段：红色->白色 (201-240步，2秒)
        // 快速从红色过渡到白色
        current_saturation = 100 - ((progress - 200) * 100) / 40; // 100 -> 0
        current_hue = 0; // 固定红色色相
    }
    
    // 将所有LED设置为当前颜色
    for (i = 0; i < RGB_LED_NUM; i++) {
        hsv_to_rgb(&sr[i], &sg[i], &sb[i], current_hue, current_saturation, current_brightness);
        rgb_colorful_buffer_set(i, sr[i], sg[i], sb[i]);
    }
    
    // 同步输出RGB值
    rgb_value_sync();
    
    // 无需额外延时，依赖线程循环的50ms延迟
    
    // 增加步数
    step_count++;
}


//模式8：星空（随机闪）
void star_twinkle( void )
{
	static u32 step_count = 0;
	static u32 last_star_update = 0;
	static u8 star_positions[2] = {0, 1};
	static u8 star_hues[2] = {0, 0};
	static u8 star_saturations[2] = {100, 100};
	static u8 star_values[2] = {100, 100};
	u8 i;
	u16 bg_hue = 220;        // 蓝色色相
	u8 bg_saturation = 80;   // 高饱和度蓝色
	u8 bg_value = 8;        // 低亮度背景
	u8 twinkle_value = 100;  // 闪烁时最大亮度

	// 检查是否是新下发的指令
	if (scene_reset_flag) {
		step_count = 0;
		last_star_update = 0;
		scene_reset_flag = false;
	}

	// 每1秒更新一次闪烁灯珠（约10个step_count，假设每个step约100ms）
	if (step_count - last_star_update >= 10) {
		// 随机选择2个不同位置的灯珠进行闪烁
		star_positions[0] = step_count % 12;  // 位置1
		star_positions[1] = (step_count + 6) % 12;  // 位置2，与位置1相隔约半圈

		// 随机生成闪烁灯珠的颜色（色相在0-360之间随机）
		star_hues[0] = (u8)(step_count * 37) % 256;  // 随机色相
		star_hues[1] = (u8)(step_count * 53 + 128) % 256;

		// 闪烁灯珠为最大亮度和饱和度
		star_saturations[0] = 100;
		star_saturations[1] = 100;
		star_values[0] = 100;
		star_values[1] = 100;

		last_star_update = step_count;
	}

	// 设置所有灯珠为夜空背景色
	for (i = 0; i < 12; i++) {
		hsv_to_rgb(&sr[i], &sg[i], &sb[i], bg_hue, bg_saturation, bg_value);
	}

	// 将闪烁灯珠设置为高亮随机颜色
	for (i = 0; i < 2; i++) {
		hsv_to_rgb(&sr[star_positions[i]], &sg[star_positions[i]], &sb[star_positions[i]],
		           star_hues[i], star_saturations[i], twinkle_value);
	}

	// 输出RGB值
	for (i = 0; i < 12; i++) {
		rgb_colorful_buffer_set(i, sr[i], sg[i], sb[i]);
	}
	rgb_value_sync();

	// 延时约100ms
	iotalink_rgb_relax_1(10);

	// 增加步数
	step_count++;
}


//模式9：浪漫
void wlt_romance(void)
{
	static u32 step_count = 0;
	static u8 twinkle_positions[3] = {0, 4, 8};  // 3颗闪烁灯珠位置
	static u8 twinkle_colors[3] = {0, 0, 0};     // 0=红色, 1=蓝色
	static u32 last_pos_update = 0;              // 上次位置更新时间
	u8 i;
	u16 bg_hue = 275;          // 紫色色相（约275°为纯紫色）
	u8 bg_saturation = 100;   // 紫色饱和度（最大）
	u8 bg_value;              // 背景亮度（呼吸效果）
	u8 twinkle_hue;           // 闪烁灯珠色相
	u8 twinkle_value;         // 闪烁灯珠亮度（呼吸效果）

	// 检查是否是新下发的指令
	if (scene_reset_flag) {
		step_count = 0;
		last_pos_update = 0;
		// 初始化背景色
		bg_value = 70;  // 初始亮度设置为中间值
		for (i = 0; i < 12; i++) {
			hsv_to_rgb(&sr_bk[i], &sg_bk[i], &sb_bk[i], bg_hue, bg_saturation, bg_value);
		}
		scene_reset_flag = false;
	}

	// 呼吸灯效果：亮度在40%-100%之间周期性变化
	// 使用正弦波效果，每3200步完成一个周期（约320秒），呼吸极慢
	bg_value = 40 + (u8)(30 * (1 + sin(step_count * 0.0019625f)));

	// 设置所有灯珠为紫色背景（带呼吸效果）
	for (i = 0; i < 12; i++) {
		hsv_to_rgb(&sr[i], &sg[i], &sb[i], bg_hue, bg_saturation, bg_value);
	}

	// 每隔约100步（10秒）更新一次闪烁灯珠的位置（减慢一倍）
	if (step_count - last_pos_update >= 100) {
		for (i = 0; i < 3; i++) {
			// 随机位置（0-11）
			twinkle_positions[i] = (u8)((step_count * 7 + i * 17) % 12);
			// 随机颜色（红色0°或蓝色240°）
			twinkle_colors[i] = ((step_count * 13 + i * 23) % 2);
		}
		last_pos_update = step_count;
	}

	// 闪烁灯珠也带呼吸效果，与背景同步但可能稍有相位差
	twinkle_value = 50 + (u8)(25 * (1 + sin((step_count + i * 30) * 0.0019625f)));

	// 将3颗闪烁灯珠设置为红色或蓝色（高亮度带呼吸）
	for (i = 0; i < 3; i++) {
		if (twinkle_colors[i] == 0) {
			twinkle_hue = 0;     // 红色
		} else {
			twinkle_hue = 240;   // 蓝色
		}
		hsv_to_rgb(&sr[twinkle_positions[i]], &sg[twinkle_positions[i]], &sb[twinkle_positions[i]],
		           twinkle_hue, 100, twinkle_value);
	}

	// 使用过渡函数实现极平滑的颜色变化，步数增加到50以获得极慢的过渡效果
	iotalink_rgb_relax_1(50);

	// 增加步数
	step_count++;
}


//模式10：迪斯科
void  wlt_disco_mode(void)
{
	unsigned char i ,j,loop;
	 unsigned char r, g, b;

     hsv_s_t ss = 100;
	 hsv_s_t sv = 100;
	 static char  num[6]={  0,1,5,6,7,8};  // 6个闪烁灯珠位置
	 hsv_s_t sh2[6]={0,    25,   50,  0,   100,125};  // 6个颜色值

	 singleColor(0, 0, 0);//清空
	 rgb_value_sync();

	for (loop = 0; loop<15; loop++)
	{
		for (i =0 ; i <6; i++)  // 循环6次，只闪烁6颗灯珠
		{
			if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit!=10)return ;

			hsv_to_rgb(&r, &g, &b,sh2[i],100, 100);	//转化	sh[i]
			rgb_colorful_buffer_set(num[i], r,	g, b ); //写入当前点rgb
			//下一次随机亮点及颜色
			num[i] = rand()%RGB_LED_NUM;
			sh2[i] = rand()%360;
	  	}
		 rgb_value_sync();//闪一次
		 wlt_ms_delay(30+magic_rate*2);
		 singleColor(0, 0, 0);//清空
	}
}

//模式11 ： 跳变彩虹     		用中间变量  sr sb sg
void  colorful_rainbow_run(void)
{
	int i,j;
	static u16 sh[] = {0,30,60,90,120,150,180,210,240,270,300,330,360};

	for (i=0;i<RGB_LED_NUM;i++)
	{
		
		sh[i]=sh[i]-20;//+5 观感向前（下） 调整大小可更改跳变感
		if (sh[i]>=360)sh[i]=360;
		hsv_to_rgb(&sr[i], &sg[i], &sb[i], sh[i], 100, 100);
		rgb_colorful_buffer_set(i, sr[i],	sg[i], sb[i]); //写入当前点rgb

   }
	rgb_value_sync();//
}

//模式13：圣诞夜
void wlt_Christmas_mode(void)
{
	unsigned char i;
	unsigned char r, g, b;
	static u8 current_mode = 0;  // 0=第一段慢闪, 1=第二段快移
	static u32 mode0_count = 0;  // 第一段计数器
	static u32 step_count = 0;   // 第二段计数器（用于计算移动位置）
	static char num[8] = {0, 1, 3, 5, 6, 7, 9, 10};  // 8个闪烁灯珠位置
	static hsv_s_t sh2[8] = {0, 25, 50, 0, 100, 125, 150, 175};  // 8个颜色值

	// 检查是否是新下发的指令
	if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 13) {
		mode0_count = 0;
		step_count = 0;
		current_mode = 0;
		scene_reset_flag = false;
		return;
	}

	if (current_mode == 0) {
		// 第一段：5秒慢速闪烁，8颗灯珠同时亮
		// 延时500ms，约10次调用达到5秒

		// 清空所有灯珠
		singleColor(0, 0, 0);

		// 同时点亮8颗灯珠
		for (i = 0; i < 8; i++) {
			hsv_to_rgb(&r, &g, &b, sh2[i], 100, 100);
			rgb_colorful_buffer_set(num[i], r, g, b);

			// 缓慢更新位置和颜色
			num[i] = (num[i] + 1) % RGB_LED_NUM;
			sh2[i] = (sh2[i] + 10) % 360;
		}
		rgb_value_sync();
		wlt_ms_delay(500);  // 500ms延时，速度很慢

		mode0_count++;

		// 5秒后切换到第二阶段（约10次 × 500ms = 5秒）
		if (mode0_count >= 10) {
			current_mode = 1;
			mode0_count = 0;
		}

	} else {
		// 第二段：5秒快速向上移动
		// 延时2ms，约500次调用达到5秒

		// 清空所有灯珠
		singleColor(0, 0, 0);

		// 向上移动效果
		for (i = 0; i < 8; i++) {
			// 计算向上移动的位置
			u8 move_pos = (i * 2 + 12 - (step_count % 12)) % RGB_LED_NUM;
			hsv_to_rgb(&r, &g, &b, sh2[i], 100, 100);
			rgb_colorful_buffer_set(move_pos, r, g, b);

			// 更新颜色
			sh2[i] = (sh2[i] + 30) % 360;
		}
		rgb_value_sync();
		wlt_ms_delay(2);  // 2ms延时，速度非常快

		step_count++;

		// 5秒后切换回第一阶段（约90次调用）
		if (step_count >= 90) {
			current_mode = 0;
			step_count = 0;
		}
	}
}


//模式 ： 流星雨  初始化后直接流动即可
void  colorful_meteor_init(void)
{
	 u8 i=0;						
	 hsv_h_t  sh=100;
	 hsv_s_t ss=100;
     hsv_s_t sh1[3] = {0,120,230};// 8, 20,  30,    40 ,  50,  75,  70,80,   100  
	 hsv_v_t sv[] = {																
		0,    0, 0,    0,    0  ,   0 ,     0,  0,    15, 20,  100,   20 ,  100,  20, 100, 20,   100, 
		0,    0, 0,    0,    0  ,   0 ,      0,  0,   15, 20,  100,   20 ,  100,  20, 100, 20,   100, 
		 0,    0, 0,    0,    0  ,   0 ,     0,  0,   15, 20,  100,   20 ,  100,  20, 100, 20,   100
	};//亮度变化顺时针流星 
	 for (i=0;i<RGB_LED_NUM_INCREASE;i++)
	 {
//颜色赋值
		sh+=10;
		hsv_to_rgb(&r, &g, &b, sh, ss ,sv[i]);
		rgb_colorful_buffer_set(i, r,g, b); //写入当前点rgb 
	 }
	 rgb_value_sync();//闪一次	 

}


//模式14：流水
void wlt_runningwater_mode(void)
{
    static unsigned int step = 0;
    unsigned char r, g, b;
    int i;
    
    // 确保灯珠数量为12
    const int total_leds = 12;
    
    // 蓝色渐变亮度数组：从暗到亮再到暗，形成流动的波浪效果
    const unsigned char brightness_gradient[12] = {30, 60, 90, 120, 150, 180, 210, 240, 210, 180, 150, 120};
    
    // 计算当前流水位置偏移
    int offset = step % total_leds;
    
    // 渲染流水效果
    for (i = 0; i < total_leds; i++) {
        // 检查是否需要退出
        if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit!=14)return;
        
        // 计算当前灯珠对应的渐变位置（实现流动效果）
        int gradient_pos = (i + offset) % total_leds;
        
        // 获取当前位置的亮度
        unsigned char brightness = brightness_gradient[gradient_pos];
        
        // 设置蓝色渐变效果
        r = 0;              // 红色分量为0
        g = 0;              // 绿色分量为0
        b = brightness;     // 蓝色分量为渐变亮度
        
        rgb_colorful_buffer_set(i, r, g, b);
    }
    
    // 输出数据
    rgb_value_sync();
    
    // 控制流水速度
    wlt_ms_delay(100 + (magic_rate / 10));
    
    // 更新步骤
    step++;
}

//模式20：炫彩 - 彩虹从左到右平滑流动
void wlt_Colorful_mode(void)
{
	u8 i;
	static u32 step_count = 0;
	// 彩虹色相偏移量，实现从左到右平滑移动
	static u16 hue_offset = 0;
	// 亮度呼吸相位（0-99），用查表避免浮点跳变
	static u8 breath_phase = 0;
	// 预计算呼吸亮度表：breath_table[t] = 70 + 30*sin(2π*t/100)，t∈[0,99]
	// 修正：首尾值相同(70)，避免跳变闪烁
	static const u8 breath_table[100] = {
		70,72,75,78,81,83,85,87,90,92,
		94,96,98,99,100,100,100,100,99,98,
		97,95,93,91,89,87,85,83,81,79,
		76,74,71,69,66,63,60,58,55,52,
		50,48,45,43,41,40,40,40,40,41,
		42,44,46,48,50,52,55,58,60,63,
		66,69,71,74,76,79,81,83,85,87,
		89,91,93,95,97,98,99,100,100,100,
		100,99,98,97,95,93,91,89,87,85,
		83,81,79,76,74,71,69,66,63,60
	};

	// 检查新指令
	if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 20) {
		step_count = 0;
		hue_offset = 0;
		breath_phase = 0;
		scene_reset_flag = false;
		return;
	}

	// 色相偏移量递增（加快1.5倍速度），实现从右到左平滑流动
	hue_offset = (hue_offset + 3) % 360;

	// 亮度呼吸：查表法，完全平滑无跳变
	u8 brightness = breath_table[breath_phase];
	breath_phase = (breath_phase + 1) % 100;

	// 计算每颗灯珠的色相：基于位置 + 偏移量，相邻灯珠之间色相差60°
	for (i = 0; i < 12; i++) {
		// 色相 = 基础位置(i*60) - 偏移量，形成从右到左流动的彩虹
		u16 hue = (i * 60 + 360 - hue_offset) % 360;

		// 直接设置灯珠
		u8 r, g, b;
		hsv_to_rgb(&r, &g, &b, hue, 100, brightness);
		rgb_colorful_buffer_set(i, r, g, b);
	}

	rgb_value_sync();

	// 延时约100ms（不调用 relax 系列）
	wlt_ms_delay(100);

	step_count++;
}



//模式21：柔和
void wlt_gentle_mode(void)
{
	u8 i;
	static u16 hue = 0;  // 当前色相，每步递减1°
	static u16 step = 0;

	// 检查新指令
	if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 21) {
		hue = 0;   // 从0°(红)开始
		step = 0;
		scene_reset_flag = false;
		return;
	}

	// 每步递减1°，0°时直接绕回359°，色相环无缝衔接
	if (hue == 0) {
		hue = 359;
	} else {
		hue--;
	}

	// 12颗灯珠颜色完全相同，亮度100%
	u8 r, g, b;
	hsv_to_rgb(&r, &g, &b, hue, 100, 100);
	for (i = 0; i < 12; i++) {
		rgb_colorful_buffer_set(i, r, g, b);
	}

	rgb_value_sync();

	// 延时约100ms
	wlt_ms_delay(100);

	step++;
}


// 场景22：结婚纪念日 （紫色为底，3个红色跳变）
void wlt_wedding_mode(void)
{
	u8 i;
	static u16 step = 0;
	static u8 red_positions[3] = {0, 4, 8};  // 3颗红色灯珠位置

	// 检查新指令
	if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 22) {
		step = 0;
		red_positions[0] = 0;
		red_positions[1] = 4;
		red_positions[2] = 8;
		scene_reset_flag = false;
		return;
	}

	// 设置固定紫色背景
	u8 r, g, b;
	hsv_to_rgb(&r, &g, &b, 275, 100, 100);  // 紫色(275°)
	for (i = 0; i < 12; i++) {
		rgb_colorful_buffer_set(i, r, g, b);
	}

	// 每隔约10步更新3颗红色灯珠的随机位置
	if (step % 10 == 0) {
		for (i = 0; i < 3; i++) {
			red_positions[i] = rand() % 12;
		}
	}

	// 设置3颗红色灯珠
	u8 r_red, g_red, b_red;
	hsv_to_rgb(&r_red, &g_red, &b_red, 0, 100, 100);  // 红色(0°)
	for (i = 0; i < 3; i++) {
		rgb_colorful_buffer_set(red_positions[i], r_red, g_red, b_red);
	}

	rgb_value_sync();

	// 延时约100ms
	wlt_ms_delay(10);

	step++;
}


//模式23 雪花
void wlt_snowflakes_mode(void)
{
	u8 i, j;
	static u16 step = 0;
	// 12个雪花结构：位置、大小、亮度、保持时间
	static struct {
		u8 pos;       // 起始位置
		u8 size;      // 大小（1-4随机）
		u8 bright;    // 当前亮度(0-100)
		u8 hold;      // 保持时间计数器(0-10)
		u8 active;    // 是否已激活出现
	} flakes[12];

	// 检查新指令
	if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 23) {
		step = 0;
		// 初始化12个雪花
		for (i = 0; i < 12; i++) {
			flakes[i].pos = rand() % 12;
			flakes[i].size = 1 + (rand() % 4);  // 大小1-4随机
			flakes[i].bright = 0;
			flakes[i].hold = 0;
			flakes[i].active = 0;
		}
		scene_reset_flag = false;
		return;
	}

	// 设置黑色背景
	for (i = 0; i < 12; i++) {
		rgb_colorful_buffer_set(i, 0, 0, 0);
	}

	// 逐个激活雪花：每10步激活一个（1秒间隔）
	u8 activate_idx = step / 10;
	if (activate_idx > 11) activate_idx = 11;
	for (i = 0; i < 12; i++) {
		if (!flakes[i].active && i == activate_idx) {
			flakes[i].active = 1;
			flakes[i].bright = 100;
			flakes[i].hold = 10;  // 保持1秒
		}
	}

	// 更新所有雪花
	for (i = 0; i < 12; i++) {
		if (!flakes[i].active) continue;

		if (flakes[i].hold > 0) {
			// 保持阶段：亮度100%，倒计时
			flakes[i].hold--;
		} else if (flakes[i].bright > 0) {
			// 淡出阶段：亮度每步减2（缩短为约5秒消失）
			flakes[i].bright -= 2;
			if (flakes[i].bright < 2) flakes[i].bright = 0;
			// 淡出到0时立即重生
			if (flakes[i].bright == 0) {
				flakes[i].pos = rand() % 12;  // 位置随机，可能重叠
				flakes[i].size = 1 + (rand() % 4);  // 大小1-4随机
				flakes[i].bright = 100;
				flakes[i].hold = 10;  // 保持1秒
			}
		}
	}

	// 绘制所有雪花（白色）
	for (i = 0; i < 12; i++) {
		if (flakes[i].active && flakes[i].bright > 0) {
			for (j = 0; j < flakes[i].size; j++) {
				u8 led_pos = (flakes[i].pos + j) % 12;
				// 亮度转换为0-255
				u8 brightness = (flakes[i].bright * 255) / 100;
				rgb_colorful_buffer_set(led_pos, brightness, brightness, brightness);
			}
		}
	}

	rgb_value_sync();

	// 延时约100ms
	wlt_ms_delay(100);

	step++;
}



//模式24：火焰
void wlt_fire_mode (void )
{
	static unsigned int step = 0;
	unsigned char r, g, b;
	int i;
		
	// 确保灯珠数量为12
	const int total_leds = 12;
	
	// 随机数生成相关变量
	static unsigned int seed = 0;
	unsigned int random_val;
	
	// 更新随机种子
	seed = seed * 1103515245 + 12345;
	random_val = (seed / 65536) % 32768;
	
	// 火焰高度：模拟火焰向上燃烧的动态效果
	static int fire_height = 6;
	
	// 随机调整火焰高度，模拟火焰跳动
	int height_change = (random_val % 3) - 1;
	fire_height += height_change;
	if (fire_height < 4) fire_height = 4;
	if (fire_height > total_leds) fire_height = total_leds;
	
	// 计算每个灯珠的颜色
	for (i = 0; i < total_leds; i++) {
		// 计算当前灯珠在火焰中的位置比例 (0.0 底部 -> 1.0 顶部)
		float flame_ratio = (float)i / (total_leds - 1);
		
		// 为每个灯珠生成独立的随机种子
		unsigned int led_seed = seed + i;
		led_seed = led_seed * 1103515245 + 12345;
		unsigned int led_rand = (led_seed / 65536) % 32768;
		
		// 根据位置设置基础颜色
		if (i < total_leds - fire_height) {
			// 火焰以下区域：深红色，几乎不发光
			r = 50 + (led_rand % 20);
			g = 0;
			b = 0;
		} else {
			// 火焰区域：从深红色到黄色渐变
			float fire_pos_ratio = (float)(i - (total_leds - fire_height)) / (fire_height - 1);
			
			// 红色分量：底部深红色(128) → 顶部亮红色(255)
			unsigned char base_r = (unsigned char)(128 + (127 * fire_pos_ratio));
			// 绿色分量：底部几乎没有(0) → 顶部黄色(180)，确保不会产生单独的绿色
			unsigned char base_g = (unsigned char)(0 + (180 * fire_pos_ratio));
			// 蓝色分量：始终为0，确保火焰颜色纯净
			unsigned char base_b = 0;
			
			// 随机波动，模拟火焰的剧烈摇曳
			int flicker_strength = 50;
			int r_flicker = (led_rand % (flicker_strength * 2 + 1)) - flicker_strength;
			// 绿色波动幅度更小，且不超过红色分量的80%，确保只产生黄色而不是绿色
			int g_flicker = (led_rand % (flicker_strength + 1)) - (flicker_strength / 2);
			
			// 应用波动
			r = base_r + r_flicker;
			g = base_g + g_flicker;
			b = base_b;
			
			// 确保绿色分量不会超过红色分量的80%，避免产生绿色调
			if (g > (r * 0.8)) {
				g = (unsigned char)(r * 0.8);
			}
			
			// 边界检查
			r = (r > 255) ? 255 : (r < 0 ? 0 : r);
			g = (g > 255) ? 255 : (g < 0 ? 0 : g);
			b = (b > 255) ? 255 : (b < 0 ? 0 : b);
		}
		
		// 检查是否需要退出
		if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit!=24)return;
		
		rgb_colorful_buffer_set(i, r, g, b);
	}
	
	// 输出数据
	rgb_value_sync();
	
	// 延迟控制动画速度，加入随机波动使火焰闪烁更自然
	unsigned int flicker_delay = 20 + (magic_rate / 3) + (random_val % 15);
	wlt_ms_delay(flicker_delay);
	
	// 更新步骤
	step++;
}


//模式25：闪电
void wlt_lightning_mode(void)
{
	u8 i, j;
	static u16 step = 0;
	static u16 next_lightning = 0;  // 下一次闪电的时间

	// 检查新指令
	if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 25) {
		step = 0;
		next_lightning = 0;
		scene_reset_flag = false;
		return;
	}

	// 黑色背景
	for (i = 0; i < 12; i++) {
		rgb_colorful_buffer_set(i, 0, 0, 0);
	}

	// 判断是否触发闪电
	if (step >= next_lightning) {
		// 闪电参数：随机位置，大小尽量大
		u8 pos = rand() % 12;
		u8 size = 6 + (rand() % 7);  // 大小6-12（几乎全亮）
		u8 brightness = 240 + (rand() % 16);  // 亮度240-255

		// 绘制闪电（多次极快闪烁效果）
		for (j = 0; j < 8; j++) {
			// 亮
			for (i = 0; i < size; i++) {
				u8 led_pos = (pos + i) % 12;
				rgb_colorful_buffer_set(led_pos, brightness, brightness, brightness);
			}
			rgb_value_sync();
			wlt_ms_delay(5 + rand() % 10);  // 亮5-15ms

			// 检查退出
			if (LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 25) return;

			// 灭
			for (i = 0; i < 12; i++) {
				rgb_colorful_buffer_set(i, 0, 0, 0);
			}
			rgb_value_sync();
			wlt_ms_delay(2 + rand() % 5);  // 灭2-7ms（极短）
		}

		// 设置下一次闪电的随机间隔（0.1-0.5秒，极短空白时间）
		next_lightning = step + 1 + (rand() % 4);
	}

	rgb_value_sync();
	wlt_ms_delay(100);
	step++;
}


//模式26：情人节
void wlt_valentine_mode(void)
{
	u8 i;
	static u16 step_count = 0;
	// 呼吸亮度表：5%-100%平滑过渡，对称呼吸，约5秒一个周期
	static const u8 breath_table[50] = {
		5,9,14,20,26,33,40,48,56,64,
		72,79,86,92,97,100,97,92,86,79,
		72,64,56,48,40,33,26,20,14,9,
		5,9,14,20,26,33,40,48,56,64,
		72,79,86,92,97,100,97,92,86,79,
		72,64,56,48,40,33,26,20,14,5
	};

	// 检查新指令
	if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 26) {
		step_count = 0;
		scene_reset_flag = false;
		return;
	}

	// 计算呼吸亮度（约5秒一个完整呼吸周期）
	u8 brightness = breath_table[step_count % 50];

	// 计算当前偏移（反转方向：向相反方向移动）
	u8 offset = (12 - (step_count % 12)) % 12;

	// 设置12颗灯珠：6颗紫色(275°) + 6颗黄色(60°)，首尾相连循环移动
	for (i = 0; i < 12; i++) {
		u8 r, g, b;
		// 根据偏移计算当前位置属于哪个色块
		u8 pos = (i + offset) % 12;
		if (pos < 6) {
			// 紫色
			hsv_to_rgb(&r, &g, &b, 275, 100, brightness);
		} else {
			// 黄色
			hsv_to_rgb(&r, &g, &b, 60, 100, brightness);
		}
		rgb_colorful_buffer_set(i, r, g, b);
	}

	rgb_value_sync();

	// 延时约100ms
	wlt_ms_delay(100);

	step_count++;
}

//模式27：万圣节
void wlt_hallowmas_mode(void)
{
	u8 i;
	static u16 step = 0;
	static u8 stage = 0;  // 0=第一阶段, 1=第二阶段, 2=第三阶段

	// 检查新指令
	if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || sg_light_ctrl_data.magicunit != 27) {
		step = 0;
		stage = 0;
		scene_reset_flag = false;
		return;
	}

	if (stage == 0) {
		// 第一阶段：紫色从中间展开，7步
		// step=0: 全黑
		// step=1: 位置5,6为紫色(2颗)
		// step=2: 位置4,5,6,7为紫色(4颗)
		// step=3: 位置3,4,5,6,7,8为紫色(6颗)
		// step=4: 位置2,3,4,5,6,7,8,9为紫色(8颗)
		// step=5: 位置1,2,3,4,5,6,7,8,9,10为紫色(10颗)
		// step=6: 全紫色(12颗)

		for (i = 0; i < 12; i++) {
			u8 r, g, b;
			if (step == 0) {
				// 全黑
				hsv_to_rgb(&r, &g, &b, 0, 0, 0);
			} else if (i >= (6 - step) && i <= (5 + step)) {
				// 从中间展开
				hsv_to_rgb(&r, &g, &b, 275, 100, 100);  // 紫色
			} else {
				hsv_to_rgb(&r, &g, &b, 0, 0, 0);  // 黑色
			}
			rgb_colorful_buffer_set(i, r, g, b);
		}

		if (step >= 6) {
			stage = 1;
			step = 0;
		}

	} else if (stage == 1) {
		// 第二阶段：黄色从两端收合，7步
		// step=0: 保持紫色（稳定1步）
		// step=1: 位置0,11为黄色（左右各1颗）
		// step=2: 位置0,1,10,11为黄色（左右各2颗）
		// step=3: 位置0,1,2,9,10,11为黄色（左右各3颗）
		// step=4: 位置0,1,2,3,8,9,10,11为黄色（左右各4颗）
		// step=5: 位置0,1,2,3,4,7,8,9,10,11为黄色（左右各5颗）
		// step=6: 全黄色

		for (i = 0; i < 12; i++) {
			u8 r, g, b;
			if (step == 0) {
				// 保持紫色
				hsv_to_rgb(&r, &g, &b, 275, 100, 100);  // 紫色
			} else if (i < step || i >= (12 - step)) {
				// 从两端收合（i<step处理左边，i>=12-step处理右边）
				hsv_to_rgb(&r, &g, &b, 60, 100, 100);  // 黄色
			} else {
				hsv_to_rgb(&r, &g, &b, 275, 100, 100);  // 紫色
			}
			rgb_colorful_buffer_set(i, r, g, b);
		}

		if (step >= 6) {
			stage = 2;
			step = 0;
		}

	} else if (stage == 2) {
		// 第三阶段：黑色从两端收合，7步
		// step=0: 保持黄色（稳定1步）
		// step=1: 位置0,11为黑色（左右各1颗）
		// step=2: 位置0,1,10,11为黑色（左右各2颗）
		// step=3: 位置0,1,2,9,10,11为黑色（左右各3颗）
		// step=4: 位置0,1,2,3,8,9,10,11为黑色（左右各4颗）
		// step=5: 位置0,1,2,3,4,7,8,9,10,11为黑色（左右各5颗）
		// step=6: 全黑色

		for (i = 0; i < 12; i++) {
			u8 r, g, b;
			if (step == 0) {
				// 保持黄色
				hsv_to_rgb(&r, &g, &b, 60, 100, 100);  // 黄色
			} else if (i < step || i >= (12 - step)) {
				// 从两端收合（i<step处理左边，i>=12-step处理右边）
				hsv_to_rgb(&r, &g, &b, 0, 0, 0);  // 黑色
			} else {
				hsv_to_rgb(&r, &g, &b, 60, 100, 100);  // 黄色
			}
			rgb_colorful_buffer_set(i, r, g, b);
		}

		if (step >= 6) {
			stage = 0;
			step = 0;
		}
	}

	rgb_value_sync();
	wlt_ms_delay(200);  // 每步200ms，每阶段约1.2秒
	step++;
}


void wlt_110_mode()
{
	int i,cnt ,direction;
	
	cnt = 20+rand()%10;
	direction = rand()%2;

	for(i = 0; i < cnt ; i++)//temp: 10
	{	

			if(direction)
			{
				Move_Pre();
		
			}
			else Move_Back();
			
			rgb_value_sync();
			wlt_ms_delay(magic_rate*0.5+10);

		 
	}

}







void iotalink_write_hsv_buffer(void )
{
	u8 r,g,b;
	u8 i=0;
	if(sg_light_ctrl_data.switch_status==0)
	{
		rgb_colorful_buffer_clean(); 
		rgb_value_sync(); //输出数据
		
	}
	else  if(LOCAL_MAGIC_MODE)
	{
		for (i = 0; i<RGB_LED_NUM ; i++)
		{		   
		   hsv_to_rgb( &r , &g , &b , sh[i],ss[i], sv[i]);
		   rgb_colorful_buffer_set(i ,r , g , b); //写入当前点rgb	 
		}

		rgb_value_sync(); //输出数据
	}  
}


#if 0
//颜色擦除
void  wlt_29_mode( void   )
{
	  unsigned int i = 0;
	  unsigned int j = 0;
	
	 for(j= 0 ; j< MAGIC_SCENE_DATA.magicallcnt ; j++)
	  {
	  			//第一个白色
  	 	  index_hsv_Move_Back(0,RGB_LED_NUM,MAGIC_SCENE_DATA.magic_hsv[0].sv,0,100);
		  iotalink_write_hsv_buffer();
		  
		  for(i = 1 ; i<RGB_LED_NUM ; i++)
		  {					 
			 if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit!=29)return;
			  
			  index_hsv_Move_Back(0,RGB_LED_NUM,MAGIC_SCENE_DATA.magic_hsv[j].sh, 100 ,100);
			  iotalink_write_hsv_buffer();
			  			 		 	 
			  wlt_ms_delay(5+magic_rate*0.8);   //35
		  }	
		  wlt_ms_delay(100);   //35
	 }
}
void  wlt_30_mode( void   )
{
	 unsigned int i = 0;

	rgbsingleColor(2,0,128);
	for (i = 1; i<RGB_LED_NUM ; i++)
	{
		if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit !=30)return;
		rgb_colorful_buffer_set(i-1 ,2,0,128); //写入当前点rgb	 
		rgb_colorful_buffer_set(i ,128 ,128  , 128 ); //写入当前点rgb	 
		wlt_ms_delay(5+magic_rate*0.8);   //35

		rgb_value_sync(); //输出数据
	}


	  

}

void wlt_33_mode()
{
	int i;
	static j=0;
	
	static u16 sh[] = {0,30,60,90,120,150,180,210,240,270,300,330,360};
	int sv;
	for (i=0;i<RGB_LED_NUM*2;i++)
	{

		if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit !=33)return;
		sv = 100-10*i;
		if(sv<=0)sv=0;

		hsv_to_rgb(&sr[i], &sg[i], &sb[i], sh[j], 100, sv);
		index_colorful_buffer_Move_Back(0,RGB_LED_NUM,sr[i],	sg[i], sb[i]);
		
		wlt_ms_delay(magic_rate*0.5+10);
  		rgb_value_sync();// 

   }

	j++;
	if(j==12) {j=0;}
	rgb_value_sync();// 

}

#endif


//时光机1 - 红黄灯珠带着白条移动，直到全部变白
void wlt_timemachine1_mode(void)
{
    int i;
    static u8 phase = 0;              // 当前阶段：0=红，1=黄
    static u8 head_pos = 0;           // 彩色灯珠的位置(0-11)
    static u8 white_count = 0;       // 白色灯珠的数量
    static u16 step_count = 0;       // 步数计数器
    static bool completed = false;   // 是否已完成全白状态
    
    // 检查新指令，初始化
    if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || (sg_light_ctrl_data.magicunit != 29 && sg_light_ctrl_data.magicunit != 150)) {
        phase = 0;      // 从红色开始
        head_pos = 0;  // 从位置0开始
        white_count = 0;
        step_count = 0;
        completed = false;
        scene_reset_flag = false;
        return;
    }
    
    // 已完成全白状态，维持白色
    if (completed) {
        for (i = 0; i < 12; i++) {
            rgb_colorful_buffer_set(i, 255, 255, 255);
        }
        rgb_value_sync();
        wlt_ms_delay(10);
        return;
    }
    
    // 颜色切换：每5步切换一次(红->黄->红)，每50ms一步
    if (step_count > 0 && step_count % 5 == 0) {
        phase = !phase;
    }
    
    // 位置移动：每10步(显示完红+黄后)前进一格
    if (step_count > 0 && step_count % 10 == 0) {
        if (head_pos < 12) {
            // 白色条扩展到当前位置
            white_count = head_pos + 1;
            
            // 彩色灯珠前进到下一个位置
            head_pos++;
            
            // 限制白色数量不超过12
            if (white_count > 12) {
                white_count = 12;
            }
        }
        
        // 当12颗灯珠全部为白色时，标记完成
        if (white_count >= 12) {
            white_count = 12;
            completed = true;
        }
    }
    
    // 设置每个灯珠的颜色
    for (i = 0; i < 12; i++) {
        u8 r, g, b;
        
        if (i < white_count) {
            // 在白色范围内的灯珠设为白色
            r = 255;
            g = 255;
            b = 255;
        } else if (i == head_pos && head_pos < 12) {
            // 彩色灯珠位置：红色或黄色
            if (phase == 0) {
                r = 255; g = 0; b = 0;   // 红色
            } else {
                r = 255; g = 255; b = 0;  // 黄色
            }
        } else {
            // 其余灯珠为黑色
            r = 0; g = 0; b = 0;
        }
        
        rgb_colorful_buffer_set(i, r, g, b);
    }
    
    rgb_value_sync();
    
    // 延时约25ms（速度加快1倍）
    wlt_ms_delay(10);
    
    step_count++;
}



//时光机2 - 白条从左到右移动，直到全部变白
void wlt_timemachine2_mode(void)
{
    int i;
    static u8 white_count = 0;       // 白色灯珠的数量
    static u16 step_count = 0;       // 步数计数器
    static bool completed = false;   // 是否已完成全白状态
    
    // 检查新指令，初始化
    if (scene_reset_flag || LOCAL_MAGIC_MODE == 0 || (sg_light_ctrl_data.magicunit != 30 && sg_light_ctrl_data.magicunit != 152)) {
        white_count = 0;
        step_count = 0;
        completed = false;
        scene_reset_flag = false;
        return;
    }
    
    // 已完成全白状态，维持白色
    if (completed) {
        for (i = 0; i < 12; i++) {
            rgb_colorful_buffer_set(i, 255, 255, 255);
        }
        rgb_value_sync();
        wlt_ms_delay(1);
        return;
    }
    
    // 白条扩展：每5步向前移动一格（比时光机1快一倍）
    if (step_count > 0 && step_count % 5 == 0) {
        if (white_count < 12) {
            white_count++;
        }
        
        // 当12颗灯珠全部为白色时，标记完成
        if (white_count >= 12) {
            white_count = 12;
            completed = true;
        }
    }
    
    // 设置每个灯珠的颜色
    for (i = 0; i < 12; i++) {
        u8 r, g, b;
        
        if (i < white_count) {
            // 在白色范围内的灯珠设为白色
            r = 255;
            g = 255;
            b = 255;
        } else {
            // 其余灯珠为黑色
            r = 0;
            g = 0;
            b = 0;
        }
        
        rgb_colorful_buffer_set(i, r, g, b);
    }
    
    rgb_value_sync();
    
    // 延时约10ms（比时光机1快一倍）
    wlt_ms_delay(1);
    
    step_count++;
}



//流星1 - 模拟单个流星从左到右划过LED灯带的效果
void wlt_meteor1_mode(void)
{
    int i;
    static u8 meteor_pos = 0;         // 流星当前位置
    static u8 tail_length = 8;        // 流星尾巴长度
    static u8 meteor_brightness = 100;// 流星头部亮度
    static u8 speed = 3;              // 流星移动速度
    static u8 speed_counter = 0;      // 速度计数器，控制移动频率

    // 增加速度计数器
    speed_counter++;
    if (speed_counter < speed) {
        // 还未达到移动条件，保持当前状态
        // 直接同步输出当前颜色
        rgb_value_sync();
        wlt_ms_delay(10);
        return;
    }
    speed_counter = 0;

    // 清除所有灯珠颜色
    for (i = 0; i < RGB_LED_NUM; i++) {
        rgb_colorful_buffer_set(i, 0, 0, 0);
    }

    // 绘制流星（白色）
    for (i = 0; i < tail_length; i++) {
        if (meteor_pos - i >= 0 && meteor_pos - i < RGB_LED_NUM) {
            u8 r, g, b;
            // 计算当前位置的亮度，从头部到尾部逐渐变暗
            u8 brightness = meteor_brightness - (i * meteor_brightness / tail_length);
            if (brightness < 5) brightness = 5; // 确保尾部有最低亮度

            // 将HSV转换为RGB（白色：色相任意，饱和度0，亮度brightness）
            hsv_to_rgb(&r, &g, &b, 0, 0, brightness);

            // 设置流星当前位置的颜色
            rgb_colorful_buffer_set(meteor_pos - i, r, g, b);
        }
    }

    // 更新流星位置
    meteor_pos++;
    if (meteor_pos >= RGB_LED_NUM + tail_length) {
        // 流星已经划过整个灯带，重新开始
        meteor_pos = 0;
        // 随机调整尾巴长度（6-12个灯珠）
        tail_length = 6 + (rand() % 7);
    }

    // 将颜色数据同步输出到LED灯带
    rgb_value_sync();

    // 添加适当的延迟，控制动画速度
    wlt_ms_delay(10);
}



//流星2 - 模拟单个流星从右到左划过LED灯带的效果
void wlt_meteor2_mode(void)
{
    int i;
    static signed char meteor_pos = 11; // 流星当前位置（从右往左，初始在位置11）
    static u8 tail_length = 8;       // 流星尾巴长度
    static u8 meteor_brightness = 100;// 流星头部亮度
    static u8 speed = 3;             // 流星移动速度
    static u8 speed_counter = 0;    // 速度计数器，控制移动频率
    static u8 initialized = 0;      // 初始化标志

    // 首次调用时初始化位置
    if (!initialized) {
        meteor_pos = RGB_LED_NUM - 1;  // 从最右边开始
        tail_length = 6 + (rand() % 7);
        initialized = 1;
    }

    // 增加速度计数器
    speed_counter++;
    if (speed_counter < speed) {
        // 还未达到移动条件，保持当前状态
        rgb_value_sync();
        wlt_ms_delay(10);
        return;
    }
    speed_counter = 0;

    // 清除所有灯珠颜色
    for (i = 0; i < RGB_LED_NUM; i++) {
        rgb_colorful_buffer_set(i, 0, 0, 0);
    }

    // 绘制流星（白色，从右到左）
    for (i = 0; i < tail_length; i++) {
        if (meteor_pos + i >= 0 && meteor_pos + i < RGB_LED_NUM) {
            u8 r, g, b;
            // 计算当前位置的亮度，从头部到尾部逐渐变暗
            u8 brightness = meteor_brightness - (i * meteor_brightness / tail_length);
            if (brightness < 5) brightness = 5; // 确保尾部有最低亮度

            // 将HSV转换为RGB（白色：饱和度0，亮度brightness）
            hsv_to_rgb(&r, &g, &b, 0, 0, brightness);

            // 设置流星当前位置的颜色（meteor_pos + i 向左移动）
            rgb_colorful_buffer_set(meteor_pos + i, r, g, b);
        }
    }

    // 更新流星位置（从右往左移动）
    if (meteor_pos <= -tail_length) {
        // 流星完全离开灯带，重新从右边出现
        meteor_pos = RGB_LED_NUM - 1;
        tail_length = 6 + (rand() % 7);
        speed_counter = 0;  // 重置计数器，立即开始移动
    }
    meteor_pos--;

    // 将颜色数据同步输出到LED灯带
    rgb_value_sync();

    // 添加适当的延迟，控制动画速度
    wlt_ms_delay(10);
}


//烟花秀 - 模拟流星划过LED灯带的效果（7彩色+白色）
void wlt_fireworkshow_mode(void)
{
    int i;
    static u8 meteor_pos = 0;         // 流星当前位置
    static u8 tail_length = 8;        // 流星尾巴长度
    static u8 meteor_brightness = 100;// 流星头部亮度
    static u8 speed = 3;              // 流星移动速度
    static u8 speed_counter = 0;      // 速度计数器，控制移动频率
    static u8 current_hue = 0;         // 当前流星颜色（色相）
    static bool initialized = false;   // 初始化标志

    // 首次调用时初始化随机颜色
    if (!initialized) {
        // 8种颜色：7彩虹色(H=0,30,60,120,180,240,270) + 白色(S=0)
        // 随机选择一种颜色
        u8 color_idx = rand() % 8;
        if (color_idx < 7) {
            // 7彩虹色：红、橙、黄、绿、青、蓝、紫
            current_hue = color_idx * 40;  // 0, 40, 80, 120, 160, 200, 240
        } else {
            // 白色：使用特殊标记值255，在绘制时单独处理
            current_hue = 255;
        }
        initialized = true;
    }

    // 增加速度计数器
    speed_counter++;
    if (speed_counter < speed) {
        // 还未达到移动条件，保持当前状态
        // 直接同步输出当前颜色
        rgb_value_sync();
        wlt_ms_delay(10);
        return;
    }
    speed_counter = 0;

    // 清除所有灯珠颜色
    for (i = 0; i < RGB_LED_NUM; i++) {
        rgb_colorful_buffer_set(i, 0, 0, 0);
    }

    // 绘制流星（7彩色+白色）
    for (i = 0; i < tail_length; i++) {
        if (meteor_pos - i >= 0 && meteor_pos - i < RGB_LED_NUM) {
            u8 r, g, b;
            // 计算当前位置的亮度，从头部到尾部逐渐变暗
            u8 brightness = meteor_brightness - (i * meteor_brightness / tail_length);
            if (brightness < 5) brightness = 5; // 确保尾部有最低亮度

            // 将HSV转换为RGB
            if (current_hue == 255) {
                // 白色：色相任意，饱和度0
                hsv_to_rgb(&r, &g, &b, 0, 0, brightness);
            } else {
                // 7彩色：饱和度100
                hsv_to_rgb(&r, &g, &b, current_hue, 100, brightness);
            }

            // 设置流星当前位置的颜色
            rgb_colorful_buffer_set(meteor_pos - i, r, g, b);
        }
    }

    // 更新流星位置
    meteor_pos++;
    if (meteor_pos >= RGB_LED_NUM + tail_length) {
        // 流星已经划过整个灯带，重新开始
        meteor_pos = 0;
        // 随机调整尾巴长度（6-12个灯珠）
        tail_length = 6 + (rand() % 7);
        // 随机选择新颜色
        u8 color_idx = rand() % 8;
        if (color_idx < 7) {
            current_hue = color_idx * 40;
        } else {
            current_hue = 255;
        }
    }

    // 将颜色数据同步输出到LED灯带
    rgb_value_sync();

    // 添加适当的延迟，控制动画速度
    wlt_ms_delay(10);
}


//------------------------------------------------------------------------------------------------//


// 幻彩流动效果（支持正反向切换）
void haoyida_magic_flow_effect( int mode)
{
    static u16 start_hue = 0;  // 起始色相（彩虹色偏移）
    u16 led_idx = 0;
    u8 r, g, b;
	static bool flow_direction = 0; // 0=正向流动（默认），1=反向流动	
	flow_direction = mode%2;

    // 1. 清空颜色缓存
    rgb_colorful_buffer_clean();

    // 2. 生成流动光带（彩虹色+亮度渐变）
    for (led_idx = 0; led_idx < RGB_LED_NUM ; led_idx++)
    {
        // 色相渐变（每灯珠偏移10°，反向时色相递减）
        u16 current_hue = flow_direction ? 
                          (start_hue - led_idx * 5 + 360) % 360 :  // 反向：色相递减（+360避免负数）
                          (start_hue + led_idx * 5) % 360;         // 正向：色相递增
        
        // 光带亮度渐变（正反通用）
        u8 current_bright =  100*(1 - (led_idx / (float)RGB_LED_NUM) * 0.7); 
        
        // HSV转RGB（复用原代码工具函数）
        hsv_to_rgb(&r, &g, &b, current_hue, 100, current_bright);
        
        // 计算当前灯珠位置（循环流动，反向时位置递减）
        u16 pos = flow_direction ? 
                  (RGB_LED_NUM + (start_hue / 5 - led_idx)) % RGB_LED_NUM :  // 反向：位置递减
                  (start_hue / 5 + led_idx) % RGB_LED_NUM;                   // 正向：位置递增
        
        // 写入颜色缓存
        rgb_colorful_buffer_set(pos, r, g, b);
    }

    // 3. 刷新灯带显示
    rgb_value_sync();

    // 4. 更新起始色相（控制流动方向和速度）
    if (flow_direction)
    {
        // 反向：色相递减（+360避免负数）
        start_hue = (start_hue - magic_rate / 5 + 360) % 360;
    }
    else
    {
        // 正向：色相递增
        start_hue = (start_hue + magic_rate / 5) % 360;
    }

    // 5. 速度延时（复用原代码延时函数）
    wlt_ms_delay(magic_rate);
}

// 明暗过渡
void haoyida_shade_light_transition(bool dir)
{

	static u8 sv[] = {0,3,4,5,6, 
					  10,12,15,20, 30,
					  30, 32,38,40,45,
					  50, 45,40,38,32,
					  30, 20,15,12,10,
					  6,5 ,4,3,0};

//	static u8 sv[] = {0,8,20,50,20,8,0,8,20,50,20,8,0};//亮度直接流动
	static int i ,j,  K ;//颜色

//流动下一个再变
    if(dir)	
    {

		
		index_hsv_Move_Back(0,RGB_LED_NUM, MAGIC_SCENE_DATA.magic_hsv[j].sh,0,0);
		for(int w=0;w<5;w++)//变5次再动
		{
			Array_Color_Move_Back(0,sizeof(sv),sv);
			for (i=0;i<RGB_LED_NUM;i++)
			{
				hsv_to_rgb(&r, &g, &b, sh[i] , 100, sv[i]/2);
				rgb_colorful_buffer_set(i ,r ,g  , b ); //写入当前点rgb	 
			}
			rgb_value_sync();// 

			wlt_ms_delay(magic_rate*0.5+20);
		}
	}
    else 	
    {
		
		
		index_hsv_Move_Pre(0,RGB_LED_NUM, MAGIC_SCENE_DATA.magic_hsv[j].sh,0,0);

		for(int w=0;w<5;w++)//
		{

			Array_Color_Move_Pre(0,sizeof(sv),sv);
			for (i=0;i<RGB_LED_NUM;i++)
			{
				hsv_to_rgb(&r, &g, &b, sh[i] , 100, sv[i]/2);
				rgb_colorful_buffer_set(i ,r ,g  , b ); //写入当前点rgb	 
			}
			rgb_value_sync();// 

			wlt_ms_delay(magic_rate*0.5+20);
		}
	}

	K++;
	if(K==RGB_LED_NUM/2)//一半颜色块
	{
		K=0;
		j++;	
		if(j>=MAGIC_SCENE_DATA.magicallcnt) j=0;
	}

}
// 明暗过渡x色 9-22
void haoyida_shade_light_transition_xse( int mode)
{
	hsv_h_t sh[]={0,120,240,240,30,300,377};
	if(mode<21)
	hsv_to_rgb(&r, &g, &b, sh[(mode-9)/2] , 100, 100);
	else //白色
	hsv_to_rgb(&r, &g, &b, 0 , 0, 100);	

	for (int i= 0; i<MAGIC_SCENE_DATA.magicallcnt; i++)//颜色组
	{
		if (sg_light_ctrl_data.custome_unit!=mode)
		{
			return ;
	    }
		if(mode%2)
		{
			
			index_rgb_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);
			iotalink_rgb_relax_2(20);
			index_rgb_Move_Back(0, RGB_LED_NUM, r, g, b);
			iotalink_rgb_relax_2(20);

		}
		else 
		{

			index_rgb_Move_Pre(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);
			iotalink_rgb_relax_2(20);
			index_rgb_Move_Pre(0, RGB_LED_NUM, r, g, b);
			iotalink_rgb_relax_2(20);

		}




	//	index_rgb_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);
	//	iotalink_rgb_relax();
	}

	
}

// 拖尾 23-38
void haoyida_tuowei_transition( int mode)
{

//	static u8 sv[] = {0,8,20,50,20,8,0,8,20,50,20,8,0};//亮度直接流动
	static int i ;//颜色
	int ss =100;//颜色
	if(mode==37||mode == 38 ) ss= 0;//白色

	mode_k++;
	if(mode_k==RGB_LED_NUM/2)//一半颜色块
////	if(sv[0]==0)
	{
		mode_k=0;
		mode_i++;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
	}



//流动下一个再变
    if(mode%2)	
    {
		static u8 sv[] = {
							0,1,2,3,5, 
						  10,12,15,20, 30,
						  30, 32,38,40,45,
						  0,1,2,3,5, 
						  10,12,15,20, 30,
						  30, 32,38,40,45};

		
		index_hsv_Move_Back(0,RGB_LED_NUM, MAGIC_SCENE_DATA.magic_hsv[mode_i].sh,0,0);
		for(int w=0;w<5;w++)//变5次再动
		{
			Array_Color_Move_Back(0,sizeof(sv),sv);
			for (i=0;i<RGB_LED_NUM;i++)
			{
				hsv_to_rgb(&r, &g, &b, sh[i] , ss, sv[i]/2);
				rgb_colorful_buffer_set(i ,r ,g  , b ); //写入当前点rgb	 
			}
			rgb_value_sync();// 

			wlt_ms_delay(magic_rate*0.5+20);
		}
	}
    else 	
    {
			static u8 sv[] = {
							 45,40,38,32,30,
					 		 30, 20,15,12,10,
					 		 6,5 ,4,3,0,
					 		 45,40,38,32,30,
					 		 30, 20,15,12,10,
					 		 6,5 ,4,3,0};
		
		index_hsv_Move_Pre(0,RGB_LED_NUM, MAGIC_SCENE_DATA.magic_hsv[mode_i].sh,0,0);

		for(int w=0;w<5;w++)//
		{

			Array_Color_Move_Pre(0,sizeof(sv),sv);
			for (i=0;i<RGB_LED_NUM;i++)
			{
				hsv_to_rgb(&r, &g, &b, sh[i] , ss, sv[i]/2);
				rgb_colorful_buffer_set(i ,r ,g  , b ); //写入当前点rgb	 
			}
			rgb_value_sync();// 

			wlt_ms_delay(magic_rate*0.5+20);
		}
	}

}

// // 39-56
void haoyida_liushui( int mode)
{

	mode_k++;
	if(mode_k>RGB_LED_NUM/2)//一半颜色块
	{
		mode_k=0;
		mode_i++;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
	}

    if(mode%2)		
    {
    	index_colorful_buffer_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate*0.5+50);
		rgb_value_sync(); //输出数据	
    }
	else
	{
		index_colorful_buffer_Move_Pre(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate*0.5+50);
		rgb_value_sync(); //输出数据	
	}

}

//------------------------------------------------------------------------------------------------//
#if 1

// 闭幕/拉幕效果核心函数 57-76
void haoyida_curtain_effect( int mode)
{
    u16 mid_led = RGB_LED_NUM / 2; // 灯带中点
	mode_k++;
	if(mode_k>RGB_LED_NUM/2)//一半颜色块
	{
		mode_k=0;
		mode_i++;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
	}

    if(mode%2)		
    {

		if(MAGIC_SCENE_DATA.magicallcnt==1)
		{

		
			index_colorful_buffer_Move_Back(0, mid_led,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);		
			index_colorful_buffer_Move_Pre(mid_led,RGB_LED_NUM ,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);


		    rgb_value_sync();
			wlt_ms_delay(magic_rate+50);

			index_colorful_buffer_Move_Back(0, mid_led,0,0,0);
			index_colorful_buffer_Move_Pre(mid_led,RGB_LED_NUM,0,0,0);

		}
		else
		{
			
	    	index_colorful_buffer_Move_Back(0, mid_led,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
			index_colorful_buffer_Move_Pre(mid_led,RGB_LED_NUM ,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);

		}

		
    }
	else
	{
		if(MAGIC_SCENE_DATA.magicallcnt==1)
		{
			index_colorful_buffer_Move_Pre(0, mid_led,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
			index_colorful_buffer_Move_Back(mid_led, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
			    // 4. 刷新灯带显示
		    rgb_value_sync();
			wlt_ms_delay(magic_rate+50);

			index_colorful_buffer_Move_Pre(0, mid_led,0,0,0);
			index_colorful_buffer_Move_Back(mid_led,RGB_LED_NUM,0,0,0);

		}
		else
		{

			index_colorful_buffer_Move_Pre(0, mid_led,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
			index_colorful_buffer_Move_Back(mid_led, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);

		}
	
	}

		// 4. 刷新灯带显示
	rgb_value_sync();
	wlt_ms_delay(magic_rate+50);



}
//77-82 追光
void haoyida_Following_light( int mode)
{

	mode_k++;
	if(mode_k>RGB_LED_NUM)//
	{
		mode_k=0;
		mode_i++;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
	}
    if(mode%2)		
    {
    	index_colorful_buffer_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
    }
	else
	{
		index_colorful_buffer_Move_Pre(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
	}


}
// 83-88 飘动
void haoyida_83_88_effect( int mode)
{
	

	mode_i++;	
	if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
	

	if(mode%2)		
    {
    	index_colorful_buffer_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
    }
	else
	{
		index_colorful_buffer_Move_Pre(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
	}

}
// 89-112 跑动
void haoyida_89_112_effect( int mode)
{
	
	mode_k++;
	if(mode_k>RGB_LED_NUM/2)//
	{
		mode_k=0;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
	}
	
    if(mode%2)		
    {
    

		
		index_colorful_buffer_Move_Back(RGB_LED_NUM/2, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		
		mode_i++;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
		index_colorful_buffer_Move_Back(0, RGB_LED_NUM/2,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate+100);
		rgb_value_sync(); //输出数据	
		
		for(int i =0;i<RGB_LED_NUM/2-1 ;i++)//
		{

			
			index_colorful_buffer_Move_Back(0, RGB_LED_NUM/2,0,0,0);
			index_colorful_buffer_Move_Back( RGB_LED_NUM/2, RGB_LED_NUM,0,0,0);

			wlt_ms_delay(magic_rate+100);
			rgb_value_sync(); //输出数据	
		}
	
    }
	else
	{
		
		
		

		index_colorful_buffer_Move_Pre(0, RGB_LED_NUM/2,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		mode_i++;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;

		index_colorful_buffer_Move_Pre(RGB_LED_NUM/2, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);

		
		wlt_ms_delay(magic_rate+100);
		rgb_value_sync(); //输出数据	
		
		for(int i =0;i<RGB_LED_NUM/2-1 ;i++)//
		{
		
			
			index_colorful_buffer_Move_Pre(0, RGB_LED_NUM/2,0,0,0);
			index_colorful_buffer_Move_Pre( RGB_LED_NUM/2, RGB_LED_NUM,0,0,0);
		
			wlt_ms_delay(magic_rate+100);
			rgb_value_sync(); //输出数据	
		}


	}
}
// 113_142 带底色跑动
void haoyida_113_142_effect( int mode)
{
	mode_k++;
	if(mode_k>RGB_LED_NUM/2)//
	{
		mode_k=0;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=1;
	}
	
    if(mode%2)		
    {
    

		
		index_colorful_buffer_Move_Back(RGB_LED_NUM/2, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		
		mode_i++;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=1;
		index_colorful_buffer_Move_Back(0, RGB_LED_NUM/2,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate+100);
		rgb_value_sync(); //输出数据	
		
		for(int i =0;i<RGB_LED_NUM/2-1 ;i++)//
		{

			index_colorful_buffer_Move_Back(0, RGB_LED_NUM/2, MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			index_colorful_buffer_Move_Back(RGB_LED_NUM/2,RGB_LED_NUM, MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);


			wlt_ms_delay(magic_rate+100);
			rgb_value_sync(); //输出数据	
		}
	
    }
	else
	{
		

		index_colorful_buffer_Move_Pre(0, RGB_LED_NUM/2,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		mode_i++;	
		if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=1;

		index_colorful_buffer_Move_Pre(RGB_LED_NUM/2, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);

		wlt_ms_delay(magic_rate+100);
		rgb_value_sync(); //输出数据	
		
		for(int i =0;i<RGB_LED_NUM/2-1 ; i++)//
		{
		
			index_colorful_buffer_Move_Pre(0, RGB_LED_NUM/2, MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			index_colorful_buffer_Move_Pre(RGB_LED_NUM/2,RGB_LED_NUM, MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
		
			wlt_ms_delay(magic_rate+100);
			rgb_value_sync(); //输出数据	
		}


	}	

}
// YXXY流动
void haoyida_143_166_effect( int mode)
{
	if(mode%2)		 
	{

		Move_Back();
	}
	else 
		Move_Pre();


	rgb_value_sync(); 
	wlt_ms_delay(magic_rate+100);


}
//  
void haoyida_167_180_effect( int mode)
{
	if(mode%2)		 
	{
		Move_Back();
	}
	else 
		Move_Pre();
	
	rgb_value_sync(); 
	wlt_ms_delay(magic_rate+100);

}


void haoyida_181_186_effect( int mode)
{
	int mode_i_2;
	

	if(mode%2)		
    {
		mode_i_2 = mode_i-1;

		if(mode_i_2<0) mode_i_2= MAGIC_SCENE_DATA.magicallcnt-1;

		index_colorful_buffer_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].r,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].g,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].b);
			wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
		
    	index_colorful_buffer_Move_Back(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	

	
		mode_k++;
		if(mode_k>RGB_LED_NUM/2)	//刷完2色 下一色
		{
			mode_k=0;
			mode_i++;
			
			if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
			rgbsingleColor(MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
			wlt_ms_delay(magic_rate+50);
			rgb_value_sync(); //输出数据			
		}	
	}
	else
	{
		mode_i_2 = mode_i-1;
		
		if(mode_i_2<0) mode_i_2= MAGIC_SCENE_DATA.magicallcnt-1;
		
		index_colorful_buffer_Move_Pre(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].r,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].g,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].b);
			wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
		
		index_colorful_buffer_Move_Pre(0, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
		
		
		mode_k++;
		if(mode_k>RGB_LED_NUM/2)	//刷完2色 下一色
		{
			mode_k=0;
			mode_i++;
			if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
			rgbsingleColor(MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
			wlt_ms_delay(magic_rate+50);
			rgb_value_sync(); //输出数据			
		}


	}





}

void haoyida_187_192_effect( int mode)
{
	int mode_i_2;
	
	if(mode%2)		
    {
		mode_i_2 = mode_i-1;
		if(mode_i_2<0) mode_i_2= MAGIC_SCENE_DATA.magicallcnt-1;

		index_colorful_buffer_Move_Back(0, RGB_LED_NUM/2,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].r,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].g,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].b);
		index_colorful_buffer_Move_Pre( RGB_LED_NUM/2, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].r,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].g,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].b);
			wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
		
    	index_colorful_buffer_Move_Back(0, RGB_LED_NUM/2,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		index_colorful_buffer_Move_Pre( RGB_LED_NUM/2, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);

		
		wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	

	
		mode_k++;
		if(mode_k>RGB_LED_NUM/4)	//刷完2色 下一色
		{
			mode_k=0;
			mode_i++;
			
			if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
			rgbsingleColor(MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
			wlt_ms_delay(magic_rate+50);
			rgb_value_sync(); //输出数据			
		}	
	}
	else
	{
		mode_i_2 = mode_i-1;
		
		if(mode_i_2<0) mode_i_2= MAGIC_SCENE_DATA.magicallcnt-1;
		
		index_colorful_buffer_Move_Pre(0, RGB_LED_NUM/2,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].r,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].g,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].b);
		index_colorful_buffer_Move_Back( RGB_LED_NUM/2, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].r,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].g,MAGIC_SCENE_DATA.magic_rgb[mode_i_2].b);
			wlt_ms_delay(magic_rate+50);
		rgb_value_sync(); //输出数据	
		
    	index_colorful_buffer_Move_Pre(0, RGB_LED_NUM/2,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
		index_colorful_buffer_Move_Back( RGB_LED_NUM/2, RGB_LED_NUM,MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);

		mode_k++;
		if(mode_k>RGB_LED_NUM/4)	//刷完2色 下一色
		{
			mode_k=0;
			mode_i++;
			if(mode_i>=MAGIC_SCENE_DATA.magicallcnt) mode_i=0;
			rgbsingleColor(MAGIC_SCENE_DATA.magic_rgb[mode_i].r,MAGIC_SCENE_DATA.magic_rgb[mode_i].g,MAGIC_SCENE_DATA.magic_rgb[mode_i].b);
			wlt_ms_delay(magic_rate+50);
			rgb_value_sync(); //输出数据			
		}
	}

}
// 明暗过渡流水
void  haoyida_214_220_effect(void)
{
	u8 step=10;
	static	s32 gain_red,gain_green,gain_blue ;
	static s32 diffRed ,diffGreen,diffBlue;
	static u8 magicunit_bak = 0;

	static u8 sv[] = {0,6, 20,50,20,6,0,
					     6,20,50,20,6,0,
					      6,20,50,20,6,0};
	static int i ,j, k ;//颜色

	Array_Color_Move_Back(0,sizeof(sv),sv);
	for (i=0;i<RGB_LED_NUM;i++)
	{
		hsv_to_rgb(&sr[i], &sg[i], &sb[i], MAGIC_SCENE_DATA.magic_hsv[0].sh , 100, sv[i]/2);
		//rgb_colorful_buffer_set(i ,r ,g  , b ); //写入当前点rgb	
	}
	for (i=1;i<step;i++)
	{
		if (magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
		{
			 magicunit_bak = sg_light_ctrl_data.custome_unit; 
			return ;
		}	 
		 for (k=0;k<RGB_LED_NUM;k++)
		 {
				//实际rgb差值
				 diffRed   = sr[k] - sr_bk[k];
				 diffGreen = sg[k] - sg_bk[k];
				 diffBlue  = sb[k] - sb_bk[k];	
				 
				 //步长
				 gain_red=diffRed/step;
				 gain_green=diffGreen/step;
				 gain_blue=diffBlue/step; 
				 
				 //目标数值  
				 rgb_colorful_values[k][0]=sr_bk[k]+gain_red*i; 				 
				 rgb_colorful_values[k][1]=sg_bk[k]+gain_green*i;
				 rgb_colorful_values[k][2]=sb_bk[k]+gain_blue*i;
				 //关灯
				 if((LOCAL_MAGIC_MODE==false))
				 {
						 k=RGB_LED_NUM;
						 return ;
				 }	
		 }//magic_rate/10+20
		 wlt_ms_delay(2);
		 rgb_value_sync(); //输出数据 
	}	
	memcpy(sr_bk,sr,sizeof(sr));
	memcpy(sg_bk,sg,sizeof(sg));
	memcpy(sb_bk,sb,sizeof(sb));
	
	wlt_ms_delay(magic_rate*0.1);

}

void haoyida_221_228_effect(void)
{
		unsigned int i = 0, j = 0;
	static int magicunit_bak;

	static unsigned char num = 6;      // 从中间1个点开始扩散
	static unsigned char color_i = 0;      // 从中间1个点开始扩散
	u8 interval = RGB_LED_NUM / 2;     // 中间位置
	unsigned char r, g, b;

	//hsv_to_rgb(&r, &g, &b, sh1, ss, sv);

	// 从中间向两端扩散 i 代表扩散层数
	for (i = 0; i < num; i++)
	{
		// 点亮：中间 ±i 位置
		for (j = 0; j < 2; j++)
		{
			int pos;
			if (j == 0)
				pos = interval - i;    // 左侧
			else
				pos = interval + i;    // 右侧

			// 边界保护，防止越界
			if (pos >= 0 && pos < RGB_LED_NUM)
				rgb_colorful_buffer_set(pos, MAGIC_SCENE_DATA.magic_rgb[color_i].r,MAGIC_SCENE_DATA.magic_rgb[color_i].g,MAGIC_SCENE_DATA.magic_rgb[color_i].b);
		}
		
		rgb_value_sync();
		wlt_ms_delay(magic_rate*2+10);
		
		//if ( magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
		if ( magicunit_bak != sg_light_ctrl_data.custome_unit||LOCAL_MAGIC_MODE==0)
		{
			magicunit_bak = sg_light_ctrl_data.custome_unit;
			color_i = 0;
			num = 1; 
			return;
		}

		// 灭灯（移动效果用）
		if (i < num-1)
		{
			for (j = 0; j < 2; j++)
			{
				int pos;
				if (j == 0)
					pos = interval - i;
				else
					pos = interval + i;

				if (pos >= 0 && pos < RGB_LED_NUM)
					rgb_colorful_buffer_set(pos, 0, 0, 0);
			}
		}
		//增加闪烁感 
		//rgb_value_sync();
		//wlt_ms_delay(magic_rate*2+10);
	}

	num--;  // 扩散层数递减
	// 一轮扩散完成，切换颜色
	if (num == 0)
	{
		num = interval;  // 最大扩散到两端
		color_i++;
		if(color_i>=MAGIC_SCENE_DATA.magicallcnt) color_i=0;
		rgb_colorful_buffer_clean();
	}

	// 保持当前扩散终点常亮
	for (j = 0; j < 2; j++)
	{
		int pos;
		if (j == 0)
			pos = interval - num;    // 左侧当前终点
		else
			pos = interval + num;    // 右侧当前终点

		if (pos >= 0 && pos < RGB_LED_NUM)
			rgb_colorful_buffer_set(pos, MAGIC_SCENE_DATA.magic_rgb[color_i].r,MAGIC_SCENE_DATA.magic_rgb[color_i].g,MAGIC_SCENE_DATA.magic_rgb[color_i].b);
	}

	rgb_value_sync();
	wlt_ms_delay(magic_rate*2+10);
}

//------------------------------------------------------------------------------------------------//

void haoyida_234_mode_deal( int modeunite)
{

	switch (modeunite)
	{
		case 0://自动循环
			break;
		case 1://正向幻彩		
			break;	
		case 2://反向幻彩
			haoyida_magic_flow_effect(modeunite%2);
			break;

		case 3://正向七色明暗过渡
		case 4://反向七色明暗过渡
		case 5://正向红绿蓝明暗过渡
		case 6://反向红绿蓝明暗过渡
		case 7://正向黄青紫明暗过渡
		case 8://反向黄青紫明暗过渡
			haoyida_shade_light_transition(modeunite%2);
			break;

		case 9: //正向六色明暗过渡红色
		case 11://正向六色明暗过渡绿色
		case 13://正向六色明暗过渡蓝色
		case 15://正向六色明暗过渡青色
		case 17://正向六色明暗过渡黄色
		case 19://正向六色明暗过渡紫色
		case 21://正向六色明暗过渡白色
		case 10://反向六色明暗过渡红色
		case 12://反向六色明暗过渡绿色
		case 14://反向六色明暗过渡蓝色
		case 16://反向六色明暗过渡青色
		case 18://反向六色明暗过渡黄色
		case 20://反向六色明暗过渡紫色
		case 22://反向六色明暗过渡白色
			haoyida_shade_light_transition_xse(modeunite);
			break;
		case 23://正向七彩拖尾
		case 24://反向七彩拖尾			
		case 25://正向红色拖尾			
		case 26://反向红色拖尾		
		case 27://正向绿色拖尾			
		case 28://反向绿色拖尾			
		case 29://正向蓝色拖尾			
		case 30://反向蓝色拖尾			
		case 31://正向黄色拖尾			
		case 32://反向黄色拖尾		
		case 33://正向青色拖尾			
		case 34://反向青色拖尾		
		case 35://正向紫色拖尾	
		case 36://反向紫色拖尾	
		case 37://正向白色拖尾
		case 38://反向白色拖尾
			haoyida_tuowei_transition(modeunite);	
			break;
		case 39://正向七彩流水
		case 40://反向七彩流水
		case 41://正向红绿蓝流水
		case 42://反向红绿蓝流水
		case 43://正向黄青紫流水
		case 44://反向黄青紫流水
		case 45://正向红绿流水
		case 46://反向红绿流水
		case 47://正向绿蓝流水
		case 48://反向绿蓝流水
		case 49://正向黄蓝流水
		case 50://反向黄蓝流水
		case 51://正向黄青流水
		case 52://反向黄青流水
		case 53://正向青紫流水
		case 54://反向青紫流水
		case 55://正向黑白流水
		case 56://反向黑白流水
			 haoyida_liushui(modeunite);
			break;
		case 57://七彩闭幕
		case 58://七彩拉幕
		case 59://红绿蓝闭幕
		case 60://红绿蓝拉幕
		case 61://黄青紫闭幕
		case 62://黄青紫拉幕
		case 63://红色闭幕
		case 64://红色拉幕
		case 65://绿色闭幕
		case 66://绿色拉幕
		case 67://蓝色闭幕
		case 68://蓝色拉幕
		case 69://黄色闭幕
		case 70://黄色拉幕
		case 71://青色闭幕
		case 72://青色拉幕
		case 73://紫色闭幕
		case 74://紫色拉幕
		case 75://白色闭幕
		case 76://白色拉幕
			haoyida_curtain_effect(modeunite);

			break;
		case 77://正向七彩追光	
		case 78://反向七彩追光		
		case 79://正向红绿蓝追光
		case 80://反向红绿蓝追光
		case 81://正向黄青紫追光
		case 82://反向黄青紫追光		
			haoyida_Following_light(modeunite);
			break;

		case 83://七彩飘动 一个一色点
		case 84:	
		case 85:
		case 86:
		case 87:
		case 88:
			haoyida_83_88_effect(modeunite);
			break;
//---------- 89-112 跑动---------------
		case 89:
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
		case 98:
		case 99:
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
		case 108:
		case 109:
		case 110:
		case 111:
		case 112:
			 haoyida_89_112_effect(modeunite);
		 	break;
		case 113:
		case 114:
		case 115:
		case 116:
		case 117:
		case 118:
		case 119:
		case 120:
		case 121:
		case 122:
		case 123:
		case 124:
		case 125:
		case 126:
		case 127:
		case 128:
		case 129:
		case 130:
		case 131:
		case 132:
		case 133:
		case 134:
		case 135:
		case 136:
		case 137:
		case 138:
		case 139:
		case 140:
		case 141:
		case 142:
			haoyida_113_142_effect(modeunite);
			break;
		case 143:
		case 144:
		case 145:
		case 146:
		case 147:
		case 148:
		case 149:
		case 150:
		case 151:
		case 152:
		case 153:
		case 154:
		case 155:
		case 156:
		case 157:
		case 158:
		case 159:
		case 160:
		case 161:
		case 162:
		case 163:
		case 164:
		case 165:
		case 166:
		case 167:
			haoyida_143_166_effect(modeunite);

		case 168:
		case 169:
		case 170:
		case 171:
		case 172:
		case 173:
		case 174:
		case 175:
		case 176:
		case 177:
		case 178:
		case 179:
		case 180:
			haoyida_167_180_effect(modeunite);
			break;
		case 181:
		case 182:
		case 183:
		case 184:
		case 185:
		case 186:
			haoyida_181_186_effect(modeunite);
			break;
		case 187://闭幕拉幕2 同刷色 起点不一致 
		case 188:
		case 189:
		case 190:
		case 191:
		case 192:
			haoyida_187_192_effect(modeunite);


			break;
		case 193://跳变
		case 194:
		case 195:
			iotalink_magic_scene_relax(1,MAGIC_SCENE_JUMP);
		
			break;
		
		case 196://频闪
		case 197:
		case 198:
			iotalink_magic_scene_relax(1,MAGIC_SCENE_TWINKLE);
	 		
			break;
		
		case 199://渐变
		case 200:
		case 201:
		case 202:
		case 203:
		case 204:
			iotalink_magic_scene_relax(1,MAGIC_SCENE_BREATHE);//
			break;
		case 205://跑马
		case 206:
		case 207:
		case 208:
		case 209:
		case 210:
		case 211:

			Move_Pre();
			rgb_value_sync(); 
			wlt_ms_delay(magic_rate*1+20);	
			break;
		case 212://七彩能量->就是七彩跳变
			iotalink_magic_scene_relax(1,MAGIC_SCENE_JUMP);
			break;
		case 213://特别七色-->就是七色开幕
			haoyida_curtain_effect(modeunite+1);
			break;
		case 214://过渡流水  	
		case 215:
		case 216:
		case 217:
		case 218:
		case 219:
		case 220:
		//	haoyida_shade_light_transition(1);
			haoyida_214_220_effect();

			break;
		case 221://堆砌 
		case 222:
		case 223:
		case 224:
		case 225:
		case 226:
		case 227:
		case 228:
			haoyida_221_228_effect();	
			break;
		case 229://七彩渐变
		case 230:// 过渡
		case 231:
		case 232:
		case 233:
			iotalink_magic_scene_relax(1,MAGIC_SCENE_RELAX);//淡入淡出
			break;
		default :
		  	index_colorful_buffer_Move_Back(0, RGB_LED_NUM,modeunite ,modeunite,modeunite);
			wlt_ms_delay(magic_rate*0.5+50);
			rgb_value_sync(); //输出数据		
			break;
			
	}


}


/*
红 (Red)	0° / 360°
橙 (Orange)	30°
黄 (Yellow)	60°
绿 (Green)	120°
青 (Cyan)	180°
蓝 (Blue)	240°
紫 (Violet)	300 1,45°
*/
unsigned char  default_mode[110][38]=
{
	{0,0,0,0,0},// 在几个模式下循环
	{0,1,0,0,0},////正向幻彩 直接写
	{0,2,0,0,0},//反向幻彩 直接写
	{0,3,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,4,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},//七色明暗过渡
//红绿蓝明暗过渡	
	{0,5,2,100,50, 0,0,100,0,  0,120,100,0, 0,240,100,0 },
	{0,6,2,100,50, 0,0,100,0,  0,120,100,0, 0,240,100,0 },
//黄青紫明暗过渡
	{0,7,2,100,50, 0,60,100,0,  0,180,100,0, 1,45,100,0   },
	{0,8,2,100,50, 0,60,100,0,  0,180,100,0, 1,45,100,0 },
//明暗过渡x色 9-22	
	{1,9,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,10,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,11,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,12,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,13,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,14,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,15,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,16,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,17,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,18,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,19,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,20,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,21,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{1,22,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},

// 拖尾 23-38
	{0,23,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
//	{0,24,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
//七彩
	{0,24,2,100,50,  0,60,100,0,    0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0 ,0,0,0,0, 0,0,100,0},

	
	{0,25,2,100,50, 0,0,100,0 },
	{0,26,2,100,50, 0,0,100,0 },
	{0,27,2,100,50, 0,120,100,0},
	{0,28,2,100,50, 0,120,100,0},
	{0,29,2,100,50, 0,240,100,0},
	{0,30,2,100,50, 0,240,100,0},
	{0,31,2,100,50, 0,30,100,0},
	{0,32,2,100,50, 0,30,100,0},
	{0,33,2,100,50, 0,180,100,0},
	{0,34,2,100,50, 0,180,100,0},
	{0,35,2,100,50, 1,45,100,0},
	{0,36,2,100,50, 1,45,100,0},
	{0,37,2,100,50, 0,0,0,0},
	{0,38,2,100,50, 0,0,0,0},

	
// 流水
	{0,39,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	{0,40,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	//红绿蓝
	{0,41,2,100,50, 0,0,100,0,  0,120,100,0, 0,240,100,0 },
	{0,42,2,100,50, 0,0,100,0,  0,120,100,0, 0,240,100,0 },
	//青黄紫
	{0,43,2,100,50, 0,180,100,0,   0,240,100,0, 1,45,100,0 },
	{0,44,2,100,50, 0,180,100,0,   0,240,100,0, 1,45,100,0 },
	//红绿
	{0,45,2,100,50, 0,0,100,0,   0, 120,100,0  },
	{0,46,2,100,50, 0,0,100,0,   0, 120,100,0  },
	//绿蓝
	{0,47,2,100,50, 0,120,100,0,   0, 240,100,0  },
	{0,48,2,100,50, 0,120,100,0,   0, 240,100,0 },
	//黄蓝流水
	{0,49,2,100,50, 0,60,100,0,   0, 240,100,0  },
	{0,50,2,100,50, 0,60,100,0,   0, 240,100,0 },

	//黄青流水
	{0,51,2,100,50, 0,60,100,0,   0, 180,100,0  },
	{0,52,2,100,50, 0,60,100,0,   0, 180,100,0 },
	//青紫流水
	{0,53,2,100,50, 0,180,100,0,   1, 45,100,0  },
	{0,54,2,100,50, 0,180,100,0,   1, 45,100,0 },
	//黑白流水 未用直接赋值
	{0,55,2,100,50, 0,180,100,0,   1, 45,100,0 },
	{0,56,2,100,50, 0,180,100,0,   1, 45,100,0},

//蓝 紫 青
	{0,57,2,100,50,  0,240,100,0, 1,45,100,0,  0,180,100,0,},
//蓝 绿 青
	{0,58,2,100,50,  0,240,100,0, 0,120,100,0, 0,180,100,0,},
//-------------
//白 红跑动
	{0,59,2,100,50,  0,0,0,0, 0,0,100,0},
//红绿跑动
	{0,60,2,100,50,  0,0,100,0, 0,120,100,0},
//正向绿底蓝点跑动
	{0,61,2,100,50,  0,120,100,0, 0,240,100,0},
//反向蓝底黄点跑动
	{0,62,2,100,50,  0,240,100,0, 0,60,100,0},
//反向黄底青点跑动
	{0,63,2,100,50,  0,60,100,0, 0,180,100,0},		
	//反向青底紫点跑动
	{0,64,2,100,50,  0,180,100,0, 1,45,100,0},	
//反向紫底白点跑动
	{0,65,2,100,50,  1,45,100,0, 0,0,0,0},	
//反向红底白点跑动
	{0,66,2,100,50,  0,0,100,0, 0,0,0,0},
//反向红底七色跑动
	{0,67,2,100,50,  0,0,100,0, 0,0,100,0, 0,60,100,0,    0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0 ,0,0,0,0,},
//正向绿底七色跑动 
	{0,68,2,100,50,  0,120,100,0, 0,0,100,0, 0,60,100,0,    0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0 ,0,0,0,0,},
//正向蓝底七色跑动
	{0,69,2,100,50,  0,240,100,0, 0,0,100,0, 0,60,100,0,    0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0 ,0,0,0,0,},
//反向黄底七色跑动
	{0,70,2,100,50,  0,60,100,0, 0,0,100,0, 0,60,100,0,    0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0 ,0,0,0,0,},
//正向青底七色跑动
	{0,71,2,100,50,  0,180,100,0, 0,0,100,0, 0,60,100,0,    0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0 ,0,0,0,0,},
//反向紫底七色跑动
	{0,72,2,100,50,1,45,100,0,  0,0,100,0, 0,60,100,0,    0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0 ,0,0,0,0,},
//正向白底七色跑动
	{0,73,2,100,50,  0,0,0,0 , 0,0,100,0, 0,60,100,0,    0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0 ,0,0,0,0,},

//-------------
//白红白流动
	{0,74,2,100,50,  0,0,0,0,   0,0,100,0},	
//正向白绿白流动	
	{0,75,2,100,50,  0,0,0,0,	0,120,100,0}, 
//	白蓝白流动
	{0,76,2,100,50,  0,0,0,0,	0,240,100,0}, 

//反向白黄白流动
	{0,77,2,100,50,  0,0,0,0,	0,60,100,0}, 

//正向白青白流动
	{0,78,2,100,50,  0,0,0,0,	0,180,100,0}, 
//正向白紫白流动
	{0,79,2,100,50,  0,0,0,0,	1,45,100,0}, 
	
//正向红白红流动
	{0,80,2,100,50,     0,0,100,0, 0,0,0,0 },	
//正向绿白绿流动
//正向蓝白蓝流动
//正向黄白黄流动
//正向青白青流动
//正向紫白紫流动
	{0,81,2,100,50,   0,120,100,0, 0,0,0,0,}, 
	{0,82,2,100,50,	  0,240,100,0,  0,0,0,0}, 
	{0,83,2,100,50,   0,60,100,0,   0,0,0,0}, 
	{0,84,2,100,50,   0,180,100,0,  0,0,0,0}, 
	{0,85,2,100,50,   1,45,100,0,    0,0,0,0}, 
//----------------------------------------------------
//正向蓝底绿点跑动
	{0,86,2,100,50,   0,240,100,0,	 0,120,100,0}, 
//正向红底绿点跑动
	{0,87,2,100,50,   0,0,100,0,	 0,120,100,0}, 
//反向蓝底红点跑动
	{0,88,2,100,50,   0,240,100,0,	 0,0,100,0}, 
//反向黄底青点跑动
	{0,89,2,100,50,   0,60,100,0,	 0,180,100,0},

//反向紫底黄点跑动
	{0,90,2,100,50,    1,45,100,0,	 0,60,100,0},
//反向黄底白点跑动
	{0,91,2,100,50,    0,60,100,0,	 0,0,0,0},
//反向白底黄点跑动
	{0,92,2,100,50,	 0,0,0,0 ,    0,60,100,0},
//----------------------------------------------------
//红黄交替渐变
	{0,93,2,100,50,	 0,0,100,0 ,    0,60,100,0},		
//红紫交替渐变
	{0,94,2,100,50,	 0,0,100,0 ,    1,45,100,0},
//绿青交替渐变
	{0,95,2,100,50,  0,120,100,0 ,   0,180,100,0},

//绿黄交替渐变
	{0,96,2,100,50,  0,120,100,0 ,   0,60,100,0},

//蓝紫交替渐变	
	{0,97,2,100,50,  0,240,100,0 ,   1,45,100,0},

//----------------------------------------------------
//青蓝200
	{0,98,2,100,50,  0,200,100,0 },
//红橙 15
	{0,99,2,100,50,  0,10,100,0 },

	{0,100,2,100,50,  0,10,100,0 , 0,60,100,0}


};


// 初始化sh
void  iotalink_234_mode_mode_init(void)
{

   unsigned char i=0,j=0 ,loop=(RGB_LED_NUM)/(MAGIC_SCENE_DATA.magicallcnt),num=0, num_tep=0; 

	for (i= 0; i<MAGIC_SCENE_DATA.magicallcnt; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
	{
	   for (j= 0; j<loop;j++)
	   {		   
			index_hsv_Move_Pre(0,RGB_LED_NUM, MAGIC_SCENE_DATA.magic_hsv[i].sh,MAGIC_SCENE_DATA.magic_hsv[i].ss,0); 
			num++;//灯珠点
		   // if (num>=RGB_LED_NUM)break;
	   }
	   //if (num>=RGB_LED_NUM)break;
	}
	num_tep=num;
	for (i= num; i<RGB_LED_NUM_INCREASE; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
	{
		index_hsv_Move_Pre(0,RGB_LED_NUM, MAGIC_SCENE_DATA.magic_hsv[MAGIC_SCENE_DATA.magicallcnt-1].sh,MAGIC_SCENE_DATA.magic_hsv[i].ss,0);     
	}	   
	//rgb_value_sync();//输出
}

void haoyida_234_mode_init( int modeunite)
{

	switch (modeunite)
	{

		case 0://自动循环
			wlt_ble_remote_control(7);
			
			printf(" ---init ------> AUTO \n");
			break;
		case 1://正向幻彩
		
	
		case 2://反向幻彩
	
			break;
		case 3://正向七色明暗过渡
		case 4://反向七色明暗过渡
			iotalink_color_transition(7,1,&default_mode[4] , &MAGIC_SCENE_DATA);//解析出颜色
			haoyida_shade_light_transition(0); // 重置静态变量j和K
			break;
		case 5://正向红绿蓝明暗过渡
		case 6://反向红绿蓝明暗过渡
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			haoyida_shade_light_transition(0); // 重置静态变量j和K
			break;
		case 7://正向黄青紫明暗过渡
		case 8://反向黄青紫明暗过渡
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色
			haoyida_shade_light_transition(0); // 重置静态变量j和K
			break;
		
		case 9://正向六色明暗过渡红色
		case 10://反向六色明暗过渡红色
		case 11://正向六色明暗过渡绿色
		case 12://反向六色明暗过渡绿色
		case 13://正向六色明暗过渡蓝色
		case 14://反向六色明暗过渡蓝色
		case 15://正向六色明暗过渡青色
		case 16://反向六色明暗过渡青色
		case 17://正向六色明暗过渡黄色
		case 18://反向六色明暗过渡黄色			
		case 19://正向六色明暗过渡紫色
		case 20://反向六色明暗过渡紫色	
		case 21://正向六色明暗过渡白色	
		case 22://反向六色明暗过渡白色
			iotalink_color_transition(7,1,&default_mode[22] , &MAGIC_SCENE_DATA);//解析出颜色	
		
			break;

			
		case 23://正向七彩拖尾
		case 24://反向七彩拖尾
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			
			break;
		case 25://正向红色拖尾
		case 26://反向红色拖尾
			iotalink_color_transition(1,1,&default_mode[26] , &MAGIC_SCENE_DATA);//解析出颜色
		
			break;
		case 27://正向绿色拖尾
		case 28://反向绿色拖尾
			iotalink_color_transition(1,1,&default_mode[28] , &MAGIC_SCENE_DATA);//解析出颜色
			
		
			break;
		case 29://正向蓝色拖尾
		case 30://反向蓝色拖尾
			iotalink_color_transition(1,1,&default_mode[30] , &MAGIC_SCENE_DATA);//解析出颜色
		
			
			break;
		case 31://正向黄色拖尾
		case 32://反向黄色拖尾
			iotalink_color_transition(1,1,&default_mode[32] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 33://正向青色拖尾
		case 34://反向青色拖尾
			iotalink_color_transition(1,1,&default_mode[34] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 35://正向紫色拖尾
		case 36://反向紫色拖尾
			iotalink_color_transition(1,1,&default_mode[36] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 37://正向白色拖尾
		case 38://反向白色拖尾
			iotalink_color_transition(1,1,&default_mode[38] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
//流水
		case 39://正向七彩流水
		case 40://反向七彩流水
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].r,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].g,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].b);
			}
			break;
		case 41://正向红绿蓝流水
		case 42://反向红绿蓝流水
		case 43://正向黄青紫流水
		case 44://反向黄青紫流水
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(3,1,&default_mode[modeunite] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].r,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].g,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].b);
			}
			break;
		case 45://正向红绿流水
		case 46://反向红绿流水
		case 47://正向绿蓝流水
		case 48://反向绿蓝流水
		case 49://正向黄蓝流水
		case 50://反向黄蓝流水
		case 51://正向黄青流水
		case 52://反向黄青流水
		case 53://正向青紫流水
		case 54://反向青紫流水
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(2,1,&default_mode[modeunite] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].r,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].g,MAGIC_SCENE_DATA.magic_rgb[i/(RGB_LED_NUM/2)].b);
			}
			break;
		case 55://正向黑白流水
		case 56://反向黑白流水
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(2,1,&default_mode[modeunite] , &MAGIC_SCENE_DATA);//解析出颜色
			
			MAGIC_SCENE_DATA.magic_rgb[0].r = 100;
			MAGIC_SCENE_DATA.magic_rgb[0].g = 100;
			MAGIC_SCENE_DATA.magic_rgb[0].b = 100;
			MAGIC_SCENE_DATA.magic_rgb[1].r= 0;
			MAGIC_SCENE_DATA.magic_rgb[1].r = 0;
			MAGIC_SCENE_DATA.magic_rgb[1].b = 0;
			
			Color_Section(0,RGB_LED_NUM/3,100,rgb_colorful_values[0]);
			Color_Section(0,RGB_LED_NUM/3,100,rgb_colorful_values[1]);
			Color_Section(0,RGB_LED_NUM/3,100,rgb_colorful_values[2]);
			Color_Section(RGB_LED_NUM/2,RGB_LED_NUM,0,rgb_colorful_values[0]);	
			Color_Section(RGB_LED_NUM/2,RGB_LED_NUM,0,rgb_colorful_values[1]);
			Color_Section(RGB_LED_NUM/2,RGB_LED_NUM,0,rgb_colorful_values[2]);

			
			break;
		case 57://七彩闭幕
		case 58://七彩拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 59://红绿蓝闭幕
		
		case 60://红绿蓝拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 61://黄青紫闭幕
		case 62://黄青紫拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 63://红色闭幕
		case 64://红色拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(1,1,&default_mode[26] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 65://绿色闭幕
		case 66://绿色拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(1,1,&default_mode[28] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 67://蓝色闭幕
		case 68://蓝色拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(1,1,&default_mode[30] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 69://黄色闭幕
		case 70://黄色拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(1,1,&default_mode[32] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 71://青色闭幕
		case 72://青色拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(1,1,&default_mode[34] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 73://紫色闭幕
		case 74://紫色拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(1,1,&default_mode[36] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 75://白色闭幕
		case 76://白色拉幕
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(1,1,&default_mode[38] , &MAGIC_SCENE_DATA);//解析出颜色
			break;

		
		case 77://正向七彩追光
		case 78://反向七彩追光
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 79://正向红绿蓝追光
		case 80://反向红绿蓝追光
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 81://正向黄青紫追光
		case 82://反向黄青紫追光
			mode_i = 0; mode_k = 0;  // 重置静态变量
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色
			break;

//--------------------83-88-------------------			
		case 83://正向七彩飘动
		case 84://反向七彩飘动
		
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i%7].r,MAGIC_SCENE_DATA.magic_rgb[i%7].g,MAGIC_SCENE_DATA.magic_rgb[(i%7)].b);
			}
			break;
		case 85://正向红绿蓝飘动
		case 86://反向红绿蓝飘动
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i%3].r,MAGIC_SCENE_DATA.magic_rgb[i%3].g,MAGIC_SCENE_DATA.magic_rgb[(i%3)].b);
			}
			break;
		case 87://正向黄青紫飘动
		case 88://反向黄青紫飘动
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i%3].r,MAGIC_SCENE_DATA.magic_rgb[i%3].g,MAGIC_SCENE_DATA.magic_rgb[(i%3)].b);
			}
			break;
//---------------------------------------
		case 89://正向红色跑动
		case 90://反向红色跑动
			iotalink_color_transition(1,1,&default_mode[26] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 91://正向绿色跑动			
		case 92://反向绿色跑动
			iotalink_color_transition(1,1,&default_mode[28] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 93://正向蓝色跑动
		case 94://反向蓝色跑动
			iotalink_color_transition(1,1,&default_mode[30] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 95://正向黄色跑动
		case 96://反向黄色跑动
			iotalink_color_transition(1,1,&default_mode[32] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 97://正向青色跑动
		case 98://反向青色跑动
			iotalink_color_transition(1,1,&default_mode[34] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 99://正向紫色跑动
		case 100://反向紫色跑动
			iotalink_color_transition(1,1,&default_mode[36] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 101://正向白色跑动
		case 102://反向白色跑动
			iotalink_color_transition(1,1,&default_mode[38] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		
		case 103://正向七色跑动
		case 104://反向七色跑动
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 105://正向红绿蓝跑动
		case 106://反向红绿蓝跑动
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 107://正向黄青紫跑动
		case 108://反向黄青紫跑动
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 109://正向蓝紫青黄跑动
		case 110://反向蓝紫青黄跑动
			iotalink_color_transition(3,1,&default_mode[57] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 111://正向蓝绿青黄跑动
		case 112://反向蓝绿青黄跑动
			iotalink_color_transition(3,1,&default_mode[58] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
//---------------------------------------		
		case 113://正向白底红点跑动
		case 114://反向白底红点跑动

			iotalink_color_transition(2,1,&default_mode[59] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}			
			break;
		case 115://正向红底绿点跑动
		case 116://反向红底绿点跑动
			iotalink_color_transition(2,1,&default_mode[60] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 117://正向绿底蓝点跑动
		case 118://反向绿底蓝点跑动
				iotalink_color_transition(2,1,&default_mode[61] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 119://正向蓝底黄点跑动
		case 120://反向蓝底黄点跑动
			iotalink_color_transition(2,1,&default_mode[62] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 121://正向黄底青点跑动
		case 122://反向黄底青点跑动
			iotalink_color_transition(2,1,&default_mode[63] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 123://正向青底紫点跑动
		case 124://反向青底紫点跑动
			iotalink_color_transition(2,1,&default_mode[64] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;

		case 125://正向紫底白点跑动
		case 126://反向紫底白点跑动
			iotalink_color_transition(2,1,&default_mode[65] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 127://正向红底白点跑动
		case 128://反向红底白点跑动
			iotalink_color_transition(2,1,&default_mode[66] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;

		case 129://正向红底七色跑动
		case 130://反向红底七色跑动
		
			iotalink_color_transition(8,1,&default_mode[67] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;

			break;
		case 131://正向绿底七色跑动	
		case 132://反向绿底七色跑动
			iotalink_color_transition(8,1,&default_mode[68] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 133://正向蓝底七色跑动
		case 134://反向蓝底七色跑动
			iotalink_color_transition(8,1,&default_mode[69] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 135://正向黄底七色跑动
		case 136://反向黄底七色跑动
		
			iotalink_color_transition(8,1,&default_mode[70] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;

		case 137://正向青底七色跑动
		case 138://反向青底七色跑动
			iotalink_color_transition(8,1,&default_mode[71] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;		
		case 139://正向紫底七色跑动
		case 140://反向紫底七色跑动
			iotalink_color_transition(8,1,&default_mode[72] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 141://正向白底七色跑动
		case 142://反向白底七色跑
			iotalink_color_transition(8,1,&default_mode[73] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;

//---------------------------------------2026年3月22日	
		case 143://正向白红白流动
		case 144://反向白红白流动
			iotalink_color_transition(2,1,&default_mode[74] , &MAGIC_SCENE_DATA);//解析出颜色
			
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);

			
			break;
		case 145://正向白绿白流动
		case 146://反向白绿白流动
			iotalink_color_transition(2,1,&default_mode[75] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);


			break;
		case 147://正向白蓝白流动
		case 148://反向白蓝白流动
			iotalink_color_transition(2,1,&default_mode[76] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);


			break;
		case 149://正向白黄白流动
		case 150://反向白黄白流动
			iotalink_color_transition(2,1,&default_mode[77] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);

			break;
		case 151://正向白青白流动
		case 152://反向白青白流动
			iotalink_color_transition(2,1,&default_mode[78] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);

			break;
		case 153://正向白紫白流动
		case 154://反向白紫白流动
			iotalink_color_transition(2,1,&default_mode[79] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);


			break;
		case 155://正向红白红流动
		case 156://反向红白红流动
			iotalink_color_transition(2,1,&default_mode[80] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);

			break;
		case 157://正向绿白绿流动
		case 158://反向绿白绿流动
			iotalink_color_transition(2,1,&default_mode[81] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);

			break;
		case 159://正向蓝白蓝流动
		case 160://反向蓝白蓝流动
			iotalink_color_transition(2,1,&default_mode[82] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);

			break;
		case 161://正向黄白黄流动
		case 162://反向黄白黄流动
			iotalink_color_transition(2,1,&default_mode[83] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);

			break;
		case 163://正向青白青流动
		case 164://反向青白青流动
			iotalink_color_transition(2,1,&default_mode[84] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);


			break;
		case 165://正向紫白紫流动
		case 166://反向紫白紫流动
			iotalink_color_transition(2,1,&default_mode[85] , &MAGIC_SCENE_DATA);//解析出颜色
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(3,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			rgb_colorful_buffer_set(1,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);		
			rgb_colorful_buffer_set(2,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);
			break;
//--------------------------------------------------------------------------------------------------------			
		case 167://正向蓝底绿点跑动
		case 168://反向蓝底绿点跑动

			iotalink_color_transition(2,1,&default_mode[86] , &MAGIC_SCENE_DATA);//解析出颜色
			
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);
			for (int i= 1; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}
			break;
		case 169://正向红底绿点跑动
		case 170://反向红底绿点跑动
			iotalink_color_transition(2,1,&default_mode[87] , &MAGIC_SCENE_DATA);//解析出颜色
			
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);
			for (int i= 1; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}

			break;
		case 171://正向蓝底红点跑动
		case 172://反向蓝底红点跑动
			iotalink_color_transition(2,1,&default_mode[88] , &MAGIC_SCENE_DATA);//解析出颜色
			
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);
			for (int i= 1; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}

			break;
		case 173://正向黄底青点跑动
		case 174://反向黄底青点跑动
			iotalink_color_transition(2,1,&default_mode[89] , &MAGIC_SCENE_DATA);//解析出颜色
			
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);
			for (int i= 1; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}

			break;
		case 175://正向紫底黄点跑动
		case 176://反向紫底黄点跑动
			iotalink_color_transition(2,1,&default_mode[90] , &MAGIC_SCENE_DATA);//解析出颜色
			
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);
			for (int i= 1; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}

			break;
		case 177://正向黄底白点跑动
		case 178://反向黄底白点跑动
			iotalink_color_transition(2,1,&default_mode[91] , &MAGIC_SCENE_DATA);//解析出颜色
			
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);
			for (int i= 1; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}

			break;
		case 179://正向白底黄点跑动
		case 180://反向白底黄点跑动
			iotalink_color_transition(2,1,&default_mode[92] , &MAGIC_SCENE_DATA);//解析出颜色
			
			rgb_colorful_buffer_set(0,MAGIC_SCENE_DATA.magic_rgb[1].r,MAGIC_SCENE_DATA.magic_rgb[1].g,MAGIC_SCENE_DATA.magic_rgb[1].b);
			for (int i= 1; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r/5,MAGIC_SCENE_DATA.magic_rgb[0].g/5,MAGIC_SCENE_DATA.magic_rgb[0].b/5);
			}

			break;
//--------------------------------------------------------------------------------------------------------	

		case 181://正向七彩刷色
		case 182://反向七彩刷色
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			}

			break;
		case 183://正向红绿蓝刷色
		case 184://反向红绿蓝刷色
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			}
			break;
		case 185://正向黄青紫刷色
		case 186://反向黄青紫刷色
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色	
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			}
			break;
//--------------------------------------------------------------------------------------------------------	

		case 187://七彩刷色闭幕
		case 188://七彩刷色拉幕
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			}

		
			break;
		case 189://红绿蓝刷色闭幕
		case 190://红绿蓝刷色拉幕
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			}
			break;

		case 191://黄青紫刷色闭幕
		case 192://黄青紫刷色拉幕
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色	
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
			}
			break;
//--------------------------------------------------------------------------------------------------------	

		case 193://七色跳变
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 194://红绿蓝跳变
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 195://黄青紫跳变
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色	
			break;
		case 196://七色频闪
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 197://红绿蓝频闪
			iotalink_color_transition(3,1,&default_mode[6] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 198://黄青紫频闪
			iotalink_color_transition(3,1,&default_mode[8] , &MAGIC_SCENE_DATA);//解析出颜色	
			break;
		case 199://七色渐变
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 200://红黄交替渐变
			iotalink_color_transition(2,1,&default_mode[93] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 201://红紫交替渐变
			iotalink_color_transition(2,1,&default_mode[94] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 202://绿青交替渐变
			iotalink_color_transition(2,1,&default_mode[95] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 203://绿黄交替渐变
			iotalink_color_transition(2,1,&default_mode[96] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 204://蓝紫交替渐变
			iotalink_color_transition(2,1,&default_mode[97] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
//--------------------------------------------------------------------------------------------------------			
		case 205://红色跑马
			iotalink_color_transition(1,1,&default_mode[67] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				if(i%2)
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
				else rgb_colorful_buffer_set(i,0,0,0);
			}
			break;
		case 206://绿色跑马
			iotalink_color_transition(1,1,&default_mode[68] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				if(i%2)
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
				else rgb_colorful_buffer_set(i,0,0,0);
			}

			break;
		case 207://蓝色跑马
			iotalink_color_transition(1,1,&default_mode[69] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				if(i%2)
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
				else rgb_colorful_buffer_set(i,0,0,0);
			}
			break;
		case 208://黄色跑马
			iotalink_color_transition(1,1,&default_mode[70] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				if(i%2)
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
				else rgb_colorful_buffer_set(i,0,0,0);
			}
			break;
		case 209://青色跑马
			iotalink_color_transition(1,1,&default_mode[71] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				if(i%2)
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
				else rgb_colorful_buffer_set(i,0,0,0);
			}

			break;
		case 210://紫色跑马
			iotalink_color_transition(1,1,&default_mode[72] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				if(i%2)
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
				else rgb_colorful_buffer_set(i,0,0,0);
			}

			break;
		case 211://白色跑马
			iotalink_color_transition(1,1,&default_mode[73] , &MAGIC_SCENE_DATA);//解析出颜色
			for (int i= 0; i<RGB_LED_NUM; i++)
			{
				if(i%2)
				rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[0].r,MAGIC_SCENE_DATA.magic_rgb[0].g,MAGIC_SCENE_DATA.magic_rgb[0].b);
				else rgb_colorful_buffer_set(i,0,0,0);
			}

			break;
		case 212://七彩能量
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 213://特别七色25点开幕
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 214://红橙暗亮暗过渡流水		
			iotalink_color_transition(1,1,&default_mode[99] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 215://黄绿暗亮暗过渡流水
			iotalink_color_transition(1,1,&default_mode[70] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 216://绿色暗亮暗过渡流水
			iotalink_color_transition(1,1,&default_mode[68] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 217://青蓝暗亮暗过渡流水
			iotalink_color_transition(1,1,&default_mode[98] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 218://蓝色暗亮暗过渡流水
			iotalink_color_transition(1,1,&default_mode[71] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 219://紫色暗亮暗过渡流水
			iotalink_color_transition(1,1,&default_mode[72] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 220://红色暗亮暗过渡流水
			iotalink_color_transition(1,1,&default_mode[67] , &MAGIC_SCENE_DATA);//解析出颜色	
			break;
		case 221://七色堆砌
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 222://橙色堆砌
			iotalink_color_transition(1,1,&default_mode[99] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 223://黄绿堆砌
			iotalink_color_transition(1,1,&default_mode[70] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 224://绿色堆砌
			iotalink_color_transition(1,1,&default_mode[68] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 225://青蓝堆砌
			iotalink_color_transition(1,1,&default_mode[98] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 226://蓝色堆砌
			iotalink_color_transition(1,1,&default_mode[71] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 227://紫色堆砌
			iotalink_color_transition(1,1,&default_mode[72] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 228://红色堆砌
			iotalink_color_transition(1,1,&default_mode[67] , &MAGIC_SCENE_DATA);//解析出颜色	
			break;
		case 229://七彩渐变
		case 230://七彩过渡
			iotalink_color_transition(7,1,&default_mode[24] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 231://红紫过渡
			iotalink_color_transition(2,1,&default_mode[94] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 232://黄白过渡
			iotalink_color_transition(2,1,&default_mode[92] , &MAGIC_SCENE_DATA);//解析出颜色
			break;
		case 233://黄橙过渡
			iotalink_color_transition(2,1,&default_mode[100] , &MAGIC_SCENE_DATA);//解析出颜色
			break;

	}

}

#endif
/*
红 (Red)	0° / 360°
橙 (Orange)	30°
黄 (Yellow)	60°
绿 (Green)	120°
青 (Cyan)	180°
蓝 (Blue)	240°
紫 (Violet)	300 1,45°
*/
unsigned char  default_scene[33+1][38]=
{
	{2,0,0,100,0,255,255,0,100,1,104,100,0,0,132,100,0,255,255,0,100,0,0,100,0,0,139,100,0},
	{2,1,0,100,0,0,0,100,0,0,0,0,0,0,0,100,0,0,0,0,0,0,0,100,0,0,0,0,0},//1 -》
	{2,2,0,100,0,0,0,100,0,0,15,100,0,0,40,100,0,0,0,100,0,0,15,100,0,0,40,100,0},//2
	
	{4,3,2,100,2, 0,0,100,0, 0,0,100,0 ,0,45,100,0 ,0,45,100,0,0,0,100,0, 0,0,100,0},//3
	
	{4,4,2,100,100,0,45,100,0,0,120,100,0,0,180,100,0,0,240,100,0,1,24,100,0,0,0,100,0},//4
	{5,5,0,100, 0,0,117,100, 0,0,0,100, 0,1,49,100, 0,0,240,100,0},//music 用 5
	{2,6,2,100,0,0,38,15,0,0,83,90,0,0,65,93,0,0,48,100,0,0,0,100,0,0,0,100,0},// 火焰底色用 6
	{4,7,2,100,50,0,3,100,0,0,23,100,0,0,5,100,0,0,20,100,0},//火焰底色用 4-      7 test
	
	{4,8,2,100,50,0,0,100,0,0,120,100,0,0,120,100,0,0,240,100,0,0,240,100,0,0,0,100,0,0,0,100,0,0,0,100,0},
	{4,9,2,100,50,0},
	{4,10,2,100,50},
	
	{0,11,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	
	{4,12,2,100,50,0,226,100,0},
	
	{4,13,0,0,0},
	{4,14,0,0,0},
//睡眠
	{4,15,2,32, 10,  0,41,100,0},
	
//	{4,16,2,100,34, 0,243,100,100,    0,243,100,5,   0,243,100,0,   0,243,100,5},
	{4,16,2,100,34, 0,243,100,0 ,0,0,0,0,0,243,100,0 ,0,0,0,0},
	{4,17,2,100,50, 0,122,100,0 ,0,0,0,0,0,122,100,0,0,0,0,0},
	
	{4,18,2,81,34, 0,0,0,0},
	{4,19,2,100,34, 0,0,0,0},
	
	{1,20,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	
	{0,21,2,100,50, 0,0,100,0, 0,30,100,0, 0,60,100,0, 0,120,100,0, 0,180,100,0, 0,140,100,0 ,1,45,100,0},
	
	{4,22,0,0,0},
	{4,23,0,0,0},
	{4,24,0,0,0},
	{4,25,0,0,0},

	{4,26,2,100,50,1,62,100,0,1,37,100,0,0,44,100,0,0,51,100,0},
	
	{4,27,0,0,0},

	{0,28,2, 100,50, 0,240,100,0,0,0,100,0,0,120,0,0,0,240,100,0,0,0,100,0,0,120,0,0},

	{0,29,2,100,100,0,0,100,0,0,120,100,0,0,240,100,0},///29 30用

};

/*>>>>>>> 载入静态数据，不需要再循环体一直写入数据在这里写入 <<<<<<<*/
/*>>>>>>>	 		幻彩情景静态数据初始化 							 <<<<<<<*/
 void iotalink_magic_process_init (void )
 {

 
	MAGIC_SCENE_DATA.magic_hsv[0].sv = 100;
	printf("iotalink_magic_process_init \n");
	// 设置场景重置标记
	scene_reset_flag = true;
	//rgb_colorful_buffer_clean();
	//rgb_value_sync();//输出


	if(sg_light_ctrl_data.mode ==CUSTOME_MODE)
	{
			//场景

		 haoyida_234_mode_init(sg_light_ctrl_data.custome_unit);
			
	}
	else 
	{

	 	//场景
		 switch (sg_light_ctrl_data.magicunit)
	    {
			case 1:
			case 2:	
			
				break;

			case 3:
				// iotalink_color_transition(6,5,&default_scene[4] , &MAGIC_SCENE_DATA);//解析出颜色
				// for (int i= 0; i<MAGIC_SCENE_DATA.magicallcnt; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
				// {
				// 	rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);
				// }

			break;

			case 4://烛光
			
				break;
			case 5: // 烟花：七色 + 无色随机显示
				break;
			case 6: // 聚会：七色随机频闪
				break;
			case 7: // 约会：红粉紫过渡渐变（每到渐变到一种颜色时，闪一下）
				break;
			case 8: // 星空：青蓝流动
				break;
			case 9: // 浪漫：红粉紫随机显示（三个点一个颜色）
				break;
			case 10: // 迪斯科：七色随机显示（一个点一个颜色），全部点数都显示
				break;
			 case 11:
				
				break;	
			case 12: // 电影：青蓝色静态
				iotalink_color_transition(1,1,&default_scene[12] , &MAGIC_SCENE_DATA);//解析出颜色
			 	//iotalink_magic_scene_static();//写入颜色
				break;
			case 13: // 圣诞夜：七色随机显示 + 无色随机显示（三个点一个颜色）3秒钟，再向上跑动3秒，向下跑动3秒时间

				colorful_paoma_init();
				break;
			case 14: // 流水：白色流动
				break;
			case 15: // 睡眠：RGB熄灭，PWM灯亮
			{
				// RGB灯熄灭
				rgb_colorful_buffer_clean();
				rgb_value_sync();
				// PWM灯亮起（20%占空比）
				wlt_led_pwm_set_duty(LED_PWM_CH_0, 20);
			}
				break;
			case 16: // 海洋：青蓝色流动 待优化颜色
			
				iotalink_color_transition(4,5,&default_scene[16] , &MAGIC_SCENE_DATA);//解析出颜色
				//iotalink_magic_scene_static();//写入颜色
				for (int i= 0; i<MAGIC_SCENE_DATA.magicallcnt; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
				{
					rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);
				}

			
				break;
			case 17: // 森林：绿黄色流动
			
				iotalink_color_transition(4,5,&default_scene[17] , &MAGIC_SCENE_DATA);//解析出颜色
			 ///	iotalink_magic_scene_static();//写入颜色
				for (int i= 0; i<MAGIC_SCENE_DATA.magicallcnt; i++)//颜色组:MAGIC_SCENE_DATA.magicallcnt（过渡色在解析里加）
				{
					rgb_colorful_buffer_set(i,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);
				}
				break;
			case 18: // 阅读：冷色或RGB白色80%亮度
				iotalink_color_transition(1,1,&default_scene[18] , &MAGIC_SCENE_DATA);//解析出颜色
				iotalink_magic_scene_static();//写入颜色

			
				break;
			case 19: // 工作：冷色或RGB白色100%亮度
				iotalink_color_transition(1,1,&default_scene[19] , &MAGIC_SCENE_DATA);//解析出颜色
				iotalink_magic_scene_static();//写入颜色

				break;
			case 20: // 炫彩：七彩流动 不过渡
				iotalink_color_transition(7,2,&default_scene[20] , &MAGIC_SCENE_DATA);//解析出颜色
				//iotalink_magic_scene_static();//写入颜色
				break;

				
			case 21: // 柔和：七彩渐变过渡（亮度100%）

				iotalink_color_transition(7,1,&default_scene[21] , &MAGIC_SCENE_DATA);//解析出颜色

				break;
			
			case 22: // 结婚纪念日：红粉紫随机跳变
				break;
			case 23: // 雪花：白色+无色随机显示（一个点一个颜色），白色到无色需要渐变过程
				break;
			case 24: // 火焰：顶部显示偏黄色，底部显示偏红色，中间是两种颜色的渐变过程，随机显示颜色的占比

				break;
			case 25: // 闪电：白色闪屏（白色显示时间和闪屏速度随机）
			
				break;
			case 26: // 情人节：红粉紫随机流动
				// iotalink_color_transition(3,7,&default_scene[26] , &MAGIC_SCENE_DATA);//解析出颜色
				// iotalink_magic_scene_static();//写入颜色
				break;

			case 27: // 万圣节：橙色渐变
				break;

			case 28: // 报警：白色+蓝牙+无色 随机显示（三个点一个颜色）
				iotalink_color_transition(6,1,&default_scene[28] , &MAGIC_SCENE_DATA);//解析出颜色
				iotalink_magic_scene_static();//写入颜色
				break;


			// case 29: // 时光机：正向蓝白色渐变流动
			// case 30: // 时光机2：反向蓝白色渐变流动
			// 		iotalink_color_transition(3,1,&default_scene[29] , &MAGIC_SCENE_DATA);//解析出颜色
					
			// 	break;
			// case 31: // 流星：正向蓝底白色流动
			// case 32: // 流星2：反向蓝底白色流动
			// 	colorful_meteor_init();

			// 	break;
			// case 33: // 烟花秀：七彩流动（七彩颜色+无色随机发送），三个点一个颜色，中间在隔一个无色
		
			// 	break;

			 break;
			 case 100://内置
				 iotalink_color_transition(6,5,&default_scene[3] , &MAGIC_SCENE_DATA);//解析出颜色
				 iotalink_magic_scene_static();//写入颜色
				 break;
			 case 101: //涡流 
				 iotalink_color_transition(6,5,&default_scene[4] , &MAGIC_SCENE_DATA);//解析出颜色
				 iotalink_magic_scene_static();//写入颜色
				 break;
			 case 102://脉搏 全彩
			 
					iotalink_mode_caihong_init();
			
				 break;

			 case 108: 
				 //火焰 19
				 
				 iotalink_color_transition(4,5,&default_scene[7] , &MAGIC_SCENE_DATA);//底色
				 rgb_colorful_buffer_clean();//清空原数据
				 
				 break;
			 case 109:
				 iotalink_color_transition(6,5,&default_scene[0] , &MAGIC_SCENE_DATA);//解析出颜色
				 iotalink_magic_scene_static();//写入颜色
				 break;
			 case 110:
				 iotalink_color_transition(6,5,&default_scene[1] , &MAGIC_SCENE_DATA);
				 iotalink_magic_scene_static();
				 break;
			 case 111:
				 iotalink_color_transition(6,5,&default_scene[2] , &MAGIC_SCENE_DATA);
				 iotalink_magic_scene_static();
				 break;
				 
				break; 
			 default://200（自定义部分）

					//好易达场景

		 		
			 break; 	 
		}	 
	}
 }

void iotalink_magic_lantern_process (void * argv)
{


	static unsigned char  i=0,num=0;

//	sg_light_ctrl_data.magicunit =3;
	while (1)
	{

		if( sg_light_ctrl_data.switch_status)
		{
			static unsigned char   i=0,num=0;

		switch(sg_light_ctrl_data.mode)
		{
			case WHITE_MODE:

				//wlt_light_set_rgbcw(100,100,0,0,0);
				wlt_light_set_rgbcw(sg_light_ctrl_data.target_val.white,sg_light_ctrl_data.target_val.warm,0,0,0);
				break;	
			
			case COLOR_MODE:
				
				//wlt_light_set_rgbcw(0,0,100,0,0);
				wlt_light_set_rgbcw(0,0,sg_light_ctrl_data.target_val.red,sg_light_ctrl_data.target_val.green,sg_light_ctrl_data.target_val.blue);

			
				break;
			case SCENE_MODE:
			//if(sg_light_ctrl_data.mode == SCENE_MODE)
			{	
				// 协同颜色亮度渐变用
				//iotalink_rgb_relax();

				switch (sg_light_ctrl_data.magicunit)//场景号
				{
					case 1://日出    过渡流动
						wlt_mode_sunup();
						break;

					case 2://日落   过渡流动
						wlt_mode_sundown();	
						break;

					case 3://生日   
						wlt_birthday();
						// Move_Mun_Pre(30);
						// rgb_value_sync(); 
					    // wlt_ms_delay(magic_rate*0.35+2);
						break;

					case 4://烛光
						wlt_candlelight();
						wlt_ms_delay(magic_rate*0.35+2);
						break;

					case 5: // 烟花
						colorful_firework();//*
						break;

					case 6: // 聚会：七色随机频闪
						wlt_juhui();
						break;

					case 7: // 约会：红粉紫过渡渐变
						wlt_yuehui();
						break;

					case 8: // 星空：随机闪
						star_twinkle();//*
						break;

					case 9: // 浪漫：红粉紫随机显示（三个点一个颜色）
						wlt_romance();
						break;

					case 10: // 迪斯科：
						wlt_disco_mode();//*原花落
						break;

					case 11: // 彩虹：
						colorful_rainbow_run();
						wlt_ms_delay(magic_rate*1+20);
						break;

					case 12: // 电影：青蓝色静态oo
						singleColor(226,100,50);
						rgb_value_sync();
						wlt_ms_delay(100);				
						break;

					case 13: // 圣诞夜：七色随机显示 + 无色随机显示（三个点一个颜色）3秒钟，再向上跑动3秒，向下跑动3秒时间
						wlt_Christmas_mode();
						// Move_Pre();
						// rgb_value_sync(); 
						// wlt_ms_delay(magic_rate*1+20);
						break;

					case 14: // 流水：白色流动
						wlt_runningwater_mode();
						//colorful_test2_2();
						break;

				case 15: // 睡眠：RGB熄灭，PWM灯亮
				{
					// RGB灯熄灭
					rgb_colorful_buffer_clean();
					rgb_value_sync();
					// 睡眠灯：使用低占空比PWM（10%），适合睡眠
					// 只设置一次PWM，避免频繁重启timer导致闪烁
					wlt_led_pwm_set_duty(LED_PWM_CH_0, 10);
				}
					break;
						
					case 16: // 海洋：青蓝色流动 
						Move_Mun_Back(20);
						rgb_value_sync(); 
						wlt_ms_delay(magic_rate*0.5+8);		
						break;

					case 17: // 森林：绿黄色流动
						Move_Mun_Back(20);
						rgb_value_sync();
						wlt_ms_delay(magic_rate*0.3 + 5);
						break;

					case 18: // 阅读：冷色或RGB白色80%亮度 -
						singleColor(0,0,60);
						rgb_value_sync();
						wlt_ms_delay(100);
						break;

					case 19: // 工作：冷色或RGB白色100%亮度 - 
						singleColor(0,0,100);
						rgb_value_sync();
						wlt_ms_delay(100);
						break;

					case 20: // 炫彩：七彩随机流动
						// for (int i= 0; i<MAGIC_SCENE_DATA.magicallcnt; i++)//颜色组
						// {
						// 	if (LOCAL_MAGIC_MODE==0||sg_light_ctrl_data.magicunit!=20)break;//>>!!!解决延时造成的无法及时切换
						// 	index_rgb_Move_Back(0, RGB_LED_NUM, magic_rgb_relax[i].r, magic_rgb_relax[i].g, magic_rgb_relax[i].b);
						// 	iotalink_rgb_relax_1(30);
						// }
						wlt_Colorful_mode();
						break;

					case 21: // 柔和：七彩渐变过渡（亮度100%）
						// iotalink_magic_scene_relax(1,MAGIC_SCENE_BREATHE);
						// wlt_ms_delay(magic_rate+2);	
						wlt_gentle_mode();
						break;

					case 22: // 结婚纪念日：红粉紫随机跳变
						wlt_wedding_mode();
						//wlt_ms_delay(magic_rate*10+100);	
						break;

					case 23: // 雪花：白色+无色随机显示（一个点一个颜色），白色到无色需要渐变过程
						wlt_snowflakes_mode();//*
						break;

					case 24: // 火焰：，中间是两种颜色的渐变过程，随机显示颜色的占比
						wlt_fire_mode();//*
						break;

					case 25: // 闪电：白色闪屏（白色显示时间和闪屏速度随机） 用的炫龙
						wlt_lightning_mode();
						break;

					case 26: // 情人节：红粉紫随机流动
						// iotalink_magic_scene_relax(2,MAGIC_SCENE_BREATHE);
						// wlt_ms_delay(magic_rate+2); 
						wlt_valentine_mode();
						break;

					case 27: // 万圣节：（粉黄收缩）
						wlt_hallowmas_mode();
						break;

					case 28: // 报警：白色+蓝牙+无色 随机显示（三个点一个颜色）
						wlt_110_mode();
						break;

					// case 29: // 时光机：正向蓝白色渐变流动
					// //todo
					// 	wlt_29_mode();
					// 	break;
					// case 30: // 时光机2：反向蓝白色渐变流动
					// //todo
					// 	wlt_30_mode();
					// 	break;
					// case 31: // 流星：正向蓝底白色流动 
					// 	Move_Back();
					// 	rgb_value_sync();
					// 	wlt_ms_delay(magic_rate*2);
					// 	break;
					// case 32: // 流星2：反向蓝底白色流动

					// 	Move_Pre();
					// 	rgb_value_sync();
					// 	wlt_ms_delay(magic_rate*2);
					// 	break;

					// case 33: // 烟花秀：七彩流动（七彩颜上升尾部逐渐变暗)
					
					// 	wlt_33_mode();
						
					// 	break;

					case 29:
					case 150:  //新增时光机1：正向蓝白色渐变流动，增加过渡色，优化颜色
						wlt_timemachine1_mode();
						break;

					case 30:
					case 152:  //新增时光机2：
						wlt_timemachine2_mode();
						break;

					case 31:
					case 154:  //新增流星1：
						wlt_meteor1_mode();
						break;

					case 32:
					case 155:  //新增流星2：
						wlt_meteor2_mode();
						break;

					case 33:
					case 156:  //新增烟花秀：
						wlt_fireworkshow_mode();
						break;



//-----------------------------------------------------------------------------------------------------------/						
					case 100:	
					case 101:
					//	colorful_test4();
						Move_Pre();
						rgb_value_sync(); 
					    wlt_ms_delay(magic_rate*0.35+2);
						
						break;			
					case 102://彩虹	
					
						iotalink_mode_caihong_sync();
						wlt_ms_delay(magic_rate*0.35+2);
					
						break;					


					case 103:// 	
	
						break;

				
					case 109://节日
					case 110://薄荷糖
					case 111://万圣节 	
						Move_Pre();
						rgb_value_sync(); 
						wlt_ms_delay(magic_rate*0.5+8);
						break;


					case 112://DISCO
					
						break;


						
						}
					}
				break;
				

			case CUSTOME_MODE:
			{

				haoyida_234_mode_deal(sg_light_ctrl_data.custome_unit);
				
			}
			break;
			case  MUSIC_MODE: //5
			{

	          //  switch (local_music_mode)
	        	switch (sg_light_ctrl_data.musicunit-1)
	            {     
		            case 0:
						
		                iotalink_music_process_0_0();
		                break;
		            case 1:
						
						 iotalink_music_process_0_1();			
				
		                break;		         
					case 2:
								
						iotalink_music_process_0_2();
		                break;
						
		            case 3:
		                iotalink_music_process_1();
		                break;
		            case 4:
		                iotalink_music_process_2();
		                break;
		            case 5:
						iotalink_music_process_3();
		                break;
		            case 6:
		                iotalink_music_process_2();
		                break;
		            case 7:
						iotalink_music_process_3();
		                break;	
					default:
						break;
			
          	 	 }		

			 }
			break;

			default :
				break;

			
			}
		}
		else 
		{

			rgb_colorful_buffer_clean();
			rgb_value_sync();
			wlt_led_pwm_set_duty(0,0); 
			wlt_ms_delay(100);

		}
		osDelay(MS_TO_TICKS(50));
	}
}


//int iotalink_magic_init(light_color_mode_e light_color_mode)
int iotalink_magic_init(void)
{
	u8 ret =0;
		//ws_light_color_mode = light_color_mode;
	//	 xTaskCreate(iotalink_magic_lantern_process, "led_task", 1024*2, NULL, configMAX_PRIORITIES-1,, NULL);//configMAX_PRIORITIES 8

		// scm_adc_reset();
		
	osThreadAttr_t attr = {
		.name 		= "iotalink_magic_task",
		.stack_size = 1024 * 4,
		.priority 	= osPriorityNormal,    //osPriorityNormal  = 24, osPriorityRealtime = 48,      ,//osPriorityLowcon
	};
	/* run the demo in a new thread to allow further CLI */
	if (osThreadNew(iotalink_magic_lantern_process, NULL, &attr) == NULL) 
	{
		printf("Ayla application start failed\n");
	}
	return ret;
}



#define CR_MAX(x,y,z) x > y ? (x > z ? x : z) : (y > z ? y : z) 
#define CR_MIN(x,y,z) x < y ? (x < z ? x : z) : (y < z ? y : z)

//RGB 转HSV 
void rgb_to_hsv(unsigned char R, unsigned char G, unsigned char B, unsigned short *H, unsigned char *S, unsigned char *V)
{
    float  max = CR_MAX(R, G, B);
    float  min = CR_MIN(R, G, B);
    float  delta = 0;

    float hsv_h, hsv_s, hsv_v;
    do {


        hsv_v = max / 2.55f;

        delta = max - min;

        if (max != 0)
        {
            hsv_s = (delta * 100) / max;
        }
        else
        {
            hsv_s = 0;
            hsv_h = 0;

            break;
        }
        if (R == max)
        {
            hsv_h = (G - B) / delta;
        }
        else if (G == max)
        {
            hsv_h = 2 + (B - R) / delta;
        }
        else
        {
            hsv_h = 4 + (R - G) / delta;
        }

        hsv_h *= 60;
        if (hsv_h < 0)
        {
            hsv_h += 360;
        }
    } while (0);

    *H = (unsigned short)hsv_h;
    *S = (unsigned short)hsv_s;
    *V= (unsigned short)hsv_v;
}

/**
  * HSV（360,100,100） 转 RGB(0~255)
  * @paRam[out] R   指向RGB模型 - R的数据指针
  * @paRam[out] G   指向RGB模型 - G的数据指针
  * @paRam[out] B   指向RGB模型 - B的数据指针
  * @paRam[in]  h   HSV模型  -----------------  色调
  * @paRam[in]  s   HSV模型 ----------------饱和度
  * @paRam[in]  v   HSV模型 ----------------亮度
  */
void  hsv_to_rgb(unsigned char *R, unsigned char *G, unsigned char *B, unsigned short H, unsigned char S, unsigned char V)
{
	int i = 0;
	int rgb_max, rgb_min, rgb_adj;
	rgb_max = rgb_min = rgb_adj = 0;	
	rgb_max = 255 * V;
	rgb_min = rgb_max * (100 - S) / 100;	
	i = H / 60;
	int difs = H % 60;
	rgb_adj = (rgb_max - rgb_min) * difs / 60;	
	
	rgb_max /= 100;
	rgb_adj /= 100;
	rgb_min /= 100;	
	switch (i)
	{
	case 6:
	case 0:
		*R = (unsigned char)rgb_max;
		*G = (unsigned char)(rgb_min + rgb_adj);
		*B = (unsigned char)rgb_min;
		break;
	case 1:
		*R = (unsigned char)(rgb_max - rgb_adj);
		*G = (unsigned char)rgb_max;
		*B = (unsigned char)rgb_min;
		break;
	case 2:
		*R = (unsigned char)rgb_min;
		*G = (unsigned char)rgb_max;
		*B = (unsigned char)(rgb_min + rgb_adj);
		break;
	case 3:
		*R = (unsigned char)rgb_min;
		*G = (unsigned char)(rgb_max - rgb_adj);
		*B = (unsigned char)rgb_max;
		break;
	case 4:
		*R = (unsigned char)(rgb_min + rgb_adj);
		*G = (unsigned char)rgb_min;
		*B = (unsigned char)rgb_max;
		break;
	default: // case 5:
		*R = (unsigned char)rgb_max;
		*G = (unsigned char)rgb_min;
		*B = (unsigned char)(rgb_max - rgb_adj);
		break;
	}
}
