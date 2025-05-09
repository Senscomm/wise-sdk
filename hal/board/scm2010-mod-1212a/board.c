/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <soc.h>
#include <hal/kernel.h>
#include <hal/clk.h>
#include <hal/device.h>
#include <hal/pinctrl.h>
#include <hal/console.h>
#include <hal/timer.h>

#include <string.h>

/*
 * SCM2010 MODULE 1212(Type A) pin assignment
 */

static struct pinctrl_pin_map pin_map[] = {
#ifdef CONFIG_USE_UART0
	/* UART0 */
	pinmap(21, "atcuart.0", "rxd", 0),
	pinmap(22, "atcuart.0", "txd", 0),
#endif

#ifdef CONFIG_USE_UART1
	/* UART1 */
	pinmap(17, "atcuart.1", "rxd", 0),
	pinmap(18, "atcuart.1", "txd", 0),
#endif

#ifdef CONFIG_USE_SPI0
    /* SPI0 */
    pinmap(12, "atcspi200-xip", "clk", 0),
    pinmap(11, "atcspi200-xip", "cs",  0),
    pinmap(13, "atcspi200-xip", "mosi",0),
    pinmap(14, "atcspi200-xip", "miso",0),
    pinmap(10, "atcspi200-xip", "wp"  ,0),
    pinmap( 9, "atcspi200-xip", "hold",0),
#endif

#ifdef CONFIG_USE_TIMER0_PWM
	/* PWM */
    pinmap( 0, "timer.0", "pwm0",  0),
    pinmap( 1, "timer.0", "pwm1",  0),
    pinmap(23, "timer.0", "pwm2",  0),
    pinmap(24, "timer.0", "pwm3",  0),
#endif

#ifdef CONFIG_USE_TIMER1_PWM
	/* PWM */
    pinmap(20, "timer.1", "pwm1",  0),
    pinmap( 7, "timer.1", "pwm3",  0),
#endif

#ifdef CONFIG_USE_I2C0
	/* I2C0 */
	pinmap(23, "atci2c.0", "scl", 0),
	pinmap(24, "atci2c.0", "sda", 0),
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

	printk("BOARD: 12*12 Module Type A\n");
}
