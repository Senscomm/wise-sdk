ccflags-y += -I$(srctree)/include/FreeRTOS -I$(srctree)/lib -I$(srctree)/lib/net80211 -I$(srctree)/lib/lwip/src/include -I$(srctree)/lib/lwip/ports/freertos/include

obj-y += main.o
obj-$(CONFIG_DEBUG_TYPE_EXCEPTION) += exception.o
obj-$(CONFIG_DEBUG_TYPE_ASSERTION) += assertion.o
obj-$(CONFIG_DEBUG_TYPE_STACK_OVF) += stackoverflow.o
obj-$(CONFIG_DEBUG_TYPE_HEAP_LEAK) += heapleak.o
