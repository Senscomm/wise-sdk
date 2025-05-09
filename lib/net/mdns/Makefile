ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/
ccflags-y += -I$(srctree)/lib/net80211
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(src)

ccflags-$(CONFIG_LWIP_DEBUG_ENABLE) += -DLWIP_DEBUG

obj-y += mdns.o mdns_domain.o mdns_out.o

