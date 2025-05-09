ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/lib/net80211
ccflags-y += -I$(srctree)/lib/libifconfig

ifndef CONFIG_IFCONFIG_WIRELESS
ccflags-y += -Wno-unused-function
endif

obj-$(CONFIG_IFCONFIG) += ifconfig.o
