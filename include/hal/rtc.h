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

#ifndef _RTC_H_
#define _RTC_H_

#include <hal/types.h>
#include <hal/device.h>
#include <hal/clk.h>

/*
 * Generic RTC interface.
 */

/*
 * The struct used to pass data from the generic interface code to
 * the hardware depended low-level code and vice versa.
 *
 * Note that there are small but significant differences to the
 * common "struct time":
 *
 *		struct time:		struct rtc_time:
 * tm_mon	0 ... 11		1 ... 12
 * tm_year	years since 1900	years since 0
 */

struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

#define _32khz			(32768)

#define RTC_COUNT_MAX	0xA8BFFFFF

/* a : add rtc, b : current rtc*/
#define RTC_WRAP(a, b)	((a) > (RTC_COUNT_MAX - (b)))

/* a : before rtc, b : current rtc */
#define RTC_ELAPSED(a, b) (((b) >= (a)) ? ((b) - (a)) : ((RTC_COUNT_MAX - (a)) + (b)))

struct rtc_ops {
	/**
	 * get() - get the current time
	 *
	 * Returns the current time read from the RTC device. The driver
	 * is responsible for setting up every field in the structure.
	 *
	 * @dev:	Device to read from
	 * @time:	Place to put the time that is read
	 */
	int (*get)(struct device *dev, struct rtc_time *time);

	/**
	 * set() - set the current time
	 *
	 * Sets the time in the RTC device. The driver can expect every
	 * field to be set correctly.
	 *
	 * @dev:	Device to set
	 * @time:	Time to write
	 */
	int (*set)(struct device *dev, const struct rtc_time *time);

	/**
	 * reset() - reset the RTC to a known-good state
	 *
	 * This function resets the RTC to a known-good state. The time may
	 * be unset by this method, so should be set after this method is
	 * called.
	 *
	 * @dev:	Device to reset
	 */
	int (*reset)(struct device *dev);

	/**
	 * get_32khz_count() - get the 32khz clock counter
	 *
	 * Returns the current count of 32khz clock fed to the RTC.
	 *
	 * @dev:	Device to read from
	 * @count:	Place to put the count that is read
	 */
	int (*get_32khz_count)(struct device *dev, uint32_t *count);

	/**
	 * set_32khz_alarm() - set alarm wakeup
	 *
	 * Sets the target count of 32khz clock for RTC to generate wakeup alarm.
	 *
	 * @dev:	Device to set
	 * @count:	Count to set
	 */
	int (*set_32khz_alarm)(struct device *dev, uint32_t count);

	/**
	 * clear_32khz_alarm() - clear alarm wakeup
	 *
	 * Clears the generated wakeup alarm.
	 *
	 * @dev:	Device to set
	 */
	int (*clear_32khz_alarm)(struct device *dev);

};

/* Access the operations for an RTC device */
#define rtc_get_ops(dev)	((struct rtc_ops *)(dev)->driver->ops)

/**
 * rtc_get() - Read the time from an RTC
 *
 * @dev:	Device to read from
 * @time:	Place to put the current time
 * @return 0 if OK, -1 on error
 */
static __inline__ int rtc_get(struct device *dev, struct rtc_time *time)
{
	if (!dev)
		return -ENODEV;
	if (!rtc_get_ops(dev)->get)
		return -ENOSYS;

	return rtc_get_ops(dev)->get(dev, time);

}

/**
 * rtc_set() - Write a time to an RTC
 *
 * @dev:	Device to set
 * @time:	Time to write into the RTC
 * @return 0 if OK, -1 on error
 */
static __inline__ int rtc_set(struct device *dev, struct rtc_time *time)
{
	if (!dev)
		return -ENODEV;
	if (!rtc_get_ops(dev)->set)
		return -ENOSYS;

	return rtc_get_ops(dev)->set(dev, time);

}

/**
 * rtc_reset() - reset the RTC to a known-good state
 *
 * If the RTC appears to be broken (e.g. it is not counting up in seconds)
 * it may need to be reset to a known good state. This function achieves this.
 * After resetting the RTC the time should then be set to a known value by
 * the caller.
 *
 * @dev:	Device to reset
 * @return 0 if OK, -1 on error
 */
static __inline__ int rtc_reset(struct device *dev)
{
	if (!dev)
		return -ENODEV;
	if (!rtc_get_ops(dev)->reset)
		return -ENOSYS;

	return rtc_get_ops(dev)->reset(dev);

}

/**
 * rtc_get_32khz_count() - Read the 32khz clock count from an RTC
 *
 * @dev:	Device to read from
 * @count:	Place to put the current count
 * @return 0 if OK, -1 on error
 */
static __inline__ int rtc_get_32khz_count(struct device *dev, uint32_t *count)
{
	if (!dev)
		return -ENODEV;
	if (!rtc_get_ops(dev)->get_32khz_count)
		return -ENOSYS;

	return rtc_get_ops(dev)->get_32khz_count(dev, count);

}

/**
 * rtc_set_32khz_alarm() - Set the target 32khz clock count for alarm wakeup
 *
 * @dev:	Device to set
 * @count:	Target count
 * @return 0 if OK, -1 on error
 */
static __inline__ int rtc_set_32khz_alarm(struct device *dev, uint32_t count)
{
	if (!dev)
		return -ENODEV;
	if (!rtc_get_ops(dev)->set_32khz_alarm)
		return -ENOSYS;

	return rtc_get_ops(dev)->set_32khz_alarm(dev, count);

}

/**
 * rtc_clear_32khz_alarm() - Clear the alarm wakeup
 *
 * @dev:	Device to set
 * @return 0 if OK, -1 on error
 */
static __inline__ int rtc_clear_32khz_alarm(struct device *dev)
{
	if (!dev)
		return -ENODEV;
	if (!rtc_get_ops(dev)->clear_32khz_alarm)
		return -ENOSYS;

	return rtc_get_ops(dev)->clear_32khz_alarm(dev);

}
#endif  /* _RTC_H_ */
