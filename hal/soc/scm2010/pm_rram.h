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

#ifndef _PM_RRAM_H_
#define _PM_RRAM_H_

#ifdef CONFIG_PM_MULTI_CORE
#define SCM2010_PM_RRAM_SIG_DATA            __pmsigram_start
#endif

/* share memory address */
#define SCM2010_PM_RRAM_DATA_ADDR           __pmrram_start

/* firmware address */
#define SCM2010_PM_RRAM_FW_ADDR             (u32)__watcher_start

/* pm signalling data */
#define SCM2010_PM_RRAM_INFO_ADDR           (SCM2010_PM_RRAM_DATA_ADDR)

/* wakeup flag */
#define SCM2010_PM_RRAM_WAKEUP_FLAG         (0)
#define SCM2010_PM_RRAM_WAKEUP_WATCHER      (1 << 0)
#define SCM2010_PM_RRAM_WAKEUP_RESET        (1 << 1)
#define SCM2010_PM_RRAM_EXT_HIB_TIME		(1 << 2)

/* sync_flag */
#define SCM2010_PM_RRAM_SYNC_FLAG           (1)
#define SCM2010_PM_RRAM_SAVE_FLASH          (1 << 0)
#define SCM2010_PM_RRAM_SAVE_CANCEL         (1 << 1)

/* watcher flag */
#define SCM2010_PM_RRAM_WATCHER_FLAG        (2)
#define SCM2010_PM_RRAM_WATCHER_EXEC        (1 << 0)
#define SCM2010_PM_RRAM_WATCHER_SLEPT       (1 << 1)

/* pm state */
#define SCM2010_PM_RRAM_MODE                (3)
#define SCM2010_PM_RRAM_MODE_ACTIVE         (0)
#define SCM2010_PM_RRAM_MODE_ILDE           (1)
#define SCM2010_PM_RRAM_MODE_LIGHT_SLEEP    (2)
#define SCM2010_PM_RRAM_MODE_SLEEP          (3)
#define SCM2010_PM_RRAM_MODE_DEEP_SLEEP_0   (4)
#define SCM2010_PM_RRAM_MODE_DEEP_SLEEP_1   (5)
#define SCM2010_PM_RRAM_MODE_HIBERNATION    (6)

/* flash parameter */
#define SCM2010_PM_RRAM_FLASH_RD_CMD        (5)
#define SCM2010_PM_RRAM_FLASH_ER1_CMD       (6)
#define SCM2010_PM_RRAM_FLASH_ER2_CMD       (7)
#define SCM2010_PM_RRAM_FLASH_ER1_SIZE      (8)
#define SCM2010_PM_RRAM_FLASH_ER2_SIZE      (12)
#define SCM2010_PM_RRAM_FLASH_TIMING        (16)
#define SCM2010_PM_RRAM_FLASH_CLK           (20)

#define SCM2010_PM_RRAM_CRYPTO_CFG          (48)

#define SCM2010_PM_RRAM_CRYPTO_KEY          (52)
#define SCM2010_PM_RRAM_CRYPTO_KEY0         (52)
#define SCM2010_PM_RRAM_CRYPTO_KEY1         (56)
#define SCM2010_PM_RRAM_CRYPTO_KEY2         (60)
#define SCM2010_PM_RRAM_CRYPTO_KEY3         (64)

#define SCM2010_PM_RRAM_CRYPTO_IV           (68)
#define SCM2010_PM_RRAM_CRYPTO_IV0         	(68)
#define SCM2010_PM_RRAM_CRYPTO_IV1         	(72)
#define SCM2010_PM_RRAM_CRYPTO_IV2         	(76)
#define SCM2010_PM_RRAM_CRYPTO_IV3         	(80)

#ifndef __ASSEMBLY__

enum pm_feature {
	PM_FEATURE_VOLTAGE_CTRL 	= (1 << 0),
	PM_FEATURE_AON_VOLTAGE_CTRL = (1 << 1),
	PM_FEATURE_PLL_CTRL			= (1 << 2),
	PM_FEATURE_WAKEUP_IO_OFF	= (1 << 3),
	PM_FEATURE_FLASH_DPD		= (1 << 4),
	PM_FEATURE_WC_LOG			= (1 << 5),
	PM_FEATURE_RTC_FIX_RATIO	= (1 << 6),
	PM_FEATURE_WC_UART1			= (1 << 7),
	PM_FEATURE_DLDO_DCDC_CTRL	= (1 << 8),
	PM_FEATURE_AON_VOLTAGE_CTRL_OCR = (1 << 9),
	PM_FEATURE_WAKEUP_LOG_ON	= (1 << 10),
};

#define KEY_NUM 4

struct pm_rram_info
{
	uint8_t  wakeup_flag;                  /* 0 */
	uint8_t  sync_flag;                    /* 1 */
	uint8_t  exec_watcher;                 /* 2 */
	uint8_t  pm_mode;                      /* 3 */

	uint8_t  rsv1;                         /* 4 */
	/* SCM2010_PM_RRAM_FLASH_RD_CMD         (5) */
	uint8_t  flash_mem_rd_cmd;
	/* SCM2010_PM_RRAM_FLASH_ER1_CMD        (6) */
	uint8_t  flash_er1_cmd;
	/* SCM2010_PM_RRAM_FLASH_ER2_CMD        (7) */
	uint8_t  flash_er2_cmd;

	/* SCM2010_PM_RRAM_FLASH_ER1_SIZE       (8) */
	uint32_t flash_er1_size;
	/* SCM2010_PM_RRAM_FLASH_ER2_SIZE      (12) */
	uint32_t flash_er2_size;
	/* SCM2010_PM_RRAM_FLASH_TIMING        (16) */
	uint32_t flash_timing;
	/* SCM2010_PM_RRAM_FLASH_CLK           (20) */
	uint32_t flash_clk;

	uint32_t rsv2;                        /* 24 */
	uint32_t rtc_value;                   /* 28 */
	uint64_t mtime_value;                 /* 32 */
	uint64_t next_mtime_value;            /* 40 */

	/* SCM2010_PM_RRAM_CRYPTO_CFG          (48) */
	uint32_t crypto_cfg;
	/* SCM2010_PM_RRAM_CRYPTO_KEY          (52) */
	/* SCM2010_PM_RRAM_CRYPTO_KEY0         (52) */
	/* SCM2010_PM_RRAM_CRYPTO_KEY1         (56) */
	/* SCM2010_PM_RRAM_CRYPTO_KEY2         (60) */
	/* SCM2010_PM_RRAM_CRYPTO_KEY3         (64) */
	uint32_t crypto_key[KEY_NUM];
	/* SCM2010_PM_RRAM_CRYPTO_IV           (68) */
	/* SCM2010_PM_RRAM_CRYPTO_IV0          (68) */
	/* SCM2010_PM_RRAM_CRYPTO_IV0          (72) */
	/* SCM2010_PM_RRAM_CRYPTO_IV0          (76) */
	/* SCM2010_PM_RRAM_CRYPTO_IV0          (80) */
	uint32_t crypto_iv[KEY_NUM];

	uint32_t feature;                     /* 84 */

	uint32_t wk_entry;                    /* 88 */
	uint32_t wk_src;                      /* 92 */
	uint32_t wk_evt;                      /* 96 */
	uint16_t wk_fb_reason;               /* 100 */
	uint16_t wk_fb_type;                 /* 102 */
	uint64_t wk_time;                    /* 104 */
	uint32_t act_ratio;                  /* 112 */
	uint32_t ratio;                      /* 116 */
	uint64_t ratio_last_measure_time;    /* 120 */
	                                 /* end 128 */
}__attribute__((packed));

#define IS_FIXED_RATIO(r) (r->feature & PM_FEATURE_RTC_FIX_RATIO)

#endif /* __ASSEMBLY__ */

#endif /* _PM_RRAM_H_ */
