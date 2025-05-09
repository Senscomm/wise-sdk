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

#include <string.h>

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/pm.h>
#include <sys/queue.h>
#include <hal/systimer.h>
#include <hal/timer.h>
#include <hal/rtc.h>
#include <soc.h>
#include <linker.h>
#include <pm_rram.h>
#include "pmu.h"

#include <cli.h>

#include "FreeRTOS_tick_config.h"
#include <freebsd/kernel.h>

#define	MS_TO_TICKS(ms)	(((ms)*hz)/1000)
#define RCT_CAL_INTERVAL_MS 50

#define SNAP_VALID_TIME_USEC        (1000 * 1000 * 3)

#define SNAP_REG_TO_RATIO(reg) \
		(((reg >> 8) * 100) + ((reg & 0xFF) * 100 / 255))
#define SNAP_RATIO_TO_FREQ(ratio) \
		(20000000 * 100 / ratio)

#define SYS_TIMER_DIFF(a, b, max) (((a) >= (b)) ? ((a) - (b)) : ((max) - (b) + (a)))

struct systimer_ctx {
	/** Snap Callback function */
	systimer_cb snap_cb_func;
};

struct bsp_timer {
	TAILQ_HEAD(systimer_qhead, systimer) systimer_q;
	struct device *dev;
};

struct bsp_timer g_systimer_bsp_timer;
struct device *g_systimer_dev;
struct systimer_ctx g_systimer_ctx;
struct snap_ctx g_systimer_snap;

uint8_t g_trim;

static void scm2010_systimer_snap(void)
{
	volatile uint32_t rtc;
	volatile uint32_t pit;

	pit = SYS_TIMER_SNAPSHOT;
	rtc = SYS_TIMER_RTC_SNAPSHOT;

	SYS_TIMER_CTRL |= SYS_TIMER_CTRL_SNAP;

	while (1) {
		if (rtc != SYS_TIMER_RTC_SNAPSHOT && pit != SYS_TIMER_SNAPSHOT) {
			break;
		}
	}
}

__ilm__ static uint32_t scm2010_systimer_calc_ratio(void)
{
	uint32_t ratio;
	uint32_t pit_diff;
	uint32_t rtc_diff;

	pit_diff = SYS_TIMER_DIFF(g_systimer_snap.pit2, g_systimer_snap.pit1, SYS_TIMER_PIT_COUNT_MAX);
	rtc_diff = SYS_TIMER_DIFF(g_systimer_snap.rtc2, g_systimer_snap.rtc1, SYS_TIMER_RTC_COUNT_MAX);

	ratio = (uint64_t)pit_diff * 100 / rtc_diff;

#if 0
	if (g_trim == 0 && ratio > 70000) {
		printk("pit  : %lu-%lu=%lu\n", g_systimer_snap.pit2, g_systimer_snap.pit1, pit_diff);
		printk("rtc  : %lu-%lu=%lu\n", g_systimer_snap.rtc2, g_systimer_snap.rtc1, rtc_diff);
		printk("ratio: %lu\n", ratio);

		assert(0);
	}
#endif

	return ratio;
}

static uint32_t scm2010_systimer_update_ratio(struct device *dev)
{
	struct pm_rram_info *rram = (struct pm_rram_info *)SCM2010_PM_RRAM_INFO_ADDR;

	/* RTC snap when qLR is 0.7v */
	scm2010_systimer_snap();

	return rram->ratio;
}

static void scm2010_systimer_snap_op(struct device *dev, uint32_t intv)
{

	assert(g_systimer_snap.status == SNAP_STATUS_DONE);

	g_systimer_snap.status = SNAP_STATUS_BUSY;

	scm2010_systimer_snap();

	g_systimer_snap.pit1 = SYS_TIMER_SNAPSHOT;
	g_systimer_snap.rtc1 = SYS_TIMER_RTC_SNAPSHOT;

	udelay(intv);

	scm2010_systimer_snap();

	g_systimer_snap.pit2 = SYS_TIMER_SNAPSHOT;
	g_systimer_snap.rtc2 = SYS_TIMER_RTC_SNAPSHOT;

	g_systimer_snap.status = SNAP_STATUS_DONE;
}

__ilm__ static void scm2010_systimer_set_cmp(struct systimer *timer)
{
	uint32_t cur;

	SYS_TIMER_CTRL |= SYS_TIMER_CTRL_CMP_INTR_EN;
	SYS_TIMER_COMPARE = timer->expiry;
	cur = SYS_TIMER_COUNT;

	if ((cur >= timer->expiry && (cur - timer->expiry) < 0xF0000000) ||
		((cur < timer->expiry) && (timer->expiry - cur) > 0xF0000000)) {
		__nds__plic_set_pending(g_systimer_dev->irq[0]);
	}
}

__ilm__ static void scm2010_systimer_disable_cmp(void)
{
	SYS_TIMER_CTRL &= ~SYS_TIMER_CTRL_CMP_INTR_EN;
}

static int scm2010_systimer_get_wakeup_info(struct device *dev, struct systimer_wakeup_info *info)
{
	struct bsp_timer *bsp_timer = &g_systimer_bsp_timer;
	uint32_t cur_time;
	uint32_t expire_time;
	struct systimer *timer;
	struct systimer *next_timer;

	timer = TAILQ_FIRST(&bsp_timer->systimer_q);
	if (!timer) {
		return -1;
	}

	cur_time = SYS_TIMER_COUNT;
	expire_time = abs(timer->expiry - cur_time);

	/*
	 * PIT is 20Mhz, base time is 10Mhz, try changing to 10Mhz PIT
	 */
	info->wtime = prvReadMtime() + (expire_time / 2);
	info->wtype = timer->type;

	next_timer = TAILQ_NEXT(timer, link);
	if (next_timer) {
		expire_time = abs(next_timer->expiry - cur_time);

		info->wtime_next = prvReadMtime() + (expire_time / 2);
		info->wtype_next = next_timer->type;
		info->wtime_next_valid = 1;
	} else {
		info->wtime_next_valid = 0;
	}

	return 0;
}

static int scm2010_systimer_load(struct device *dev)
{
	/* request LOAD */
	SYS_TIMER_CTRL |= SYS_TIMER_CTRL_LOAD;

	/*
	 * No way to clear LOAD complete interrupt
	 * Alternatively, wait until the RTC tick is incremented
	 * assuming the next tick will complete the LOAD by design
	 */
	udelay(40);

	return 0;
}

static int scm2010_systimer_restart(struct device *dev, int is_exec)
{
	struct bsp_timer *bsp_timer = &g_systimer_bsp_timer;
	struct systimer *timer;
	struct systimer *entry = NULL;

	if (is_exec) {
		timer = TAILQ_FIRST(&bsp_timer->systimer_q);
		assert(timer->type == SYSTIMER_TYPE_WAKEUP_IDLE);

		TAILQ_REMOVE(&bsp_timer->systimer_q, timer, link);
		timer->enqueued = 0;
	}

	entry = TAILQ_FIRST(&bsp_timer->systimer_q);
	if (entry) {
		scm2010_systimer_set_cmp(entry);
	} else {
		scm2010_systimer_disable_cmp();
	}

	return 0;
}

static int scm2010_systimer_set_cmp_cb(struct device *dev, struct systimer *timer, enum systimer_type type,
							systimer_cb cmp_cb_func, void *arg)
{
	timer->type = type;
	timer->cmp_cb_func = cmp_cb_func;
	timer->cmp_cb_arg = arg;
	timer->enqueued = 0;

	return 0;
}

static int scm2010_systimer_set_snap_cb(struct device *dev, systimer_cb snap_cb_func)
{
	if (!g_systimer_ctx.snap_cb_func) {
		g_systimer_ctx.snap_cb_func = snap_cb_func;
	}

	return 0;
}

__ilm__ int scm2010_systimer_start_at(struct device *dev, struct systimer *timer, uint32_t tick)
{
	uint32_t flags;
	uint32_t cur_time;
	uint32_t timer_dur;
	uint32_t entry_dur;
	struct systimer *entry;
	struct bsp_timer *bsp_timer;

	if (timer->enqueued != 0) {
		assert(0);
	}

	bsp_timer = &g_systimer_bsp_timer;
	timer->expiry = tick;

	local_irq_save(flags);

	if (TAILQ_EMPTY(&bsp_timer->systimer_q)) {
		TAILQ_INSERT_HEAD(&bsp_timer->systimer_q, timer, link);
	} else {
		cur_time = SYS_TIMER_COUNT;
		timer_dur = abs(timer->expiry - cur_time);

		TAILQ_FOREACH(entry, &bsp_timer->systimer_q, link) {
			entry_dur = abs(entry->expiry - cur_time);
			if (timer_dur < entry_dur) {
				TAILQ_INSERT_BEFORE(entry, timer, link);
				goto added;
			}
		}

		TAILQ_INSERT_TAIL(&bsp_timer->systimer_q, timer, link);
	}

added:

	timer->enqueued = 1;

	if (timer == TAILQ_FIRST(&bsp_timer->systimer_q)) {
		scm2010_systimer_set_cmp(timer);
	}

	local_irq_restore(flags);

	return 0;
}

static int scm2010_systimer_stop(struct device *dev, struct systimer *timer)
{
	uint32_t flags;
	int reset_cmp;
	struct systimer *entry;
	struct bsp_timer *bsp_timer;

	if (!timer->enqueued) {
		return -1;
	}

	bsp_timer = &g_systimer_bsp_timer;

	local_irq_save(flags);

	reset_cmp = 0;

	if (timer == TAILQ_FIRST(&bsp_timer->systimer_q)) {
		entry = TAILQ_NEXT(timer, link);
		reset_cmp = 1;
	}

	TAILQ_REMOVE(&bsp_timer->systimer_q, timer, link);
	timer->enqueued = 0;

	if (reset_cmp) {
		if (entry) {
			scm2010_systimer_set_cmp(entry);
		} else {
			scm2010_systimer_disable_cmp();
		}
	}

	local_irq_restore(flags);

	return 0;
}

__ilm__ static uint32_t scm2010_systimer_get_counter(struct device *dev)
{
	return SYS_TIMER_COUNT;
}

__ilm__ static void scm2010_systimer_irq_cmp(struct device *dev)
{
	uint32_t flags;
	struct bsp_timer *bsp_timer = &g_systimer_bsp_timer;
	struct systimer *timer;
	struct systimer *next;
	uint32_t cur;

	local_irq_save(flags);

	timer = TAILQ_FIRST(&bsp_timer->systimer_q);
	if (timer) {
		cur = SYS_TIMER_COUNT;
		if ((cur >= timer->expiry && (cur - timer->expiry) < 0xF0000000) ||
		    ((cur < timer->expiry) && (timer->expiry - cur) > 0xF0000000)) {
			TAILQ_REMOVE(&bsp_timer->systimer_q, timer, link);
			timer->enqueued = 0;
			timer->cmp_cb_func(timer->cmp_cb_arg);
		}

		next = TAILQ_FIRST(&bsp_timer->systimer_q);
		if (next) {
			scm2010_systimer_set_cmp(next);
		} else {
			scm2010_systimer_disable_cmp();
		}
	}

	local_irq_restore(flags);
}

__ilm__ static int scm2010_systimer_irq(int irq, void *data)
{
	struct device *dev = (struct device *)data;

	/* clear the interrupt */
	SYS_TIMER_CTRL |= SYS_TIMER_CTRL_CLR_INTR;

	scm2010_systimer_irq_cmp(dev);

	return 0;
}

static int scm2010_systimer_rtc_cal(struct device *dev, uint32_t interval)
{
	struct pm_rram_info *rram = (struct pm_rram_info *)SCM2010_PM_RRAM_INFO_ADDR;
	uint32_t ratio;

	scm2010_systimer_snap_op(dev, interval);
	ratio = scm2010_systimer_calc_ratio();

	if (!rram->act_ratio) {
		rram->act_ratio = ratio;
	} else {
		rram->act_ratio = SNAP_MOVING_AVERAGE_WEIGHT(rram->act_ratio, ratio);
	}

	SYS_TIMER_RATIO = SNAP_RATIO_TO_REG(rram->act_ratio);

	return 0;
}

static int scm2010_systimer_probe(struct device *dev)
{
	struct pm_rram_info *rram = (struct pm_rram_info *)SCM2010_PM_RRAM_INFO_ADDR;

	/* BT timer control */
	SYS_TIMER_COMPARE = 0;
	SYS_TIMER_CTRL = SYS_TIMER_CTRL_EN;

	g_systimer_dev = dev;

	memset(&g_systimer_ctx, 0, sizeof(g_systimer_ctx));
	memset(&g_systimer_snap, 0, sizeof(g_systimer_snap));

	request_irq(dev->irq[0], scm2010_systimer_irq, "systimer",
				dev->pri[0], dev);

	/* BT PIT1 (20Mhz Timer) */
	writel(0x01, dev->base[0] + TIMER_CHCTL(0)); /* External & 32BIT */
	writel(SYS_TIMER_PIT_COUNT_MAX, dev->base[0] + TIMER_CHRLD(0)); /* Counter init */
	writel(0x01, dev->base[0] + TIMER_CHEN); /* Enable */

	systimer_trim(dev);

#ifdef CONFIG_PM_AON_VOLTAGE_CTRL
	pmu_ctlr_aon(PMU_AON_VOLTAGE_qLR_0V7);
#else
	pmu_ctlr_aon(PMU_AON_VOLTAGE_qLR_0V8);
#endif

	rram->act_ratio = 0;
	systimer_rtc_cal(dev, SNAP_LONG_INTERVAL_USEC * 5);

	rram->ratio_last_measure_time = prvReadMtime();

	pmu_ctlr_aon(PMU_AON_VOLTAGE_iLR_0V8);

	return 0;
}

static int scm2010_systimer_shutdown(struct device *dev)
{
	writel(0x00, dev->base[0] + TIMER_CHEN);

	SYS_TIMER_CTRL = 0;

	free_irq(dev->irq[0], dev_name(dev));

	return 0;
}

#ifdef CONFIG_PM_DM
static int scm2010_systimer_suspend(struct device *dev, u32 *idle)
{
	/* nothing required for the suspend, but to be safe, reset the compare */
	SYS_TIMER_COMPARE = 0;

	return 0;
}

static int scm2010_systimer_resume(struct device *dev)
{
	/* resume is handled by watcher */
	return 0;
}
#endif

static void scm2010_systimer_trim(struct device *dev)
{
	uint32_t ratio;
	uint32_t gap = UINT32_MAX;
	uint8_t trim = 0;
	uint8_t i;

	g_trim = 1;

	for (i = 0; i <= 0x3f; i++) {
		SYS_TIMER_OSC32K_TRIM = i;
		scm2010_systimer_snap_op(dev, SNAP_SHORT_INTERVAL_USEC);

		while (g_systimer_snap.status != SNAP_STATUS_DONE) {
			udelay(100);
		}

		ratio = scm2010_systimer_calc_ratio();
		/* 20M / 32768 = 610.35 */
		if (gap > abs(61035 - ratio)) {
			gap = abs(61035 - ratio);
			trim = i;
		}
	}

	g_trim = 0;

	SYS_TIMER_OSC32K_TRIM = trim;
}



static struct systimer_ops scm2010_systimer_ops = {
	.trim               = scm2010_systimer_trim,
	.rtc_cal            = scm2010_systimer_rtc_cal,
	.update_ratio       = scm2010_systimer_update_ratio,
	.set_cmp_cb         = scm2010_systimer_set_cmp_cb,
	.set_snap_cb        = scm2010_systimer_set_snap_cb,
	.start_at           = scm2010_systimer_start_at,
	.stop               = scm2010_systimer_stop,
	.get_counter        = scm2010_systimer_get_counter,
	.get_wakeup_info    = scm2010_systimer_get_wakeup_info,
	.load               = scm2010_systimer_load,
	.restart            = scm2010_systimer_restart,
};

static declare_driver(systimer) = {
	.name       = "systimer",
	.probe      = scm2010_systimer_probe,
	.shutdown   = scm2010_systimer_shutdown,
#ifdef CONFIG_PM_DM
	.suspend    = scm2010_systimer_suspend,
	.resume     = scm2010_systimer_resume,
#endif
	.ops        = &scm2010_systimer_ops,
};
