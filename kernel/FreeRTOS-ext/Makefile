.SECONDARY:

ccflags-y += -I$(srctree)/include/FreeRTOS

ifdef CONFIG_FREERTOS_ASSERT_VERBOSITY
ccflags-y += -DASSERT_VERBOSITY=$(CONFIG_FREERTOS_ASSERT_VERBOSITY)
endif

obj-$(CONFIG_MEM_HEAP_EXT) += heap_5_ext.o
