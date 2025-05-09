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

#ifndef __SOC_H__
#define __SOC_H__

#include "platform.h"

/* Interrupt number */
typedef enum IRQn {
    /* XXX: PLIC int_src starts from 1. */
	IRQn_RTC_PERIOD	= 1,
	IRQn_RTC_ALARM	= 2,
	IRQn_TIMER1		= 3,
	IRQn_SPI1		= 4,
	IRQn_SPI2		= 5,
	IRQn_I2C1		= 6,
	IRQn_GPIO		= 7,
	IRQn_UART1		= 8,
	IRQn_UART2		= 9,
	IRQn_DMAC1		= 10,
	IRQn_BMC		= 11,
    IRQn_DMAC2      = 12,
	IRQn_TIMER2		= 13,
	IRQn_SPI3		= 14,
	IRQn_I2C2	    = 15,
	IRQn_UART3	    = 16,
	IRQn_SSP	    = 17,
	IRQn_SDP0		= 18,
	IRQn_AUXADC     = 19,
	IRQn_MAC		= 20,
    /* 21: reserved */
	IRQn_TP		    = 22,
    /* + SMU */
    IRQn_PCS3_STBY  = 23,
    IRQn_PCS4_STBY  = 24,
    IRQn_PCS5_STBY  = 25,
    IRQn_PCS6_STBY  = 26,
    IRQn_PCS3_WKUP  = 27,
    IRQn_PCS4_WKUP  = 28,
    IRQn_PCS5_WKUP  = 29,
    IRQn_SLEEP_REJECT   = 30,
    IRQn_WKUP           = 31,
    /* - SMU */
    IRQn_WLAN0      = 32,
    IRQn_WLAN1      = 33,
    IRQn_BLE0       = 34,
    IRQn_BLE1       = 35,
    IRQn_BLE_TIMER  = 41,
    IRQn_SDIO       = 42,
    IRQn_USB        = 43,
    IRQn_USB_DMA    = 44,
    IRQn_I2S        = 45,
    IRQn_PMU        = 46,
} IRQn_Type;

#include "mmap.h"

#endif
