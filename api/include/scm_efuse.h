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

#ifndef __SCM_EFUSE_H__
#define __SCM_EFUSE_H__

#include "wise_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCM_EFUSE_ADDR_ROOT_KEY         0
#define SCM_EFUSE_SIZE_ROOT_KEY         128

#define SCM_EFUSE_ADDR_PARITY           128
#define SCM_EFUSE_SIZE_PARITY           1

#define SCM_EFUSE_ADDR_HARD_KEY         129
#define SCM_EFUSE_SIZE_HARD_KEY         1

#define SCM_EFUSE_ADDR_FLASH_PROT       130
#define SCM_EFUSE_SIZE_FLASh_PROT       1

#define SCM_EFUSE_ADDR_SECURE_BOOT      131
#define SCM_EFUSE_SIZE_SECURE_BOOT      1

#define SCM_EFUSE_ADDR_AR_BL_EN         132
#define SCM_EFUSE_SIZE_AR_BL_EN         1

#define SCM_EFUSE_ADDR_SDIO_OCR_EN      132
#define SCM_EFUSE_SIZE_SDIO_OCR_EN      1

#define SCM_EFUSE_ADDR_AR_FW_EN         133
#define SCM_EFUSE_SIZE_AR_FW_EN         1

#define SCM_EFUSE_ADDR_CUST_ID          160
#define SCM_EFUSE_SIZE_CUST_ID          8

#define SCM_EFUSE_ADDR_CHIP_ID          192
#define SCM_EFUSE_SIZE_CHIP_ID          32

#define SCM_EFUSE_ADDR_PK_HASH          224
#define SCM_EFUSE_SIZE_PK_HASH          256

#define SCM_EFUSE_ADDR_SDIO_OCR         544
#define SCM_EFUSE_SIZE_SDIO_OCR         20

#define SCM_EFUSE_ADDR_WLAN_MAC_ADDR    576
#define SCM_EFUSE_SIZE_WLAN_MAC_ADDR    48

#define SCM_EFUSE_ADDR_BLE_MAC_ADDR     624
#define SCM_EFUSE_SIZE_BLE_MAC_ADDR     48

#define SCM_EFUSE_ADDR_RF_CAL           672
#define SCM_EFUSE_SIZE_RF_CAL           64

#define SCM_EFUSE_ADDR_AL_BL_VER        736
#define SCM_EFUSE_SIZE_AL_BL_VER        64


#define SCM_EFUSE_ADDR_AUXADC_LINEAR    800
#define SCM_EFUSE_SIZE_AUXADC_LINEAR    32

#define SCM_EFUSE_ADDR_RESERVED         832
#define SCM_EFUSE_SIZE_RESERVED         192

enum scm_efuse_mode {
	SCM_EFUSE_MODE_RAW			= 0,
	SCM_EFUSE_MODE_RAM_BUFFER	= 1,
	SCM_EFUSE_MODE_FLASH_BUFFER = 2,
};

/**
 * @brief
 *
 */
int scm_efuse_read(uint16_t bit_offset, uint16_t bit_count, uint8_t *val);

/**
 * @brief
 *
 */
int scm_efuse_write(uint16_t bit_offset, uint16_t bit_count, const uint8_t *val);

/**
 * @brief
 *
 */
int scm_efuse_clr_buffer(uint16_t bit_offset, uint16_t bit_count);

/**
 * @brief
 *
 */
int scm_efuse_set_mode(enum scm_efuse_mode mode);

/**
 * @brief
 *
 */
int scm_efuse_get_mode(enum scm_efuse_mode *mode);

/**
 * @brief
 *
 */
int scm_efuse_sync(void);

/**
 * @brief
 *
 */
int scm_efuse_load(void);

#ifdef __cplusplus
}
#endif

#endif //__SCM_EFUSE_H__
