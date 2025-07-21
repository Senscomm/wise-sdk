ccflags-y += -I$(srctree)/include/freebsd -I$(srctree)/lib/net80211 -I$(srctree)/lib/lwip/src/include -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-$(CONFIG_WATCHER) += -I$(srctree)/hal/drivers/wlan/senscomm
ccflags-$(CONFIG_IRQ_DISPATCHER) += -I$(srctree)/hal/drivers/sdio

obj-$(CONFIG_DEMO_CMDLINE) += cli/main.o
obj-$(CONFIG_MCUBOOT_BL)   += mcuboot/main.o
obj-$(CONFIG_DEMO_HOW_TO_DEBUG) += how-to-debug/

obj-$(CONFIG_AYLA)         += ayla/

