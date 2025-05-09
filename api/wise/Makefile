ccflags-y += -I$(srctree)/lib
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/lib/hostap/wpa_supplicant
ccflags-y += -I$(srctree)/lib/hostap/src/
ccflags-y += -I$(srctree)/lib/hostap/src/utils
ccflags-y += -I$(srctree)/lib/ifconfig
ccflags-y += -I$(srctree)/lib/libifconfig
ccflags-y += -I$(srctree)/lib/net/dhcps
ccflags-y += -I$(srctree)/api/include

obj-$(CONFIG_WISE_API_WIFI) += event_default_handlers.o
obj-$(CONFIG_WISE_API_WIFI) += event_loop.o
obj-$(CONFIG_WISE_API_WIFI) += wise_wifi.o
obj-$(CONFIG_WISE_API_SYSTEM) += wise_log.o
obj-$(CONFIG_WISE_API_SYSTEM) += wise_system.o
obj-$(CONFIG_API_SCMCHANNEL) += wise_channel.o
obj-$(CONFIG_WISE_API_SYSTEM) += wise_task_wdt.o
