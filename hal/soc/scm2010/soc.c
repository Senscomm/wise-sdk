/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <soc.h>
#include <linker.h>
#include <cmsis_os.h>
#include <FreeRTOS/FreeRTOS.h>
#include <hal/ndsv5/core_v5.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/machine.h>
#include <hal/console.h>
#include <hal/irq.h>
#include <hal/clk.h>
#include <hal/timer.h>
#include <hal/init.h>
#include <hal/ipc.h>
#include <hal/wlan.h>

#include <cli.h>

#include "pmu.h"
#include "hal/pm.h"


#ifdef CONFIG_PORT_NEWLIB

size_t heap_total_size = 0;

DECLARE_SECTION_INFO(heap);

void heap_init(void)
{
	heap_total_size = SSIZE(heap);
}

#else

#define start pucStartAddress
#define size  xSizeInBytes

#ifdef CONFIG_HEAP_AUTO_SIZE

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
static HeapRegion_t xHeapRegions[2];
static HeapRegion_t xHeapRegionsDMA[3];
#else
static HeapRegion_t xHeapRegions[4];
#endif
size_t heap_total_size = 0;

DECLARE_SECTION_INFO(heap);
DECLARE_SECTION_INFO(heapext1);
DECLARE_SECTION_INFO(heapext2);

#else

u8 ucHeap[ CONFIG_HEAP1_SIZE ] __section(".heap");
#ifdef CONFIG_N22_ONLY
u8 ucHeapx1[ CONFIG_HEAP2_SIZE ] __section(".heap_ext1");
u8 ucHeapx2[ CONFIG_HEAP3_SIZE ] __section(".heap_ext2");
#endif

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC

static HeapRegion_t xHeapRegions[] = {
  { ucHeap, CONFIG_HEAP1_SIZE },
  { NULL,   0                     }
};

static HeapRegion_t xHeapRegionsDMA[] = {
#ifdef CONFIG_N22_ONLY
  { ucHeapx1, CONFIG_HEAP2_SIZE },
  { ucHeapx2, CONFIG_HEAP3_SIZE },
#endif
  { NULL,   0                     }
};

#else

static HeapRegion_t xHeapRegions[] = {
  { ucHeap, CONFIG_HEAP1_SIZE },
#ifdef CONFIG_N22_ONLY
  { ucHeapx1, CONFIG_HEAP2_SIZE },
  { ucHeapx2, CONFIG_HEAP3_SIZE },
#endif
  { NULL,   0                     }
};

#endif

#endif

void heap_init(void)
{
	int i;
#ifdef CONFIG_HEAP_AUTO_SIZE
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC

	xHeapRegions[0].start = (unsigned char *)__heap_start;
	xHeapRegions[0].size = __heap_end - __heap_start;
#ifdef CONFIG_N22_ONLY
	xHeapRegionsDMA[0].start = (unsigned char *)__heapext1_start;
	xHeapRegionsDMA[0].size = __heapext1_end - __heapext1_start;
	xHeapRegionsDMA[1].start = (unsigned char *)__heapext2_start;
	xHeapRegionsDMA[1].size = __heapext2_end - __heapext2_start;
#endif
#else
	xHeapRegions[0].start = (unsigned char *)__heap_start;
	xHeapRegions[0].size = __heap_end - __heap_start;
#ifdef CONFIG_N22_ONLY
	xHeapRegions[1].start = (unsigned char *)__heapext1_start;
	xHeapRegions[1].size = __heapext1_end - __heapext1_start;
	xHeapRegions[2].start = (unsigned char *)__heapext2_start;
	xHeapRegions[2].size = __heapext2_end - __heapext2_start;
#endif

#endif
#endif

	for (i = 0; xHeapRegions[i].pucStartAddress; i++) {
		memset(xHeapRegions[i].start, 0, xHeapRegions[i].size);
#ifdef CONFIG_HEAP_AUTO_SIZE
		heap_total_size += xHeapRegions[i].size;
#endif
	}

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
	for (i = 0; xHeapRegionsDMA[i].pucStartAddress; i++) {
		memset(xHeapRegionsDMA[i].start, 0, xHeapRegionsDMA[i].size);
#ifdef CONFIG_HEAP_AUTO_SIZE
		heap_total_size += xHeapRegionsDMA[i].size;
#endif
	}
#endif

	vPortDefineHeapRegions (xHeapRegions);

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
	if (xHeapRegionsDMA[0].start != NULL) {
		/* This function must be called after vPortDefineHeapRegions */
		vPortDefineHeapRegionsDMA (xHeapRegionsDMA);
	}
#endif
}

#endif

__attribute__((weak)) char *_sbrk(int incr) { return 0; }

/* SCM2010 SOC IPs */
#ifdef CONFIG_SERIAL
static declare_device_array(uart, 3) = {
#ifdef CONFIG_USE_UART0
	{
		.name = "atcuart.0",
		.base[0] = (void *) UART0_BASE_ADDR,
		.irq[0] = IRQn_UART1,
		.pri[0] = 1,
	},
#endif
#ifdef CONFIG_USE_UART1
	{
		.name = "atcuart.1",
		.base[0] = (void *) UART1_BASE_ADDR,
		.irq[0] = IRQn_UART2,
		.pri[0] = 1,
	},
#endif
#ifdef CONFIG_USE_UART2
	{
		.name = "atcuart.2",
		.base[0] = (void *) UART2_BASE_ADDR,
		.irq[0] = IRQn_UART3,
		.pri[0] = 1,
	},
#endif
};
#endif
#ifdef CONFIG_TIMER
/* PIT0 timers : ch0 ~ ch3 */
/*
 * Timer0 - Timer3 reside in the PIT0 HW block
 * each corresponding to channel0 - channel3 and
 * therefore, sharing the same base address and irq
 */
static struct device timer[] __device__(2) = {
#ifdef CONFIG_USE_TIMER0
	{
		.name = "timer.0", /* timer0 */
		.base[0] = (void *) TIMER0_BASE_ADDR,
		.irq[0] = IRQn_TIMER1,
		.pri[0] = 1,
	},
#endif
#ifdef CONFIG_USE_TIMER1
	{
		.name = "timer.1", /* timer1 */
		.base[0] = (void *) TIMER1_BASE_ADDR,
		.irq[0] = IRQn_TIMER2,
		.pri[0] = 1,
	},
#endif
};
#endif
static struct device system_peripherals[] __device__(1) = {
#ifdef CONFIG_PINCTRL_SCM2010
	{
		.name = "scm2010,pinctrl",
		.base[0] = (void *) GPIO_BASE_ADDR,
		.base[1] = (void *) IOMUX_BASE_ADDR,
		.irq[0] = IRQn_GPIO,
		.pri[0] = 1,
	},
#endif
#ifdef CONFIG_RTC_ATCRTC
	{
		.name = "atcrtc",
		.base[0] = (void *) RTC_BASE_ADDR,
		.irq[0] = IRQn_RTC_ALARM,
		.pri[0] = 1,
	},
#endif
#ifdef CONFIG_WDT_ATCWDT
	{
		.name = "atcwdt",
		.base[0] = (void *) WDT_BASE_ADDR,
		.irq[0] = -1,
		.pri[0] = 0,
	},
#endif
};

static declare_device_array(spi, 2) = {
#ifdef CONFIG_USE_SPI0
    {
#ifdef CONFIG_SPI_FLASH
        .name = "atcspi200-xip.0",
        .base[0] = (void *) SPI0_BASE_ADDR,
        .base[1] = (void *) FLASH_BASE,
#else
        .name = "atcspi.0",
        .base[0] = (void *) SPI0_BASE_ADDR,
		.irq[0] = IRQn_SPI1,
		.pri[0] = 1,
#endif
    },
#endif
#ifdef CONFIG_USE_SPI1
    {
        .name = "atcspi.1",
        .base[0] = (void *) SPI1_BASE_ADDR,
		.irq[0] = IRQn_SPI2,
		.pri[0] = 1,
    },
#endif
#ifdef CONFIG_USE_SPI2
    {
        .name = "atcspi.2",
        .base[0] = (void *) SPI2_BASE_ADDR,
		.irq[0] = IRQn_SPI3,
		.pri[0] = 1,
    }
#endif
};

static declare_device_array(i2c, 3) = {
#ifdef CONFIG_USE_I2C0
    {
        .name = "atci2c.0",
        .base[0] = (void *) I2C0_BASE_ADDR,
		.irq[0] = IRQn_I2C1,
		.pri[0] = 1,
    },
#endif
#ifdef CONFIG_USE_I2C1
    {
        .name = "atci2c.1",
        .base[0] = (void *) I2C1_BASE_ADDR,
		.irq[0] = IRQn_I2C2,
		.pri[0] = 1,
    },
#endif
};

#ifdef CONFIG_I2C_GPIO
static declare_device_array(i2c_gpio, 2) = {
	{
		.name = "i2c-gpio"
	}
};
#endif

#ifdef CONFIG_PTA
/* pta must be initialized before rf, put pta in device 1 */
static declare_device_single(pta, 2) = {
	.name = "pta",
	.base[0] = (void *) COEX_BASE_START,
	.base[1] = (void *) PTA_BASE_START,
};
#endif

#ifdef CONFIG_I2S
static declare_device_single(i2s, 2) = {
	.name = "python-i2s",
	.base[0] = (void *) SYS(I2S_CFG(0)),
	.base[1] = (void *) I2S_BASE_ADDR,
};
#endif

#ifdef CONFIG_WLAN
static declare_device_array(wlan, 3) = {
	{
		.name = "scm2020-wlan",

		.flags = (0
#ifdef CONFIG_SUPPORT_DUAL_VIF
					| WLAN_DEV_F_DUAL_VIF_EN
#endif
					),
		.base[0] = (void *) MAC_BASE_START,
		.base[1] = (void *) PHY_BASE_START,
		.irq[0] = IRQn_WLAN0,
		.irq[1] = -1, /*IRQn_WLAN1,*/
		.pri[0] = 1,
		.pri[1] = 1,
	},
};
#endif

#ifdef CONFIG_RF
static declare_device_single(rf, 3) = {
	.name = "rf",
	.base[0] = (void *) RF_BASE_START,
	.irq[0] = -1,
};
#endif

#ifdef CONFIG_BLE
static declare_device_single(ble, 4) = {
	.name = "ble",
	.irq[0] = IRQn_BLE1,
	.pri[0] = 1,
};
#endif

#ifdef CONFIG_SYSTIMER
static declare_device_single(systimer, 3) = {
	.name = "systimer",
	.base[0] = (void *) SYS_TIMER_BASE_ADDR,
	.irq[0] = IRQn_BLE_TIMER,
	.pri[0] = 1,
};
#endif

#ifdef CONFIG_IPC
static struct device shm_ipc __device__(3) = {
    .name = "scm2010-ipc",
    .base[0] = (u8 *)__shm2_start,
    .base[1] = (u8 *)__shm2_end,
    .base[2] = (u8 *)__shm1_start,
    .base[3] = (u8 *)__shm1_end,
    .irq[0] = CONFIG_SCM2010_IPC_RX_IRQ,
    .irq[1] = CONFIG_SCM2010_IPC_TX_IRQ,
    .pri[0] = 1,
    .pri[1] = 1,
};
#endif

#ifdef CONFIG_DMA
static declare_device_array(dma, 3) = {
	{
		.name = "dmac.0",
		.base[0] = (void *)DMAC0_BASE_ADDR,
		.irq[0] = IRQn_DMAC1,
		.pri[0] = 1,
	},
#if CONFIG_SCM2010_DMAC_NUM > 1
	{
		.name = "dmac.1",
		.base[0] = (void *)DMAC1_BASE_ADDR,
		.irq[0] = IRQn_DMAC2,
		.pri[0] = 1,
	}
#endif
};

#endif

#ifdef CONFIG_HW_CRYPTO
static declare_device_single(crypto_pke, 3) = {
    .name = "pke",
    .base[0] = (void *)PKE_BASE_ADDR,
	.irq[0] = -1,
    .pri[0] = 0,
};

static declare_device_single(crypto_trng, 3) = {
    .name = "trng",
    .base[0] = (void *)TRNG_BASE_ADDR,
    .irq[0] = -1,
    .pri[0] = 0,
};
#endif

#ifdef CONFIG_EFUSE
static declare_device_single(efuse, 3) = {
	.name = "efuse-scm2010",
	.base[0] = (void *) EFUSE_BASE_ADDR,
	.irq[0] = -1,
};
#endif

#ifdef CONFIG_TINYUSB
static declare_device_single(usb, 4) = {
	.name = "musbhsfc.0",
    .base[0] = (void *)USB_BASE_ADDR,
    .base[1] = (void *)USB_DMAC_ADDR,
	.irq[0] = IRQn_USB,
	.irq[1] = IRQn_USB_DMA,
	.pri[0] = 1,
	.pri[1] = 1,
};

extern void start_tusb(void);
#endif

#ifdef CONFIG_SDIO
static declare_device_single(sdio, 3) = {
    .name = "sdio",
    .base[0] = (void *)SDIO_BASE_ADDR,
    .base[1] = (void *)SYS(SDIO_CFG(0)),
    .base[2] = (void *)SYS(SDIO_BAR(0)),
    .irq[0] = IRQn_SDIO,
    .pri[0] = 1,
};
#endif

#ifdef CONFIG_USE_AUXADC
static declare_device_single(adc, 2) = {
	.name = "auxadc-xrc",
    .base[0] = (void *)SYS(AUXADC_CFG),
    .base[1] = (void *)SYS(AUXADC_CTRL),
    .base[2] = (void *)SYS(AUXADC_DATA(0)),
    .irq[0] = IRQn_AUXADC,
    .pri[0] = 1,
};
#endif

__attribute__((weak)) void clock_init(void) {}

void clock_postinit(void)
{
#if defined(CONFIG_USE_DEFAULT_CLK) || defined(CONFIG_IPC)
	/* If there is a IPC host, e.g., NuttX, it will take care
	 * of setting up the clock tree.
	 */
	return;
#else
	struct clk *div_pclk;
	struct clk *div_hclk;
	struct clk *div_n22;
	struct clk *div_d25;
	struct clk *mux_core;
	struct clk *pll_480m;
	u32 flags;

	local_irq_save(flags);

	/*
	 * Clock settings shall be changed from the leaf
	 * to the source not to exceed any hard limitations
	 * that are imposed by peripherals.
	 */

	div_pclk = clk_get(NULL, "div_pclk");
	div_hclk = clk_get(NULL, "div_hclk");
	div_n22 = clk_get(NULL, "div_n22");
	div_d25 = clk_get(NULL, "div_d25");
	mux_core = clk_get(NULL, "core");
	pll_480m = clk_get(NULL, "pll_480m");

	/*
	 * 1. Set APB divider to 1/2.
	 * 2. Set AHB divider to 1/2.
	 * 3. Set N22 divider to 1.
	 * 4. Set D25 divider to 3 or 2.
	 * 5. Switch to PLL.
	 */
	clk_set_div(div_pclk, 2);
	clk_set_div(div_hclk, 2);
	clk_set_div(div_n22, 1);
#ifdef CONFIG_SUPPORT_CORE_240MHZ
	clk_set_div(div_d25, 2);
#else
	clk_set_div(div_d25, 3); /* 160MHz */
#endif
	clk_set_parent(mux_core, pll_480m);

	local_irq_restore(flags);
#endif
}

int scm2010_early_init(void)
{
	clock_init();

	/* Need to check PLL enabled */
	clock_postinit();

	heap_init();

	return 0;
}
__initcall__(early, scm2010_early_init);

__attribute__((weak)) int pinctrl_request_pin(struct device *device, const char *id, unsigned pin)
{
	return 0;
}

__attribute__((weak)) int pinctrl_free_pin(struct device *device, const char *id, unsigned pin)
{
	return 0;
}

__attribute__((weak)) struct pinctrl_pin_map *pinctrl_lookup_platform_pinmap(struct device *device, const char *id)
{
	return NULL;
}

void hal_timer_init(void)
{
#ifdef CONFIG_WALL_TIMER

	char name[16] = {0,};
	struct device *timer;

	strcpy(name, "timer.");
	name[strlen(name)] = '0' + CONFIG_WALL_TIMER_ID;
	name[strlen(name)] = 0;

	timer = device_get_by_name(name);
	if (!timer) {
		printk("timer not found\n");
	}
	device_bind_driver(timer, NULL);

	timer_setup(timer, 0, HAL_TIMER_FREERUN, time_hz, NULL, NULL);
	timer_start(timer, 0);
	register_timer(timer);
#endif
}

void set_reset_vec(u8 hart, u32 addr)
{
    writel(addr, SMU(RESET_VECTOR(hart)));
}

void park(void)
{
	unsigned long f;

	hal_fini(subsystem);
	hal_fini(filesystem);
	hal_fini(early);
	hal_fini(arch);

    driver_deinit();

	/* reset to the ROM */
	set_reset_vec(0, 0x00100000);
	set_reset_vec(1, 0x00100000);

	/* NB: Need to be after driver_deinit() because
	 * driver_deinit() might enable interrupts globally
	 * during pPortFree().
	 */

	local_irq_save(f);

	/* Invalidate all I-Cache lines AND
	 * write back all D-Cache lines, if applicable.
	 */

	__nds__fencei();

	(void) f;
}

#ifdef CONFIG_SCM2010_IPC_SYNC

int sync_ipc(void)
{
	struct device * ipc_dev;

	ipc_dev = device_get_by_name("scm2010-ipc");
	return ipc_sync(ipc_dev);
}

#endif

#ifdef CONFIG_CMD_JUMP

int do_jump(int argc, char *argv[])
{
	unsigned long addr;
	void (*entry)(void);

	if (argc < 2)
		return CMD_RET_USAGE;

	addr = strtoul(argv[1], NULL, 16);
	entry = (void (*)(void))addr;

	park();

	(*entry)();

	/* will never be reached */

	return CMD_RET_SUCCESS;
}

CMD(jump, do_jump,
	"jump <address(hex)>",
	"jump to an address(hex)"
);

#endif

#ifdef CONFIG_CMD_RESET

int do_reset(int argc, char *argv[])
{
	u32 addr;
    u8 hart = __nds__mfsr(NDS_MHARTID);

	if (argc < 2) {
		addr = (u32)__init_start;
	} else {
		addr = strtoul(argv[1], NULL, 16);
	}

	park();

	set_reset_vec(hart, addr);

	writel(0x1 << (hart * 4), SYS(CORE_RESET_CTRL));

	/* will never be reached */

	return CMD_RET_SUCCESS;
}

CMD(reset, do_reset,
	"reset <address(hex)>",
	"reset to an address(hex)"
);

#endif

#ifdef CONFIG_HOSTBOOT

#include <version.h>

void soc_get_revinfo(struct sncmf_rev_info *revinfo)
{
#ifdef CONFIG_BLE
	revinfo->chipnum = 0x2011;
#else
	revinfo->chipnum = 0x2010;
#endif
	/* SoC version? */
	revinfo->chiprev = 0x1;
}

#endif

void soc_init(void)
{
	u32 v;
	printk("SOC: SCM2010\n");

	/* Clear enable CHIP SLEEP */
	v = readl(SMU(LOWPOWER_CTRL));
	v &= ~(1 << 14);
	writel(v, SMU(LOWPOWER_CTRL));

	/* Restore from Hibneration */
	v = readl(SMU(TOP_CFG));
	if (v & ( 1 << 5)) {
		/* DCDC backup to 1.2V */
		pmu_set_dcdc_volt(PMU_DCDC_1V2);

		/* disable capless LDO low power mode */
		v &= ~(1 << 5);
		writel(v, SMU(TOP_CFG));

		/* capless LDO backup to 0.8V */
		pmu_set_dldo_volt(PMU_DLDO_0V8);
	}

	/* Enable interrupts (MEIE, MSIE) */
	set_csr(NDS_MIE, MIP_MEIP | MIP_MSIP);

    /* MTIP will be turned on in vPortSetupTimerInterrupt */

    /* Enable MIE in MSTATUS */
    set_csr(NDS_MSTATUS, MSTATUS_MIE);

	/* Enable system reset by WDT2 */
	v = readl(SMU(TOP_CFG));
	writel(v | (1 << 2), SMU(TOP_CFG));

#ifdef CONFIG_BLE_NIMBLE_CTRL
	extern void nimble_port_init(void);
	nimble_port_init();
#endif

#ifdef CONFIG_TINYUSB
#ifndef CONFIG_BOOTROM
    start_tusb();
#endif
#endif

#ifdef CONFIG_SDIO_NETIF
extern int sdio_netif_init(void);
	if (sdio_netif_init() < 0) {
		printk("sdio_netif_init: error!\n");
	}
#endif

    pm_init();

#ifdef CONFIG_SYNC_FLASH_ACCESS
extern void scm2010_sync_init(void);
    scm2010_sync_init();
#endif
}
