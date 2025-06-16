ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/lib
ccflags-y += -D__WISE__
ccflags-y += -Dprintf=printk

ifdef CONFIG_FREEBSD_ASSERT
ccflags-y += -DASSERT_VERBOSITY=$(CONFIG_FREEBSD_ASSERT)
endif

ifeq ($(CONFIG_NET),y)
obj-y += fbsd_ether.o
obj-y += if.o
obj-y += fbsd_if_media.o
obj-y += rtsock.o
obj-y += getifaddrs.o
endif
