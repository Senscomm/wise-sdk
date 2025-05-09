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

#include <hal/timer.h>
#include <hal/rtc.h>
#include <hal/pm.h>
#include <hal/init.h>
#include <hal/console.h>
#include <sys/queue.h>
#include <hal/systimer.h>
#include <freebsd/kernel.h>

#include "FreeRTOS_tick_config.h"
#include "linker.h"

#ifdef CONFIG_PM_SCM2010

#include <stdio.h>
#include "mmap.h"
#include "pm_rram.h"
#include "pmu.h"
#include "pm_test.h"

#ifdef CONFIG_PM_MULTI_CORE
#include "pm_sig.h"
#endif

DECLARE_SECTION_INFO(buffer);

#define DEFAULT_SAVE_ADDR __buffer_start
#define D25_ILM_SIZE 32 * 1024

#define pm_wfi()                        __asm volatile( "wfi" )

#define PM_MAX_LOWPOWER_MTIME_DURATION  (((1000000ULL * 60 * 60 * 23) - 10000) * 10)

#define XTAL_CLOCK_HZ CONFIG_XTAL_CLOCK_HZ
#define MTIME_CLK_DIV CONFIG_MTIME_CLK_DIV

#define USTIME (1000 * 1000)
#define XTALTIME (XTAL_CLOCK_HZ / MTIME_CLK_DIV / USTIME)
#define RTCTIME (100 * 20)

/* default parameters */
#define PM_DEFAULT_WAKEUP_SRC           (WAKEUP_SRC_ALL)
#define PM_DEFAULT_SUSPEND_DURATION     (0)

#ifdef CONFIG_PM_AON_VOLTAGE_CTRL_OCR
/* set sleep ratio to 0.933 * active ratio as a default value */
#define PM_DEFAULT_SLEEP_RATIO_RATE     933  /* sleep ratio is about 0.933 time of active ratio */
#define PM_ACT_RATIO_TO_SLEEP_RATIO(x)  ((x * PM_DEFAULT_SLEEP_RATIO_RATE) / 1000)
#else
#define PM_ACT_RATIO_TO_SLEEP_RATIO(x)  (x)
#endif

#ifdef CONFIG_SDIO_PM
#define PM_SDIO_SW_PREPARE_TIME 20000
#else
#define PM_SDIO_SW_PREPARE_TIME 0
#endif

/* TODO: Optimize */
#define PM_DEFAULT_SLEEP_THRESHOLD      (PM_SDIO_SW_PREPARE_TIME + 4000 + 5000)  // usec

#define PM_HIB_PREP_EXTEND_USEC			(100 * 1000) // usec

#define PLIC_INT_ENABLE_REG(reg, hart_id)      ( \
												NDS_PLIC_BASE + \
												PLIC_ENABLE_OFFSET + \
												(hart_id << PLIC_ENABLE_SHIFT_PER_TARGET) + \
												(reg << 2))

#define PLIC_SW_INT_ENABLE_REG(reg, hart_id)    ( \
												NDS_PLIC_SW_BASE + \
												PLIC_ENABLE_OFFSET + \
												(hart_id << PLIC_ENABLE_SHIFT_PER_TARGET) + \
												(reg << 2))

enum {
	PM_LIGHT_SLEEP = 0,
	PM_SLEEP,
	PM_DEEP_SLEEP,
	PM_HIBERNATION,
	PM_MAX // This will give you the number of power management modes
};

struct pm_time_settings{
	uint32_t sw_save_time;
	uint32_t hw_save_time;
	uint32_t sw_restore_time;
	uint32_t hw_restore_time;
	uint32_t wh_restore_time;
};

const struct pm_time_settings pm_settings[] = {
	{1500 + 5000, 0, 1100, 20, 0},          //PM_LIGHT_SLEEP
	{1500 + 5000, 0, 1100, 6000, 0},        //PM_SLEEP
	{PM_SDIO_SW_PREPARE_TIME + 6500 + 5000, 0, 6800, 6000, 3800},    	//PM_DEEP_SLEEP
	{PM_SDIO_SW_PREPARE_TIME + 1650000 + 5000, 0, 54000, 6000, 0}, 	  //PM_HIBERNATION
};

#define SW_SAVE_TIME(name)      pm_settings[name].sw_save_time
#define HW_SAVE_TIME(name)      pm_settings[name].hw_save_time
#define SW_RESTORE_TIME(name)   pm_settings[name].sw_restore_time
#define HW_RESTORE_TIME(name)   pm_settings[name].hw_restore_time
/* TODO : need timing measure */
#define WH_RESOTRE_TIME(name)   pm_settings[name].wh_restore_time

#define SAVE_TIME(name)         SW_SAVE_TIME(name) + HW_SAVE_TIME(name)
#define RESTORE_TIME(name)      SW_RESTORE_TIME(name) + HW_RESTORE_TIME(name)
#define MIN_TIME(name)          SAVE_TIME(name) + RESTORE_TIME(name)

#define PM_HIBERNATION_LIMIT_COUNT      (CONFIG_PM_HIBERNATION_LIMIT_MAX)
#define PM_HIBERNATION_LIMIT_DURATION   (CONFIG_PM_HIBERNATION_LIMIT_DURATION)

#define SYS_CRYPTO_CFG_CIPHER_EN		(1 << 31)

#define SMU_AON_MEMEMA					0x210
#define SMU_AON_MEMEMA_RET				(1 << 17 | /* SYS_RF2P_RET_EN */     \
										 1 << 18 | /* SYS_UHDSPRAM_RET_EN */ \
										 1 << 19 | /* N22_UHDSPRAM_RET_EN */ \
										 1 << 20 | /* N22_RFSP_RET_EN */     \
										 1 << 21 | /* D25_UHDSPRAM_RET_EN */ \
										 1 << 22 | /* D25_RFSP_RET_EN */     \
										 1 << 23 | /* WF_RF2P_RET_EN */      \
										 1 << 24 | /* WF_RFSP_RET_EN */      \
										 1 << 25)  /* WF_UHDSPRAM_RET_EN */


#define SMU_LOWPOWER_CHIP_IDLE_EN		(1 << 13)
#define SMU_LOWPOWER_CHIP_SLEEP_EN		(1 << 14)
#define SMU_LOWPOWER_WAKEUP_EVENT_MASK		((BIT(11)-1) << 19)
#define SMU_LOWPOWER_WAKEUP_EVENT_CLR	(1 << 18)

#define SMU_TOPCFG_DIG_LDO_LP_MODE		(1 << 5)

#define PM_FEAT_ALL_ON		0x0
#define PM_FEAT_TIMER_OFF	0x1 //(1 << 0)
#define PM_FEAT_SYS_CG		0x2 //(1 << 1)
#define PM_FEAT_SYS_PG		0x4 //(1 << 2)
#define PM_FEAT_CORE_PG		0x8 //(1 << 3)

static const uint8_t pm_feat[] = {
	[PM_MODE_IDLE]		= PM_FEAT_ALL_ON,
	[PM_MODE_LIGHT_SLEEP]	= PM_FEAT_TIMER_OFF | PM_FEAT_SYS_CG,
	[PM_MODE_SLEEP]		    = PM_FEAT_TIMER_OFF | PM_FEAT_SYS_CG,
	[PM_MODE_DEEP_SLEEP_0]	= PM_FEAT_TIMER_OFF | PM_FEAT_SYS_CG  | PM_FEAT_CORE_PG,
	[PM_MODE_DEEP_SLEEP_1]	= PM_FEAT_TIMER_OFF | PM_FEAT_SYS_PG  | PM_FEAT_CORE_PG,
	[PM_MODE_HIBERNATION]	= PM_FEAT_TIMER_OFF | PM_FEAT_SYS_PG  | PM_FEAT_CORE_PG,
};

static const uint32_t pm_restore_time_offset[PM_WAKEUP_TYPE_MAX][PM_MODE_MAX] = {
	{
		[PM_MODE_IDLE]			= 0,
		[PM_MODE_LIGHT_SLEEP]	= RESTORE_TIME(PM_LIGHT_SLEEP),
		[PM_MODE_SLEEP]		= RESTORE_TIME(PM_SLEEP),
		[PM_MODE_DEEP_SLEEP_0]	= RESTORE_TIME(PM_DEEP_SLEEP),
		[PM_MODE_DEEP_SLEEP_1]	= RESTORE_TIME(PM_DEEP_SLEEP),
		[PM_MODE_HIBERNATION]	= RESTORE_TIME(PM_HIBERNATION),
	},
	{
		[PM_MODE_IDLE]			= 0,
		[PM_MODE_LIGHT_SLEEP]	= RESTORE_TIME(PM_LIGHT_SLEEP),
		[PM_MODE_SLEEP]		= RESTORE_TIME(PM_SLEEP),
		[PM_MODE_DEEP_SLEEP_0]	= WH_RESOTRE_TIME(PM_DEEP_SLEEP),
		[PM_MODE_DEEP_SLEEP_1]	= WH_RESOTRE_TIME(PM_DEEP_SLEEP),
		[PM_MODE_HIBERNATION]	= WH_RESOTRE_TIME(PM_HIBERNATION),
	},
};

enum {
    PMU_LOWPOWER = 0,
    PMU_WAKEUP,
    PMU_MODE_MAX,
};

static uint32_t pm_pmu_mode[PMU_MODE_MAX][PM_MODE_MAX] __maybe_unused = {
	{
		[PM_MODE_IDLE]			= ACTIVE_TO_IDLE,
		[PM_MODE_LIGHT_SLEEP]	= ACTIVE_TO_LIGHT_SLEEP,
		[PM_MODE_SLEEP]			= ACTIVE_TO_SLEEP,
		[PM_MODE_DEEP_SLEEP_0]	= ACTIVE_TO_DEEP_SLEEP_0,
		[PM_MODE_DEEP_SLEEP_1]	= ACTIVE_TO_DEEP_SLEEP_1,
		[PM_MODE_HIBERNATION]	= ACTIVE_TO_HIBERNATION,
	},
	{
		[PM_MODE_IDLE]			= WAKEUP_TO_ACTIVE,
		[PM_MODE_LIGHT_SLEEP]	= WAKEUP_TO_ACTIVE,
		[PM_MODE_SLEEP] 		= WAKEUP_TO_ACTIVE,
		[PM_MODE_DEEP_SLEEP_0]	= WAKEUP_TO_ACTIVE,
		[PM_MODE_DEEP_SLEEP_1]	= WAKEUP_TO_ACTIVE,
		[PM_MODE_HIBERNATION]	= WAKEUP_TO_ACTIVE,
	},
};

#define PMU_MODE(pmu_mode, pm_mode) pm_pmu_mode[pmu_mode][pm_mode];

uint32_t __d25_ilm_save[1];
uint32_t __enc_buffer[1];

/* assembly function for [save, wfi, restore] procedure */
extern void pm_lowpower_start(void);
extern void pm_wakeup_entry(void);
extern void pm_wakeup_wfi(void);
#ifdef CONFIG_PM_TEST
extern void pm_test_wakeup_wfi(void);
#endif
#ifdef CONFIG_PM_GPIO_DBG
extern void pm_gpio_dbg_set(uint8_t pin);
extern void pm_gpio_dbg_clr(uint8_t pin);
#endif

static const u8 watcher_fw[] = {
#include "./scm2020-watcher.inc"
};

struct pwr_mgmt_ctx {
	uint32_t residual;				/* usec */

	uint64_t mtime;					/* machine time before entering low power mode */
	uint64_t mtimecmp;				/* machine compare time of hart1 */
	uint64_t wtime;					/* wakeup time of device */
	uint64_t wtime_next;			/* next wakeup time of device */
	uint64_t ttime;					/* wakeup target time */
	uint64_t ttime_next;			/* next wakeup target time */
	uint8_t wtype;					/* wakeup type full or watcher */

	uint64_t hib_limit_clr;			/* hibernation limit clear time */
	uint32_t hib_limit_cnt;			/* hibernation limit count */
	uint32_t hib_max_cnt;			/* hiberantion max count */

	uint32_t hib_min_time;			/* threshold time for entering hibernation */
	uint32_t hib_save_time;			/* threshold SW save time for entering hibernation */

	uint32_t power_down;			/* power down duration */

	enum pm_mode pm_mode;			/* pm mode */
	uint8_t pm_mode_enabled;		/* pm mode enabled by user */
	uint8_t pm_lpwr_io_off; 		/* PM_FEATURE_LOWPOWER_IO_OFF */

	void (*callback)(int mode);		/* callback for the pm mode change */

	/* ---------------------------------------------------------- */

	uint16_t wakeup_src;			/* wakeup source */
	uint16_t wakeup_evt;			/* wakeup evt */
	uint16_t wakeup_reason;			/* wakeup reason */
	uint16_t wakeup_type;			/* wakeup type */

	uint8_t gpio_enabled;			/* gpio enabled by user */
	uint32_t gpio_pu;				/* gpio pull up */
	uint32_t gpio_pd;				/* gpio pull down */
	uint32_t gpio_ie;				/* gpio input enable */
	uint32_t gpio_oen;				/* gpio output enable */
	uint32_t gpio_mode;				/* gpio mode for AON 0~7 */
	uint32_t gpio_rise;				/* gpio rising interrupt */
	uint32_t gpio_fall;				/* gpio falling interrupt */

	struct device *systimer;		/* systimer device */
	struct device *rtc_dev;			/* rtc device */
	struct device *ext_rtc_dev;			/* rtc device */
	uint32_t rtc32k;				/* rtc 32k count */
	uint32_t ratio;					/* rtc to systimer ratio * 100 */
	uint32_t rtc;					/* rtc before entering low power mode */

	uint32_t plic_en_s0_h0;			/* PLIC interrupt enable source 0 value of hart0 */
	uint32_t plic_en_s1_h0;			/* PLIC interrupt enable source 1 value of hart0 */
	uint32_t plic_en_s0_h1;			/* PLIC interrupt enable source 0 value of hart1 */
	uint32_t plic_en_s1_h1;			/* PLIC interrupt enable source 1 value of hart1 */

	uint32_t stay_active;			/* prohibit enter low power mode */
	uint64_t stay_active_mtime;		/* stay active timeout end mtime */
	osTimerId_t stay_timer;			/* stay active timeout timer */

	uint32_t rtc_ratio_invalid_time;

#ifdef CONFIG_PM_MULTI_CORE
	uint32_t peer_text_lma;
	uint32_t peer_resume_addr;
	uint64_t peer_wtime;

	uint32_t plic_sw_en_s0_h0;		/* PLIC SW interrupt enable source 0 value of hart0 */
	uint32_t plic_sw_en_s0_h1;		/* PLIC SW interrupt enable source 0 value of hart1 */
#endif
};

static struct pwr_mgmt_ctx pm_ctx;
static struct pwr_mgmt_ctx *ctx = &pm_ctx;
static struct pm_rram_info *rram;


#if 0
#define PM_GET_FIELD(_field) ctx->_field
#define PM_SET_FIELD(_field, _v) \
do { \
	ctx->_field = _v; \
} while (0)

#define IDLE_DUR() (PM_GET_FIELD(wtime) - PM_GET_FIELD(mtime))
#define NEXT_DUR() (PM_GET_FIELD(wtime_next) - PM_GET_FIELD(mtime))

#define TARGET_TIME() (PM_GET_FIELD(wtime) - pm_usec_to_mtime(pm_restore_time_offset[PM_GET_FIELD(wtype)][PM_GET_FIELD(pm_mode)]))
#define NEXT_TARGET_TIME() (PM_GET_FIELD(wtime_next) - pm_usec_to_mtime(pm_restore_time_offset[PM_GET_FIELD(wtype)][PM_GET_FIELD(pm_mode)]))


#define MAX_WAKE_TIME() (PM_GET_FIELD(mtime) + PM_MAX_LOWPOWER_MTIME_DURATION)
/* sleep threshold */
#define SLEEP_THRESHOLD() (PM_GET_FIELD(mtime) + pm_usec_to_mtime(PM_GET_FIELD(lowpower_threshold)))
#define OVER_SLEEP_THRESHOLD() (PM_GET_FIELD(wtime) < SLEEP_THRESHOLD())
#endif

#define IDLE_DUR() (ctx->wtime - ctx->mtime)
#define NEXT_DUR() (ctx->wtime_next - ctx->mtime)
#define MAX_IDLE_DUR() (ctx->mtime + PM_MAX_LOWPOWER_MTIME_DURATION)
#define HIB_CLR_DUR() (ctx->mtime + pm_usec_to_mtime((uint64_t)PM_HIBERNATION_LIMIT_DURATION * USTIME))
/* sleep threshold */
#define SLEEP_THRESHOLD() (ctx->mtime + pm_usec_to_mtime(PM_DEFAULT_SLEEP_THRESHOLD))
#define OVER_SLEEP_THRESHOLD(wt) (wt < SLEEP_THRESHOLD())

#define TARGET_TIME(type, mode) (ctx->wtime - pm_usec_to_mtime(pm_restore_time_offset[type][mode]))
#define NEXT_TARGET_TIME(type, mode) (ctx->wtime_next - pm_usec_to_mtime(pm_restore_time_offset[type][mode]))

static inline uint64_t pm_usec_to_mtime(uint64_t usec)
{
	return (usec * XTALTIME);
}

static inline uint64_t pm_mtime_to_usec(uint64_t mtime)
{
	return (mtime / XTALTIME);
}

static inline uint64_t pm_rtc_to_usec(uint32_t tick)
{
	return ((uint64_t)tick * ctx->ratio / RTCTIME);
}

static inline uint32_t pm_usec_to_rtc(uint64_t usec)
{
	return (uint64_t)usec * RTCTIME / ctx->ratio;
}

static void pm_activetimeout(void *arg)
{
	pm_relax(PM_DEVICE_TIMED);
}

static int pm_update_idle_time(void)
{
	struct systimer_wakeup_info wakeup_info;
	int ret;

	/* os wakeup time */
	ctx->wtime = prvReadMtimecmp();
	ctx->wtime_next = 0;

	/* current time and rtc */
	ctx->mtime = prvReadMtime();

	/* check if requested sleep ticks are greater than sleep threshold */
	if (OVER_SLEEP_THRESHOLD(ctx->wtime)) {
		return -1;
	}

	/* get earliest wakeup information in systimer list */
	ret = systimer_get_wakeup_info(ctx->systimer, &wakeup_info);
	if (!ret) {
		if (ctx->wtime > wakeup_info.wtime) {
			ctx->wtime = wakeup_info.wtime;

			/* check if requested sleep ticks are greater than sleep threshold */
			if (OVER_SLEEP_THRESHOLD(ctx->wtime)) {
				return -1;
			}

			if (wakeup_info.wtype == SYSTIMER_TYPE_WAKEUP_IDLE) {
				/* get second wakeup information in systimer list */
				ctx->wtime_next = prvReadMtimecmp();
				if (wakeup_info.wtime_next_valid) {
					if (ctx->wtime_next > wakeup_info.wtime_next) {
						ctx->wtime_next = wakeup_info.wtime_next;
					}
				}
			}
		}
	}

	ctx->wtype = (ctx->wtime_next) ? PM_WAKEUP_TYPE_WATCHER : PM_WAKEUP_TYPE_FULL;

#ifdef CONFIG_PM_MULTI_CORE
	if (ctx->wtype == PM_WAKEUP_TYPE_FULL) {
		if (ctx->wtime > ctx->peer_wtime) {
			ctx->wtime = ctx->peer_wtime;
		}
	} else {
		if (ctx->wtime_next > ctx->peer_wtime) {
			if (ctx->wtime > ctx->peer_wtime) {
				ctx->wtime = ctx->peer_wtime;
				ctx->wtype = PM_WAKEUP_TYPE_FULL;
			} else {
				ctx->wtime_next = ctx->peer_wtime;
			}
		}
	}
#endif

	if (IDLE_DUR() > PM_MAX_LOWPOWER_MTIME_DURATION) {
		ctx->wtime = MAX_IDLE_DUR();
		ctx->wtype = PM_WAKEUP_TYPE_FULL;
	}

	return 0;
}

static int pm_mode_sel(void)
{
	uint32_t min_times[PM_MODE_MAX] = {
		[PM_MODE_HIBERNATION] = ctx->hib_min_time,
		[PM_MODE_DEEP_SLEEP] = MIN_TIME(PM_DEEP_SLEEP),
		[PM_MODE_LIGHT_SLEEP] = MIN_TIME(PM_LIGHT_SLEEP),
	};

	uint64_t dur = pm_mtime_to_usec(IDLE_DUR());

	for (int i = PM_MODE_MAX - 1; i >= 0; i--) {
		if (min_times[i] != 0 && dur > min_times[i]) {
			ctx->pm_mode = i;
			//printk("%d PM_MODE: %d, mt: %d, dur:%llu\n", i, ctx->pm_mode, min_times[i], dur);
			goto done;
		}
	}

	return -1;

done:

	/*
	 * If low power mode is deep sleep and next wakeup type is watcher
	 * and next wakeup time is longer than hibernation require time,
	 * change low power mode to hibernation.
	 */
	if (ctx->pm_mode >= PM_MODE_DEEP_SLEEP) {
		if (ctx->wtype == PM_WAKEUP_TYPE_WATCHER) {
			uint64_t next_dur;
			next_dur = pm_mtime_to_usec(NEXT_DUR());
			if (next_dur > ctx->hib_min_time) {
				ctx->pm_mode = PM_MODE_HIBERNATION;
			}
		}
	}

	return 0;
}

static int pm_mode_check(void)
{
	if (ctx->pm_mode == PM_MODE_HIBERNATION) {
		if (ctx->hib_limit_cnt >= ctx->hib_max_cnt) {
			ctx->pm_mode = PM_MODE_DEEP_SLEEP;
		}
	}

	if (!(ctx->pm_mode_enabled & (1 << ctx->pm_mode))) {
		int next_mode = ctx->pm_mode - 1;

		while (next_mode) {
			if (ctx->pm_mode_enabled & (1 << next_mode))
				break;
			next_mode--;
		}

		//printk ("input mode: %d, mode to be: %d, \n", ctx->pm_mode, next_mode);

		ctx->pm_mode = next_mode;
	}

	if (ctx->pm_mode == PM_MODE_ACTIVE)
		return -1;

	return 0;
}

static void pm_reset_hib_limit(void)
{
	/* reset hib_limit_cnt if hit HIB_CLR_DUR */
	if (ctx->hib_limit_clr < ctx->mtime) {
		ctx->hib_limit_clr = HIB_CLR_DUR();
		ctx->hib_limit_cnt = 0;
	}
}

static void pm_update_target_time(void)
{
	/* taraget time */
	ctx->ttime = TARGET_TIME(ctx->wtype, ctx->pm_mode);

	if (ctx->wtype == PM_WAKEUP_TYPE_WATCHER) {
		if (ctx->wtime_next) {
			ctx->ttime_next = NEXT_TARGET_TIME(ctx->wtype, ctx->pm_mode);
		}
	}
}

static int pm_update_mode(void)
{
	if (pm_mode_sel())
		return -1;

	/* before mode check, reset hib limit clr if expired */
	pm_reset_hib_limit();

	if (pm_mode_check())
		return -1;

	/* update ttime when mode update */
	pm_update_target_time();

	return 0;
}

static void pm_update_rram(void)
{
	/*
	 * set information
	 * - set pm mode SLEEP or DEEP SLEEP or HIBERNATION
	 * - wake type (watcher or full wakeup)
	 * - current time (for adjust time in watcher wakeup)
	 * - next wakeup time (select next wakeup mode int watcher wakeup)
	 * - current rtc value (for adjust time int watcher wakeup)
	 */
	rram->pm_mode = ctx->pm_mode;
	rram->wk_src = ctx->wakeup_src;
	rram->wk_time = ctx->ttime;

	if (rram->pm_mode == SCM2010_PM_RRAM_MODE_HIBERNATION) {
		rram->sync_flag = SCM2010_PM_RRAM_SAVE_FLASH;
	}

	if (ctx->wtype == PM_WAKEUP_TYPE_WATCHER) {
		rram->rtc_value = ctx->rtc;
		rram->mtime_value = ctx->mtime;
		rram->next_mtime_value = ctx->ttime_next;
		rram->wakeup_flag |= SCM2010_PM_RRAM_WAKEUP_WATCHER;
	}
}

static void pm_update_hib_prep_time(void)
{
	uint64_t mtime_elapsed;

	mtime_elapsed = prvReadMtime() - ctx->mtime;

	if ((rram->sync_flag & SCM2010_PM_RRAM_SAVE_CANCEL) &&
		(rram->wakeup_flag & SCM2010_PM_RRAM_EXT_HIB_TIME)) {

		if (mtime_elapsed - ctx->hib_min_time) {
			printk("previous HIB save time %d usec\n", ctx->hib_save_time);
			ctx->hib_min_time -= ctx->hib_save_time;
			ctx->hib_save_time = (uint32_t)pm_mtime_to_usec(mtime_elapsed) + PM_HIB_PREP_EXTEND_USEC;
			printk("update HIB save time %d usec\n", ctx->hib_save_time);
			ctx->hib_min_time += ctx->hib_save_time;
		}
	}
}

static void pm_clear_rram(void)
{
	/* clear all retention ram values */
	rram->wakeup_flag = 0;
	rram->exec_watcher = 0;
	rram->sync_flag = 0;
	rram->pm_mode = 0;
	rram->wk_src = 0;
	rram->wk_evt = 0;
	rram->wk_time = 0;
	rram->rtc_value = 0;
	rram->mtime_value = 0;
}

__maybe_unused static uint32_t pm_rtc_time_next(uint64_t duration)
{
	uint32_t rtc_cur;
	uint32_t rtc_idle;

	rtc_get_32khz_count(ctx->rtc_dev, &rtc_cur);
	//rtc_cur = ctx->rtc;

	rtc_idle = pm_usec_to_rtc(duration);

	assert(rtc_idle < RTC_COUNT_MAX);

	if (RTC_WRAP(rtc_idle, rtc_cur)) {
		return rtc_idle - (RTC_COUNT_MAX - rtc_cur);
	} else {
		return rtc_cur + rtc_idle;
	}
}

static void pm_set_alarm(uint64_t duration)
{
	uint32_t rtc_next;
	rtc_next = pm_rtc_time_next(duration);

	rtc_set_32khz_alarm(ctx->rtc_dev, rtc_next);
}

static void pm_set_rtc_wakeup(void)
{
	uint64_t duration;

	duration = pm_mtime_to_usec(ctx->ttime - ctx->mtime);

	pm_set_alarm(duration);
}

static void pm_save_time(void)
{
	rtc_get_32khz_count(ctx->rtc_dev, &ctx->rtc);

	ctx->mtime = prvReadMtime();
	ctx->mtimecmp = prvStopMtimeIrq();
}

static uint32_t pm_rtc_time_elapsed(void)
{
	uint32_t rtc;
	uint32_t rtc_elapsed;

	/* wait until changing rtc count value */
	while (1) {
		if (ctx->rtc32k != readl(RTC_BASE_ADDR + 0x04)) {
			break;
		}
	}

	rtc_get_32khz_count(ctx->rtc_dev, &rtc);
	if (rram->rtc_value) {
		rtc_elapsed = RTC_ELAPSED(rram->rtc_value, rtc);
	} else {
		rtc_elapsed = RTC_ELAPSED(ctx->rtc, rtc);
	}

	return pm_rtc_to_usec(rtc_elapsed);
}

static void pm_restore_mtime(void)
{
	uint64_t mtime;
	uint64_t mtime_elapsed;

	mtime_elapsed = pm_usec_to_mtime(pm_rtc_time_elapsed());
	if (rram->mtime_value) {
		mtime = rram->mtime_value + mtime_elapsed;
	} else {
		mtime = ctx->mtime + mtime_elapsed;
	}

	prvWriteMtime(mtime);
}

static void pm_restore_time(void)
{
	pm_restore_mtime();
	prvWriteMtimecmp(ctx->mtimecmp);
	pm_update_hib_prep_time();
}

static void pm_save_plic_h0(void)
{
	/* save PLIC interrupt enable hart 0 */
	ctx->plic_en_s0_h0 = readl(PLIC_INT_ENABLE_REG(0, 0));
	ctx->plic_en_s1_h0 = readl(PLIC_INT_ENABLE_REG(1, 0));

#ifdef CONFIG_PM_MULTI_CORE
	ctx->plic_sw_en_s0_h0 = readl(PLIC_SW_INT_ENABLE_REG(0, 0));
#endif

	/* disable all interrupt of D25 to avoid race conditions */
	writel(0, PLIC_INT_ENABLE_REG(0, 0));
	writel(0, PLIC_INT_ENABLE_REG(1, 0));

#ifdef CONFIG_PM_MULTI_CORE
	writel(0, PLIC_SW_INT_ENABLE_REG(0, 0));
#endif
}

static void pm_restore_plic_h0(void)
{
	/* restore PLIC interrupt enable hart 0 */
	writel(ctx->plic_en_s0_h0, PLIC_INT_ENABLE_REG(0, 0));
	writel(ctx->plic_en_s1_h0, PLIC_INT_ENABLE_REG(1, 0));

#ifdef CONFIG_PM_MULTI_CORE
	writel(ctx->plic_sw_en_s0_h0, PLIC_SW_INT_ENABLE_REG(0, 0));
#endif
}

static void pm_suspend_h0(void)
{
	/* make D25 to wakeup to wfi */
	writel((uint32_t)pm_wakeup_wfi, SMU(RESET_VECTOR(0)));
	writel(0x01, SYS(CORE_RESET_CTRL));
}

static void pm_resume_h0(void)
{
#if defined (CONFIG_SDIO_USE_IRQD)
	/* make D25 to go to SDIO dispatcher */
	writel(ILM_BASE, SMU(RESET_VECTOR(0)));
	writel(0x01, SYS(CORE_RESET_CTRL));
#elif defined (CONFIG_PM_MULTI_CORE)
	/* make D25 to go to peer resume address */
#if 0
	printk("resume D25 : 0x%08x\n", ctx->peer_resume_addr);
#endif
	writel(ctx->peer_resume_addr, SMU(RESET_VECTOR(0)));
	writel(0x01, SYS(CORE_RESET_CTRL));
#else
	/* make D25 to go to wfi */
	writel((uint32_t)pm_wakeup_wfi, SMU(RESET_VECTOR(0)));
	writel(0x01, SYS(CORE_RESET_CTRL));
#endif
}

static void pm_save_plic_h1(void)
{
	/* save PLIC interrupt enable hart 1 */
	ctx->plic_en_s0_h1 = readl(PLIC_INT_ENABLE_REG(0, 1));
	ctx->plic_en_s1_h1 = readl(PLIC_INT_ENABLE_REG(1, 1));

#ifdef CONFIG_PM_MULTI_CORE
	ctx->plic_sw_en_s0_h1 = readl(PLIC_SW_INT_ENABLE_REG(0, 1));
#endif

	/* disable all interrupt of N22 to avoid race conditions */
	writel(0, PLIC_INT_ENABLE_REG(0, 1));
	writel(0, PLIC_INT_ENABLE_REG(1, 1));

#ifdef CONFIG_PM_MULTI_CORE
	writel(0, PLIC_SW_INT_ENABLE_REG(0, 1));
#endif
}

static void pm_restore_plic_h1(void)
{
	/* restore PLIC interrupt enable hart 1 */
	writel(ctx->plic_en_s0_h1, PLIC_INT_ENABLE_REG(0, 1));
	writel(ctx->plic_en_s1_h1, PLIC_INT_ENABLE_REG(1, 1));

#ifdef CONFIG_PM_MULTI_CORE
	writel(ctx->plic_sw_en_s0_h1, PLIC_SW_INT_ENABLE_REG(0, 1));
#endif
}

static void pm_restore_clocks(void)
{
	/* If we only stay with default clock setting,
	 * there is no need to re-inialize clock tree objects
	 * because DLM is intact anyway after wakeup.
	 */
#if defined(CONFIG_USE_DEFAULT_CLK)
	/* If there is a IPC host, e.g., NuttX, it will take care
	 * of setting up the clock tree.
	 */
	return;
#else
	/* When scm2010 wakes up with the SYS register turned off,
	 * clock tree objects should be initialized because of mismatch
	 * between clock objects and HW.
	 */

	uint32_t clkobj_vma = (uint32_t)ll_entry_start(struct clk, clk);
	uint32_t clkobj_lma = (uint32_t)LMA(data) + (clkobj_vma - (uint32_t)VMA(data));
	uint32_t clkobj_len = (uint32_t)ll_entry_end(struct clk, clk) - clkobj_vma;

	memcpy((void *)clkobj_vma, (void *)clkobj_lma, clkobj_len);

	clock_init();
	clock_postinit();
#endif
}


#ifdef CONFIG_PM_AON_GPIO_CTRL

static void pm_store_gpio(void)
{
	ctx->gpio_pu = readl(GPIO_BASE_ADDR + 0x04);
	ctx->gpio_pd = readl(GPIO_BASE_ADDR + 0x08);
	ctx->gpio_ie = readl(GPIO_BASE_ADDR + 0x0c);
	ctx->gpio_oen = readl(GPIO_BASE_ADDR + 0x10);
	ctx->gpio_mode = readl(GPIO_BASE_ADDR + 0x20);
	ctx->gpio_rise = readl(GPIO_BASE_ADDR + 0x30);
	ctx->gpio_fall = readl(GPIO_BASE_ADDR + 0x40);
}

static void pm_restore_gpio(void)
{
	writel(ctx->gpio_pu, GPIO_BASE_ADDR + 0x04);
	writel(ctx->gpio_pd, GPIO_BASE_ADDR + 0x08);
	writel(ctx->gpio_ie, GPIO_BASE_ADDR + 0x0c);
	writel(ctx->gpio_oen, GPIO_BASE_ADDR + 0x10);
	writel(ctx->gpio_mode, GPIO_BASE_ADDR + 0x20);
	writel(ctx->gpio_rise, GPIO_BASE_ADDR + 0x30);
	writel(ctx->gpio_fall, GPIO_BASE_ADDR + 0x40);
}

static void pm_pull_down_gpio(void)
{
	uint32_t v;
	uint32_t gpio_pu;
	uint32_t gpio_pd;
	uint32_t gpio_ie;
	uint32_t gpio_oen;
	uint32_t gpio_mode;
	uint32_t gpio_rise;
	uint32_t gpio_fall;

	/*
	 * For those AON pins not requested by the application
	 * make them input with pull down
	 */

	gpio_pu = ctx->gpio_pu;
	gpio_pd = ctx->gpio_pd;
	gpio_ie = ctx->gpio_ie;
	gpio_oen = ctx->gpio_oen;
	gpio_mode = ctx->gpio_mode;
	gpio_rise = ctx->gpio_rise;
	gpio_fall = ctx->gpio_fall;

	for (int i = 0; i < 8; i++) {
		if (ctx->gpio_enabled & (1 << i)) {
			continue;
		}
		gpio_pu &= ~(1 << i);
		gpio_pd |= (1 << i);
		gpio_ie |= (1 << i);
		gpio_oen &= ~(1 << i);
		gpio_mode &= ~(0xf << (i * 4));
		gpio_mode |= (0x8 << (i * 4));
		gpio_rise &= ~(1 << i);
		gpio_fall &= ~(1 << i);
	}

	writel(gpio_pu, GPIO_BASE_ADDR + 0x04);
	writel(gpio_pd, GPIO_BASE_ADDR + 0x08);
	writel(gpio_ie, GPIO_BASE_ADDR + 0x0c);
	writel(gpio_oen, GPIO_BASE_ADDR + 0x10);
	writel(gpio_mode, GPIO_BASE_ADDR + 0x20);
	writel(gpio_rise, GPIO_BASE_ADDR + 0x30);
	writel(gpio_fall, GPIO_BASE_ADDR + 0x40);

	/* If interrupt is cleared in gpio handler,
	 * no need this routine
	 */

	/* Clear interrupt */
	v = readl(GPIO_BASE_ADDR + 0x60) & 0xFF;
	if (v) {
		uint32_t v2;

		v2 = readl(GPIO_BASE_ADDR + 0x50);

		v2 |= v;
		writel(v2, GPIO_BASE_ADDR + 0x50);
		v2 &= ~v;
		writel(v2, GPIO_BASE_ADDR + 0x50);
	}
}

static void pm_save_gpio(void)
{
	pm_store_gpio();
	pm_pull_down_gpio();
}

#else

#define pm_save_gpio()
#define pm_restore_gpio()

#endif /* CONFIG_PM_AON_GPIO_CTRL */

#ifdef CONFIG_PM_TEST
static int pm_update_test(void)
{
	struct pm_test_ctx *pmt_ctx;
	uint8_t enter_mode;
	uint8_t pm_mode;
	uint8_t test_stop;
	uint64_t dur;

	pmt_ctx = pm_test_get_config();

	if (pmt_ctx->flag == TEST_STOP) {
		return 0;
	}

	enter_mode = pmt_ctx->lowpower_mode;

	dur = pm_mtime_to_usec(IDLE_DUR());
	test_stop = 0;
	switch (enter_mode) {
		case ACTIVE_TO_IDLE:
		case ACTIVE_TO_IDLE_IO_OFF:
			pm_mode = PM_MODE_IDLE;
			break;
		case ACTIVE_TO_LIGHT_SLEEP:
		case ACTIVE_IO_OFF_TO_LIGHT_SLEEP_IO_OFF:
			if (dur < MIN_TIME(PM_LIGHT_SLEEP)) {
				test_stop = 1;
			}
			pm_mode = PM_MODE_LIGHT_SLEEP;
			break;
		case ACTIVE_TO_SLEEP:
		case ACTIVE_TO_SLEEP_IO_OFF:
			if (dur < MIN_TIME(PM_SLEEP)) {
				test_stop = 1;
			}
			pm_mode = PM_MODE_SLEEP;
			break;
		case ACTIVE_TO_DEEP_SLEEP_0:
		case ACTIVE_TO_DEEP_SLEEP_0_IO_OFF:
			if (ctx->wtype != PM_WAKEUP_TYPE_WATCHER) {
				if (dur < MIN_TIME(PM_DEEP_SLEEP)) {
					test_stop = 1;
				}
			}
			pm_mode = PM_MODE_DEEP_SLEEP_0;
			break;
		case ACTIVE_TO_DEEP_SLEEP_1:
		case ACTIVE_TO_DEEP_SLEEP_1_IO_OFF:
			if (ctx->wtype != PM_WAKEUP_TYPE_WATCHER) {
				if (dur < MIN_TIME(PM_DEEP_SLEEP)) {
					test_stop = 1;
				}
			}
			pm_mode = PM_MODE_DEEP_SLEEP_1;
			break;
		case ACTIVE_TO_HIBERNATION:
		case ACTIVE_TO_HIBERNATION_IO_OFF:
			if (ctx->wtype != PM_WAKEUP_TYPE_WATCHER) {
				if (dur < ctx->hib_min_time) {
					test_stop = 1;
				}
			}
			pm_mode = PM_MODE_HIBERNATION;
			break;
		default:
			test_stop = 1;
			break;
	}

	if (test_stop) {
		pm_test_stop();
		ctx->pm_mode = PM_MODE_ACTIVE;
		return -1;
	}

	ctx->pm_mode = pm_mode;

	return 0;
}
#endif

static void pm_prepare_lowpower(void)
{
	uint32_t v;
	uint32_t lowpower_mode;
	uint32_t wakeup_mode;

	/* control SMU low power */
	v = readl(SMU(LOWPOWER_CTRL));

#ifdef CONFIG_PM_TEST
	struct pm_test_ctx *pmt_ctx;

	pmt_ctx = pm_test_get_config();

	lowpower_mode = pmt_ctx->lowpower_mode;
	wakeup_mode = pmt_ctx->wakeup_mode;

	if (!(pm_feat[ctx->pm_mode] & PM_FEAT_CORE_PG)) {
		v |= pmt_ctx->wakeup_src;
	}

	if (ctx->pm_mode == PM_MODE_IDLE) {
#ifndef CONFIG_PM_MULTI_CORE
		writel((uint32_t)pm_test_wakeup_wfi, SMU(RESET_VECTOR(0)));
		writel(0x01, SYS(CORE_RESET_CTRL));
#endif
		v |= (SMU_LOWPOWER_CHIP_IDLE_EN);
	} else {
		if (!(pm_feat[ctx->pm_mode] & PM_FEAT_CORE_PG)) {
			v |= (SMU_LOWPOWER_CHIP_SLEEP_EN);
		}
	}
#else
	lowpower_mode = PMU_MODE(PMU_LOWPOWER, ctx->pm_mode);
	wakeup_mode = PMU_MODE(PMU_WAKEUP, ctx->pm_mode);

	/* wakeup source is configured by watcher if this is PG */
	if (!(pm_feat[ctx->pm_mode] & PM_FEAT_CORE_PG)) {
		v |= (SMU_LOWPOWER_CHIP_SLEEP_EN);
		v |= ctx->wakeup_src;
	}
#endif

	writel(v, SMU(LOWPOWER_CTRL));

	ctx->ratio = systimer_update_ratio(ctx->systimer);

	/* Hibernation keep awake for a while when enter watcher,
	 * wise needs to calculate wakeup time with act ratio.
	 */
	if (ctx->pm_mode == PM_MODE_HIBERNATION || ctx->pm_mode <= PM_MODE_SLEEP)
		ctx->ratio = rram->act_ratio;

	/* configure RTC if the wakeup source has RTC */
	if (ctx->wakeup_src & WAKEUP_SRC_RTC) {
		pm_set_rtc_wakeup();
	}

	/* control pmu irq */
	if ((pm_feat[ctx->pm_mode] & PM_FEAT_CORE_PG)) {
		/* for SYS PG, disable it so that watcher can run */
		pmu_irq_disable();
	} else {
		/* this is work around to exit WFI after wakeup in LIGHT_SLEEP/SLEEP mode */
		pmu_irq_enable();
	}

	/* set the PMU modes */
	pmu_set_mode(lowpower_mode);
	pmu_set_mode(wakeup_mode);

	/* unconditionally USB force suspend */
	v = readl(SMU(SMU_AON_MEMEMA));
	v &= ~(1 << 6);
	writel(v, SMU(SMU_AON_MEMEMA));

	/* unconditionally enable uart lossy wakeup */
	writel(readl(SYS(UART0_CFG)) | 1 << 11, SYS(UART0_CFG));
	writel(readl(SYS(UART1_CFG)) | 1 << 11, SYS(UART1_CFG));
}

static void pm_update_stats(void)
{
	uint32_t v;

	/* clear all wakeup source, disable low power, clear wakeup event */
	v = readl(SMU(LOWPOWER_CTRL));

	/* save wakeup reason */
	if (pm_feat[ctx->pm_mode] & PM_FEAT_CORE_PG) {
		ctx->wakeup_evt = rram->wk_evt;
		ctx->wakeup_reason = rram->wk_fb_reason;
		ctx->wakeup_type = rram->wk_fb_type;
	} else {
		ctx->wakeup_evt = (v >> 19) & WAKEUP_SRC_ALL;
	}
}

static void pm_prepare_active(void)
{
	uint32_t v;

#ifdef CONFIG_PM_TEST
	if (ctx->pm_mode != PM_MODE_IDLE) {
		writel(readl(SYS(UART0_CFG)) & ~(1 << 11), SYS(UART0_CFG));
	}
	writel(readl(SYS(UART1_CFG)) & ~(1 << 11), SYS(UART1_CFG));
#else
	/* unconditionally disable uart lossy wakeup */
	writel(readl(SYS(UART0_CFG)) & ~(1 << 11), SYS(UART0_CFG));
	writel(readl(SYS(UART1_CFG)) & ~(1 << 11), SYS(UART1_CFG));
#endif

	/* unconditionally USB force suspend disable */
	v = readl(SMU(SMU_AON_MEMEMA));
	v |= (1 << 6);
	writel(v, SMU(SMU_AON_MEMEMA));

	/* clear all wakeup source, disable low power, clear wakeup event */
	v = readl(SMU(LOWPOWER_CTRL));
#ifdef CONFIG_PM_TEST
	if (ctx->pm_mode != PM_MODE_IDLE) {
		v &= ~WAKEUP_SRC_ALL;
		v &= ~(SMU_LOWPOWER_CHIP_SLEEP_EN);
		v |= SMU_LOWPOWER_WAKEUP_EVENT_CLR;
		writel(v, SMU(LOWPOWER_CTRL));
	}
#else
	v &= ~WAKEUP_SRC_ALL;
	v &= ~(SMU_LOWPOWER_CHIP_SLEEP_EN | SMU_LOWPOWER_CHIP_IDLE_EN);
	v |= SMU_LOWPOWER_WAKEUP_EVENT_CLR;
	writel(v, SMU(LOWPOWER_CTRL));
#endif

	/* always enable pmu irq regardless of the mode */
	pmu_irq_enable();
}

static void pm_enter_power_down(uint32_t duration)
{
	uint32_t rtc_cur;
	uint32_t rtc_next;
	uint32_t rtc_idle;
	uint32_t v;

	writel(0, PLIC_INT_ENABLE_REG(0, 0));
	writel(0, PLIC_INT_ENABLE_REG(1, 0));
	writel(0, PLIC_INT_ENABLE_REG(0, 1));
	writel(0, PLIC_INT_ENABLE_REG(1, 1));

	/* control SMU low power */
	v = readl(SMU(LOWPOWER_CTRL));
	v |= SMU_LOWPOWER_CHIP_SLEEP_EN;
	v &= ~WAKEUP_SRC_ALL;
	v |= WAKEUP_SRC_GPIO;

	pm_save_gpio();

	/* set up rtc if duration is given */
	if (duration) {
		ctx->ratio = systimer_update_ratio(ctx->systimer);

		if (ctx->pm_mode == PM_MODE_HIBERNATION || ctx->pm_mode <= PM_MODE_SLEEP)
			ctx->ratio = rram->act_ratio;

		rtc_get_32khz_count(ctx->rtc_dev, &rtc_cur);
		rtc_idle = pm_usec_to_rtc((uint64_t)duration * 1000);

		if (rtc_idle > RTC_COUNT_MAX) {
			rtc_idle = RTC_COUNT_MAX;
		}

		if (RTC_WRAP(rtc_idle, rtc_cur)) {
			rtc_next = rtc_idle - (RTC_COUNT_MAX - rtc_cur);
		} else {
			rtc_next = rtc_cur + rtc_idle;
		}

		rtc_set_32khz_alarm(ctx->rtc_dev, rtc_next);
		v |= WAKEUP_SRC_RTC;
	}

	writel(v, SMU(LOWPOWER_CTRL));

	/* set the PMU modes */
	pmu_irq_disable();
	pmu_set_mode(ACTIVE_TO_HIBERNATION_IO_OFF);
	pmu_set_mode(WAKEUP_TO_ACTIVE);

	if (rram->feature & PM_FEATURE_VOLTAGE_CTRL) {
		/* capless LDO reduce to 0.75V */
		pmu_set_dldo_volt(PMU_DLDO_0V75);

		/* enable capless LDO low power mode */
		v = readl(SMU(TOP_CFG));
		v |= SMU_TOPCFG_DIG_LDO_LP_MODE;
		writel(v, SMU(TOP_CFG));

		/* DCDC reduce to 1.15V */
		pmu_set_dcdc_volt(PMU_DCDC_1V15);
	}

	/* unconditionally USB force suspend */
	v = readl(SMU(SMU_AON_MEMEMA));
	v &= ~(1 << 6);
	writel(v, SMU(SMU_AON_MEMEMA));

	writel(0x100000, SMU(RESET_VECTOR(0)));
	writel(0x100000, SMU(RESET_VECTOR(1)));

	while (1) {
		v = readl(SMU(LOWPOWER_CTRL));
		if ((v & 1 << (11))) {
			break;
		}
	}

	pm_wfi();

	/* will never be reached */
	assert(0);
}

#ifndef CONFIG_WLAN

static int pm_check_ratio_valid(void)
{
	uint64_t time_diff;
	uint64_t sleep_dur;
	uint32_t measure_intv = SNAP_LONG_INTERVAL_USEC;

	time_diff = prvReadMtime() - rram->ratio_last_measure_time;

	/* Need to measure RTC ratio, because rtc ratio is too old. */
	if (pm_mtime_to_usec(time_diff) > (uint64_t)ctx->rtc_ratio_invalid_time) {

		sleep_dur = ctx->wtime - prvReadMtime();

		/* If sleep duration is shoter than snap long interval(20ms),
		 * use sleep duration as snap interval.
		 */
		if (pm_mtime_to_usec(sleep_dur) < SNAP_LONG_INTERVAL_USEC) {
			measure_intv = (uint32_t)pm_mtime_to_usec(sleep_dur);
		}

		if (rram->feature & PM_FEATURE_AON_VOLTAGE_CTRL)
			pmu_ctlr_aon(PMU_AON_VOLTAGE_qLR_0V7);
		else
			pmu_ctlr_aon(PMU_AON_VOLTAGE_qLR_0V8);

		systimer_rtc_cal(ctx->systimer, measure_intv);
		rram->ratio_last_measure_time = prvReadMtime();

		pmu_ctlr_aon(PMU_AON_VOLTAGE_iLR_0V8);

		return -1;
	}

	return 0;
}

#endif

void pm_pre_sleep_processing(TickType_t tick)
{
	uint32_t v;

	if (ctx->power_down) {
		pm_enter_power_down(ctx->power_down);
	}

	if (ctx->stay_active) {
		pm_wfi();
		return;
	}

#ifdef CONFIG_PM_MULTI_CORE
	if (ctx->peer_wtime < prvReadMtime()) {
		pm_wfi();
		return;
	}
#endif

	/*
	 * All module are inactive so we should stop rtc calibration
	 */
	if (pm_update_idle_time()) {
		pm_wfi();
		return;
	}

#ifndef CONFIG_WLAN

	if (pm_check_ratio_valid()) {
		/* No need wait. Retry enter sleep mode */
		return;
	}

#endif

	if (pm_update_mode()) {
		pm_wfi();
		return;
	}

#ifdef CONFIG_PM_TEST
	if (pm_update_test()) {
		pm_wfi();
		return;
	}
#endif

#ifdef CONFIG_PM_GPIO_DBG
	pm_gpio_dbg_set(6);
#endif

#ifndef CONFIG_PM_MULTI_CORE
	/* last point to check D25 status */
	v = readl(SMU(LOWPOWER_CTRL));
	if (!(v & 1 << (11))) {
#ifdef CONFIG_SDIO_PM
		pm_wfi();
#else
		assert(0);
#endif
		return;
	}
#endif

	/* SYSTEM power off */
	if (pm_feat[ctx->pm_mode] & PM_FEAT_SYS_PG) {
		int ret;
		pm_save_plic_h1();
		/* Need to update idle time here because
		 * device suspend may take some time.
		 */
		ret = pm_suspend_device(PM_DEFAULT_SUSPEND_DURATION);
		if (ret) {
			pm_restore_plic_h1();
			ctx->pm_mode = PM_MODE_ACTIVE;
			return;
		}
	}

#ifdef CONFIG_PM_MULTI_CORE
	v = readl(SMU(LOWPOWER_CTRL));
	if (!(v & 1 << (11))) {
		pm_resume_device();
		pm_restore_plic_h1();
		ctx->pm_mode = PM_MODE_ACTIVE;
		return;
	}
#endif

	/* save plic and disable all interrupt to avoid race condition */
	pm_save_plic_h0();

	/* invoke user callback for the lowpower mode */
	if (ctx->callback) {
		ctx->callback(ctx->pm_mode);
	}

	if (ctx->ttime < prvReadMtime() + pm_usec_to_mtime(1000)) {
		assert(0);
	}

	if (rram->feature & PM_FEATURE_AON_VOLTAGE_CTRL) {
		pmu_ctlr_aon(PMU_AON_VOLTAGE_qLR_0V7);
	} else {
		pmu_ctlr_aon(PMU_AON_VOLTAGE_qLR_0V8);
	}

	/* XTAL clock gating or off*/
	if (pm_feat[ctx->pm_mode] & PM_FEAT_TIMER_OFF) {
		pm_suspend_h0();
		pm_save_time();
		pm_save_gpio();
	}

	/* setup sys/pmu/smu */
	pm_prepare_lowpower();

	/* workaround for RTC value is not changed after wakeup */
	ctx->rtc32k = readl(RTC_BASE_ADDR + 0x04);

	/* enter wfi to indicate low power mode */
	if (pm_feat[ctx->pm_mode] & PM_FEAT_CORE_PG) {
		pm_update_rram();

		if (ctx->pm_mode == PM_MODE_HIBERNATION) {
#ifndef CONFIG_PM_TEST
			if (ctx->hib_limit_cnt < ctx->hib_max_cnt) {
				ctx->hib_limit_cnt++;
			}
#endif
		}

		/* set the wakeup entry */
		writel(SCM2010_PM_RRAM_FW_ADDR, SMU(RESET_VECTOR(1)));

		/* jump to watcher */
		writel(SCM2010_PM_RRAM_FW_ADDR, SMU(RESET_VECTOR(0)));
		writel(0x01, SYS(CORE_RESET_CTRL));

		pm_lowpower_start();
	} else {
#ifdef CONFIG_PM_GPIO_DBG
		pm_gpio_dbg_clr(6);
#endif
		pm_wfi();
#ifdef CONFIG_PM_GPIO_DBG
		pm_gpio_dbg_set(7);
#endif
	}
}

void pm_post_sleep_processing(TickType_t tick)
{
#ifdef CONFIG_PM_MULTI_CORE
	enum pm_mode pm_mode = ctx->pm_mode;
#endif

	if (ctx->pm_mode == PM_MODE_ACTIVE) {
		return;
	}

	if (pm_feat[ctx->pm_mode] & PM_FEAT_SYS_CG) {
		systimer_load(ctx->systimer);
	}

	pmu_ctlr_aon(PMU_AON_VOLTAGE_iLR_0V8);

	/* Update stats before resuming devices so that
	 * drivers can refer to them.
	 */
	pm_update_stats();

	if (pm_feat[ctx->pm_mode] & PM_FEAT_SYS_PG) {
		/* change to 160Mhz PLL */
		pm_restore_clocks();
		pm_resume_device();
		pm_restore_plic_h1();
	}

	if (pm_feat[ctx->pm_mode] & PM_FEAT_TIMER_OFF) {
		pm_restore_time();
		systimer_rtc_cal(ctx->systimer, SNAP_SHORT_INTERVAL_USEC);
#ifndef CONFIG_PM_MULTI_CORE
		pm_resume_h0();
#endif
		pm_restore_gpio();
	}

	if (pm_feat[ctx->pm_mode] & PM_FEAT_CORE_PG) {
		systimer_restart(ctx->systimer, (rram->exec_watcher & SCM2010_PM_RRAM_WATCHER_EXEC));
	}

#ifndef CONFIG_PM_MULTI_CORE
	/* restore plic after everything else is done */
	pm_restore_plic_h0();
#endif

	/* setup sys/pmu/smu */
	pm_prepare_active();

	/* switch mode to active */
	ctx->pm_mode = PM_MODE_ACTIVE;

	/* invoke user callback for the active mode */
	if (ctx->callback) {
		ctx->callback(ctx->pm_mode);
	}

#ifdef CONFIG_PM_TEST
	pm_test_complete();
#endif

#ifdef CONFIG_PM_GPIO_DBG
	pm_gpio_dbg_clr(7);
#endif

	pm_clear_rram();

	/* Global interrupt must not have been enabled yet. */
	assert(!(__nds__read_csr(NDS_MSTATUS) & MSTATUS_MIE));

#ifdef CONFIG_PM_MULTI_CORE
	if (pm_feat[pm_mode] & PM_FEAT_TIMER_OFF) {
		/* restore hart0 after hart1 ready*/
		pm_resume_h0();
	}

	/* restore plic after everything else is done */
	pm_restore_plic_h0();
#endif
}

void pm_assert(void)
{
	pm_restore_gpio();
	assert(0);
}

static void pm_feature_init(void)
{
#ifdef CONFIG_PM_VOLTAGE_CTRL
	rram->feature |= PM_FEATURE_VOLTAGE_CTRL;
#endif

#ifdef CONFIG_PM_AON_VOLTAGE_CTRL
	rram->feature |= PM_FEATURE_AON_VOLTAGE_CTRL;
#endif

#ifdef CONFIG_PM_AON_VOLTAGE_CTRL_OCR
	rram->feature |= PM_FEATURE_AON_VOLTAGE_CTRL_OCR;
#endif

#ifdef CONFIG_PM_PLL_CTRL
	rram->feature |= PM_FEATURE_PLL_CTRL;
#endif

/*  no rram required, local para for this feature */
#ifdef CONFIG_PM_LOWPOWER_IO_OFF
	ctx->pm_lpwr_io_off = 1;
#endif

#ifdef CONFIG_PM_WAKEUP_IO_OFF
	rram->feature |= PM_FEATURE_WAKEUP_IO_OFF;
#endif

#ifdef CONFIG_PM_WAKEUP_LOG_ON
	rram->feature |= PM_FEATURE_WAKEUP_LOG_ON;
#endif

#ifdef CONFIG_PM_FLASH_DPD
	rram->feature |= PM_FEATURE_FLASH_DPD;
#endif

#ifdef CONFIG_PM_WC_LOG
	rram->feature |= PM_FEATURE_WC_LOG;
#endif

#ifdef CONFIG_PM_WC_UART1
	rram->feature |= PM_FEATURE_WC_UART1;
#endif

#ifdef CONFIG_PM_DLDO_DCDC_CTRL
	rram->feature |= PM_FEATURE_DLDO_DCDC_CTRL;
#endif

	printk("PM feature : 0x%04x\n", rram->feature);
}

static void pm_para_init(void)
{
#ifdef CONFIG_PM_GPIO_DBG
	(*(uint32_t *)(GPIO_BASE_ADDR + 0x10) |= (1 << 6));
	(*(uint32_t *)(GPIO_BASE_ADDR + 0x10) |= (1 << 7));
	pm_gpio_dbg_clr(6);
	pm_gpio_dbg_clr(7);
#endif

	ctx->wakeup_src = PM_DEFAULT_WAKEUP_SRC;
	ctx->pm_mode = PM_MODE_ACTIVE;
	ctx->hib_max_cnt = PM_HIBERNATION_LIMIT_COUNT;

	ctx->stay_active = 1 << PM_DEVICE_APP;

	ctx->pm_mode_enabled =
		(1 << PM_MODE_ACTIVE) | \
		(1 << PM_MODE_LIGHT_SLEEP) | \
		(1 << PM_MODE_DEEP_SLEEP) | \
		(1 << PM_MODE_HIBERNATION);
	ctx->gpio_enabled = 0;

#ifdef CONFIG_SUPPORT_EXT_RTC
	ctx->gpio_enabled |= (1 << 2);
	ctx->gpio_enabled |= (1 << 6);
	ctx->gpio_enabled |= (1 << 7);
#endif

	/* default D25 ILM save address to be safe */
	__d25_ilm_save[0] = (uint32_t) DEFAULT_SAVE_ADDR;
#ifndef CONFIG_SDIO_USE_IRQD  /* SDIO uses D25 run IRQ dispatcher */
#ifndef CONFIG_PM_MULTI_CORE  /* Application is running in D25 */
	/* set the reset vector for the hart0, and make it jump */
	writel((uint32_t)pm_wakeup_wfi, SMU(RESET_VECTOR(0)));
	writel(0x01, SYS(CORE_RESET_CTRL));
#endif
#endif

	__enc_buffer[0] = (uint32_t) DEFAULT_SAVE_ADDR;
}

static void pm_rram_init(void)
{
	uint32_t ratio;

	rram = (struct pm_rram_info *)SCM2010_PM_RRAM_INFO_ADDR;
	/*
	 * RTC ratio is calculated in systimer driver probe.
	 * So, ratio must be maintained.
	 */
	ratio = rram->act_ratio;
	memset(rram, 0, sizeof(struct pm_rram_info));
	if (sizeof(struct pm_rram_info) > 128) {
		assert(0);
	}
	rram->act_ratio = ratio;
	rram->ratio = PM_ACT_RATIO_TO_SLEEP_RATIO(rram->act_ratio);

	printk("ratio: %d\n", ratio);

	/*
	 * TODO: from flash driver
	 * When lowpower mode is hibernation, have to save DLM and ILM to save to flash.
	 * Data size is aligned 32Kb, so we set erase unit as 32Kb to reduce erase time.
	 * However these setting restric save area of start address to aligned 32Kb
	 * If set erase unit as 4Kb, save time is over 4.48s.
	 */
	rram->flash_mem_rd_cmd = readl(SPI0_BASE_ADDR + 0x50) & 0x0F;
	rram->flash_er1_cmd = 0xd8;
	rram->flash_er1_size = 64 * 1024;
	rram->flash_er2_cmd = 0x52;
	rram->flash_er2_size = 32 * 1024;
	rram->flash_timing = readl(0xf0900040);
	rram->flash_clk = readl(SYS(SPI0_CFG));

	/* if firmware is encrypted, save key and iv to retention ram */
	rram->crypto_cfg = readl(SYS(CRYPTO_CFG));

	if (rram->crypto_cfg & SYS_CRYPTO_CFG_CIPHER_EN) {
		for (int i = 0; i < KEY_NUM; i++) {
			rram->crypto_key[i] = readl(SYS(KEY_CFG(i)));
			rram->crypto_iv[i] = readl(SYS(IV_CFG(i)));
		}
	}

	ctx->rtc_ratio_invalid_time = CONFIG_PM_RTC_RATIO_INVALID_TIME * USTIME;

	rram->wk_entry = (uint32_t)pm_wakeup_entry;

	/* retention bits enable */
	writel(readl(SMU(SMU_AON_MEMEMA)) | SMU_AON_MEMEMA_RET,
			SMU(SMU_AON_MEMEMA));

	/* clear lowpower ctrl register */
	writel(0, SMU(LOWPOWER_CTRL));
}

static void pm_timer_init(void)
{
	ctx->systimer = device_get_by_name("systimer");
	if (!ctx->systimer) {
		printk("No systimer device\n");
		assert(0);
	}

	ctx->stay_active_mtime = 0;
	ctx->stay_timer = osTimerNew(pm_activetimeout, osTimerOnce, NULL, NULL);

	ctx->rtc_dev = device_get_by_name("atcrtc");
	if (!ctx->rtc_dev) {
		printk("No RTC device \n");
		assert(0);
	}

#ifdef CONFIG_SD3068
	ctx->ext_rtc_dev = device_get_by_name("sd3068");
	if (!ctx->ext_rtc_dev) {
		printk("No EXT RTC device \n");
		assert(0);
	}
#endif

	ctx->hib_min_time = MIN_TIME(PM_HIBERNATION);
	ctx->hib_save_time = SW_SAVE_TIME(PM_HIBERNATION);

	printk("PM LS : %d+%d=%d\n", SAVE_TIME(PM_LIGHT_SLEEP), RESTORE_TIME(PM_LIGHT_SLEEP), MIN_TIME(PM_LIGHT_SLEEP));
	printk("PM SL : %d+%d=%d\n", SAVE_TIME(PM_SLEEP), RESTORE_TIME(PM_SLEEP), MIN_TIME(PM_SLEEP));
	printk("PM DS : %d+%d=%d\n", SAVE_TIME(PM_DEEP_SLEEP), RESTORE_TIME(PM_DEEP_SLEEP), MIN_TIME(PM_DEEP_SLEEP));
	printk("PM HB : %d+%d=%d\n", SAVE_TIME(PM_HIBERNATION), RESTORE_TIME(PM_HIBERNATION), MIN_TIME(PM_HIBERNATION));
}

static void pm_pmu_lowpower_mode_update(void)
{
	if (ctx->pm_lpwr_io_off) {
		pm_pmu_mode[PMU_LOWPOWER][PM_MODE_SLEEP] 		= ACTIVE_TO_SLEEP_IO_OFF;
		pm_pmu_mode[PMU_LOWPOWER][PM_MODE_DEEP_SLEEP_0]	= ACTIVE_TO_DEEP_SLEEP_0_IO_OFF;
		pm_pmu_mode[PMU_LOWPOWER][PM_MODE_DEEP_SLEEP_1]	= ACTIVE_TO_DEEP_SLEEP_1_IO_OFF;
		pm_pmu_mode[PMU_LOWPOWER][PM_MODE_HIBERNATION]	= ACTIVE_TO_HIBERNATION_IO_OFF;
	}
	else {
		pm_pmu_mode[PMU_LOWPOWER][PM_MODE_SLEEP]		= ACTIVE_TO_SLEEP;
		pm_pmu_mode[PMU_LOWPOWER][PM_MODE_DEEP_SLEEP_0] = ACTIVE_TO_DEEP_SLEEP_0;
		pm_pmu_mode[PMU_LOWPOWER][PM_MODE_DEEP_SLEEP_1] = ACTIVE_TO_DEEP_SLEEP_1;
		pm_pmu_mode[PMU_LOWPOWER][PM_MODE_HIBERNATION]	= ACTIVE_TO_HIBERNATION;
	}
}

static void pm_pmu_wakeup_mode_update(void)
{
	if (rram->feature & PM_FEATURE_WAKEUP_IO_OFF) {
		pm_pmu_mode[PMU_WAKEUP][PM_MODE_DEEP_SLEEP_1]	= WAKEUP_TO_ACTIVE_IO_OFF;
		pm_pmu_mode[PMU_WAKEUP][PM_MODE_HIBERNATION]	= WAKEUP_TO_ACTIVE_IO_OFF;
	}
	else {
		pm_pmu_mode[PMU_WAKEUP][PM_MODE_DEEP_SLEEP_1]	= WAKEUP_TO_ACTIVE;
		pm_pmu_mode[PMU_WAKEUP][PM_MODE_HIBERNATION]	= WAKEUP_TO_ACTIVE;
	}
}

static void pm_pmu_init(void)
{
	pm_pmu_lowpower_mode_update();
	pm_pmu_wakeup_mode_update();
	pmu_init();
}

static void pm_watcher_init(void)
{
	memcpy((void *)SCM2010_PM_RRAM_FW_ADDR, (void *)watcher_fw, sizeof(watcher_fw));
}

static void pm_rf_init(void)
{
#ifdef CONFIG_PM_RFC_LP_BYS
	/* RFC LP bypass */
	writel(0x67, RF_BASE_START + 0x4d8);
#endif
}

#ifdef CONFIG_PM_MULTI_CORE
static void pm_sig_irq(struct pm_sig_msg *msg)
{
	if (msg->type == PM_SIG_MSG_PEER_INFO) {
		ctx->peer_text_lma = msg->item.peer_info.text_lma;
		ctx->peer_resume_addr = msg->item.peer_info.resume_addr;
	} else if (msg->type == PM_SIG_MSG_WAKEUP_TIME) {
		ctx->peer_wtime = msg->item.wakeup_time;
	}
}
#endif

int pm_init(void)
{
	pm_para_init();

	pm_rram_init();

	pm_feature_init();

	pm_pmu_init();

	pm_watcher_init();

	pm_timer_init();

#ifdef CONFIG_PM_MULTI_CORE
	pm_sig_init(pm_sig_irq);
#endif

	pm_rf_init();

#ifdef CONFIG_PM_TEST
	/* keep in latest */
	pm_test_init();
#endif

	return 0;
}

void _pm_stay(enum pm_device pm_dev)
{
	__atomic_fetch_or(&ctx->stay_active, 1 << pm_dev, __ATOMIC_SEQ_CST);
}

/****************************************************************************
 * Name: pm_staytimeout
 *
 * Description:
 *   This function is called by a device driver to indicate that it is
 *   performing meaningful activities (non-idle), needs the power at kept
 *   last the specified level.
 *   And this will be timeout after time (ms), means auto pm_relax
 *
****************************************************************************/
void _pm_staytimeout(u32 ms)
{
	uint64_t stay_mtime;
	stay_mtime = prvReadMtime() + pm_usec_to_mtime(ms * 1000);

	if (osTimerIsRunning(ctx->stay_timer)) {
		if (ctx->stay_active_mtime >= stay_mtime) {
			return;
		}
	}

	pm_stay(PM_DEVICE_TIMED);

	osTimerStart(ctx->stay_timer, ms * hz / 1000);

	ctx->stay_active_mtime = stay_mtime;

}

void _pm_relax(enum pm_device pm_dev)
{
	__atomic_fetch_and(&ctx->stay_active, ~(1 << pm_dev), __ATOMIC_SEQ_CST);

#if defined(CONFIG_LWIP_TIMERS) && !defined(CONFIG_LWIP_TIMERS_ONDEMAND)
	/* Without Ondemand timer, will be wakeup by LWIP timer */
	if (pm_dev == PM_DEVICE_APP) {
		/* Disable lwIP timers */
		extern void sys_timeouts_deinit(void);
		LOCK_TCPIP_CORE();
		sys_timeouts_deinit();
		UNLOCK_TCPIP_CORE();
	}
#endif
}

uint32_t _pm_status(void)
{
	return ctx->stay_active;
}

uint64_t _pm_get_cur_time(void)
{
	return prvReadMtime();
}

void _pm_set_residual(u32 residual)
{
	ctx->residual = residual;
}

/* It used just for monitoring */
u32 _pm_get_residual(void)
{
	return (ctx->residual);
}

int pm_reserved_buf_get_size(void)
{
	/* Return the size of D25 ILM */
	return (D25_ILM_SIZE);
}

void pm_reserved_buf_set_addr(uint32_t addr)
{
	__d25_ilm_save[0] = addr;
	__enc_buffer[0] = addr;
}

uint32_t pm_reserved_buf_get_addr(void)
{
	return __d25_ilm_save[0];
}

uint8_t pm_wakeup_is_himode(void)
{
	if (ctx->pm_mode == SCM2010_PM_RRAM_MODE_HIBERNATION)
		return 1;

	return 0;
}

uint16_t pm_wakeup_get_reason(void)
{
	return ctx->wakeup_reason;
}

uint16_t pm_wakeup_get_type(void)
{
	return ((ctx->wakeup_type & 0xFF00) >> 8);
}

void pm_wakeup_rest_type(void)
{
	ctx->wakeup_type = 0;
	rram->wk_fb_type = 0;
}

uint16_t pm_wakeup_get_subtype(void)
{
	return (ctx->wakeup_type & 0xFF);
}

uint16_t pm_wakeup_get_event(void)
{
	return ctx->wakeup_evt;
}

bool pm_wakeup_is_reset(void)
{
	if (rram->wakeup_flag & SCM2010_PM_RRAM_WAKEUP_RESET)
		return 1;

	return 0;
}

void pm_power_down(uint32_t duration)
{
	if (duration == 0) {
		duration = UINT32_MAX;
	}
	ctx->power_down = duration;
}

void pm_enable_wakeup_io(uint8_t pin)
{
	ctx->gpio_enabled |= (1 << pin);
}

void pm_disable_wakeup_io(uint8_t pin)
{
	ctx->gpio_enabled &= ~(1 << pin);
}

uint8_t pm_query_mode(void)
{
	return ctx->pm_mode_enabled;
}

void pm_enable_mode(uint8_t mode)
{
	ctx->pm_mode_enabled |= (1 << mode);
}

void pm_disable_mode(uint8_t mode)
{
	ctx->pm_mode_enabled &= ~(1 << mode);
}

void pm_register_handler(void (*callback)(int mode))
{
	ctx->callback = callback;
}

void pm_unregister_handler(void)
{
	ctx->callback = NULL;
}

void pm_set_hib_max_count(uint32_t hib_max)
{
	ctx->hib_limit_clr = HIB_CLR_DUR();
	ctx->hib_limit_cnt = 0;
	ctx->hib_max_cnt = hib_max;
}

int pm_feature_set_voltage_ctrl(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (en)
		rram->feature |= PM_FEATURE_VOLTAGE_CTRL;
	else
		rram->feature &= ~PM_FEATURE_VOLTAGE_CTRL;

	return 0;
}

int pm_feature_set_aon_voltage_ctrl(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (rram->feature & PM_FEATURE_AON_VOLTAGE_CTRL_OCR) {
		printf("AON controlled by OCR\n");
		return -1;
	}

	if (en)
		rram->feature |= PM_FEATURE_AON_VOLTAGE_CTRL;
	else
		rram->feature &= ~PM_FEATURE_AON_VOLTAGE_CTRL;

	return 0;
}

int pm_feature_set_pll_ctrl(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;


	if (en)
		rram->feature |= PM_FEATURE_PLL_CTRL;
	else
		rram->feature &= ~PM_FEATURE_PLL_CTRL;

	return 0;
}

int pm_feature_set_lowpower_io(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (en)
		ctx->pm_lpwr_io_off = 0;
	else
		ctx->pm_lpwr_io_off = 1;

	pm_pmu_lowpower_mode_update();

	return 0;
}


int pm_feature_set_wakeup_io(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (en)
		rram->feature &= ~PM_FEATURE_WAKEUP_IO_OFF;
	else
		rram->feature |= PM_FEATURE_WAKEUP_IO_OFF;

	pm_pmu_wakeup_mode_update();

	return 0;
}

int pm_feature_set_wakeup_log(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (en)
		rram->feature |= PM_FEATURE_WAKEUP_LOG_ON;
	else
		rram->feature &= ~PM_FEATURE_WAKEUP_LOG_ON;

	return 0;
}

int pm_feature_set_flash_dpd(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (en)
		rram->feature |= PM_FEATURE_FLASH_DPD;
	else
		rram->feature &= ~PM_FEATURE_FLASH_DPD;

	return 0;
}

int pm_feature_set_wc_log(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (en)
		rram->feature |= PM_FEATURE_WC_LOG;
	else
		rram->feature &= ~PM_FEATURE_WC_LOG;

	return 0;
}

int pm_feature_set_wc_uart1(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (en)
		rram->feature |= PM_FEATURE_WC_UART1;
	else
		rram->feature &= ~PM_FEATURE_WC_UART1;

	return 0;
}

int pm_feature_set_rfc_bypass(uint8_t en)
{
	if (!ctx->stay_active)
		return -1;

	if (en) {
		/* RFC LP bypass */
		writel(0x67, RF_BASE_START + 0x4d8);
	}
	else {
		/* RF sleep mode */
		writel(0x58, RF_BASE_START + 0x4d8);
	}
	return 0;
}

#else /* CONFIG_PM_SCM2010 */

void _pm_stay(enum pm_device pm_dev) {}
void _pm_staytimeout(u32 ms) {}
void _pm_relax(enum pm_device pm_dev) {}
uint64_t _pm_get_cur_time(void) { return 0; }
uint32_t _pm_status(void) { return 0; }
void _pm_set_residual(u32 residual) {}
u32 _pm_get_residual(void) { return 0; }

int pm_init(void) { return 0; }
int pm_reserved_buf_get_size(void) { return 0; }
void pm_reserved_buf_set_addr(uint32_t addr) {}
uint32_t pm_reserved_buf_get_addr(void) { return 0; }
uint8_t pm_wakeup_is_himode(void) { return 0; }
uint16_t pm_wakeup_get_reason(void) { return 0; }
uint16_t pm_wakeup_get_type(void) { return 0; }
uint16_t pm_wakeup_get_subtype(void) { return 0; }
uint16_t pm_wakeup_get_event(void) { return 0; }
bool pm_wakeup_is_reset(void) { return 0; }

int pm_feature_set_voltage_ctrl(uint8_t en) { return 0; }
int pm_feature_set_aon_voltage_ctrl(uint8_t en) { return 0; }
int pm_feature_set_pll_ctrl(uint8_t en) { return 0; }
int pm_feature_set_lowpower_io(uint8_t en) { return 0; }
int pm_feature_set_wakeup_io(uint8_t en) { return 0; }
int pm_feature_set_wakeup_log(uint8_t en) { return 0; }
int pm_feature_set_flash_dpd(uint8_t en) { return 0; }
int pm_feature_set_wc_log(uint8_t en) { return 0; }
int pm_feature_set_wc_uart1(uint8_t en) { return 0; }

#endif /* CONFIG_PM_SCM2010 */

#ifdef CONFIG_LINK_TO_ROM

PROVIDE(pm_stay, &pm_stay, &_pm_stay);
PROVIDE(pm_staytimeout, &pm_staytimeout, &_pm_staytimeout);
PROVIDE(pm_relax, &pm_relax, &_pm_relax);
PROVIDE(pm_get_cur_time, &pm_get_cur_time, &_pm_get_cur_time);
PROVIDE(pm_status, &pm_status, &_pm_status);
PROVIDE(pm_set_residual, &pm_set_residual, &_pm_set_residual);
PROVIDE(pm_get_residual, &pm_get_residual, &_pm_get_residual);

#else

__func_tab__ void (*pm_stay)(enum pm_device pm_dev) = _pm_stay;
__func_tab__ void (*pm_staytimeout)(u32 ms) = _pm_staytimeout;
__func_tab__ void (*pm_relax)(enum pm_device pm_dev) = _pm_relax;
__func_tab__ uint64_t (*pm_get_cur_time)(void) = _pm_get_cur_time;
__func_tab__ uint32_t (*pm_status)(void) = _pm_status;
__func_tab__ void (*pm_set_residual)(u32) = _pm_set_residual;
__func_tab__ u32 (*pm_get_residual)(void) = _pm_get_residual;

#endif


#ifdef CONFIG_CMDLINE

#ifdef CONFIG_SUPPORT_PMP_CMD

/**
 * RTC calibration CLI commands
 */
#include <cli.h>

/* pm set features */
static int do_pm_set_features(int argc, char *argv[])
{
	int ret = CMD_RET_SUCCESS;

	if (argc != 3) {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	if (!strcmp(argv[1], "vol")) /* get beacon rx status */
		ret = pm_feature_set_voltage_ctrl(atoi(argv[2]));
	else if (!strcmp(argv[1], "aonvol"))
		ret = pm_feature_set_aon_voltage_ctrl(atoi(argv[2]));
	else if (!strcmp(argv[1], "pll"))
		ret = pm_feature_set_pll_ctrl(atoi(argv[2]));
	else if (!strcmp(argv[1], "lpio"))
		ret = pm_feature_set_lowpower_io(atoi(argv[2]));
	else if (!strcmp(argv[1], "wuio"))
		ret = pm_feature_set_wakeup_io(atoi(argv[2]));
	else if (!strcmp(argv[1], "wulog"))
		ret = pm_feature_set_wakeup_log(atoi(argv[2]));
	else if (!strcmp(argv[1], "flashdpd"))
		ret = pm_feature_set_flash_dpd(atoi(argv[2]));
	else if (!strcmp(argv[1], "wclog"))
		ret = pm_feature_set_wc_log(atoi(argv[2]));
	else if (!strcmp(argv[1], "wcuart1"))
		ret = pm_feature_set_wc_uart1(atoi(argv[2]));
	else if (!strcmp(argv[1], "rfcbypass"))
		ret = pm_feature_set_rfc_bypass(atoi(argv[2]));
	else {
		printf("\nnot support %s\n", argv[1]);
		ret = CMD_RET_USAGE;
	}

exit:
	if (ret) {
		printf("\nTo disable pm before you set"
		OR "\nUsage: pmp set"
		OR "vol <0/1>"
		OR "aonvol <0/1>"
		OR "pll <0/1>"
		OR "lpio <0/1>"
		OR "wuio <0/1>"
		OR "wulog <0/1>"
		OR "flashdpd <0/1>\n"
		OR "wclog <0/1>\n"
		OR "wcuart1 <0/1>\n"
		OR "rfcbypass <0/1>\n"
		);
	}

	return ret;
}

static const struct cli_cmd do_pmp_cmd[] = {
	CMDENTRY(set, do_pm_set_features, "", ""),
};

static int do_pmp(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], do_pmp_cmd, ARRAY_SIZE(do_pmp_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(pmp, do_pmp,
	"CLI for PM debug",
	"set [vol, aonvol, pll, lpio, wuio, wulog, flashdpd, wclog, wcuart1, rfcbypass] [en]"
);

#endif /* CONFIG_SUPPORT_PMP_CMD */

#ifdef CONFIG_PM_CMD_RTC_CAL

#define SYS_TIMER_CTRL				(*(volatile uint32_t *)(SMU(TIMER_CFG0)))
#define SYS_TIMER_SNAPSHOT			(*(volatile uint32_t *)(SMU(TIMER_CFG1)))
#define SYS_TIMER_RTC_SNAPSHOT		(*(volatile uint32_t *)(SMU(TIMER_CFG2)))
#define SYS_TIMER_RATIO				(*(volatile uint32_t *)(SMU(TIMER_CFG3)))
#define SYS_TIMER_COUNT				(*(volatile uint32_t *)(SMU(TIMER_CFG5)))
#define SYS_TIMER_RTC_COUNT			(*(volatile uint32_t *)(SMU(TIMER_CFG6)))


#define SYS_TIMER_CTRL_SNAP         (1 << 0)
#define SYS_TIMER_CTRL_LOAD         (1 << 1)
#define SYS_TIMER_CTRL_SNAP_INTR    (1 << 2) /* int status: snap not set at all */
#define SYS_TIMER_CTRL_LOAD_INTR    (1 << 3) /* int status: load */
#define SYS_TIMER_CTRL_CMP_INTR     (1 << 4) /* int status: cmp */
#define SYS_TIMER_CTRL_CLR_INTR     (1 << 5)
#define SYS_TIMER_CTRL_EN           (1 << 8)
#define SYS_TIMER_CTRL_SNAP_INTR_EN (1 << 9)
#define SYS_TIMER_CTRL_LOAD_INTR_EN (1 << 10)
#define SYS_TIMER_CTRL_CMP_INTR_EN  (1 << 11)


void systimer_snap_poll(void)
{
	volatile uint32_t pit;
	volatile uint32_t rtc;

	pit = SYS_TIMER_SNAPSHOT;
	rtc = SYS_TIMER_RTC_SNAPSHOT;

	SYS_TIMER_CTRL |= SYS_TIMER_CTRL_SNAP;

	while (1) {
		if (rtc != SYS_TIMER_RTC_SNAPSHOT && pit != SYS_TIMER_SNAPSHOT) {
			break;
		}
	}
}

static int systimer_measure_ratio(int interval_us)
{
	uint32_t prev_pit;
	uint32_t prev_rtc;
	uint32_t now_pit;
	uint32_t now_rtc;
	uint32_t pit_diff;
	uint32_t rtc_diff;
	uint32_t ratio;
	uint32_t cur;

	systimer_snap_poll();

	prev_pit = SYS_TIMER_SNAPSHOT;
	prev_rtc = SYS_TIMER_RTC_SNAPSHOT;

	cur = SYS_TIMER_COUNT;

	while (1) {
		if (cur + (20 * interval_us) <= SYS_TIMER_COUNT) {
			break;
		}
	}

	systimer_snap_poll();

	now_pit = SYS_TIMER_SNAPSHOT;
	now_rtc = SYS_TIMER_RTC_SNAPSHOT;

	if (now_pit >= prev_pit) {
		pit_diff = now_pit - prev_pit;
	} else {
		pit_diff = SYS_TIMER_PIT_COUNT_MAX - prev_pit + now_pit;
	}

	if (now_rtc >= prev_rtc) {
		rtc_diff = now_rtc - prev_rtc;
	} else {
		rtc_diff = SYS_TIMER_RTC_COUNT_MAX - prev_rtc + now_rtc;
	}

	ratio = (uint64_t)pit_diff * 100 / rtc_diff;

	printf("do pit_diff:%d rtc_diff:%d ratio:%d \n",pit_diff, rtc_diff, ratio);

	return ratio;
}

static int rtc_fix_ratio(int argc, char *argv[])
{
	uint32_t interval_us;
	int count;
	int i;
	int ratio_total = 0;
	int ratio = 0;
	int offset = 0;
	int max = 0;
	int min=  70000;


	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	interval_us = (uint32_t)atoi(argv[1]);
	count = (uint32_t)atoi(argv[2]);
	offset = (uint32_t)atoi(argv[3]);

	if (interval_us == 0)
		return CMD_RET_USAGE;

	if (count == 0) {
		rram->feature &= ~PM_FEATURE_RTC_FIX_RATIO;
	} else {
		for (i = 0; i < count; i++) {
			ratio = systimer_measure_ratio(interval_us);
			ratio_total += ratio;
			if (ratio > max)
				max = ratio;
			if (ratio < min)
				min = ratio;
		}

		ratio = ratio_total/count;

		printf("old rram->ratio:%d, fix rram->ratio:%d, ratio: %d, offset: %d, diff: %d\n",
			rram->ratio, ratio - offset, ratio, offset, max - min);

		rram->ratio = ratio;
		rram->feature |= PM_FEATURE_RTC_FIX_RATIO;

		if (offset)
			rram->ratio -= offset;


	}

	return CMD_RET_SUCCESS;
}

static int rtc_ratio_invalid(int argc, char *argv[])
{
	uint32_t invalid_sec;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	printf("Change RTC invalid time\n");

	printf("CUR : %ds -> ", ctx->rtc_ratio_invalid_time / (1000000));

	invalid_sec = (uint32_t)atoi(argv[1]);
	ctx->rtc_ratio_invalid_time =  invalid_sec * (1000000);

	printf("New : %ds\n", invalid_sec);

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd do_rtc_cmd[] = {
	CMDENTRY(inval, rtc_ratio_invalid, "", ""),
	CMDENTRY(ratio, rtc_fix_ratio, "", ""),
};

static int do_rtc_cal(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], do_rtc_cmd, ARRAY_SIZE(do_rtc_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}


CMD(rtc_cal, do_rtc_cal,
		"set RTC calibration interval",
		"rtc_cal inval <sec>"OR
		"rtc_cal ratio <us> <count> <offset>"
);

#endif
#endif
