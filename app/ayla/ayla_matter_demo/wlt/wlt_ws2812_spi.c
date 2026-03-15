

// 注：此处需根据实际使用的WiFi芯片SDK补充SPI硬件驱动头文件（如spi.h、delay.h等）
// 示例假设SDK提供spi_master_init、spi_master_send、delay_us、delay_ms函数
#include <cmsis_os.h>

#include "scm_spi.h"

#include "hal/timer.h"
#include "hal/io.h"
#include <string.h>

#include "iotalink_control.h"



//#define WS2812_LOGIC_1 0xF0  /// 150*4 + 150*4 = 600ns + 600ns
#if 1
#define WS2812_LOGIC_1 0xFC  /// 150*6 + 150*2 = 900ns + 300ns 1111 1100
#define WS2812_LOGIC_0 0xC0  /// 150*2 + 150*6 = 300ns + 900ns 1100 0000
#else

#define WS2812_LOGIC_1 0x03  /// 150*6 + 150*2 = 900ns + 300ns 0000 0011
#define WS2812_LOGIC_0 0x3F/// 150*2 + 150*6 = 300ns + 900ns   0011 1111

#endif
#define WS2812_PIXEL_NUM        12 // 单字节模式最多20个
#define WS2812_PIXEL_SPI_BYTES  24

#define WS2812_SPI_TIMEOUT_MS 3000 /// 1s max. delay for SPI blocking I/O
#define SPI2_IDX 2

///#include "iotalink.h"

// 静态SPI发送缓冲区：WS2812_PIXEL_NUM个像素点 × 24字节/像素点
static uint8_t __attribute__((section(".dma_buffer"))) g_ws2812_spi_buf[WS2812_PIXEL_NUM * WS2812_PIXEL_SPI_BYTES] = {0};

typedef struct ws2812_pixel
{
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	
} ws2812_pixel_t;


/**
 * @brief 初始化硬件SPI（按WS2812时序要求配置）
 * @return 0：初始化成功；非0：初始化失败
 */
int ws2812_spi_init(void) {
    
/*
	scm_spi_cfg spi_cfg = {0};

    // 1. 配置SPI基础参数（符合文档6.1节硬件SPI要求）
    spi_cfg.role = SCM_SPI_ROLE_MASTER;          // 主机模式
    spi_cfg.mode = SCM_SPI_MODE_0;               // SPI模式0（CPOL=0，CPHA=0）
    spi_cfg.clk_freq = WS2812_SPI_CLK_FREQ;  // 8MHz时钟（文档指定）
    spi_cfg.bit_order = SPI_BIT_ORDER_MSB;   // 高位先发（匹配WS2812 24bit数据顺序）
    spi_cfg.data_width = SPI_DATA_WIDTH_8BIT;// 8bit数据宽度（单个SPI字节对应WS2812 1bit）
    spi_cfg.dma_en = 1;                      // 开启DMA（避免CPU阻塞，确保发送稳定，文档建议硬件SPI稳定）

    // 2. 初始化SPI硬件
    if (spi_master_init(SPI_NUM_0, &spi_cfg) != 0) {
        return -1; // SPI初始化失败
    }
*/
	static struct scm_spi_cfg scm_cli_spi_cfg = {
		.role = SCM_SPI_ROLE_MASTER,
		.mode = SCM_SPI_MODE_0,
		.data_io_format = SCM_SPI_DATA_IO_FORMAT_SINGLE,
//		.data_unit = SCM_SPI_DATA_UNIT_DW,
//		.data_unit = SCM_SPI_DATA_UNIT_BYTE,

		.bit_order = SCM_SPI_BIT_ORDER_MSB_FIRST,
		.slave_extra_dummy_cycle = SCM_SPI_DUMMY_CYCLE_NONE,
		.master_cs_bitmap = 0,
		.clk_src = SCM_SPI_CLK_SRC_XTAL, /// SCM_SPI_CLK_SRC_PLL
		.clk_div_2mul = 3, // SCLK= SRC/(clk_div_2mul*2) = 40/6 = 6.7MHz, 1 cycle=150ns 0==> 1100 0000, 1==>1111 1111
		.dma_en = 1,
	};




#if 0		
	u32 v = readl(0xf0700028);
	v &= ~(0xF << 4);
	v |= (0x5 << 4);
	writel(v, 0xf0700028); // GPIO17 = 5, configure as SPI2_DAT0
	printf("GPIO pinmux=0x%x\n", v);

	v = readl(0xf0700010);
	v |= (0x1 << 17);
	writel(v, 0xf0700010); // GPIO17 output enabled
	printf("GPIO OEN=0x%x\n", v);
#endif

	scm_spi_init(SPI2_IDX);

	scm_spi_configure(SPI2_IDX, &scm_cli_spi_cfg, NULL, NULL);

//	scm_spi_slave_set_rx_buf(1, sl_rx_buf, TEST_MSG_SIZE);


    printf("WS2812 SPI init success! ");
    printf("Note: Ensure 3.3V->5V level shifter and hot-plug resistor are connected.\n");

    return 0;
}


/**
 * @brief 将RGB像素数组转换为WS2812所需的SPI发送缓冲区
 * @param pixels：输入RGB像素数组（长度需等于WS2812_PIXEL_NUM）
 * @param spi_buf：输出SPI发送缓冲区（长度需≥WS2812_PIXEL_NUM×WS2812_PIXEL_SPI_BYTES）
 */
 	
void ws2812_rgb_to_spi(const ws2812_pixel_t *pixels, uint8_t *spi_buf)
{
    if (pixels == NULL || spi_buf == NULL) {
        return;
    }

    uint32_t buf_idx = 0;

	int bright = light_bright_get();
	
	///printf("bright %d \n",bright);
	if(MATTER_MODE == light_mode_get())bright=100;
	
    for (uint16_t pixel_idx = 0; pixel_idx < WS2812_PIXEL_NUM; pixel_idx++)
	{
        const ws2812_pixel_t *curr_pixel = &pixels[pixel_idx];

//GRB  5V 调试

#if 0
        // 1. 处理绿色分量（R7~R0，高位先发，文档5.2节24bit数据结构）
        for (int8_t bit = 7; bit >= 0; bit--)
		{
            spi_buf[buf_idx++] = (bright*(curr_pixel->green)/100 & (1 << bit)) ? WS2812_LOGIC_1 : WS2812_LOGIC_0;
        }
        // 2. 处理红色分量（G7~G0，高位先发，文档5.2节24bit数据结构）
        for (int8_t bit = 7; bit >= 0; bit--) 
		{
            spi_buf[buf_idx++] = (bright*(curr_pixel->red )/100& (1 << bit)) ? WS2812_LOGIC_1 : WS2812_LOGIC_0;
        }

        // 3. 处理蓝色分量（B7~B0，高位先发，文档5.2节24bit数据结构）
        for (int8_t bit = 7; bit >= 0; bit--)
		{
            spi_buf[buf_idx++] = (bright*(curr_pixel->blue )/100& (1 << bit)) ? WS2812_LOGIC_1 : WS2812_LOGIC_0;
        }
#else	//RBG  落地灯

	// 1. 处理红色分量

	for (int8_t bit = 7; bit >= 0; bit--)
	{
		spi_buf[buf_idx++] = (bright*(curr_pixel->red)/100 & (1 << bit)) ? WS2812_LOGIC_1 : WS2812_LOGIC_0;
	}
	// 2. 处理红色分量（G7~G0，高位先发，文档5.2节24bit数据结构）
	for (int8_t bit = 7; bit >= 0; bit--) 
	{
		spi_buf[buf_idx++] = (bright*(curr_pixel->blue )/100& (1 << bit)) ? WS2812_LOGIC_1 : WS2812_LOGIC_0;
	}
	
	// 3. 处理蓝色分量（B7~B0，高位先发，文档5.2节24bit数据结构）
	for (int8_t bit = 7; bit >= 0; bit--)
	{
		spi_buf[buf_idx++] = (bright*(curr_pixel->green )/100& (1 << bit)) ? WS2812_LOGIC_1 : WS2812_LOGIC_0;
	}


#endif
    }
}


/**
 * @brief 发送SPI数据到WS2812，刷新灯带显示
 * @param spi_buf：SPI发送缓冲区（已通过ws2812_rgb_to_spi转换）
 * @return 0：发送成功；非0：发送失败
 */
int ws2812_spi_send( uint8_t *spi_buf) {
    if (spi_buf == NULL) 
	{
        return -1;
    }

    // 1. 计算SPI发送数据长度
    uint32_t send_len = WS2812_PIXEL_NUM * WS2812_PIXEL_SPI_BYTES;
	//uint32_t send_dw_len = send_len >> 2; // send_len/4

	//if (send_len % 4) send_dw_len += 1;

    if (scm_spi_master_tx(SPI2_IDX , 0 , spi_buf, send_len, WS2812_SPI_TIMEOUT_MS) != 0) 

 //   if (scm_spi_master_tx(SPI2_IDX , 0 , spi_buf, send_dw_len, WS2812_SPI_TIMEOUT_MS) != 0) 
    {
        printf("WS2812 SPI send failed! %d\n",scm_spi_master_tx(SPI2_IDX , 0 , spi_buf, send_len, WS2812_SPI_TIMEOUT_MS));
        return -2;
    }


    // 3. 发送复位信号（≥280us低电平，文档5.1节RESET码要求）
  //  sys_usleep(WS2812_RESET_DELAY_US);


    return 0;
}

/**
 * @brief 测试函数：实现流水灯效果（验证驱动正确性）
 * @param cycle_ms：流水灯切换周期（建议≥200ms，避免闪烁过快）
 */
void ws2812_test_run(uint32_t cycle_ms)
{
    ws2812_pixel_t pixels[WS2812_PIXEL_NUM] = {0}; // 初始化所有像素为黑色（RGB：0,0,0）

    printf("WS2812 test start! Cycle: %d ms\n", cycle_ms);

    while (1)
	{
        // 循环点亮每个像素点（红色，RGB：255,0,0）
        for (uint16_t rgb_idx = 0; rgb_idx < WS2812_PIXEL_NUM; rgb_idx++) 
		{
            // 1. 重置所有像素为黑色
            memset(pixels, 0, sizeof(pixels));
            // 2. 点亮当前索引的像素点（红色）
            pixels[rgb_idx].red = 255;
            pixels[rgb_idx].green = 0;
            pixels[rgb_idx].blue = 0;

            // 3. RGB数据转换为SPI缓冲区
            ws2812_rgb_to_spi(pixels, g_ws2812_spi_buf);
			//printf("send spi buffer\n");

			wlt_light_set_cw(0,rgb_idx*10);

            // 4. 发送SPI数据，刷新显示
            if (ws2812_spi_send(g_ws2812_spi_buf) != 0)
			{
				
				udelay(cycle_ms*1000);///sys_msleep(cycle_ms);
                continue;
            }
            // 5. 保持当前状态，等待下一次切换
            udelay(cycle_ms*1000); ///sys_msleep(cycle_ms);
        }
    }
}

// 主函数示例（可根据实际SDK入口函数调整）

int wlt_ws2812_test(void)
{
	wlt_pwm_timer_init();

    // 1. 初始化WS2812 SPI驱动
    if (ws2812_spi_init() != 0) 
	{
        printf("WS2812 init failed! Exit.\n");
       // while (1);
    }

    // 2. 运行流水灯测试（周期300ms）
 //	ws2812_test_run(300);

    return 0;
}

//发送 RGB_LED_NUM 个一整串数据包

void wlt_ms_delay( int ms)
{

	udelay(ms*1000);

}



#include "scm_timer.h"

 /**************************************************** pwm ***********************************************************/
 
 /*
#ifdef CONFIG_USE_TIMER0_PWM
		 
		 pinmap(15, "timer.0", "pwm0",	0),
		 pinmap(16, "timer.0", "pwm1",	0),
		 pinmap(17, "timer.0", "pwm2",	0),
		 pinmap(18, "timer.0", "pwm3",	0),
#endif
	 
#ifdef CONFIG_USE_TIMER1_PWM	
		 pinmap(19, "timer.1", "pwm0",	0),
		 pinmap(20, "timer.1", "pwm1",	0),
#endif
 */
   
   typedef enum
   {
	   LED_PWM_CH_0,
	   LED_PWM_CH_1,
	   LED_PWM_CH_2,
	   LED_PWM_CH_3,
	   LED_PWM_CH_MAX,
   }E_LED_PWM_CHANNEL;
	   
#define LED_PWM_CYCLE 1000
 
#define USE_PWM_NUM 1


static struct scm_timer_cfg ledc_config[USE_PWM_NUM];


 int wlt_pwm_timer_init(void)
{

	int ret;

	//if(USE_PWM_NUM<=2)
	{
		for(int ch = 0 ; ch< USE_PWM_NUM ;ch++)
		{
			
			ledc_config[ch].mode = SCM_TIMER_MODE_PWM;
			ledc_config[ch].intr_en = 0;
			ledc_config[ch].data.pwm.high =LED_PWM_CYCLE ;
			ledc_config[ch].data.pwm.low  = LED_PWM_CYCLE;
			ledc_config[ch].data.pwm.park = 0;

	//只先使用timer 0
			scm_timer_configure(SCM_TIMER_IDX_0, ch, &ledc_config[ch], NULL, NULL);
			if (ret)
			{
				printf("timer start error = %x\n", ret);
			}
			else printf("pwm  start success = %x\n", ret);
			scm_timer_start(SCM_TIMER_IDX_0, ch);
		}

	}
	

	return 0;
}
	

void wlt_led_pwm_set_duty(E_LED_PWM_CHANNEL ch, u8 duty)
{
    if(duty >= 100)
	{
        duty = 99;
    }
    ledc_config[ch].data.pwm.high = LED_PWM_CYCLE * duty / 100;
    ledc_config[ch].data.pwm.low = LED_PWM_CYCLE - ledc_config[ch].data.pwm.high;
	
    // printf(" ch : %d ,higt : %d , low : %d \n",ch ,ledc_config[ch].data.pwm.high , ledc_config[ch].data.pwm.low);  
    scm_timer_stop(SCM_TIMER_IDX_0,ch);
    // SCM_INFO_LOG(TAG,"TIMER PWM channel %d set duty = %d", ch, duty);
    if(duty > 0)
	{
        int ret = scm_timer_configure(SCM_TIMER_IDX_0, ch, &ledc_config[ch], NULL, NULL);
        if (ret)
		{
            printf("TIMER PWM channel %d configure error = %x", ch, ret);
        } 
		else 
        {            /* Start the TIMER */
            scm_timer_start(SCM_TIMER_IDX_0,ch);
        }
    }

}

//pwm 冷暖输出
void wlt_light_set_cw(u16 cold, u16 warm)
{	
	
	if ((cold!=0)||(warm!=0))//(sg_light_ctrl_data.mode == 0)&&
	{

		wlt_led_pwm_set_duty(0,warm);
		printf("  cw_warm %d   rrrr \n ",warm);
		rgbsingleColor(cold,cold,cold);	
		rgb_value_sync();
	}

}

// 
//uint8_t rgb_colorful_values[20][3];
extern u8 rgb_colorful_values[100+1][3];

void rgb_value_sync(void)
{ 

	ws2812_rgb_to_spi((const ws2812_pixel_t*)rgb_colorful_values, g_ws2812_spi_buf);
	ws2812_spi_send(g_ws2812_spi_buf);
}




void wlt_light_set_rgbcw (unsigned short cold, unsigned short warm,unsigned short red, unsigned short green,unsigned short blue)
{

	if ((cold!=0)||(warm!=0))//(sg_light_ctrl_data.mode == 0)&&
	{
	//	warm = warm* light_bright_get()/100;

		int light   =light_bright_get();
	
		u8 cw_warm = warm* light/100;

		wlt_led_pwm_set_duty(0,cw_warm);
//		printf("   warm %d	 \n  " ,cw_warm);

		rgbsingleColor(cold,cold,cold);	
		rgb_value_sync();
	}

	else 
	{
		wlt_led_pwm_set_duty(0,0);
	
		rgbsingleColor(red, green ,blue);
		rgb_value_sync();
	}
	



}



void matter_wlt_light_set_rgbcw(unsigned short cold, unsigned short warm,unsigned short red, unsigned short green,unsigned short blue)
{


	light_power_set(1);
	light_mode_set(MATTER_MODE);
	
	iotalink_light_ctrl_process();

	if ((cold!=0)||(warm!=0))//(sg_light_ctrl_data.mode == 0)&&
	{

		

		u8 cw_cold = cold * 100/1024;
		u8 cw_warm = warm * 100/1024;	

		wlt_led_pwm_set_duty(0,cw_warm);
		printf(" cw_warm %d   \n " ,cw_warm);
		rgbsingleColor(cw_cold,cw_cold,cw_cold);	
		rgb_value_sync();
		
		cwrgb_target_val_set (cw_cold, cw_warm ,0 , 0,0);

		
	}
	else 
	{

		//light_mode_set(COLOR_MODE);
		
		wlt_led_pwm_set_duty(0,0);
		u8 rgb_red  =  red * 255/1024;
		u8 rgb_green = green * 255/1024;	
		u8 rgb_blue  = blue* 255/1024;		
		rgbsingleColor(rgb_red, rgb_green ,rgb_blue);
		rgb_value_sync();

		cwrgb_target_val_set (0, 0 ,rgb_red , rgb_green,rgb_blue);
	}
	




}


