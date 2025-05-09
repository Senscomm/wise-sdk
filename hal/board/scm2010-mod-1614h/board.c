/*
 * Copyright 2025-2026 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <soc.h>
#include <hal/kernel.h>
#include <hal/clk.h>
#include <hal/device.h>
#include <hal/pinctrl.h>
#include <hal/console.h>
#include <hal/timer.h>

/*
 * Define on-board devices.
 */

#ifdef CONFIG_ES8311
static declare_device_single(audio, 3) = {
    .name = "es8311",
    .io.bus = "atci2c.1"
};
#endif

/*
 * SCM2010-MOD-1614 pin assignment
 */

static struct pinctrl_pin_map pin_map[] = {
#ifdef CONFIG_USE_UART0
	/* UART0 */
	pinmap(21, "atcuart.0", "rxd", 0),
	pinmap(22, "atcuart.0", "txd", 0),
#endif

#ifdef CONFIG_USE_UART1
	/* UART1 */
	pinmap( 0, "atcuart.1", "rxd", 0),
	pinmap( 1, "atcuart.1", "txd", 0),
#endif

#ifdef CONFIG_USE_UART2
	/* UART2 */
	pinmap(15, "atcuart.2", "rxd", 0),
	pinmap(16, "atcuart.2", "txd", 0),
#endif

#ifdef CONFIG_USE_SPI0
#ifdef CONFIG_USE_SPI0_FLASH
    /* SPI0 */
    pinmap(12, "atcspi200-xip.0", "clk", 0),
    pinmap(11, "atcspi200-xip.0", "cs",  0),
    pinmap(13, "atcspi200-xip.0", "mosi",0),
    pinmap(14, "atcspi200-xip.0", "miso",0),
    pinmap(10, "atcspi200-xip.0", "wp"  ,0),
    pinmap( 9, "atcspi200-xip.0", "hold",0),
#else
    pinmap(12, "atcspi.0", "clk", 0),
    pinmap(11, "atcspi.0", "cs",  0),
    pinmap(13, "atcspi.0", "mosi",0),
    pinmap(14, "atcspi.0", "miso",0),
    pinmap(10, "atcspi.0", "wp"  ,0),
    pinmap( 9, "atcspi.0", "hold",0),
#endif
#endif

#ifdef CONFIG_USE_SPI1
#ifdef CONFIG_USE_SPI1_FLASH
    pinmap(16, "atcspi200-xip.1", "clk", 0),
    pinmap(15, "atcspi200-xip.1", "cs",  0),
    pinmap(17, "atcspi200-xip.1", "mosi",0),
    pinmap(18, "atcspi200-xip.1", "miso",0),
    pinmap(19, "atcspi200-xip.1", "wp"  ,0),
    pinmap(20, "atcspi200-xip.1", "hold",0),
#else
	/* SPI1 */
    pinmap(16, "atcspi.1", "clk", 0),
    pinmap(15, "atcspi.1", "cs",  0),
    pinmap(17, "atcspi.1", "mosi",0), 	/* dat0 */
    pinmap(18, "atcspi.1", "miso",0), 	/* dat1 */
    pinmap(19, "atcspi.1", "wp",  0),   /* dat2 */
    pinmap(20, "atcspi.1", "hold",0),	/* dat3	*/
#endif
#endif

#ifdef CONFIG_USE_SPI2
#ifdef CONFIG_USE_SPI2_FLASH
    pinmap( 3, "atcspi200-xip.2", "clk", 0),
    pinmap( 2, "atcspi200-xip.2", "cs",  0),
    pinmap( 4, "atcspi200-xip.2", "mosi",0),
    pinmap( 5, "atcspi200-xip.2", "miso",0),
#if 0 /* WP and HOLD not connected. */
    pinmap( 6, "atcspi200-xip.2", "wp"  ,0),
    pinmap( 7, "atcspi200-xip.2", "hold",0),
#endif
#else
    pinmap( 3, "atcspi.2", "clk", 0),
    pinmap( 2, "atcspi.2", "cs",  0),
    pinmap( 4, "atcspi.2", "mosi",0), 	/* dat0 */
    pinmap( 5, "atcspi.2", "miso",0), 	/* dat1 */
#if 0 /* WP and HOLD not connected. */
    pinmap( 6, "atcspi.2", "wp",  0),   /* dat2 */
    pinmap( 7, "atcspi.2", "hold",0),	/* dat3	*/
#endif
#endif
#endif

#ifdef CONFIG_SDIO
	/* SDIO */
    pinmap(17, "sdio", "clk",    0),
    pinmap(15, "sdio", "data2",  0),
    pinmap(16, "sdio", "data3",  0),
    pinmap(18, "sdio", "cmd",    0),
    pinmap(19, "sdio", "data0",  0),
    pinmap(20, "sdio", "data1",  0),
#endif

#ifdef CONFIG_USE_TIMER0_PWM
	/* PWM */
    pinmap(15, "timer.0", "pwm0",  0),
    pinmap(16, "timer.0", "pwm1",  0),
    pinmap(17, "timer.0", "pwm2",  0),
    pinmap(18, "timer.0", "pwm3",  0),
#endif

#ifdef CONFIG_USE_TIMER1_PWM
	/* PWM */
    pinmap(19, "timer.1", "pwm0",  0),
    pinmap(20, "timer.1", "pwm1",  0),
#endif

#ifdef CONFIG_USE_I2C0
	/* I2C0 */
	pinmap(15, "atci2c.0", "scl", 0),
	pinmap(16, "atci2c.0", "sda", 0),
#endif

#ifdef CONFIG_USE_I2C1
	/* I2C1 */
	pinmap( 6, "atci2c.1", "scl", 0),
	pinmap( 7, "atci2c.1", "sda", 0),
#endif

#ifdef CONFIG_USE_I2S
	/* I2S */
	pinmap(16, "python-i2s", "din",  0),
	pinmap(17, "python-i2s", "wclk", 0),
	pinmap(18, "python-i2s", "dout", 0),
	pinmap(19, "python-i2s", "bclk", 0),
	pinmap(20, "python-i2s", "mclk", 0),
#endif

#ifdef CONFIG_ES8311
    /* Ctrl (Audio Amplifier) */
	pinmap(15, "es8311", "ctrl",  0),
#endif
};

static struct pinctrl_platform_pinmap board_pin_map = {
	.map = pin_map,
	.nr_map = ARRAY_SIZE(pin_map),
};

__attribute__((weak))
int pinctrl_set_platform_pinmap(struct pinctrl_platform_pinmap *bpm) { return 0; }

void board_init(void)
{
	pinctrl_set_platform_pinmap(&board_pin_map);

	printk("BOARD: SCM2010-MOD-1614 EVB\n");
}
