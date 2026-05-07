#include "iotalink.h"




// Connect GPIO pins [4, 7, 0, 1] to the corresponding ADC channel [4, 5, 6, 7]
//SCM_ADC_SINGLE_CH_4  gpio4 有问题
//SCM_ADC_SINGLE_CH_5  gpio7  0-3.3--> 0-4095


#define CONFIG_MUSIC_SAMPLING_COUNT 30 //adc采集数
int Voltages[CONFIG_MUSIC_SAMPLING_COUNT+1] = { 0 };


 #define num_max 10


unsigned char MUSIC_SENSITIVITY = 200; // 用于律动灵敏度调整


#define abs(a,b)  (a>b ? a-b : b-a )

#define  MUSIC_STATIC  240 // 静态无声音获取的adc值




static  u16  RGB_LED_NUM = 40 ;//按40个灯珠

extern bool MUSIC_LOCAL_MODE;	

//仅本地律动控制调用
static void iotalink_light_ctrl(u16 h, u16 s,u16 v )
{
	 h = h > 360  ? 360  : h;
     s = s > 100 ? 100 : s;
     v = v > 100 ? 100 : v;	 
	 singleColor(h,s,v);
	 rgb_value_sync();//RGB数值输出
}

/*-----------------------------《 音乐律动模式 》------------------------------------*/
/*>>>>>>>>>>>>  公版爵士 节奏：  <<<<<<<<<<<<*/
/*H: MAGIC_MUSIC_DATA.music_change_mode.quiet_state
* L: MAGIC_MUSIC_DATA.music_change_mode.dynamic_state
* H 0 L 3           摇滚
* H 0 L 2			爵士
* H 1 L 2 		    经典
*/	
void iotalink_music_process_0_0(void)
{
	static int hue=0 , v_value = 0;
	static int rhythm_value_avr =0,rhythm_value_sum=0;
	static char relax_flag =10,loop1=0;
	static float diff =0;
	int loop = 0,loop2=0;
	int max_voltage = 0;

	while(loop++< CONFIG_MUSIC_SAMPLING_COUNT )
	{
		if (max_voltage < Voltages[loop])
		{
			max_voltage = Voltages[loop];
		}
	} 
	my_printf ("-----max_voltage  :%d ----\n",max_voltage); 
	v_value = abs( max_voltage,MUSIC_STATIC );	//大约20~1584
	hue+=30;	
	if (hue > 359)
	{
		hue = 0;
	}
//获取均值线
		loop1++;
		rhythm_value_sum +=v_value; 		
		if (loop1==relax_flag)//每进十次刷新
		{		
			rhythm_value_avr=rhythm_value_sum/relax_flag;
			rhythm_value_sum = 0;
			loop1 = 0;
			my_printf("-----rhythm_value_avr:%d ----\n",rhythm_value_avr);
		}		

	if (v_value < rhythm_value_avr|| v_value < MUSIC_SENSITIVITY)
	{
		v_value  = 0;hue -= 30; 
		rgb_colorful_buffer_clean();
		wlt_ms_delay(50);
		rgb_value_sync();
	}
	else
	{	
		diff=(float)(v_value-rhythm_value_avr)/rhythm_value_avr;
		v_value= diff*50+50;//处理亮度
//分段三种闪亮
		/*if (num>10)
		{
			iotalink_light_ctrl(hue,1000,v_value);
			num=0;
		}
		else
		{
			for (loop2=0;loop2<10;loop2++) 
			{	
				hsv_to_rgb(&r, &g, &b, hue, 100, v_value);
				rgb_colorful_buffer_set(num+loop2 , r,	g, b ); 
				rgb_colorful_buffer_set(40-num+loop2 , r,  g, b );	
			}
			num+=10;
			
			wlt_ms_delay(50);
			rgb_value_sync();
			rgb_colorful_buffer_clean();
		}
		*/
	}	
	iotalink_light_ctrl(hue,100,v_value);
	
}
/*模式 0 公版爵士>>>>>>>>hue随ADC变化模式亮度不变<<<<<<<<<<<<*/
void iotalink_music_process_0_1(void)
{
	static unsigned int hue=0, v_value = 0;
	static int rhythm_value_avr =0,rhythm_value_sum=0;
	static char relax_flag =10,loop1=0;
	int loop = 0;
	int max_voltage = 0;
	while(loop++< CONFIG_MUSIC_SAMPLING_COUNT)
	{
		if (max_voltage < Voltages[loop])
		{
			max_voltage = Voltages[loop];
		}
	} 
	v_value = abs( max_voltage,MUSIC_STATIC );	//大约20~1584 
//获取均值线
	loop1++;
	rhythm_value_sum +=v_value; 		
	if (loop1==relax_flag)//每进十次刷新
	{		
		rhythm_value_avr=rhythm_value_sum/relax_flag;
		rhythm_value_sum = 0;
		loop1 = 0;
		//printf("-----rhythm_value_avr:%d ----\n",rhythm_value_avr);
	}
	hue+=30;
	if (v_value <MUSIC_SENSITIVITY)
	{
		hue-=30;
	}	
	else if (v_value < rhythm_value_avr)
	{
		hue -= 25;						
	}
	else
	{
	}

	if (hue > 359)
	{
		hue = 0;
	}
	wlt_ms_delay(5);	
	iotalink_light_ctrl(hue,100,100);
}


/*模式0-2>>>>>>>>>>>>   公版经典 亮度(采集值)平滑渐变模式(relax mode  )	  <<<<<<<<<<<<*/
void iotalink_music_process_0_2(void)
{
	static unsigned int hue=0 , v_value = 0,v_value_back=0;
	static int rhythm_value_avr =0,rhythm_value_sum=0;
	static char relax_flag =10,loop1=0;
	static float diff =0;
	int loop = 0,loop2;
	unsigned char step=5;
	int max_voltage = 0;
	while(loop++< CONFIG_MUSIC_SAMPLING_COUNT )
	{
		if (max_voltage < Voltages[loop])
		{
			max_voltage = Voltages[loop];
		}
	} 
	my_printf("-----max_voltage0:%d ----\n",max_voltage);
	
	v_value = abs( max_voltage,MUSIC_STATIC );	//大约20~1584
	hue+=30;
//获取均值线
		loop1++;
		rhythm_value_sum +=v_value; 		
		if (loop1==relax_flag)//每进十次刷新
		{		
			rhythm_value_avr=rhythm_value_sum/relax_flag;
			rhythm_value_sum = 0;
			loop1 = 0;
		}				
/********************>>>数据配置<<<***********************************/ 	
	if (v_value < rhythm_value_avr|| v_value <MUSIC_SENSITIVITY)
	{	
		v_value  = 5;hue -= 30;			
	}
	else if (v_value > rhythm_value_avr)
	{
		diff=(float)(v_value-rhythm_value_avr)/rhythm_value_avr;
		v_value= diff*50+50;//处理亮度	
		v_value=v_value>100? 100 : v_value;

	}	
	if (hue > 359)
	{
		hue = 0;
	}
/***************>>>  渐变处理	默认<<<  *********************/	
	//printf("-----v_value	:%d ----\n",v_value);	
	if (v_value_back>v_value)//亮度渐弱
	{
		//step=(v_value_back-v_value)/10;
		for (loop2=v_value_back;loop2>v_value;loop2-=step)
		{
			if (MUSIC_LOCAL_MODE==0)return ;//解决切换到彩色问题
			iotalink_light_ctrl(hue,100,loop2);
			wlt_ms_delay(20);
		}
	}
	else	//亮度渐强
	{
		for (loop2=v_value_back;loop2<v_value;loop2+=step)
		{
			if (MUSIC_LOCAL_MODE==0)return ;//解决切换到彩色问题
			iotalink_light_ctrl(hue,100,loop2);
			wlt_ms_delay(10);			
		}
	}	
	v_value_back=v_value;
}


/*******************音浪*******************/
 void iotalink_music_process_1(void)
 {
	 static int v_value ;
	 static int rhythm_value_avr =20,rhythm_value_sum=0;
	 static char relax_flag =10,loop1=0;
	 static float diff =0;
	 int loop = 0,back_i=0;
	 int max_voltage = 0;
	 	 
	 while(loop++< CONFIG_MUSIC_SAMPLING_COUNT )
	 {
		 if (max_voltage < Voltages[loop])
		 {
			 max_voltage = Voltages[loop];
		 }
	 } 
	 v_value = abs(max_voltage, MUSIC_STATIC) ;//大约20~1584
 //获取均值线
		 loop1++;
		 rhythm_value_sum +=v_value;		 
		 if (loop1==relax_flag)//每进十次刷新
		 {		 
			 rhythm_value_avr=rhythm_value_sum/relax_flag;
			 rhythm_value_sum = 0;
			 loop1 = 0;
			 my_printf("-----rhythm_value_avr:%d ----\n",rhythm_value_avr);
		 }
 /***************》》焕彩数值处理《《**********************/	 
	 unsigned int i = 0,j=0;
	 //static unsigned int cnt=0;
	 static hsv_h_t sh=0;
	 hsv_s_t ss = 100;
	 hsv_v_t sv = 50;
	 static  char num =5,num_back=0; 
	 static unsigned char r, g, b;
	 if ( v_value < MUSIC_SENSITIVITY )
	 {	 
		 num =0;
	 	 rgb_colorful_buffer_clean();
	     rgb_value_sync();
	 }
	 else if (v_value < rhythm_value_avr)
	 {
		 diff=(float)(rhythm_value_avr-v_value)/rhythm_value_avr;
		 num=  7- diff*8;  //处理num	 
	 }	 
	 else 
	 {
		 diff=(float)(v_value-rhythm_value_avr)/rhythm_value_avr;
		 num= diff*8 + 8;//处理num	 
	 } 
 //num处理
	 if (num<=0)num =0;
	 num= num > num_max ? num_max : num  ;	//原 10	
	sh+=60;
	if(sh>=360)sh=0;
	hsv_to_rgb(&r, &g, &b, sh, 100, sv);
	 if (num<num_back)	//音量下降
	 {
		 while(num<num_back)
		 { 	
			 rgb_colorful_buffer_clean();
 			
			 for (i =0 ; i<num_back ; i++)
			 {	 
				 for (j=0;j<RGB_LED_NUM/num_max;j++)//分N段: RGB_LED_NUM/10===每段最少10
				 {	 
					 if(MUSIC_LOCAL_MODE==0)return;//小概率切换彩色不完整
					// hsv_to_rgb(&r, &g, &b, sh[j], ss, sv);

					 rgb_colorful_buffer_set(i+num_max*j , r,  g, b );
				 }				 
				
	
			 } rgb_value_sync();
 			wlt_ms_delay(1);
			 num_back--;
		 }
	 }
	 else	//音量上升
	 {	 
		 while(num>num_back)
		 {
			 rgb_colorful_buffer_clean();//每变一次清零RGB数组
			 
			 for (i =0; i<num_back; i++)
			 {	 
				 for (j=0;j<RGB_LED_NUM/num_max;j++)//默认分5段
				 {
				 	if(MUSIC_LOCAL_MODE==0)return;//小概率切换彩色不完整
					rgb_colorful_buffer_set(i+num_max*j , r,  g, b );
				 }				 
					 
			 }  rgb_value_sync();
			 wlt_ms_delay(10);	
			 num_back++;
		 }
	 }
	 num_back = num ;
 }
/*模式2>>>>>>>>>>>>能量模式<<<<<<<<<<<<*/
void iotalink_music_process_2(void)
{
    static unsigned int  v_value =0;
	static int rhythm_value_avr =0,rhythm_value_sum=0;
	static char relax_flag =10,loop1=0;	
    int loop = 0,loop2;
	static unsigned char flag =0;
    int max_voltage = 0;
    while(loop++< CONFIG_MUSIC_SAMPLING_COUNT )
    {
        if (max_voltage < Voltages[loop])
        {
            max_voltage = Voltages[loop];
        }
    } 
	v_value = abs(max_voltage, MUSIC_STATIC); 
	my_printf ("pre  : %d\n",v_value);
	
	int i = 0;
	static unsigned char  j=0;
	//hsv_h_t sh[] = {0,200,120,36,243};//五段颜色
	hsv_h_t sh[] = {0,30,150,200,240};
	hsv_s_t ss = 100;
	hsv_v_t sv = 100;
	unsigned char r, g, b;
//获取均值线
		loop1++;
		rhythm_value_sum +=v_value;			
		if (loop1==relax_flag)//每进十次刷新
		{		
			rhythm_value_avr=rhythm_value_sum/relax_flag;
			rhythm_value_sum = 0;
			loop1 = 0;
			my_printf("-----process_2_avr:%d ----\n",rhythm_value_avr);
		}				
/********************>>>数据配置<<<***********************************/	

	if (v_value <MUSIC_SENSITIVITY)
	{
		rgb_colorful_buffer_clean();
		rgb_value_sync();
	}
	else if (v_value < rhythm_value_avr)
	{
		flag++;
		if (flag>1)	
		{		
			singleColor(sh[j],100, 100);//先全亮一下
			rgb_value_sync();
			wlt_ms_delay(10);
			rgb_colorful_buffer_clean();
			rgb_value_sync();
		}
		else
		{	

			static  u16  RGB_LED_NUM = 20 ;//按20个灯珠 ==>匹配单10
			
			for (i=RGB_LED_NUM/4;i>=0;i--)//两两对撞(4段)12
			{
				rgb_colorful_buffer_set( i  , 0,  0, 0);			
				rgb_colorful_buffer_set((RGB_LED_NUM/2)-i, 0,  0, 0);
				rgb_colorful_buffer_set( i+(RGB_LED_NUM/2) , 0,  0, 0);			
				rgb_colorful_buffer_set(RGB_LED_NUM-i, 0,  0, 0);
				rgb_value_sync();
				wlt_ms_delay(20-i);
			//	if(sg_light_ctrl_data.switch_status==0||MUSIC_LOCAL_MODE==0)//关灯切换
			
				if(MUSIC_LOCAL_MODE==0)//关灯切换
				{
					return;
				}
			}
		}

		my_printf ("small : %d\n",v_value);
		
	}
	else  
	{
		flag = 0 ;
		for (i=0;i<=RGB_LED_NUM/4;i++)//两两对撞
		{
			hsv_to_rgb(&r, &g, &b, sh[j], ss, sv);
			rgb_colorful_buffer_set( i	, r,  g, b);			
			rgb_colorful_buffer_set((RGB_LED_NUM/2)-i, r,  g, b);
			rgb_colorful_buffer_set( i+(RGB_LED_NUM/2) , r,  g, b); 		
			rgb_colorful_buffer_set(RGB_LED_NUM-i, r,  g, b);
			rgb_value_sync();
			wlt_ms_delay(20-i);
			
			 if(MUSIC_LOCAL_MODE==0)//关灯切换
			 {
				 return;
			 }
		}
		j++;
		my_printf ("big : %d\n",v_value);
	}	
//颜色处理
	if (j>=5) j =0;
}


/*************》》》模式0： 彩虹《《《****************/
 void iotalink_music_process_3(void)
 {
	 static int v_value ;
	 static int rhythm_value_avr =20,rhythm_value_sum=0;
	 static char relax_flag =10,loop1=0;
	 static float diff =0;
	 int loop = 0,back_i=0;
	 int max_voltage = 0;
	 while(loop++< CONFIG_MUSIC_SAMPLING_COUNT )
	 {
		 if (max_voltage < Voltages[loop])
		 {
			 max_voltage = Voltages[loop];
		 }
	 } 
	 v_value = abs(max_voltage, MUSIC_STATIC) ;//大约20~1584
 //获取均值线
	 loop1++;
	 rhythm_value_sum +=v_value;		 
	 if (loop1==relax_flag)//每进十次刷新
	 {		 
		 rhythm_value_avr=rhythm_value_sum/relax_flag;
		 rhythm_value_sum = 0;
		 loop1 = 0;
		 my_printf("-----rhythm_value_avr:%d ----\n",rhythm_value_avr);
	 }
 /***************》》焕彩数值处理《《**********************/	 
	 unsigned int i = 0,j=0;
	 //hsv_h_t sh[] = {0,200,120,36,243};//五段颜色
	 hsv_h_t sh[] = {0,30,150,200,240};
	 hsv_s_t ss = 100;
	 hsv_v_t sv = 100;
	 static  char num =5,num_back=0; 
	 unsigned char r, g, b;
		  
 
	 if ( v_value < MUSIC_SENSITIVITY )
	 {	 
		 num =0;
	 	 rgb_colorful_buffer_clean();
	     rgb_value_sync();
	 }
	 else if (v_value < rhythm_value_avr)
	 {
		 diff=(float)(rhythm_value_avr-v_value)/rhythm_value_avr;
		 num=  7- diff*5;  //处理num
		 
		 my_printf("--------diff:%.4f ----\n",diff);
	 }	 
	 else 
	 {
		 diff=(float)(v_value-rhythm_value_avr)/rhythm_value_avr;
		 num= diff*5 + 10;//处理num	 
		 my_printf("--------++++diff:%.4f ----\n",diff); 
	 }
	 
 //num处理
	 if (num<=0)num =0;
	 num= num > num_max ? num_max: num  ;
	 my_printf("-----num :%d ----\n",num);
	 
 
	 if (num<num_back)	//音量下降
	 {
		 while(num<num_back)
		 { 	
			 rgb_colorful_buffer_clean();
			 for (i =0 ; i<num_back ; i++)
			 {	 
				 for (j=0;j<RGB_LED_NUM/num_max;j++)//分N段: RGB_LED_NUM/10===每段最少10
				 {	 
					 if(MUSIC_LOCAL_MODE==0)return;//小概率切换彩色不完整
					
					// rgb_colorful_buffer_set(i+num_max*j , MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b); 	//前一位置亮10=RGB_LED_NUM/5
					 hsv_to_rgb(&r, &g, &b, sh[j], ss, sv);
					 rgb_colorful_buffer_set(9-i+10*j , r,  g, b );
				 }				 
				
			 } rgb_value_sync();
				 wlt_ms_delay(1); 
			 num_back--;
		 }
	 }
	 else	//音量上升
	 {	 
		 while(num>num_back)
		 {
			 rgb_colorful_buffer_clean();//每变一次清零RGB数组
			 
			 //singleColor(0,0,100);//底色
			 for (i =0; i<num_back; i++)
			 {	 
				 for (j=0;j<RGB_LED_NUM/num_max;j++)//默认分5段
				 {
				 	if(MUSIC_LOCAL_MODE==0)return;//小概率切换彩色不完整

					// 配置颜色
					// rgb_colorful_buffer_set(i+num_max*j ,MAGIC_SCENE_DATA.magic_rgb[i].r,MAGIC_SCENE_DATA.magic_rgb[i].g,MAGIC_SCENE_DATA.magic_rgb[i].b);//每段相差num_max
				
					hsv_to_rgb(&r, &g, &b, sh[j], ss, sv);
					 rgb_colorful_buffer_set(9-i+10*j , r,  g, b );//每段相差10	
				 }				 	 
			 }
			 	 rgb_value_sync();

			 num_back++;
		 }
	 }
	 num_back = num ;
 }


/*************》》》模式4：两端同时跳动 同色变 ****************/
 void iotalink_music_process_4(void)
 {
	 static int v_value ;
	 static int rhythm_value_avr =20,rhythm_value_sum=0;
	 static char relax_flag =10,loop1=0;
	 static float diff =0;
	 int loop = 0,back_i=0;
	 int max_voltage = 0;
	 u8  num_max_4 =10;
	 while(loop++< CONFIG_MUSIC_SAMPLING_COUNT )
	 {
		 if (max_voltage < Voltages[loop])
		 {
			 max_voltage = Voltages[loop];
		 }
	 } 
	 v_value = abs(max_voltage, MUSIC_STATIC) ;//大约20~1584
 //获取均值线
		 loop1++;
		 rhythm_value_sum +=v_value;		 
		 if (loop1==relax_flag)//每进十次刷新
		 {		 
			 rhythm_value_avr=rhythm_value_sum/relax_flag;
			 rhythm_value_sum = 0;
			 loop1 = 0;
			 my_printf("-----rhythm_value_avr:%d ----\n",rhythm_value_avr);
		 }
 /***************》》焕彩数值处理《《**********************/	 
		 unsigned int i = 0,j=0;
		 static unsigned int cnt=0;
		 static hsv_h_t sh=0;
		 hsv_s_t ss = 100;
		 hsv_v_t sv = 50;
		 static  char num =5,num_back=0; 
		 static unsigned char r, g, b;
	 if ( v_value < MUSIC_SENSITIVITY )
	 {	 
		 num =0;
	 	 rgb_colorful_buffer_clean();
	     rgb_value_sync();	 	 
	 }
	 else if (v_value < rhythm_value_avr)
	 {
		 diff=(float)(rhythm_value_avr-v_value)/rhythm_value_avr;
		 num=  7- diff*5;  //处理num	 
	 }	 
	 else 
	 {
		 diff=(float)(v_value-rhythm_value_avr)/rhythm_value_avr;
		 num= diff*5 + 5;//处理num	 
	 } 
 //num处理
	 if (num<=0)num =0;
	 num= num > num_max ? num_max : num  ;	//原 10	

	sh+=60;
	if(sh>=360)sh=0;
	hsv_to_rgb(&r, &g, &b, sh, 100, sv);
	
	 bk_printf("-----max_voltage:%d ----\n",max_voltage);
	 if (num<num_back)	//音量下降
	 {
		 while(num<num_back)
		 { 	
			 rgb_colorful_buffer_clean(); 
 			 rgb_value_sync();
			 for (i =0 ; i<num_back ; i++)
			 {	 
				 for (j=0;j<RGB_LED_NUM/(num_max_4);j++)//分N段: RGB_LED_NUM/10===每段最少10
				 {	 
					 if(MUSIC_LOCAL_MODE==0)return;//小概率切换彩色不完整
				
					 rgb_colorful_buffer_set(i+num_max_4+num_max_4*2*j, r,  g, b );
					 rgb_colorful_buffer_set(num_max_4-1-i+num_max_4*2*j , r,  g, b );
					 //bk_printf("  :%d \n",i+num_max+num_max*2*j);
				 }				 
				
			 } rgb_value_sync();
				wlt_ms_delay(10);
			 num_back--;
		 }
	 }
	 else	//音量上升
	 {	 
		 while(num>num_back)
		 {
			 rgb_colorful_buffer_clean();//每变一次清零RGB数组	 
			 //singleColor(0,0,100);//底色
			 for (i =0; i<num_back; i++)
			 {	 
				 for (j=0;j<RGB_LED_NUM/num_max_4;j++)//默认分5段
				 {
					rgb_colorful_buffer_set(i+num_max_4+num_max_4*2*j, r,  g, b );
					rgb_colorful_buffer_set(num_max_4-1-i+num_max_4*2*j , r,  g, b );
				 }				 
				  
			 } rgb_value_sync();
				 wlt_ms_delay(10);	
			 num_back++;
		 }
	 }
	 num_back = num ;
 }

void iotalink_music_process_5(void)
{
    static unsigned int hue=0 , v_value = 0,v_value_back=0;
	static int rhythm_value_avr =0,rhythm_value_sum=0;
	static char relax_flag =10,loop1=0;	
    int loop = 0,loop2;
	static unsigned char flag =0;
    int max_voltage = 0;
		
    while(loop++< CONFIG_MUSIC_SAMPLING_COUNT )
    {
        if (max_voltage < Voltages[loop])
        {
            max_voltage = Voltages[loop];
        }
    } 
	v_value = abs(max_voltage, MUSIC_STATIC); 
	my_printf ("pre  : %d\n",v_value);
	
	int i = 0;
	static int z = 0;
	static unsigned char  j=0;
	//hsv_h_t sh[] = {0,200,120,36,243};//五段颜色
	//hsv_h_t sh[] = {0,60,120,180,240,300,340};
    static hsv_h_t sh=0;
	hsv_s_t ss = 100;
	hsv_v_t sv = 50;
	unsigned char r, g, b;
//获取均值线
		loop1++;
		rhythm_value_sum +=v_value;			
		if (loop1==relax_flag)//每进十次刷新
		{		
			rhythm_value_avr=rhythm_value_sum/relax_flag;
			rhythm_value_sum = 0;
			loop1 = 0;
			my_printf("-----process_2_avr:%d ----\n",rhythm_value_avr);
		}				
/********************>>>数据配置<<<***********************************/	
	sh+=60;
	if(sh>=360)sh=0;

	rgb_colorful_buffer_clean();
  

	if (v_value <MUSIC_SENSITIVITY)
	{
	 	 rgb_colorful_buffer_clean();
	     rgb_value_sync();	
	}
	else if (v_value < rhythm_value_avr)
	{
		 for (i = 0; i < RGB_LED_NUM/3; i++)
		 {	 
		 	
			hsv_to_rgb(&r, &g, &b, sh, ss, sv);
			rgb_colorful_buffer_set(2+i*3 , r,  g, b );				
		 }

	}
	else
	{
		for (i = 0; i < RGB_LED_NUM/3; i++)
		{	
		   hsv_to_rgb(&r, &g, &b, sh, ss, sv);
		   rgb_colorful_buffer_set(1+i*3 , r,  g, b );
		   rgb_colorful_buffer_set(3+i*3 , r,  g, b );	
		}

	}
		rgb_value_sync();
		wlt_ms_delay(20);	
}


/*-----------------------------《 adc 按键调试demo》------------------------------------*/


#include "scm_adc.h"     // 芯片ADC驱动头文件

// ===================== 配置宏定义 =====================
#define ADC_KEY_CHANNEL    SCM_ADC_SINGLE_CH_4  // 选择ADC通道4（对应硬件GPIO4）
//#define ADC_KEY_CHANNEL    SCM_ADC_SINGLE_CH_5  // 选择ADC通道5（对应硬件GPIO7）

#define MUSIC_KEY  6	//暂时控制音乐播放的按键


#define ADC_SAMPLE_CNT     10                  // ADC采样滤波次数
#define KEY_DEBOUNCE_MS    20                  // 按键消抖时间(ms)
#define KEY_SCAN_INTERVAL  10                  // 按键扫描间隔(ms)

// ADC按键阈值（根据实际分压电路调整，需实测校准）
#define KEY_NONE_THRESH    800    // 无按键阈值

#define KEY1_THRESH_LOW    200    // KEY1最小值
#define KEY1_THRESH_HIGH   280   // KEY1最大值

#define KEY2_THRESH_LOW    700    // KEY2最小值
#define KEY2_THRESH_HIGH   750    // KEY2最大值

#define KEY3_THRESH_LOW    760    // KEY3最小值
#define KEY3_THRESH_HIGH   780    // KEY3最大值

// 按键枚举
typedef enum {
    KEY_NONE = 0,  // 无按键
    KEY1,          // 按键1
    KEY2,          // 按键2
    KEY3           // 按键3
} adc_key_t;

// ===================== 静态函数声明 =====================
static uint16_t adc_key_read_avg(void);
static adc_key_t adc_key_scan(void);
static void adc_key_action(adc_key_t key);

// ===================== 核心功能实现 =====================

/**
 * @brief 读取ADC平均值（滤波，抗干扰）
 * @return ADC采样平均值
 */
static uint16_t adc_key_read_avg(void)
{
    uint32_t sum = 0;
    uint16_t buf[ADC_SAMPLE_CNT];
    int ret, i;

    // 调用芯片ADC驱动读取多组数据
    ret = scm_adc_read(ADC_KEY_CHANNEL, buf, ADC_SAMPLE_CNT);
    if (ret != 0) 
	{
        printf("ADC read error: 0x%x\n", ret);
        return 0;
    }

    // 计算平均值
    for (i = 0; i < ADC_SAMPLE_CNT; i++)
	{
        sum += buf[i];
    }
	
    return (uint16_t)(sum / ADC_SAMPLE_CNT);
}

/**
 * @brief 扫描ADC按键（识别按键类型）
 * @return 按键类型
 */
static adc_key_t adc_key_scan(void)
{
    uint16_t adc_val = adc_key_read_avg();
    adc_key_t key = KEY_NONE;

    // 根据阈值判断按键
    if (adc_val > KEY_NONE_THRESH) {
        key = KEY_NONE;
    } else if (adc_val >= KEY1_THRESH_LOW && adc_val <= KEY1_THRESH_HIGH) {
        key = KEY1;
    } else if (adc_val >= KEY2_THRESH_LOW && adc_val <= KEY2_THRESH_HIGH) {
        key = KEY2;
    } else if (adc_val >= KEY3_THRESH_LOW && adc_val <= KEY3_THRESH_HIGH) {
        key = KEY3;
    }

    return key;
}

/**
 * @brief 按键动作处理
 * @param key 按下的按键
 */
static void adc_key_action(adc_key_t key)
{
	extern light_ctrl_data_t sg_light_ctrl_data;

	bool _switch = sg_light_ctrl_data.switch_status;

    switch (key) {
        case KEY3:
            printf("KEY1 pressed (on/off)\n");
				light_power_set(!_switch);
				iotalink_light_ctrl_process();		

            break;
        case KEY2:
			
            printf("KEY2 pressed (sleep)\n");

			scm_gpio_write(MUSIC_KEY , 0);
			wlt_ms_delay(200);
			scm_gpio_write(MUSIC_KEY , 1);

			light_mode_set(COLOR_MODE);
			cwrgb_target_val_set (0, 0,10,0,50);


            break;
        case KEY1:
            printf("KEY3 pressed (mode)\n");
            break;
        default:
            break;
    }
}

/**
 * @brief ADC按键任务（持续扫描+消抖）
 */
void adc_key_task(void *arg)
{
    adc_key_t last_key = KEY_NONE;
    adc_key_t cur_key;

    printf("ADC按键任务启动...\n");

    while (1) 
	{
        // 1. 扫描当前按键
        cur_key = adc_key_scan();

        // 2. 消抖处理：连续两次扫描结果一致才判定为有效按键
        if (cur_key != last_key)
		{
          // 消抖延时（需适配芯片延时接口）
			udelay(KEY_DEBOUNCE_MS*1000);
            cur_key = adc_key_scan();     // 再次扫描确认
            if (cur_key == last_key)
			{
                goto scan_end;
            }
        }

        // 3. 有效按键触发动作（仅按下时触发一次）
        if (cur_key != KEY_NONE && cur_key != last_key) 
		{
            adc_key_action(cur_key);
            last_key = cur_key;
        }

        // 4. 无按键时重置状态
        if (cur_key == KEY_NONE) 
		{
            last_key = KEY_NONE;
        }
	

scan_end:
        // 按键扫描间隔
       // hal_msleep(KEY_SCAN_INTERVAL);
		udelay(KEY_DEBOUNCE_MS*1000);
			
    }
}

// ===================== 初始化函数 =====================

/**
 * @brief ADC按键初始化
 * @return 0:成功 其他:失败
 */
int adc_key_init(void)
{
    int ret;

    // 重置ADC外设
    ///ret = scm_adc_reset();
    if (ret != 0) {
        printf("ADC reset error: 0x%x\n", ret);
        return -1;
    }

    printf("ADC按键初始化完成（通道：%d）\n", ADC_KEY_CHANNEL);

    // 创建ADC按键任务（适配芯片的任务创建接口）
  //  xTaskCreate("adc_key_task",   adc_key_task,   NULL,  1024,     10,  NULL); 
	osThreadAttr_t attr = {
		.name 		= "adc_key_task",
		.stack_size = 1024,
		.priority 	= osPriorityNormal,    //osPriorityNormal        = 24,osPriorityRealtime,//osPriorityLowcon
	};
	/* run the demo in a new thread to allow further CLI */
	osThreadNew(adc_key_task, NULL, &attr);		

    return 0;
}

// ===================== 主函数（测试用） =====================
#if 0
int main(void)
{
    // 初始化ADC按键
    adc_key_init();

    // 主循环（芯片底层已处理任务调度，此处仅占位）
    while (1) {
        hal_msleep(1000);
    }

    return 0;
}
#endif



/*-----------------------------《 mic 数据采集 》------------------------------------*/


static void iotalink_adc_get_process(void * argv)
{
	u8 i=0;
	u16 adcvalue[2];
	while(1) 
	{
		//if (MUSIC_LOCAL_MODE)
		if (light_mode_get()==MUSIC_MODE)
		{
			while(i++<CONFIG_MUSIC_SAMPLING_COUNT)
			{
			
				scm_adc_read(SCM_ADC_SINGLE_CH_5,&adcvalue,2);//io7 mic	
				Voltages[i]=adcvalue[1];
				printf("%d -", Voltages[i]);			
			}
			i=0;   
			//printf("\n", i,Voltages[i]);	
		}

#if 0
		scm_adc_read(SCM_ADC_SINGLE_CH_4,&adcvalue,2);//key
	    printf("-----------------> adc io4 %d\n",adcvalue[1]);
#endif
		
		osDelay(MS_TO_TICKS(20));
	}


}
void iotalink_adc_init(void)
{
	//通道已默认初始化 直接read
		
	//u16 buf[8]={0};
	scm_adc_reset();
	adc_key_init();
	//受spi 驱动影响要把它配置成输入
	
  	scm_gpio_configure(7,SCM_GPIO_PROP_INPUT);
	scm_gpio_write(7 , 0);
	scm_gpio_configure(4,SCM_GPIO_PROP_INPUT);//
	scm_gpio_write(4 , 0);

	
  	scm_gpio_configure(MUSIC_KEY,SCM_GPIO_PROP_OUTPUT);
	scm_gpio_write(MUSIC_KEY, 1);


	//scm_adc_read(SCM_ADC_SINGLE_CH_4, buf, 8);

	//xTaskCreate(iotalink_button_process, "iotalink_button_process",512, NULL, 7, NULL);
	osThreadAttr_t attr = {
		.name 		= "music_task",
		.stack_size = 1024,
		.priority 	= osPriorityNormal,    //osPriorityNormal        = 24,osPriorityRealtime,//osPriorityLowcon
	};
	/* run the demo in a new thread to allow further CLI */
	osThreadNew(iotalink_adc_get_process, NULL, &attr);		
}






//scm_adc_read

