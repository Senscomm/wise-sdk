ccflags-$(CONFIG_SUPPORT_GCOV_TUSB) += -fprofile-arcs -ftest-coverage
ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/lib/scdc
ccflags-y += -I$(srctree)/lib/repeater

ccflags-y += -I$(src)
ccflags-y += -I$(src)/common
ccflags-y += -I$(src)/osal
ccflags-y += -I$(src)/device
#ccflags-y += -I$(src)/host
ccflags-y += -I$(src)/class/net
ccflags-y += -I$(src)/class/midi
ccflags-y += -I$(src)/class/hid
ccflags-y += -I$(src)/class/video
ccflags-y += -I$(src)/class/audio
ccflags-y += -I$(src)/class/usbtmc
ccflags-y += -I$(src)/class/dfu
ccflags-y += -I$(src)/class/bth
ccflags-y += -I$(src)/class/msc
ccflags-y += -I$(src)/class/cdc
ccflags-y += -I$(src)/class/vendor

ccflags-y += -I$(src)/portable/openfive/musbhsfc

ccflags-y += -I$(srctree)/include/FreeRTOS

ccflags-y += -DCONFIG_TUSB_DEBUG_PRINT=$(CONFIG_TUSB_DEBUG_PRINTF:"%"=%)

obj-y += tusb_main.o
obj-y += tusb.o
obj-y += usb_descriptors.o
obj-y += common/tusb_fifo.o
obj-y += common/tusb_filter.o
obj-y += device/usbd.o
obj-y += device/usbd_control.o
obj-y += class/audio/audio_device.o
obj-y += class/cdc/cdc_device.o
obj-y += class/dfu/dfu_device.o
obj-y += class/dfu/dfu_rt_device.o
obj-y += class/hid/hid_device.o
obj-y += class/midi/midi_device.o
obj-y += class/msc/msc_device.o
obj-y += class/net/ecm_rndis_device.o
obj-y += class/net/ncm_device.o
obj-y += class/usbtmc/usbtmc_device.o
obj-y += class/video/video_device.o
obj-y += class/vendor/vendor_device.o

obj-$(CONFIG_SOC_SCM2010) += portable/openfive/musbhsfc/dcd_musbhsfc.o
