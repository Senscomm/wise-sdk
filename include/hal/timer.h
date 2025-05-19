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
#ifndef __TIMER_H__
#define __TIMER_H__

#include <hal/device.h>
#include <hal/clk.h>
#include <freebsd/errors.h>
#include <FreeRTOS/FreeRTOS.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level device driver
 */

#define IOCTL_TIMER_CONFIGURE   (1)
#define IOCTL_TIMER_START       (2)
#define IOCTL_TIMER_STOP        (3)
#define IOCTL_TIMER_START_MULTI (4)
#define IOCTL_TIMER_STOP_MULTI  (5)
#define IOCTL_TIMER_VALUE       (6)

enum timer_event_type {
	TIMER_EVENT_EXPIRE,
};

typedef int (*timer_isr)(enum timer_event_type type, void *ctx);

struct timer_cfg_arg {
	uint32_t ch;
	uint32_t config;
	uint32_t param;
	timer_isr isr;
	void *ctx;
};

struct timer_start_arg {
	uint32_t ch;
};

struct timer_stop_arg {
	uint32_t ch;
};

struct timer_start_multi_arg {
	uint32_t chs;
};

struct timer_stop_multi_arg {
	uint32_t chs;
};

struct timer_value_arg {
	uint32_t ch;
	uint32_t *value;
};

/*
 * Low-level device driver
 */

#define HAL_TIMER_PERIODIC 		0x00000001
#define HAL_TIMER_ONESHOT  		0x00000002
#define HAL_TIMER_FREERUN  		0x00000004
#define HAL_TIMER_PWM 	 		0x00000008
#define HAL_TIMER_IRQ	   		0x00000010
#define HAL_TIMER_PWM_PARK 		0x00000020

/**
 *
 * @setup: @param is interpreted either as interval or rate depending on
 * 		   @config.
 */
struct timer_ops {
	int (*setup)(struct device *timer, u8 ch, u32 config, u32 param, timer_isr isr, void *ctx);
	int (*start)(struct device *timer, u8 ch);
	int (*stop)(struct device *timer, u8 ch);
	int (*start_multi)(struct device *timer, u8 chs);
	int (*stop_multi)(struct device *timer, u8 chs);
	unsigned long (*get_rate)(struct device *timer);
	u32 (*get_value)(struct device *timer, u8 ch);
	int (*adjust)(struct device *timer, u8 ch, u32 adv); /* advance in usec */
};

#define timer_ops(x)	((struct timer_ops *)(x)->driver->ops)

static __inline__ int timer_setup(struct device *timer, u8 ch, u32 config, u32 param, timer_isr isr, void *ctx)
{
	if (!timer)
		return -ENODEV;

	return timer_ops(timer)->setup(timer, ch, config, param, isr, ctx);
}

static __inline__ int timer_start(struct device *timer, u8 ch)
{
	if (!timer)
		return -ENODEV;

	return timer_ops(timer)->start(timer, ch);
}

static __inline__ int timer_stop(struct device *timer, u8 ch)
{
	if (!timer)
		return -ENODEV;

	return timer_ops(timer)->stop(timer, ch);
}

static __inline__ int timer_start_multi(struct device *timer, u8 chs)
{
	if (!timer)
		return -ENODEV;

	return timer_ops(timer)->start_multi(timer, chs);
}

static __inline__ int timer_stop_multi(struct device *timer, u8 chs)
{
	if (!timer)
		return -ENODEV;

	return timer_ops(timer)->stop_multi(timer, chs);
}

static __inline__ u32 timer_get_rate(struct device *timer)
{
	if (!timer)
		return 0;

	return timer_ops(timer)->get_rate(timer);
}

static __inline__ u32 timer_get_value(struct device *timer, u8 ch)
{
	if (!timer)
		return 0;

	return timer_ops(timer)->get_value(timer, ch);
}

static __inline__ u32 timer_adjust(struct device *timer, u8 ch, int adj)
{
	if (!timer)
		return 0;

	return timer_ops(timer)->adjust(timer, ch, adj);
}

/* FIXME: what if CONFIG_TIMER_HZ > 1000000 */
/* FIXME: what if 1000000 % CONFIG_TIMER_HZ != 0 */
extern int time_hz;
#ifndef CONFIG_BUILD_ROM
__ilm__
#endif
static u32 __inline tick_to_us (u32 t)
{
	return t * (1000000 / time_hz);
}

static unsigned long __inline us_to_tick (unsigned long t)
{
	if (time_hz >=1000000)
		return ((t) * (time_hz/1000000));
	else
		return ((t * time_hz)/1000000);
}

#ifndef CONFIG_BUILD_ROM
__ilm__
#endif
static u32 __inline ms_to_tick (u32 t)
{
	return ((t) * (time_hz/1000));
}

#if 1
int register_timer(struct device *);
void unregister_timer(struct device *);
unsigned _ktime(void);
extern unsigned (*ktime)(void);

#define hal_timer_value ktime /* backward compatibility */
#else
u32 hal_timer_value(void);
#endif

static void __inline udelay(u32 udelay)
{
	u32 now = tick_to_us(ktime());

	while (tick_to_us(ktime()) < now + udelay)
		asm("nop");
}

#ifdef __cplusplus
}
#endif

#endif
