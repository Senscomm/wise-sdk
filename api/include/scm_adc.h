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

#ifndef __SCM_ADC_H__
#define __SCM_ADC_H__

#include <stdint.h>

#include "wise_err.h"

enum scm_adc_channel {
	SCM_ADC_SINGLE_CH_0		= 0,
	SCM_ADC_SINGLE_CH_1		= 1,
	SCM_ADC_SINGLE_CH_2		= 2,
	SCM_ADC_SINGLE_CH_3		= 3,
	SCM_ADC_SINGLE_CH_4		= 4,
	SCM_ADC_SINGLE_CH_5		= 5,
	SCM_ADC_SINGLE_CH_6		= 6,
	SCM_ADC_SINGLE_CH_7		= 7,
	SCM_ADC_DIFFER_CH_0_1	= 8,
	SCM_ADC_DIFFER_CH_2_3	= 9,
	SCM_ADC_DIFFER_CH_4_5	= 10,
	SCM_ADC_DIFFER_CH_6_7	= 11,
};

typedef void (*scm_adc_cb)(void *ctx);

/**
 * @brief ADC read data
 *
 * @param[in] ch  ADC channel
 * @param[in] buf buffer for ADC value
 * @param[in] len length of the buffer
 */
int scm_adc_read(enum scm_adc_channel ch, uint16_t *buf, uint32_t len);

/**
 * @brief ADC read data
 *
 * @param[in] ch  ADC channel
 * @param[in] buf buffer for ADC value
 * @param[in] len length of the buffer
 */
int scm_adc_read_async(enum scm_adc_channel ch, uint16_t *buf, uint32_t len, scm_adc_cb cb, void *ctx);

/**
 * @brief reset ADC peripheral
 *
 */
int scm_adc_reset(void);

#endif //__SCM_ADC_H__
