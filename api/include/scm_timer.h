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

#ifndef __SCM_TIMER_H__
#define __SCM_TIMER_H__

#include <stdint.h>

#include "wise_err.h"

/**
 * TIMER device index
 */
enum scm_timer_idx {
	SCM_TIMER_IDX_0,
	SCM_TIMER_IDX_1,
	SCM_TIMER_IDX_MAX,
};

/**
 * TIMER channels
 */
enum scm_timer_ch {
	SCM_TIMER_CH_0,
	SCM_TIMER_CH_1,
	SCM_TIMER_CH_2,
	SCM_TIMER_CH_3,
	SCM_TIMER_CH_MAX,
};

/**
 * TIMER modes
 */
enum scm_timer_mode {
	SCM_TIMER_MODE_PERIODIC,
	SCM_TIMER_MODE_ONESHOT,
	SCM_TIMER_MODE_FREERUN,
	SCM_TIMER_MODE_PWM,
};

/**
 * TIMER parameter for SCM_TIMER_MODE_PERIODIC
 */
struct scm_timer_cfg_periodic {
	uint32_t duration; /* usec, up to ~107 sec (UINT32_MAX / 40MHz) */
};

/**
 * TIMER parameter for SCM_TIMER_MODE_ONESHOT
 */
struct scm_timer_cfg_oneshot {
	uint32_t duration; /* usec, up to ~107 sec (UINT16_MAX / 40MHz) */
};

/**
 * TIMER parameter for SCM_TIMER_MODE_FREERUN
 */
struct scm_timer_cfg_freerun {
	uint32_t freq; /* freq, 1~1MHz */
};

/**
 * TIMER parameter for SCM_TIMER_MODE_PWM
 */
struct scm_timer_cfg_pwm {
	uint16_t high; /* usec, up to ~1638 usec (UINT16_MAX / 40MHz) */
	uint16_t low;  /* usec, up to ~1638 usec (UINT16_MAX / 40MHz) */
	uint8_t park;  /* level of PWM signal when the timer is stopped */
};

/**
 * TIMER configuration
 */
struct scm_timer_cfg {
	enum scm_timer_mode mode;
	uint8_t intr_en;
	union {
		struct scm_timer_cfg_periodic periodic;
		struct scm_timer_cfg_oneshot oneshot;
		struct scm_timer_cfg_freerun freerun;
		struct scm_timer_cfg_pwm pwm;
	} data;
};

/**
 * TIMER event
 */
enum scm_timer_event_type {
	SCM_TIMER_EVENT_EXPIRE,
};

/**
 * TIMER notification callback
 */
typedef int (*scm_timer_notify)(enum scm_timer_event_type type, void *ctx);

/**
 * @brief Configure TIMER
 *
 * @param[in] idx index of the TIMER
 * @param[in] ch channel number
 * @param[in] cfg mode of the timer
 * @param[in] notify notification callback
 * @param[in] ctx user context associated with the notification event
 */
int scm_timer_configure(enum scm_timer_idx idx, enum scm_timer_ch ch,
						struct scm_timer_cfg *cfg, scm_timer_notify notify, void *ctx);

/**
 * @brief Start TIMER
 *
 * @param[in] idx index of the TIMER
 * @param[in] ch channel number
 */
int scm_timer_start(enum scm_timer_idx idx, enum scm_timer_ch ch);

/**
 * @brief Stop TIMER
 *
 * @param[in] idx index of the TIMER
 * @param[in] ch channel number
 */
int scm_timer_stop(enum scm_timer_idx idx, enum scm_timer_ch ch);

/**
 * @brief Start multiple TIMER
 *
 * @param[in] idx index of the TIMER
 * @param[in] chs channel numbers ORed in bit position
 */
int scm_timer_start_multi(enum scm_timer_idx idx, uint8_t chs);

/**
 * @brief Stop multiple TIMER
 *
 * @param[in] idx index of the TIMER
 * @param[in] chs channel numbers ORed in bit position
 */
int scm_timer_stop_multi(enum scm_timer_idx idx, uint8_t chs);

/**
 * @brief Get TIMER value
 *
 * @param[in] idx index of the TIMER
 * @param[in] ch channel number
 * @param[in] value pointer to the value to receive
 *
 * If the timer was configured to be freeruning, the function returns the current counter.
 * It is software accumulated value from the start of the timer.
 *
 * If the timer was configured to be periodic or onetime, the function returns the remaining
 * time until the expiration. When the timer stops, the counter is reset to the initial reload value.
 * The hardware is down counter start from the reload value.
 */
int scm_timer_value(enum scm_timer_idx idx, enum scm_timer_ch ch, uint32_t *value);

#endif //__SCM_TIMER_H__
