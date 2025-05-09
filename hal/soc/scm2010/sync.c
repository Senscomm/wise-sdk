/*
 * Copyright (c) 2018-2019 Senscomm, Inc. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>

#include <hal/device.h>
#include <hal/console.h>
#include <hal/io.h>
#include <hal/cmsis/cmsis_os2.h>
#include "FreeRTOS_tick_config.h"

#include "soc.h"

#define N22_HART_ID                             1
#define PLIC_INT_ENABLE_REG(reg)                ( \
                                                 NDS_PLIC_BASE + \
                                                 PLIC_ENABLE_OFFSET + \
                                                 (N22_HART_ID << PLIC_ENABLE_SHIFT_PER_TARGET) + \
                                                 (reg << 2))
#define PLIC_SW_INT_ENABLE_REG(reg)              ( \
                                                 NDS_PLIC_SW_BASE + \
                                                 PLIC_ENABLE_OFFSET + \
                                                 (N22_HART_ID << PLIC_ENABLE_SHIFT_PER_TARGET) + \
                                                 (reg << 2))
static uint8_t g_sync_wait;
osSemaphoreId_t g_sync_signal;

static void sync_thread(void *param)
{
    volatile uint32_t plic_en0;
    volatile uint32_t plic_en1;
    volatile uint32_t plic_sw_en0;
    volatile uint32_t plic_sw_en1;
    volatile uint64_t mTimeComapre;

    while (1) {
        osSemaphoreAcquire(g_sync_signal, osWaitForever);

        mTimeComapre = prvStopMtimeIrq();

        plic_en0 = readl(PLIC_INT_ENABLE_REG(0));
        plic_en1 = readl(PLIC_INT_ENABLE_REG(1));

        plic_sw_en0 = readl(PLIC_SW_INT_ENABLE_REG(0));
        plic_sw_en1 = readl(PLIC_SW_INT_ENABLE_REG(1));

        /* hw plic interrupt disable */
        writel(0, PLIC_INT_ENABLE_REG(0));
        writel(1 << (CONFIG_SYNC_RX_IRQ - 32), PLIC_INT_ENABLE_REG(1));
        /* sw plic interrupt disable */
        writel(0, PLIC_SW_INT_ENABLE_REG(0));
        writel(0, PLIC_SW_INT_ENABLE_REG(1));

        prvMIE_DISABLE();

        __nds__plic_set_pending(CONFIG_SYNC_TX_IRQ);

        __asm volatile( "wfi" );

        prvWriteMtimecmp(mTimeComapre);

        /* hw plic interrupt restore */
        writel(plic_en0, PLIC_INT_ENABLE_REG(0));
        writel(plic_en1, PLIC_INT_ENABLE_REG(1));
        /* sw plic interrupt restore */
        writel(plic_sw_en0, PLIC_SW_INT_ENABLE_REG(0));
        writel(plic_sw_en1, PLIC_SW_INT_ENABLE_REG(1));

        prvMIE_ENABLE();
    }
}

static int sync_irq(int irq, void *data)
{
    volatile uint8_t *wait = data;

    if (*wait) {
        osSemaphoreRelease(g_sync_signal);
        *wait = 0;
    } else {
        *wait = 1;
        __nds__plic_set_pending(CONFIG_SYNC_TX_IRQ);
    }

    return 0;
}

void scm2010_sync_init(void)
{
	osThreadAttr_t attr = {
		.name 		= "sync",
		.stack_size = 512,
		.priority 	= osPriorityNormal,
	};

    g_sync_signal = osSemaphoreNew(1, 0, NULL);
    if (g_sync_signal == NULL) {
        printk("failed to create sync_signal\n");
        return;
    }

	if (osThreadNew(sync_thread, NULL, &attr) == NULL) {
		printk("%s: failed to create sync task\n", __func__);
        return;
    }

    g_sync_wait = 1;
    if (request_irq(CONFIG_SYNC_RX_IRQ, sync_irq, "sync", 1, &g_sync_wait)) {
        printk("failed to request irq\n");
        return;
    }
}
