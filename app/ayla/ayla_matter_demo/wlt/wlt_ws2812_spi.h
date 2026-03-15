#ifndef __WS2812_SPI_H__
#define __WS2812_SPI_H__

#include <stdint.h>
#include <string.h>





// -------------------------- WS2812核心参数（源自幻彩灯带 WS2812 驱动.pdf） --------------------------
// SPI时钟频率：文档要求8MHz（确保单bit传输时间0.125us，匹配WS2812时序）
#define WS2812_SPI_CLK_FREQ    8000000UL
// 逻辑电平对应的SPI发送字节：文档6.1节规定（0码=11000000b=0xC0，1码=11111100b=0xFC）
#define WS2812_LOGIC_0         0xC0
#define WS2812_LOGIC_1         0xFC
// 复位信号时长：文档5.1节要求>280us，取300us确保稳定
#define WS2812_RESET_DELAY_US  300
// 单个像素点数据：文档5.2节规定24bit RGB（R7~R0→G7~G0→B7~B0，高位先发）
#define WS2812_PIXEL_BITS      24
// 单个像素点对应SPI字节数：1bit RGB数据需8bit SPI数据，故24×8bit=24字节
#define WS2812_PIXEL_SPI_BYTES 24

// 可配置参数：根据实际灯带调整
#define WS2812_PIXEL_NUM       50   // 灯带总像素点数量
#define WS2812_SPI_TIMEOUT_MS  10  // SPI发送超时时间

// 单个像素点的RGB色彩数据结构
typedef struct {
    uint8_t red;    // 红色分量（0~255，对应R7~R0）
    uint8_t green;  // 绿色分量（0~255，对应G7~G0）
    uint8_t blue;   // 蓝色分量（0~255，对应B7~B0）
} ws2812_pixel_t;

// -------------------------- 函数声明 --------------------------
/**
 * @brief 初始化硬件SPI（按WS2812时序要求配置）
 * @return 0：初始化成功；非0：初始化失败
 */
int ws2812_spi_init(void);

/**
 * @brief 将RGB像素数组转换为WS2812所需的SPI发送缓冲区
 * @param pixels：输入RGB像素数组（长度需等于WS2812_PIXEL_NUM）
 * @param spi_buf：输出SPI发送缓冲区（长度需≥WS2812_PIXEL_NUM×WS2812_PIXEL_SPI_BYTES）
 */
void ws2812_rgb_to_spi(const ws2812_pixel_t *pixels, uint8_t *spi_buf);

/**
 * @brief 发送SPI数据到WS2812，刷新灯带显示
 * @param spi_buf：SPI发送缓冲区（已通过ws2812_rgb_to_spi转换）
 * @return 0：发送成功；非0：发送失败
 */
int ws2812_spi_send(const uint8_t *spi_buf);

/**
 * @brief 测试函数：实现流水灯效果（验证驱动正确性）
 * @param cycle_ms：流水灯切换周期（建议≥200ms，避免闪烁过快）
 */
void ws2812_test_run(uint32_t cycle_ms);

int wlt_ws2812_test(void);
void wlt_ms_delay( int ms);

int wlt_pwm_timer_init(void);

void wlt_light_set_rgbcw(unsigned short cold, unsigned short warm,unsigned short red, unsigned short green,unsigned short blue);

void matter_wlt_light_set_rgbcw(unsigned short cold, unsigned short warm,unsigned short red, unsigned short green,unsigned short blue);


#endif // __WS2812_SPI_H__

