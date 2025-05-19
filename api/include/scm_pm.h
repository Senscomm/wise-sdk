/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __SCM_PM_H__
#define __SCM_PM_H__

#include <stdint.h>
#include <string.h>
#include <u-boot/list.h>

#include "hal/pm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * - Any lowpower state maintains WIFI connection.
 * - The lowpower state is automatically selected by the OS based on next idle duration.
 */
enum scm_pm_state {
	SCM_PM_STATE_ACTIVE			= 0,	/**< ACTIVE - all system components are active */
	SCM_PM_STATE_LIGHT_SLEEP	= 1,	/**< LIGHT SLEEP - cores are clock gated */
	SCM_PM_STATE_DEEP_SLEEP		= 2,	/**< DEEP SLEEP - most components are powered down, XTAL/PLL */
	SCM_PM_STATE_HIBERNATION	= 3,	/**< HIBERNATION - most power efficient */
};

/** Maximum duration for the low power duration */
#define SCM_PM_DURATION_MAX	(60 * 60 * 24 * 1000 - 1)

/**
 * PM notification callback
 */
typedef void (*scm_pm_notify)(enum scm_pm_state state);

/**
 * @brief Enter the lowest power down state (hibernation) unconditionally
 *
 * The device would lose all the contexts and any network connectivity is lost.
 * If the duration is 0, device can wake up by GPIO only.
 * This function will not return if successfully put into power down state.
 * When the device wakes up, it starts from the reset vector as if it is a POR.
 *
 * @param[in] duration the duration of the power down period in millisecond
 */
int scm_pm_power_down(uint32_t duration);

/**
 * @brief Enable a GPIO pin to be used for the wakeup
 *
 * The GPIO must be configured to trigger the interrupt from
 * the application before calling this function.
 */
int scm_pm_enable_wakeup_io(uint8_t pin);

/**
 * @brief Disable a GPIO pin being used as a wakeup event.
 *
 * If the GPIO was not configured for the wakeup, it has no effect.
 */
int scm_pm_disable_wakeup_io(uint8_t pin);

/**
 * @brief Enable lowpower state
 *
 * Calling this function indicates that the caller is finished
 * with the work and it is now in idle state. The device will
 * enter the lowpower state when it is ready.
 */
int scm_pm_enable_lowpower(void);

/**
 * @brief Disable lowpower state
 *
 * Calling this function indicates that the caller is performning
 * meaningful activities and it does not want the device to enter
 * lowpower states.
 */
int scm_pm_disable_lowpower(void);

/**
 * @brief Disable lowpower state for the give duration
 *
 * The device will stay in PM ACTIVE for the duration requested starting
 * from the moment this function is called. If there is a previous call
 * to this function, and the time has not elapsed, the longer call with
 * the longest active duration will take effect.
 *
 * @param[in] duration the duration of the active period until the next lowpower
 */
int scm_pm_disable_lowpower_timeout(uint32_t duration);

/**
 * @brief Enable specific PM states
 *
 * The state parameter indicates the PM states to be enabled.
 *
 * @param[in] state state to be enabled
 */
int scm_pm_enable_state(enum scm_pm_state state);

/**
 * @brief Disable specific PM states
 *
 * The state parameter indicates the PM states to be disabled.
 *
 * @param[in] state state to be disabled
 */
int scm_pm_disable_state(enum scm_pm_state state);

/**
 * @brief Register a application specific PM callback
 *
 * This function can be used to get notified of the PM state changes.
 *
 * @param[in] pm_cb the callback context to be registered
 */
int scm_pm_register_handler(scm_pm_notify callback);

/**
 * @brief Unregister the application PM callback
 *
 * This function removes the previously registered callback.
 *
 * @param[in] pm_cb the callback context to be unregistered
 */
int scm_pm_unregister_handler(scm_pm_notify callback);

/**
 * @brief Set max count to enter hibernation mode
 *
 * This function set max count to enter hibernation mode.
 *
 * @param[in] hib_max hibernation mode max count
 */
int scm_pm_set_hib_max_count(uint32_t hib_max);

#ifdef __cplusplus
}
#endif

#endif //__SCM_PM_H__
