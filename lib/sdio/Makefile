ccflags-y += -D__WISE__
ccflags-y += -D__PRINTF__
ccflags-y += -fmerge-all-constants

ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib
ccflags-y += -I$(srctree)/lib/scdc
ccflags-y += -I$(src)
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/hal/drivers/wlan/senscomm
ccflags-y += -I$(srctree)/lib/ifconfig
ccflags-y += -I$(srctree)/lib/repeater

obj-y += sdioif.o
obj-y += sdio-fifo.o
obj-$(CONFIG_SUPPORT_SDIO_WIFI_FILTER) += sdio-filter.o