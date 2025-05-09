ccflags-y += -I$(srctree)/lib/sdio

obj-$(CONFIG_SDIO) += sdio-sdvt.o

ifeq ($(CONFIG_SDIO_USE_IRQD),y)

ifeq ($(CONFIG_SDIO_OOB_GPIO_INT),y)
IRQD_FW := wise_irqd_oob_bin
else
IRQD_FW := wise_irqd_bin
endif

# To treat it as non-intermediate
$(src)/$(IRQD_FW).c:

obj-y += $(IRQD_FW).o
ccflags-y += -Dirqd_bin=$(IRQD_FW) -Dirqd_bin_size=$(IRQD_FW)_size

endif
