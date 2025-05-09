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
obj-y += mbuf.o
ifneq ($(CONFIG_LINK_TO_ROM),y)
obj-y += fbsd_mbuf.o
obj-y += fbsd_mbuf2.o
endif
endif
