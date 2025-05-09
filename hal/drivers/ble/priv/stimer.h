/********************************************************************************************************
 * @file	stimer.h
 *
 * @brief	This is the header file for B91
 *
 * @author	Driver Group
 * @date	2019
 *
 * @par     Copyright (c) 2019, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Redistribution and use in source and binary forms, with or without
 *          modification, are permitted provided that the following conditions are met:
 *
 *              1. Redistributions of source code must retain the above copyright
 *              notice, this list of conditions and the following disclaimer.
 *
 *              2. Unless for usage inside a TELINK integrated circuit, redistributions
 *              in binary form must reproduce the above copyright notice, this list of
 *              conditions and the following disclaimer in the documentation and/or other
 *              materials provided with the distribution.
 *
 *              3. Neither the name of TELINK, nor the names of its contributors may be
 *              used to endorse or promote products derived from this software without
 *              specific prior written permission.
 *
 *              4. This software, with or without modification, must only be used with a
 *              TELINK integrated circuit. All other usages are subject to written permission
 *              from TELINK and different commercial license may apply.
 *
 *              5. Licensee shall be solely responsible for any claim to the extent arising out of or
 *              relating to such deletion(s), modification(s) or alteration(s).
 *
 *          THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *          ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *          WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *          DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
 *          DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *          (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *          LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *          ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *          (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *          SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************************************/
/**	@page STIMER
 *
 *	Introduction
 *	===============
 *	TLSRB91 stimer use 16M clock count, have stimer irq.
 *
 *	API Reference
 *	===============
 *	Header File: uart.h
 */
#ifndef STIMER_H_
#define STIMER_H_

#include "compiler.h"
#include "bit.h"
#include "types.h"

/**********************************************************************************************************************
 *                                         global constants                                                           *
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *                                           global macro                                                             *
 *********************************************************************************************************************/

#define BLE_TIME_INTR_STS           (1 << 4)
#define BLE_TIME_CFG0_CLR_INTR      (1 << 5)
#define BLE_TIME_CFG0_CTRL_EN       (1 << 8)
#define BLE_TIME_CFG0_INTR_EN       (1 << 11)

#define BLE_TIME_CFG0               (*(volatile uint32_t *)(0xF01002A8))
#define BLE_TIME_COMPARE            (*(volatile uint32_t *)(0xF01002DC))
#define BLE_TIME_COUNTER_VAL        (*(volatile uint32_t *)(0xF01002E0))

/**********************************************************************************************************************
 *                                         global data type                                                           *
 *********************************************************************************************************************/
/**********************************************************************************************************************
 *                                     global variable declaration                                                    *
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *                                      global function prototype                                                     *
 *********************************************************************************************************************/
/**
 * @brief define system clock tick per us/ms/s.
 */
enum{
#if 0
	SYSTEM_TIMER_TICK_1US 		= 10,
	SYSTEM_TIMER_TICK_1MS 		= 10000,
	SYSTEM_TIMER_TICK_1S 		= 10000000,

	SYSTEM_TIMER_TICK_625US  	= 6250,  //625*10
	SYSTEM_TIMER_TICK_1250US 	= 12500,  //1250*10
#else
	SYSTEM_TIMER_TICK_1US 		= 20,
	SYSTEM_TIMER_TICK_1MS 		= 20000,
	SYSTEM_TIMER_TICK_1S 		= 20000000,

	SYSTEM_TIMER_TICK_625US  	= 12500,  //625*20
	SYSTEM_TIMER_TICK_1250US 	= 25000,  //1250*20
#endif
};


typedef enum{
	FLD_SYSTEM_IRQ  		= 	BIT(0),
	FLD_SYSTEM_32K_IRQ  	= 	BIT(1),
}stimer_irq_e;

/**
 * @brief This function servers to set stimer irq mask.
 * @param[in]	mask - the irq mask.
 * @return  	none.
 */
static inline void stimer_set_irq_mask(stimer_irq_e mask)
{
#if 0
	reg_system_irq_mask |= mask;
#endif
}

/**
 * @brief This function servers to clear stimer irq mask.
 * @param[in] 	mask - the irq mask.
 * @return  	none.
 */
static inline void stimer_clr_irq_mask(stimer_irq_e mask)
{
#if 0
	reg_system_irq_mask &= (~mask);
#endif
}

/**
 * @brief This function servers to clear stimer irq status.
 * @param[in] 	status - the irq status.
 * @return  	none.
 */
static inline void stimer_clr_irq_status(stimer_irq_e status)
{
#if 0
	reg_system_cal_irq = (status);
#endif
}

/**
 * @brief This function servers to get stimer irq status.
 * @param[in] 	status - the irq status.
 * @return      none.
 */
static inline unsigned char stimer_get_irq_status(stimer_irq_e status)
{
#if 0
	return (reg_system_cal_irq & status);
#else
    return 0;
#endif
}

/**
 * @brief This function servers to set tick irq capture.
 * @param[in] tick - the value of irq tick.
 * @return    none.
 */
static inline void stimer_set_irq_capture(unsigned int tick)
{
#if 0
	reg_system_irq_level = (tick);
#endif
}

/**
 * @brief This function servers to set stimer tick.
 * @param[in] tick - the value of tick.
 * @return    none.
 */
static inline void stimer_set_tick(unsigned int tick)
{
#if 0
	reg_system_tick = (tick);
#endif
}

/**
 * @brief This function servers to enable stimer.
 * @return  none.
 */
static inline void stimer_enable(void)
{
#if 0
	reg_system_ctrl |= FLD_SYSTEM_TIMER_EN;
#endif
}


/**
 * @brief This function servers to disable stimer.
 * @return  none.
 */
static inline void stimer_disable(void)
{
#if 0
	reg_system_ctrl &= ~(FLD_SYSTEM_TIMER_EN);
#endif
}

/*
 * @brief     This function performs to get system timer tick.
 * @return    system timer tick value.
**/
static inline unsigned int stimer_get_tick(void)
{
#if 0
	return reg_system_tick;
#else
    return BLE_TIME_COUNTER_VAL;
#endif
}

/**
 * @brief     This function serves to set timeout by us.
 * @param[in] ref  - reference tick of system timer .
 * @param[in] us   - count by us.
 * @return    true - timeout, false - not timeout
 */
static inline _Bool clock_time_exceed(unsigned int ref, unsigned int us)
{
	return ((unsigned int)(stimer_get_tick() - ref) > us * SYSTEM_TIMER_TICK_1US);
}

typedef enum {
	STIMER_IRQ_MASK     		=   BIT(0),
	STIMER_32K_CAL_IRQ_MASK     =   BIT(1),
}stimer_irq_mask_e;

typedef enum {
	FLD_IRQ_SYSTEM_TIMER     		=   BIT(0),
}system_timer_irq_mask_e;


typedef enum {
	STIMER_IRQ_CLR	     		=   BIT(0),
	STIMER_32K_CAL_IRQ_CLR     	=   BIT(1),
}stimer_irq_clr_e;


/**
 * @brief    This function serves to enable system timer interrupt.
 * @return  none
 */
static inline void systimer_irq_enable(void)
{
#if 0
	reg_irq_src0 |= BIT(IRQ1_SYSTIMER);
	//plic_interrupt_enable(IRQ1_SYSTIMER);
#else
    BLE_TIME_CFG0 |= BLE_TIME_CFG0_INTR_EN;
#endif
}

/**
 * @brief    This function serves to disable system timer interrupt.
 * @return  none
 */
extern void ble_timer_stop(void);
static inline void systimer_irq_disable(void)
{
#if 0
	reg_irq_src0 &= ~BIT(IRQ1_SYSTIMER);
	//plic_interrupt_disable(IRQ1_SYSTIMER);
#else
    ble_timer_stop();
#endif
}

static inline void systimer_set_irq_mask(void)
{
#if 0
	reg_system_irq_mask |= STIMER_IRQ_MASK;
#endif
}

static inline void systimer_clr_irq_mask(void)
{
#if 0
	reg_system_irq_mask &= (~STIMER_IRQ_MASK);
#endif
}

static inline unsigned char systimer_get_irq_status(void)
{
#if 0
	return reg_system_cal_irq & FLD_IRQ_SYSTEM_TIMER;
#else
    /* return TIMER_INTR_SRC & (1 << 2); */
    return BLE_TIME_CFG0 & BLE_TIME_INTR_STS;
#endif

}

static inline void systimer_clr_irq_status(void)
{
#if 0
	reg_system_cal_irq = STIMER_IRQ_CLR;
#endif
}

extern void ble_timer_start(unsigned int tick);
static inline void systimer_set_irq_capture(unsigned int tick)
{
#if 0
	reg_system_irq_level = tick;
#else
    ble_timer_start(tick);
#endif
}

static inline unsigned int systimer_get_irq_capture(void)
{
#if 0
	return reg_system_irq_level;
#else
    return BLE_TIME_COMPARE;
#endif
}

static inline int tick1_exceed_tick2(u32 tick1, u32 tick2)
{
	return (u32)(tick1 - tick2) < BIT(30);

}

/**
 * @brief     This function performs to set delay time by us.
 * @param[in] microsec - need to delay.
 * @return    none
 */
_attribute_ram_code_sec_noinline_   void delay_us(unsigned int microsec);


/**
 * @brief     This function performs to set delay time by ms.
 * @param[in] millisec - need to delay.
 * @return    none
 */
_attribute_ram_code_sec_noinline_  void  delay_ms(unsigned int millisec);

#define sleep_us    delay_us
#define sleep_ms    delay_ms


#endif /* STIMER_H_ */
