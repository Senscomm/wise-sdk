/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
 * SCM2010 EVB QFN32 b/d pin assignment
 */

static struct pinctrl_pin_map pin_map[] = {
	/* UART0 */
	pinmap(21, "atcuart.0", "rxd", 0),
	pinmap(22, "atcuart.0", "txd", 0),

	/* UART1 */
	pinmap( 0, "atcuart.1", "rxd", 0),
	pinmap( 1, "atcuart.1", "txd", 0),
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

	printk("BOARD: SCM2010 EVB QFN32\n");
}

#include <version.h>

void board_get_revinfo(struct sncmf_rev_info *revinfo)
{
	revinfo->boardrev = (u32)0x000010000; /* EVB (0x000) | v01.00.00 */
}
