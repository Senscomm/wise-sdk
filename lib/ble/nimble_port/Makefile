# porting
ccflags-y += -I$(src)/../nimble_ctrl/mynewt-nimble/porting/nimble/include
ccflags-y += -I$(src)/../nimble_port/mynewt-nimble/porting/nimble/include
ccflags-y += -I$(src)/../nimble_port/mynewt-nimble/nimble/drivers/scm2010/include

# nimble
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/controller/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/transport/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/transport/common/hci_h4/include/
ccflags-y += -I$(src)/../nimble/mynewt-nimble/porting/npl/wise/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/porting/nimble/include
ccflags-y += -I$(srctree)/lib/tinycrypt/lib/include

# FreeRTOS
ccflags-y += -I$(srctree)/include/FreeRTOS

# RF
ccflags-y += -I$(srctree)/hal/drivers/ble
ccflags-y += -I$(srctree)/hal/drivers/ble/priv

# defines
ccflags-y += -DSCM2010
ccflags-y += -DNIMBLE_CFG_CONTROLLER
ifdef CONFIG_BLE_NIMBLE_HOST
ccflags-y += -DNIMBLE_CFG_HOST
endif
ccflags-y += -Dhal_timer_init=ble_timer_init

# static assert
ccflags-y += -Dstatic_assert=_Static_assert
# some of nimble code has this
ccflags-y += -Wno-maybe-uninitialized
# for quick build
ccflags-y += -Wno-unused-function -Wno-unused-const-variable -Wno-unused-variable
# for gcov
ccflags-$(CONFIG_SUPPORT_GCOV_LIB_NIMBLE) += -fprofile-arcs -ftest-coverage

obj-$(CONFIG_BLE_NIMBLE_CTRL) += \
    mynewt-nimble/nimble/controller/src/ble_ll.o \
    mynewt-nimble/nimble/controller/src/ble_ll_adv.o \
    mynewt-nimble/nimble/controller/src/ble_ll_conn.o \
    mynewt-nimble/nimble/controller/src/ble_ll_scan.o \
    mynewt-nimble/nimble/controller/src/ble_ll_scan_aux.o \
    mynewt-nimble/nimble/controller/src/ble_ll_resolv.o \
    mynewt-nimble/nimble/controller/src/ble_ll_utils.o \
    mynewt-nimble/nimble/controller/src/ble_ll_dtm_cli.o \
    mynewt-nimble/nimble/controller/src/ble_ll_cw_cli.o

obj-$(CONFIG_BLE_NIMBLE_CTRL) += \
    mynewt-nimble/nimble/transport/src/transport.o \
    mynewt-nimble/nimble/transport/common/hci_h4/src/hci_h4.o

obj-$(CONFIG_BLE_NIMBLE_CTRL) += \
    mynewt-nimble/nimble/drivers/scm2010/src/ble_hw.o \
    mynewt-nimble/nimble/drivers/scm2010/src/ble_phy.o

obj-$(CONFIG_BLE_NIMBLE_CTRL) += \
    mynewt-nimble/porting/nimble/src/nimble_port.o \
    mynewt-nimble/porting/nimble/src/os_cputime_pwr2.o \
    mynewt-nimble/porting/nimble/src/hal_timer.o

obj-$(CONFIG_BLE_NIMBLE_HCI_TRANSPORT_SCM2010_IPC) += \
    mynewt-nimble/nimble/transport/scm2010/src/ble_hci_scm2010_ipc.o

obj-$(CONFIG_BLE_NIMBLE_HCI_TRANSPORT_UART) += \
    mynewt-nimble/porting/nimble/src/hal_uart.o \
    mynewt-nimble/nimble/transport/scm2010/src/ble_hci_scm2010_uart.o

obj-$(CONFIG_BLE_ADV_DISABLE_RND_DELAY) += \
    mynewt-nimble/nimble/controller/src/ble_ll_sched.o

ifeq ($(CONFIG_LINK_TO_ROM),y)
ifneq ($(CONFIG_BLE_NIMBLE_HOST),y)
obj-$(CONFIG_BLE_NIMBLE_CTRL) += \
	../nimble/mynewt-nimble/porting/nimble/src/os_mbuf.o
endif
endif
