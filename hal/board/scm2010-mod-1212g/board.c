/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
 * SCM2010 MODULE 1212(Type G) pin assignment
 */

static struct pinctrl_pin_map pin_map[] = {
	/* UART1 */
	pinmap( 6, "atcuart.1", "rxd", 0),
	pinmap( 7, "atcuart.1", "txd", 0),

	/* SPI0 */
	pinmap(12, "atcspi200-xip", "clk", 0),
	pinmap(11, "atcspi200-xip", "cs",  0),
	pinmap(13, "atcspi200-xip", "mosi",0),
	pinmap(14, "atcspi200-xip", "miso",0),
	pinmap(10, "atcspi200-xip", "wp"  ,0),
	pinmap( 9, "atcspi200-xip", "hold",0),

	/* SDIO */
	pinmap(17, "sdio", "clk",    0),
	pinmap(15, "sdio", "data2",  0),
	pinmap(16, "sdio", "data3",  0),
	pinmap(18, "sdio", "cmd",    0),
	pinmap(19, "sdio", "data0",  0),
	pinmap(20, "sdio", "data1",  0),
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

	printk("BOARD: 12*12 Module Type G\n");
}
