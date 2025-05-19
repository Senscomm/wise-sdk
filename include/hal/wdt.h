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

#ifndef _WDT_H_
#define _WDT_H_

#include <hal/types.h>
#include <hal/device.h>
#include <hal/clk.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level device driver
 */

#define IOCTL_WDT_START			(1)
#define IOCTL_WDT_STOP			(2)
#define IOCTL_WDT_FEED			(3)
#define IOCTL_WDT_EXPIRE_NOW	(4)

/*
 * Low-level device driver
 */

/*
 * Implement a simple watchdog driver. Watchdog is basically a timer that
 * is used to detect or recover from malfunction. During normal operation
 * the watchdog would be regularly reset to prevent it from timing out.
 * If, due to a hardware fault or program error, the computer fails to reset
 * the watchdog, the timer will elapse and generate a timeout signal.
 * The timeout signal is used to initiate corrective action or actions,
 * which typically include placing the system in a safe, known state.
 */

/*
 * struct wdt_ops - Driver model wdt operations
 *
 * The driver interface is implemented by all wdt devices which use
 * driver model.
 */
struct wdt_ops {
	/*
	 * Start the timer
	 *
	 * @dev: WDT Device
	 * @timeout_ms: Number of ticks (milliseconds) before the timer expires
	 * @return: 0 if OK, -ve on error
	 */
	int (*start)(struct device *wdt, u64 timeout_ms);
	/*
	 * Stop the timer
	 *
	 * @dev: WDT Device
	 * @return: 0 if OK, -ve on error
	 */
	int (*stop)(struct device *wdt);
	/*
	 * Reset the timer, typically restoring the counter to
	 * the value configured by start()
	 *
	 * @dev: WDT Device
	 * @return: 0 if OK, -ve on error
	 */
	int (*reset)(struct device *wdt);
	/*
	 * Expire the timer, thus executing the action immediately (optional)
	 *
	 * If this function is not provided, a default implementation
	 * will be used, which sets the counter to 1
	 * and waits forever. This is good enough for system level
	 * reset, where the function is not expected to return, but might not be
	 * good enough for other use cases.
	 *
	 * @dev: WDT Device
	 * @return 0 if OK -ve on error. May not return.
	 */
	int (*expire_now)(struct device *wdt);
};

#define wdt_ops(x)	((struct wdt_ops *)(x)->driver->ops)

/*
 * Start the timer
 *
 * @dev: WDT Device
 * @timeout_ms: Number of ticks (milliseconds) before timer expires
 * @return: 0 if OK, -ve on error
 */
static __inline__ int wdt_start(struct device *wdt, u64 timeout_ms)
{
	if (!wdt)
		return -ENODEV;
	if (!wdt_ops(wdt)->start)
		return -ENOSYS;

	return wdt_ops(wdt)->start(wdt, timeout_ms);
}

/*
 * Stop the timer, thus disabling the Watchdog. Use wdt_start to start it again.
 *
 * @dev: WDT Device
 * @return: 0 if OK, -ve on error
 */
static __inline__ int wdt_stop(struct device *wdt)
{
	if (!wdt)
		return -ENODEV;
	if (!wdt_ops(wdt)->stop)
		return -ENOSYS;

	return wdt_ops(wdt)->stop(wdt);
}

/*
 * Reset the timer, typically restoring the counter to
 * the value configured by start()
 *
 * @dev: WDT Device
 * @return: 0 if OK, -ve on error
 */
static __inline__ int wdt_reset(struct device *wdt)
{
	if (!wdt)
		return -ENODEV;
	if (!wdt_ops(wdt)->reset)
		return -ENOSYS;

	return wdt_ops(wdt)->reset(wdt);
}

/*
 * Expire the timer, thus executing its action immediately.
 * This is typically used to reset the board or peripherals.
 *
 * @dev: WDT Device
 * @flags: Driver specific flags
 * @return 0 if OK -ve on error. If wdt action is system reset,
 * this function may never return.
 */
static __inline__ int wdt_expire_now(struct device *wdt)
{
	if (!wdt)
		return -ENODEV;

	if (wdt_ops(wdt)->expire_now)
		return wdt_ops(wdt)->expire_now(wdt);
	else {
		if (!wdt_ops(wdt)->start)
			return -ENOSYS;
		return wdt_ops(wdt)->start(wdt, 1);
	}
}

#ifdef __cplusplus
}
#endif

#endif  /* _WDT_H_ */
