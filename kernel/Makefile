libs-$(CONFIG_FREERTOS) += kernel/FreeRTOS/
libs-$(CONFIG_FREERTOS) += kernel/FreeRTOS-ext/
libs-$(CONFIG_CMSIS_FREERTOS) += kernel/cmsis-freertos/
ifneq ($(CONFIG_LINK_TO_ROM),y)
libs-$(CONFIG_FREEBSD) += kernel/freebsd/
else
libs-$(CONFIG_FREEBSD) += kernel/freebsd/
endif

libs-$(CONFIG_NET) += lib/net/freebsd/mbuf/


libs-y += hal/arch/$(arch)/
libs-y += hal/soc/$(soc)/
libs-y += hal/board/$(board)/
# for fine-control of relocation
libs-$(CONFIG_SPI_FLASH) += hal/drivers/spi-flash/
libs-$(CONFIG_NET) += lib/net/freebsd/
libs-$(CONFIG_WLAN) += hal/drivers/wlan/
libs-$(CONFIG_RF) += hal/drivers/rf/
libs-$(CONFIG_WDT) += hal/drivers/watchdog/
