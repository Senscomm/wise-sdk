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

#ifndef _PMU_H_
#define _PMU_H_

enum pmu_mode {
    ACTIVE_TO_DEEP_SLEEP_0              = 0,
    ACTIVE_TO_DEEP_SLEEP_0_IO_OFF       = 1,
    ACTIVE_TO_DEEP_SLEEP_1              = 2,
    ACTIVE_TO_DEEP_SLEEP_1_IO_OFF       = 3,
    ACTIVE_TO_HIBERNATION               = 4,
    ACTIVE_TO_HIBERNATION_IO_OFF        = 5,
    ACTIVE_IO_OFF_TO_LIGHT_SLEEP_IO_OFF = 6,
    ACTIVE_TO_SLEEP                     = 7,
    ACTIVE_TO_SLEEP_IO_OFF              = 8,
    ACTIVE_TO_IDLE                      = 9,
    ACTIVE_TO_IDLE_IO_OFF               = 10,
    ACTIVE_TO_LIGHT_SLEEP               = 11,
    ACTIVE_TO_ACTIVE_IO_OFF             = 12,
    ACTIVE_IO_OFF_TO_ACTIVE             = 13,
    WAKEUP_TO_ACTIVE                    = 14,
    WAKEUP_TO_ACTIVE_IO_OFF             = 15,
};

enum pmu_wakeup_src {
    WAKEUP_SRC_GPIO             = BIT(0),
    WAKEUP_SRC_RTC              = BIT(1),
    WAKEUP_SRC_UART0_LOSSLESS   = BIT(2),
    WAKEUP_SRC_UART1_LOSSLESS   = BIT(3),
    WAKEUP_SRC_UART2_LOSSLESS   = BIT(4),
    WAKEUP_SRC_UART0_LOSSY      = BIT(5),
    WAKEUP_SRC_UART1_LOSSY      = BIT(6),
    WAKEUP_SRC_UART2_LOSSY      = BIT(7),
    WAKEUP_SRC_USB              = BIT(8),
    WAKEUP_SRC_SDIO             = BIT(9),
    WAKEUP_SRC_SW               = BIT(10),
};

#define WAKEUP_SRC_ALL       (0x7ff)

enum pmu_aon_volt {
	PMU_AON_0V6			= 1,
	PMU_AON_0V65		= 2,
	PMU_AON_0V7			= 3,
	PMU_AON_0V75		= 4,
	PMU_AON_0V8			= 5,
	PMU_AON_0V85		= 6,
	PMU_AON_0V9			= 7,
	PMU_AON_0V95		= 8,
	PMU_AON_1V0			= 9,
};

enum pmu_dldo_volt {
	PMU_DLDO_0V65		= 1,
	PMU_DLDO_0V7		= 2,
	PMU_DLDO_0V75		= 3,
	PMU_DLDO_0V8		= 4,
	PMU_DLDO_0V85		= 5,
	PMU_DLDO_0V9		= 6,
};

enum pmu_dcdc_volt {
	PMU_DCDC_0V9		= 0x07,
	PMU_DCDC_0V95		= 0x08,
	PMU_DCDC_1V			= 0x09,
	PMU_DCDC_1V05		= 0x0a,
	PMU_DCDC_1V1		= 0x0b,
	PMU_DCDC_1V15		= 0x0c,
	PMU_DCDC_1V2		= 0x0d,
	PMU_DCDC_1V25		= 0x0e,
	PMU_DCDC_1V3		= 0x0f,
	PMU_DCDC_1V35		= 0x10,
	PMU_DCDC_1V4		= 0x11,
	PMU_DCDC_1V45		= 0x12,
};

#define PMU_AON_VOLTAGE_qLR_0V7		0
#define PMU_AON_VOLTAGE_iLR_0V8		1
#define PMU_AON_VOLTAGE_qLR_0V8		2

void pmu_init(void);

void pmu_irq_enable(void);

void pmu_irq_disable(void);

void pmu_set_wakeup_mode(enum pmu_mode mode);
void pmu_set_sleep_mode(enum pmu_mode mode);
void pmu_set_mode(enum pmu_mode mode);

void pmu_ctlr_aon(uint8_t flag);

void pmu_set_dldo_volt(enum pmu_dldo_volt volt);

void pmu_set_dcdc_volt(enum pmu_dcdc_volt volt);

#endif /* _PMU_H_ */
