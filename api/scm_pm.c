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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "hal/types.h"
#include "hal/pm.h"
#include "scm_pm.h"
#include "sys/queue.h"

#define GPIO_AON_MAX	7

/*
 * PM callback context
 */
struct pm_callback {
	TAILQ_ENTRY(pm_callback) link;
	void (*callback)(enum scm_pm_state state);
};

TAILQ_HEAD(pm_callback_queue, pm_callback) pm_callbacks = TAILQ_HEAD_INITIALIZER(pm_callbacks);

static const int pm_mode_to_state[] = {
	[PM_MODE_ACTIVE]			= SCM_PM_STATE_ACTIVE,
	[PM_MODE_LIGHT_SLEEP]		= SCM_PM_STATE_LIGHT_SLEEP,
	[PM_MODE_DEEP_SLEEP_1]		= SCM_PM_STATE_DEEP_SLEEP,
	[PM_MODE_HIBERNATION]		= SCM_PM_STATE_HIBERNATION,
};

static const int pm_state_to_mode[] = {
	[SCM_PM_STATE_ACTIVE]		= PM_MODE_ACTIVE,
	[SCM_PM_STATE_LIGHT_SLEEP]	= PM_MODE_LIGHT_SLEEP,
	[SCM_PM_STATE_DEEP_SLEEP]	= PM_MODE_DEEP_SLEEP,
	[SCM_PM_STATE_HIBERNATION]	= PM_MODE_HIBERNATION,
};

static void scm_pm_handler(int mode)
{
	struct pm_callback *entry;

	/* iterate through the list, and invoke callbacks */
	TAILQ_FOREACH(entry, &pm_callbacks, link) {
		entry->callback(pm_mode_to_state[mode]);
	}
}

int scm_pm_power_down(uint32_t duration)
{
	if (duration > SCM_PM_DURATION_MAX) {
		return -1;
	}

	pm_power_down(duration);

	return 0;
}

int scm_pm_enable_wakeup_io(uint8_t pin)
{
	if (pin > GPIO_AON_MAX) {
		return -1;
	}

	pm_enable_wakeup_io(pin);

	return 0;
}

int scm_pm_disable_wakeup_io(uint8_t pin)
{
	if (pin > GPIO_AON_MAX) {
		return -1;
	}

	pm_disable_wakeup_io(pin);

	return 0;
}

int scm_pm_enable_lowpower(void)
{
	pm_relax(PM_DEVICE_APP);

	return 0;
}

int scm_pm_disable_lowpower(void)
{
	pm_stay(PM_DEVICE_APP);

	return 0;
}

int scm_pm_disable_lowpower_timeout(uint32_t duration)
{
	if (duration > SCM_PM_DURATION_MAX) {
		return -1;
	}

	pm_staytimeout(duration);

	return 0;
}

int scm_pm_enable_state(enum scm_pm_state state)
{
	if ((state < SCM_PM_STATE_LIGHT_SLEEP) ||
		(state > SCM_PM_STATE_HIBERNATION)) {
		return -1;
	}

	pm_enable_mode(pm_state_to_mode[state]);

	return 0;
}

int scm_pm_disable_state(enum scm_pm_state state)
{
	if ((state < SCM_PM_STATE_LIGHT_SLEEP) ||
		(state > SCM_PM_STATE_HIBERNATION)) {
		return -1;
	}

	pm_disable_mode(pm_state_to_mode[state]);

	return 0;
}

int scm_pm_register_handler(scm_pm_notify callback)
{
	struct pm_callback *entry;

	if (callback == NULL) {
		return -1;
	}

	if (TAILQ_EMPTY(&pm_callbacks)) {
		pm_register_handler(scm_pm_handler);
	}

	/* find if handler is already in the list */
	TAILQ_FOREACH(entry, &pm_callbacks, link) {
		if (entry->callback == callback) {
			return -1;
		}
	}

	/* add handler to the list */
	entry = os_malloc(sizeof(struct pm_callback));
	entry->callback = callback;

	TAILQ_INSERT_TAIL(&pm_callbacks, entry, link);

	return 0;
}

int scm_pm_unregister_handler(scm_pm_notify callback)
{
	struct pm_callback *entry;
	uint8_t found = 0;

	if (callback == NULL) {
		return -1;
	}

	/* remove handler from the list */
	TAILQ_FOREACH(entry, &pm_callbacks, link) {
		if (entry->callback == callback) {
			found = 1;
			break;
		}
	}

	if (!found) {
		return -1;
	}

	TAILQ_REMOVE(&pm_callbacks, entry, link);
	os_free(entry);

	if (TAILQ_EMPTY(&pm_callbacks)) {
		pm_unregister_handler();
	}

	return 0;
}

int scm_pm_set_hib_max_count(uint32_t hib_max)
{
	pm_set_hib_max_count(hib_max);

	return 0;
}
