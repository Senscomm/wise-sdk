#ifndef PM_TEST_H
#define PM_TEST_H

enum pm_test_flag {
	TEST_WITH_CONFIG,
	TEST_WITH_CONFIG_REPEAT,
	TEST_WITH_REPEAT_AND_CHANGE_MODE,
#ifndef CONFIG_XIP
	TEST_IO_ON,
	TEST_IO_OFF,
#endif
	TEST_STOP,
};

enum pm_test_wakeup_src {
	TEST_GPIO_WAKEUP,
	TEST_UART0_LOSSY_WAKEUP,
	TEST_UART0_LOSSLESS_WAKEUP,
	TEST_UART1_LOSSY_WAKEUP,
	TEST_UART1_LOSSLESS_WAKEUP,
	TEST_RTC_WAKEUP,
	TEST_USB_WAKEUP,
	TEST_SDIO_WAKEUP,
};

struct pm_test_ctx {
	uint8_t flag;
	uint8_t disable_timeout;
	uint8_t lowpower_mode;
	uint8_t wakeup_mode;
	uint32_t wakeup_src;
	uint32_t lowpower_dur;
	struct pm_rram_info *rram;
	uint8_t uart_enabled;
	osSemaphoreId_t test_sig;
};

int pm_test_init(void);

struct pm_test_ctx *pm_test_get_config(void);

void pm_test_stop(void);

void pm_test_complete(void);

#endif
