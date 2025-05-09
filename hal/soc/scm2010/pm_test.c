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

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <FreeRTOS/FreeRTOS.h>

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/pm.h>
#include <sys/queue.h>
#include <hal/cmsis/cmsis_os2.h>
#include <hal/device.h>
#include <hal/pinctrl.h>
#include <hal/timer.h>
#include <linker.h>
#include <cli.h>

#include "pm_rram.h"
#include "pm_test.h"
#include "pmu.h"

#define PM_TEST_TIMEOUT						3000    //ms
#define PM_TEST_WATCHER_PERIOD				100000   //us

static char *pmu_mode_str[] = {
	"ACTIVE_TO_DEEP_SLEEP_0",
	"ACTIVE_TO_DEEP_SLEEP_0_IO_OFF",
	"ACTIVE_TO_DEEP_SLEEP_1",
	"ACTIVE_TO_DEEP_SLEEP_1_IO_OFF",
	"ACTIVE_TO_HIBERNATION",
	"ACTIVE_TO_HIBERNATION_IO_OFF",
	"ACTIVE_IO_OFF_TO_LIGHT_SLEEP_IO_OFF",
	"ACTIVE_TO_SLEEP",
	"ACTIVE_TO_SLEEP_IO_OFF",
	"ACTIVE_TO_IDLE",
	"ACTIVE_TO_IDLE_IO_OFF",
	"ACTIVE_TO_LIGHT_SLEEP",
	"ACTIVE_TO_ACTIVE_IO_OFF",
	"ACTIVE_IO_OFF_TO_ACTIVE",
	"WAKEUP_TO_ACTIVE",
	"WAKEUP_TO_ACTIVE_IO_OFF",
};

static char *wakeup_src_str[] = {
	"GPIO",
	"UART0 LOSSY",
	"UART0 LOSSLESS",
	"UART1 LOSSY",
	"UART1 LOSSLESS",
	"RTC",
	"USB",
	"SDIO",
};

static struct pm_test_ctx pmt_ctx;
static uint32_t g_repeat_cnt;

/* UART not used for the CONSOLE */
#if (CONFIG_SERIAL_CONSOLE_PORT == 0)
#define UART_CONSOLE_BASE   UART0_BASE_ADDR
#define UART_TEST_BASE      UART1_BASE_ADDR
#elif (CONFIG_SERIAL_CONSOLE_PORT == 1)
#define UART_CONSOLE_BASE   UART1_BASE_ADDR
#define UART_TEST_BASE      UART0_BASE_ADDR
#endif

#ifndef CONFIG_PM_MULTI_CORE
static void uart_print(char *str, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		writel(str[i], UART_TEST_BASE + 0x20);
		while(1) {
			if((readl(UART_TEST_BASE + 0x34) & 0x60) == 0x60) {
				break;
			}
		}
	}
}

static void setup_uart_lossless_wakeup(void)
{
	int os = readl(UART_TEST_BASE + 0x14) & 0x1F;
	int lc = readl(UART_TEST_BASE + 0x2C);
	int freq = 40000000;
	int div;

#if (CONFIG_SERIAL_CONSOLE_PORT == 0)
	/* enable hart0 UART1 PLIC for test */
	writel(1 << (9 & 0x1F), NDS_PLIC_BASE + PLIC_ENABLE_OFFSET);

	/* set gpio 0 = mode 0, gpio 01 = mode 4 */
	uint32_t pmx = readl(GPIO_BASE_ADDR + 0x20);
	pmx &= ~0x000000FF;
	writel(pmx, GPIO_BASE_ADDR + 0x20);
#else
	/* enable hart0 UART0 PLIC for test */
	writel(1 << (8 & 0x1F), NDS_PLIC_BASE + PLIC_ENABLE_OFFSET);

	/* set gpio 21 = mode 0, gpio 22 = mode 4 */
	uint32_t pmx = readl(GPIO_BASE_ADDR + 0x28);
	pmx &= ~0x0FF00000;
	writel(pmx, GPIO_BASE_ADDR + 0x28);
#endif

	div = freq / (115200 * os);
	if (abs(div * 115200 * os - freq) > abs((div +1) * 115200 * os - freq)) {
		div++;
	}

	lc |= 0x80;
	writel(lc, UART_TEST_BASE + 0x2C);

	writel((uint8_t)(div >> 8), UART_TEST_BASE + 0x24);
	writel((uint8_t)div, UART_TEST_BASE + 0x20);

	lc &= ~(0x80);
	writel(lc, UART_TEST_BASE + 0x2C);

	writel(3, UART_TEST_BASE + 0x2C);
	writel(5, UART_TEST_BASE + 0x28);
	writel(1, UART_TEST_BASE + 0x24);

	char *esc = "\E[H\E[J";
	char *start_log = "UART0 opened, Press key to wakeup\n";

	uart_print(esc, strlen(esc));
	uart_print(start_log, strlen(start_log));

	pmt_ctx.uart_enabled = 1;

}
#endif

static void pm_test_thread(void *param)
{
	struct pm_test_ctx *ctx;
	uint32_t timeout;

	ctx = (struct pm_test_ctx *)param;
	timeout = osWaitForever;

#ifndef CONFIG_PM_MULTI_CORE
	setup_uart_lossless_wakeup();
#endif

	while (1) {
		osSemaphoreAcquire(ctx->test_sig, timeout);

		if (g_repeat_cnt == 0) {
			timeout = osWaitForever;
			ctx->flag = TEST_STOP;
			pm_stay(PM_TEST);
			printf("PM test End\n");
			continue;
		}

		pm_relax(PM_TEST);

		switch (ctx->flag) {
			case TEST_WITH_CONFIG:
			case TEST_WITH_CONFIG_REPEAT:
			case TEST_WITH_REPEAT_AND_CHANGE_MODE:
				if (ctx->wakeup_src & WAKEUP_SRC_RTC) {
					if (ctx->disable_timeout == 0) {
						timeout = (ctx->lowpower_dur);
					}
				}
				break;
			default:
				timeout = osWaitForever;
				break;
		}

		if (pmt_ctx.flag == TEST_WITH_REPEAT_AND_CHANGE_MODE) {
			if (pmt_ctx.lowpower_mode == ACTIVE_TO_LIGHT_SLEEP) {
				pmt_ctx.lowpower_mode = ACTIVE_TO_DEEP_SLEEP_1;
			} else if (pmt_ctx.lowpower_mode == ACTIVE_TO_DEEP_SLEEP_1) {
				pmt_ctx.lowpower_mode = ACTIVE_TO_DEEP_SLEEP_1_IO_OFF;
			} else if (pmt_ctx.lowpower_mode == ACTIVE_TO_DEEP_SLEEP_1_IO_OFF) {
				pmt_ctx.lowpower_mode = ACTIVE_TO_HIBERNATION;
			} else if (pmt_ctx.lowpower_mode == ACTIVE_TO_HIBERNATION) {
				pmt_ctx.lowpower_mode = ACTIVE_TO_HIBERNATION_IO_OFF;
			} else {
				pmt_ctx.lowpower_mode = ACTIVE_TO_LIGHT_SLEEP;
			}
		}

		printf("t:%d(%d)\n", ctx->flag, g_repeat_cnt);

		if (g_repeat_cnt) {
			g_repeat_cnt--;
		}
	}
}

static void gpio_wakeup_irq(u32 irq, void *ctx)
{
	printk("GPIO IRQ handler\n");
}

static void setup_gpio_wakeup(void)
{
	struct device *dev;

	dev = device_get_by_name("scm2010,pinctrl");
	if (!dev) {
		printk("no pinctrl device\n");
		return;
	}

	gpio_request(dev, "gpio6", 6);

	if (gpio_direction_input(6) < 0) {
		printk("gpio direction failed\n");
		return;
	}

	if (gpio_set_config(6, PIN_CONFIG_MK_CMD(PIN_CONFIG_PULL_UP, 0)) < 0) {
		printk("gpio pull down failed\n");
		return;
	}

	if (gpio_interrupt_enable(6, PIN_INTR_FALL_EDGE, gpio_wakeup_irq, NULL) < 0) {
		printk("interrupt setup failed\n");
		return;
	}
	pm_enable_wakeup_io(6);
}

void pm_test_log(int pm_mode)
{
	static const char *pm_log_str[] = {
		[PM_MODE_ACTIVE]		= "WK",
		[PM_MODE_IDLE]			= "ID",
		[PM_MODE_LIGHT_SLEEP]	= "LS",
		[PM_MODE_SLEEP]			= "SL",
		[PM_MODE_DEEP_SLEEP_0]	= "D0",
		[PM_MODE_DEEP_SLEEP_1]	= "D1",
		[PM_MODE_HIBERNATION]	= "HI",
	};

	writel(pm_log_str[pm_mode][0], UART_CONSOLE_BASE + 0x20);
	writel(pm_log_str[pm_mode][1], UART_CONSOLE_BASE + 0x20);
	writel('\n', UART_CONSOLE_BASE + 0x20);
	while(1) {
		if((readl(UART_CONSOLE_BASE + 0x34) & 0x60) == 0x60) {
			break;
		}
	}
}

int pm_test_init(void)
{
	osThreadAttr_t attr = {
		.name       = "pm_test",
		.stack_size = 1024 * 4,
		.priority   = osPriorityNormal,

	};

	g_repeat_cnt = 0;

	pmt_ctx.flag = TEST_STOP;
	pmt_ctx.lowpower_mode = ACTIVE_TO_DEEP_SLEEP_1;
	pmt_ctx.wakeup_mode = WAKEUP_TO_ACTIVE;
	pmt_ctx.wakeup_src = WAKEUP_SRC_ALL;
	pmt_ctx.lowpower_dur = 3000;
	pmt_ctx.uart_enabled  = -1;
	pmt_ctx.rram = (struct pm_rram_info *)SCM2010_PM_RRAM_INFO_ADDR;

	pmt_ctx.test_sig = osSemaphoreNew(1, 0, NULL);
	if (pmt_ctx.test_sig == NULL) {
		printk("err: create pm sem\n");
		return -1;
	}

	if (!osThreadNew(pm_test_thread, &pmt_ctx, &attr)) {
		printk("err: create pm task\n");
		return -1;
	}

	setup_gpio_wakeup();

	pm_stay(PM_TEST);
	pm_relax(PM_DEVICE_APP);
	pm_register_handler(pm_test_log);

	memset((void *)0x4001F000, 0, 4096);

	return 0;
}

struct pm_test_ctx *pm_test_get_config(void)
{
	return &pmt_ctx;
}

void pm_test_stop(void)
{
	printk("Test cacnel\n");

	g_repeat_cnt = 0;

	osSemaphoreRelease(pmt_ctx.test_sig);
}

void pm_test_complete(void)
{
	if (pmt_ctx.flag == TEST_WITH_CONFIG ||
	    pmt_ctx.flag == TEST_WITH_CONFIG_REPEAT ||
	    pmt_ctx.flag == TEST_WITH_REPEAT_AND_CHANGE_MODE) {

		uint32_t flags;

		local_irq_save(flags);

		osSemaphoreRelease(pmt_ctx.test_sig);

		local_irq_restore(flags);
	}
}

static int do_pm_lowpower_mode_cfg(int argc, char *argv[])
{
	int lowpower_mode;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	lowpower_mode = atoi(argv[1]);

	if (lowpower_mode > ACTIVE_IO_OFF_TO_ACTIVE) {
		printf("err: invalid mode\n");
		return CMD_RET_FAILURE;
	};

#ifdef CONFIG_XIP
	if (lowpower_mode == ACTIVE_TO_ACTIVE_IO_OFF ||
			lowpower_mode == ACTIVE_IO_OFF_TO_ACTIVE ||
			lowpower_mode == ACTIVE_IO_OFF_TO_LIGHT_SLEEP_IO_OFF) {
		printf("err: IO off for XIP\n");
		return CMD_RET_FAILURE;
	}
#else
	if (lowpower_mode == ACTIVE_TO_HIBERNATION ||
			lowpower_mode == ACTIVE_TO_HIBERNATION_IO_OFF) {
		printf("can't work after wakeup from hibernation\n");
	}
#endif

	printf("Low power mode = %s\n", pmu_mode_str[lowpower_mode]);

	pmt_ctx.lowpower_mode = lowpower_mode;

	return CMD_RET_SUCCESS;
}

static int do_pm_wakeup_mode_cfg(int argc, char *argv[])
{
	int wakeup_mode;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	wakeup_mode = atoi(argv[1]);
	wakeup_mode += WAKEUP_TO_ACTIVE;

	if  (wakeup_mode > WAKEUP_TO_ACTIVE_IO_OFF) {
		printf("err: wakeup mode\n");
		return CMD_RET_FAILURE;
	}

	printf("wakeup mode = %s\n", pmu_mode_str[wakeup_mode]);

	pmt_ctx.wakeup_mode = wakeup_mode;

	return CMD_RET_SUCCESS;
}

static int do_pm_add_wakeup_src_cfg(int argc, char *argv[])
{
	int wakeup_src;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	wakeup_src = atoi(argv[1]);

	if (wakeup_src > TEST_SDIO_WAKEUP) {
		printf("err: wakeup source\n");
		return CMD_RET_FAILURE;
	}

	printf("Add ");
	if (wakeup_src == TEST_GPIO_WAKEUP) {
		printf("GPIO6\n");
		pmt_ctx.wakeup_src |= WAKEUP_SRC_GPIO;
	} else if (wakeup_src == TEST_UART0_LOSSY_WAKEUP) {
		printf("UART0 LOSSY\n");
		printf("can not wakeup from DEEP SLEEP 1 or HIBERNATION\n");
		pmt_ctx.wakeup_src |= WAKEUP_SRC_UART0_LOSSY;
	} else if (wakeup_src == TEST_UART0_LOSSLESS_WAKEUP) {
		printf("UART0 LOSSLESS\n");
		printf("can wakeup from only IDLE\n");
		pmt_ctx.wakeup_src |= WAKEUP_SRC_UART0_LOSSLESS;
	} else if (wakeup_src == TEST_UART1_LOSSY_WAKEUP) {
		printf("UART0 LOSSY\n");
		printf("can not wakeup from DEEP SLEEP 1 or HIBERNATION\n");
		pmt_ctx.wakeup_src |= WAKEUP_SRC_UART1_LOSSY;
	} else if (wakeup_src == TEST_UART1_LOSSLESS_WAKEUP) {
		printf("UART0 LOSSLESS\n");
		printf("can wakeup from only IDLE\n");
		pmt_ctx.wakeup_src |= WAKEUP_SRC_UART1_LOSSLESS;
	} else if (wakeup_src == TEST_RTC_WAKEUP) {
		printf("Add RTC\n");
		pmt_ctx.wakeup_src |= WAKEUP_SRC_RTC;
	} else if (wakeup_src == TEST_USB_WAKEUP) {
		printf("USB\n");
		printf("can not wakeup from DEEP SLEEP 1 or HIBERNATION\n");
		pmt_ctx.wakeup_src |= WAKEUP_SRC_USB;
	} else if (wakeup_src == TEST_SDIO_WAKEUP) {
		printf("SDIO\n");
		printf("can wakeup from only IDLE\n");
		pmt_ctx.wakeup_src |= WAKEUP_SRC_SDIO;
	}

	return CMD_RET_SUCCESS;
}

static int do_pm_del_wakeup_src_cfg(int argc, char *argv[])
{
	int wakeup_src;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	wakeup_src = atoi(argv[1]);

	if (wakeup_src > TEST_SDIO_WAKEUP) {
		printf("err: wakeup source\n");
		return CMD_RET_FAILURE;
	}

	if (wakeup_src == TEST_GPIO_WAKEUP) {
		printf("Del GPIO6\n");
		pmt_ctx.wakeup_src &= ~WAKEUP_SRC_GPIO;
	} else if (wakeup_src == TEST_UART0_LOSSY_WAKEUP) {
		printf("Del UART0 LOSSY\n");
		pmt_ctx.wakeup_src &= ~WAKEUP_SRC_UART0_LOSSY;
	} else if (wakeup_src == TEST_UART0_LOSSLESS_WAKEUP) {
		printf("Del UART0 LOSSLESS\n");
		pmt_ctx.wakeup_src &= ~WAKEUP_SRC_UART0_LOSSLESS;
	} else if (wakeup_src == TEST_UART1_LOSSY_WAKEUP) {
		printf("Del UART1 LOSSY\n");
		pmt_ctx.wakeup_src &= ~WAKEUP_SRC_UART1_LOSSY;
	} else if (wakeup_src == TEST_UART1_LOSSLESS_WAKEUP) {
		printf("Del UART1 LOSSLESS\n");
		pmt_ctx.wakeup_src &= ~WAKEUP_SRC_UART1_LOSSLESS;
	} else if (wakeup_src == TEST_RTC_WAKEUP) {
		printf("Del RTC\n");
		pmt_ctx.wakeup_src &= ~WAKEUP_SRC_RTC;
	} else if (wakeup_src == TEST_USB_WAKEUP) {
		printf("Del USB\n");
		pmt_ctx.wakeup_src &= ~WAKEUP_SRC_USB;
	} else if (wakeup_src == TEST_SDIO_WAKEUP) {
		printf("Del SDIO\n");
		pmt_ctx.wakeup_src &= ~WAKEUP_SRC_SDIO;
	}

	return CMD_RET_SUCCESS;
}

static int do_pm_lowpower_dur_cfg(int argc, char *argv[])
{
	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	pmt_ctx.lowpower_dur = atoi(argv[1]);

	printf("Low Power duration %dmsec\n", pmt_ctx.lowpower_dur);

	return CMD_RET_SUCCESS;
}

static int do_pm_pmu_irq(int argc, char *argv[])
{
	uint8_t en;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	en = atoi(argv[1]);

	if (en) {
		pmu_irq_enable();
	} else {
		pmu_irq_disable();
	}


	return CMD_RET_SUCCESS;
}

static int do_pm_show_cfg(int argc, char *argv[])
{
	printf("Low Power Mode: %s\n", pmu_mode_str[pmt_ctx.lowpower_mode]);
	printf("Wakeup Mode   : %s\n", pmu_mode_str[pmt_ctx.wakeup_mode]);
	printf("Wakeup Source\n");
	if (pmt_ctx.wakeup_src & WAKEUP_SRC_GPIO) {
		printf("\t%s\n", wakeup_src_str[0]);
	}
	if (pmt_ctx.wakeup_src & WAKEUP_SRC_UART0_LOSSY) {
		printf("\t%s\n", wakeup_src_str[1]);
	}
	if (pmt_ctx.wakeup_src & WAKEUP_SRC_UART0_LOSSLESS) {
		printf("\t%s\n", wakeup_src_str[2]);
	}
	if (pmt_ctx.wakeup_src & WAKEUP_SRC_UART1_LOSSY) {
		printf("\t%s\n", wakeup_src_str[3]);
	}
	if (pmt_ctx.wakeup_src & WAKEUP_SRC_UART1_LOSSLESS) {
		printf("\t%s\n", wakeup_src_str[4]);
	}
	if (pmt_ctx.wakeup_src & WAKEUP_SRC_RTC) {
		printf("\t%s\n", wakeup_src_str[5]);
	}
	if (pmt_ctx.wakeup_src & WAKEUP_SRC_USB) {
		printf("\t%s\n", wakeup_src_str[6]);
	}
	if (pmt_ctx.wakeup_src & WAKEUP_SRC_SDIO) {
		printf("\t%s\n", wakeup_src_str[7]);
	}

	if (pmt_ctx.wakeup_src & WAKEUP_SRC_RTC) {
		printf("Low Power Duration: %dmsec\n", pmt_ctx.lowpower_dur);
	}

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd pm_test_cfg_cmd[] = {
	CMDENTRY(sm, do_pm_lowpower_mode_cfg, "", ""),
	CMDENTRY(wm, do_pm_wakeup_mode_cfg, "", ""),
	CMDENTRY(wsa, do_pm_add_wakeup_src_cfg, "", ""),
	CMDENTRY(wsd, do_pm_del_wakeup_src_cfg, "", ""),
	CMDENTRY(sd, do_pm_lowpower_dur_cfg, "", ""),
	CMDENTRY(irq, do_pm_pmu_irq, "", ""),
	CMDENTRY(show, do_pm_show_cfg, "", ""),
};

static int do_pm_test_config(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], pm_test_cfg_cmd, ARRAY_SIZE(pm_test_cfg_cmd));
	if (cmd == NULL) {
		return CMD_RET_USAGE;
	}

	return cmd->handler(argc, argv);
}

CMD(pmtc, do_pm_test_config,
		"CLI commands for PM test configuration",
		"pmtc sm [sleep mode]\n"
		"\t[sleep mode]\n"
		"\t0 : Active to Deepsleep0\n"
		"\t1 : Active to Deepsleep0_IOoff\n"
		"\t2 : Active to Deepsleep1\n"
		"\t3 : Active to Deepsleep1_IOoff\n"
		"\t4 : Active to Hibernation\n"
		"\t5 : Active to Hibernation_IOoff\n"
		"\t6 : Active_IOoff to Lightsleep_IOoff\n"
		"\t7 : Active to Sleep\n"
		"\t8 : Active to Sleep_IOoff\n"
		"\t9 : Active to IDLE\n"
		"\t10: Active to IDLE_IOoff\n"
		"\t11: Active to Lightsleep\n"
		"\t12: Active to Active_IOoff\n"
		"\t13: Active_IOoff to Active" OR
		"pmtc wm [wakeup mode]\n"
		"\t[wakeup mode]\n"
		"\t0 : Wakeup to Active\n"
		"\t1 : Wakeup to Active_IOoff" OR
		"pmtc wsa [wakeup source to add]" OR
		"pmtc wsd [wakeup source to del]\n"
		"\t[wakeup source]\n"
		"\t0 : GPIO6\n"
		"\t1 : UART0 LOSSY\n"
		"\t2 : UART0 LOSSLESS\n"
		"\t3 : UART1 LOSSY\n"
		"\t4 : UART1 LOSSLESS\n"
		"\t5 : RTC\n"
		"\t6 : USB\n"
		"\t7 : SDIO" OR
		"pmtc sd [lowpower duration]\n"
		"\t[lowpower duration]\n"
		"\twake up after lowpower duration ( > 0 ) msec" OR
		"\tpmtc irq [0 or 1]\n"
		"\tpmu irq enable disable" OR
		"pmtc show\n"
		"\tshow test configuration"
);

static int do_pm_test_start(int argc, char *argv[])
{
	int test_flag;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	test_flag = atoi(argv[1]);
	if (test_flag >= TEST_STOP) {
		return CMD_RET_USAGE;
	}

#ifndef CONFIG_XIP
	if (test_flag == TEST_IO_ON) {
		uint32_t v;

		v = readl(SMU(LOWPOWER_CTRL));
		v |= 1 << 10;
		writel(v, SMU(LOWPOWER_CTRL));

		pmu_set_mode(WAKEUP_TO_ACTIVE);

		v = readl(SMU(LOWPOWER_CTRL));
		v |= 1 << 30;
		writel(v, SMU(LOWPOWER_CTRL));

		udelay(500);

		v = readl(SMU(LOWPOWER_CTRL));
		v &= ~(1 << 30 | 1 << 10);
		writel(v, SMU(LOWPOWER_CTRL));

		return CMD_RET_SUCCESS;
	} else if (test_flag == TEST_IO_OFF) {
		uint32_t v;

		v = readl(SMU(LOWPOWER_CTRL));
		v |= 1 << 10;
		writel(v, SMU(LOWPOWER_CTRL));

		pmu_set_mode(WAKEUP_TO_ACTIVE_IO_OFF);

		v = readl(SMU(LOWPOWER_CTRL));
		v |= 1 << 30;
		writel(v, SMU(LOWPOWER_CTRL));

		udelay(500);

		v = readl(SMU(LOWPOWER_CTRL));
		v &= ~(1 << 30 | 1 << 10);
		writel(v, SMU(LOWPOWER_CTRL));

		return CMD_RET_SUCCESS;
	}
#endif

	pmt_ctx.flag = test_flag;
	pmt_ctx.disable_timeout = 0;

	if (pmt_ctx.flag == TEST_WITH_CONFIG ||
	    pmt_ctx.flag == TEST_WITH_CONFIG_REPEAT ||
	    pmt_ctx.flag == TEST_WITH_REPEAT_AND_CHANGE_MODE) {

		if (pmt_ctx.lowpower_mode == ACTIVE_TO_DEEP_SLEEP_0 ||
			pmt_ctx.lowpower_mode == ACTIVE_TO_DEEP_SLEEP_0_IO_OFF ||
			pmt_ctx.lowpower_mode == ACTIVE_TO_SLEEP ||
			pmt_ctx.lowpower_mode == ACTIVE_TO_SLEEP_IO_OFF) {
			/* RF sleep mode + LP_PWR_EN */
			uint32_t v;
			v = readl(0xf0e004d8);
			if (v == 0x58)
				writel(0x5c, 0xf0e004d8);
		} else {
			/* RF sleep mode */
			uint32_t v;
			v = readl(0xf0e004d8);
			if (v == 0x5c)
				writel(0x58, 0xf0e004d8);
		}
	}

	if (pmt_ctx.flag == TEST_WITH_CONFIG) {
		if (argc == 3) {
			pmt_ctx.disable_timeout = atoi(argv[2]);
		}
		g_repeat_cnt = 1;

	} else {
		if (argc < 3) {
			return CMD_RET_USAGE;
		}

		g_repeat_cnt = atoi(argv[2]);
	}

	if (pmt_ctx.flag == TEST_WITH_REPEAT_AND_CHANGE_MODE) {
		pmt_ctx.lowpower_mode = ACTIVE_TO_HIBERNATION_IO_OFF;
	}

#ifndef CONFIG_PM_MULTI_CORE
	setup_uart_lossless_wakeup();
#endif

	osSemaphoreRelease(pmt_ctx.test_sig);

	return CMD_RET_SUCCESS;
}

CMD(pmt, do_pm_test_start,
		"CLI commands for PM test start",
		"pmt [test flag] [option]\n"
		"\t[test flag]\n"
		"\t0 : Use configuration\n"
		"\t1 : Use configuration and repated test during repeat count\n"
#ifndef CONFIG_XIP
		"\t2 : ioon\n"
		"\t3 : iooff\n"
#endif
		"\t[option]\n"
		"\tfor test flag 0  : 0=enable timeout, 1=disable timeout\n"
		"\tfor test flag 1  : repeat count\n"
		"\tfor test flag 2  : repeat count and changing sleep mode\n"
		"\tLS -> DS0 IO ON -> DS0 IO OFF -> DS1 IO ON -> DS1 IO OFF -> HIB IO ON -> HIB IO OFF"
	);
