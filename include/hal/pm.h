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

#ifndef _PM_H_
#define _PM_H_

#include <hal/types.h>

enum pm_wakeup_type {
	PM_WAKEUP_TYPE_FULL    = 0,
	PM_WAKEUP_TYPE_WATCHER,
	PM_WAKEUP_TYPE_MAX,
};

enum pm_device {
	PM_DEVICE_WIFI		= 0,
	PM_DEVICE_BLE		= 1,
	PM_DEVICE_UART		= 2,
	PM_DEVICE_TIMED		= 3,
	PM_DEVICE_APP		= 4,
	PM_TEST				= 5,
};

enum pm_mode {
	PM_MODE_ACTIVE = 0,
	PM_MODE_IDLE,
	PM_MODE_LIGHT_SLEEP,
	PM_MODE_SLEEP,
	PM_MODE_DEEP_SLEEP_0,
	PM_MODE_DEEP_SLEEP_1,
	PM_MODE_DEEP_SLEEP = PM_MODE_DEEP_SLEEP_1,
	PM_MODE_HIBERNATION,
	PM_MODE_MAX,
};

int pm_init(void);
int pm_reserved_buf_get_size(void);
void pm_reserved_buf_set_addr(uint32_t);
uint32_t pm_reserved_buf_get_addr(void);
u8 pm_wakeup_is_himode(void);
bool pm_wakeup_is_reset(void);
uint16_t pm_wakeup_get_reason(void);
uint16_t pm_wakeup_get_type(void);
void pm_wakeup_rest_type(void);
uint16_t pm_wakeup_get_subtype(void);
u16 pm_wakeup_get_event(void);

void pm_power_down(uint32_t duration);
void pm_enable_wakeup_io(uint8_t pin);
void pm_disable_wakeup_io(uint8_t pin);
uint8_t pm_query_mode(void);
void pm_enable_mode(uint8_t mode);
void pm_disable_mode(uint8_t mode);
void pm_register_handler(void (*callback)(int state));
void pm_unregister_handler(void);
void pm_set_hib_max_count(uint32_t hib_max);

int pm_feature_set_voltage_ctrl(uint8_t en);
int pm_feature_set_aon_voltage_ctrl(uint8_t en);
int pm_feature_set_pll_ctrl(uint8_t en);
int pm_feature_set_lowpower_io_off(uint8_t en);
int pm_feature_set_wakeup_io(uint8_t en);
int pm_feature_set_wakeup_log(uint8_t en);
int pm_feature_set_flash_dpd(uint8_t en);
int pm_feature_set_wc_log(uint8_t en);

extern void (*pm_stay)(enum pm_device pm_dev);
extern void (*pm_staytimeout)(u32 ms);
extern void (*pm_relax)(enum pm_device pm_dev);
extern uint64_t (*pm_get_cur_time)(void);
extern uint32_t (*pm_status)(void);
extern void (*pm_set_residual)(u32);
extern u32 (*pm_get_residual)(void);

#endif //_PM_H_
