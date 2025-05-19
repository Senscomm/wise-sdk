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

#ifndef _SYSTIMER_H_
#define _SYSTIMER_H_

#include <hal/device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define stimer_inline inline __attribute__((always_inline))

/*
 * #include <sys/queue.h>
 *
 * Only from the C source code which will use this header
 * It cannot be done here as it results in conflict (ROM already has different queue.h)
 *
 * This structure must NOT BE modified for the same reason.
 */

#define TIMER_CHEN                  0x1C
#define TIMER_CHCTL(ch)             (0x20 + ch * 0x10)
#define TIMER_CHRLD(ch)             (0x24 + ch * 0x10)

#define SYS_TIMER_CTRL				(*(volatile uint32_t *)(SMU(TIMER_CFG0)))
#define SYS_TIMER_SNAPSHOT			(*(volatile uint32_t *)(SMU(TIMER_CFG1)))
#define SYS_TIMER_RTC_SNAPSHOT		(*(volatile uint32_t *)(SMU(TIMER_CFG2)))
#define SYS_TIMER_RATIO				(*(volatile uint32_t *)(SMU(TIMER_CFG3)))
#define SYS_TIMER_COMPARE			(*(volatile uint32_t *)(SMU(TIMER_CFG4)))
#define SYS_TIMER_COUNT				(*(volatile uint32_t *)(SMU(TIMER_CFG5)))
#define SYS_TIMER_RTC_COUNT			(*(volatile uint32_t *)(SMU(TIMER_CFG6)))
#define SYS_TIMER_OSC32K_TRIM		(*(volatile uint32_t *)(SMU(OSC32K_CFG)))

#define SYS_TIMER_PIT_COUNT_MAX 0xFFFFFFFF
#define SYS_TIMER_RTC_COUNT_MAX 0xFFFFFFFF

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

#define SNAP_SHORT_INTERVAL_USEC    (500)
#define SNAP_LONG_INTERVAL_USEC     (20 * 1000)

#define SNAP_MOVING_AVERAGE_WEIGHT(avg, ratio) \
		(((avg) * 7 / 10) + ((ratio) * 3 / 10))

#define SNAP_RATIO_TO_REG(ratio) \
		(((ratio / 100) << 8) | (((ratio % 100) * 255 / 100) & 0xFF))

typedef void (*systimer_cb)(void *arg);

enum SNAP_STATUS {
	SNAP_STATUS_DONE                = 0,
	SNAP_STATUS_BUSY				= 1,
};

struct snap_ctx {
	uint8_t status;                 /* snap status */
	uint32_t start_t;               /* snap start time */
	uint32_t rtc1;                  /* first snapped rtc */
	uint32_t pit1;                  /* first snapped pit */
	uint32_t rtc2;                  /* second snapped rtc */
	uint32_t pit2;                  /* second snapped pit */
};

enum systimer_type {
	SYSTIMER_TYPE_WAKEUP_FULL,
	SYSTIMER_TYPE_WAKEUP_IDLE,
};

enum systimer_snap_complete {
	SYSTIMER_SNAP_NOT_COMPLETE,
	SYSTIMER_SNAP_COMPLETE_OLD,
	SYSTIMER_SNAP_COMPLETE_OK,
};

struct systimer {
	uint8_t enqueued;
	/** systimer type */
	uint8_t type;
	/** Compare Callback function */
	systimer_cb cmp_cb_func;
	/** Compare Callback argument */
	void *cmp_cb_arg;
	/** Tick at which timer should expire */
	uint32_t expiry;
	TAILQ_ENTRY(systimer) link;    /* Queue linked list structure */
};

struct systimer_wakeup_info {
	uint64_t wtime;
	uint64_t wtime_next;
	uint32_t wtype;
	uint32_t wtype_next;
	uint8_t  wtime_next_valid;
};

struct systimer_ops {
	void (*trim)(struct device *dev);
	int (*rtc_cal)(struct device *dev, uint32_t interval);
	uint32_t (*update_ratio)(struct device *dev);
	int (*set_cmp_cb)(struct device *dev, struct systimer *timer,
			enum systimer_type type, systimer_cb cmp_cb_func, void *arg);
	int (*set_snap_cb)(struct device *dev, systimer_cb snap_cb_func);
	int (*start_at)(struct device *dev, struct systimer *timer, uint32_t tick);
	int (*stop)(struct device *dev, struct systimer *timer);
	uint32_t (*get_counter)(struct device *dev);
	int (*get_wakeup_info)(struct device *dev, struct systimer_wakeup_info *info);
	int (*load)(struct device *dev);
	int (*restart)(struct device *dev, int is_exec);
};

#define SYSTIMER_USECS_TO_TICKS(usecs)  (20 * usecs)
#define SYSTIMER_TICKS_TO_USECS(ticks)  (ticks / 20)

#define systimer_get_ops(dev)	((struct systimer_ops *)(dev)->driver->ops)


static stimer_inline void systimer_trim(struct device *dev)
{
	systimer_get_ops(dev)->trim(dev);
}

static stimer_inline int systimer_rtc_cal(struct device *dev, uint32_t interval)
{
	return systimer_get_ops(dev)->rtc_cal(dev, interval);
}

static stimer_inline uint32_t systimer_update_ratio(struct device *dev)
{
	return systimer_get_ops(dev)->update_ratio(dev);
}

static stimer_inline int systimer_set_cmp_cb(struct device *dev, struct systimer *tmr, enum systimer_type type,
        systimer_cb cmp_cb_func, void *arg)
{
	return systimer_get_ops(dev)->set_cmp_cb(dev, tmr, type, cmp_cb_func, arg);
}

static stimer_inline int systimer_set_snap_cb(struct device *dev, systimer_cb snap_cb_func)
{
	return systimer_get_ops(dev)->set_snap_cb(dev, snap_cb_func);
}

static stimer_inline int systimer_start_at(struct device *dev, struct systimer *tmr, uint32_t tick)
{
	return systimer_get_ops(dev)->start_at(dev, tmr, tick);
}

static stimer_inline int systimer_stop(struct device *dev, struct systimer *tmr)
{
	return systimer_get_ops(dev)->stop(dev, tmr);
}

static stimer_inline uint32_t systimer_get_counter(struct device *dev)
{
	return systimer_get_ops(dev)->get_counter(dev);
}

static stimer_inline int systimer_get_wakeup_info(struct device *dev, struct systimer_wakeup_info *info)
{
	return systimer_get_ops(dev)->get_wakeup_info(dev, info);
}

static stimer_inline int systimer_load(struct device *dev)
{
	return systimer_get_ops(dev)->load(dev);
}

static stimer_inline int systimer_restart(struct device *dev, int is_exec)
{
	return systimer_get_ops(dev)->restart(dev, is_exec);
}

#ifdef __cplusplus
}
#endif

#endif //_SYSTIMER_H_
