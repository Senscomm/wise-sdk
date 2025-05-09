ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(src)

ccflags-y += -fno-inline
ccflags-$(CONFIG_SUPPORT_GCOV_WLAN) += -fprofile-arcs -ftest-coverage
ccflags-$(CONFIG_IEEE80211_DEBUG) += -DIEEE80211_DEBUG

obj-$(CONFIG_SENSCOMM_WLAN_2020) += scm2020-dev.o
obj-$(CONFIG_SCM2020_WLAN_PM) += scm2020-pm.o

ifeq ($(CONFIG_WLAN_HW_SCM2020),y)
MCU_FW := scm2020_fw_bin
else ifeq ($(CONFIG_WLAN_HW_SCM2010),y)
MCU_FW := scm2010_fw_bin
endif

# To treat it as non-intermediate
$(src)/$(MCU_FW).c:

obj-$(CONFIG_SENSCOMM_WLAN_2020) += $(MCU_FW).o
ccflags-y += -Dmcu_bin=$(MCU_FW) -Dmcu_bin_size=$(MCU_FW)_size
