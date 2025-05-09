/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/machine.h>
#include <hal/timer.h>
#include <hal/init.h>

static struct device *ktimer = NULL;

int register_timer(struct device *timer)
{
	if (ktimer != NULL) {
		printk("kernel timer already registered\n");
		return -1;
	}
	ktimer = timer;
	return 0;
}

void unregister_timer(struct device *timer)
{
	if (ktimer) {
		timer_stop(ktimer, 0);
		ktimer = NULL;
	}
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ktime, &ktime, &_ktime);
#else
__func_tab__ unsigned (*ktime)(void) = _ktime;
#endif


#ifdef CONFIG_WALL_TIMER
__ilm__ unsigned _ktime(void)
{
	if (ktimer == NULL)
		return 0;

	return timer_ops(ktimer)->get_value(ktimer, 0);
}

#else

static inline uint64_t _mtime( void )
{
    static volatile uint64_t * const pullMtimeRegister    =
    ( volatile uint64_t * const ) ( CONFIG_MTIME_BASE_ADDRESS );

	#if __riscv_xlen == 32
		#define prvREG64_HI(reg_addr) ( ( (volatile uint32_t *)reg_addr )[1] )
		#define prvREG64_LO(reg_addr) ( ( (volatile uint32_t *)reg_addr )[0] )

		uint32_t ulCurrentTimeHigh, ulCurrentTimeLow;
		do
		{
			ulCurrentTimeHigh = prvREG64_HI( pullMtimeRegister );
			ulCurrentTimeLow = prvREG64_LO( pullMtimeRegister );
		} while ( ulCurrentTimeHigh != prvREG64_HI( pullMtimeRegister ) );

		return ( ( ( uint64_t ) ulCurrentTimeHigh ) << 32 ) | ulCurrentTimeLow;
	#else
		return *pullMtimeRegister;
	#endif
}

__ilm__ unsigned _ktime(void)
{
    uint64_t t = _mtime();
    /*
     * return usec unit
     * for example,
     *     t / (40,000,000 / 4 / 1,000,000) = t / 10
     */
    return t / (CONFIG_XTAL_CLOCK_HZ / CONFIG_MTIME_CLK_DIV / CONFIG_TIMER_HZ);
}

#endif
