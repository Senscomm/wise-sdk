# porting
ccflags-y += -I$(src)/../nimble_host/mynewt-nimble/porting/nimble/include

# nimble
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/services/bas/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/services/tps/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/services/gap/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/services/lls/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/services/ias/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/services/dis/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/services/gatt/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/services/ans/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/util/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/store/ram/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/host/store/config/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/transport/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/nimble/transport/common/hci_h4/include/
ccflags-y += -I$(src)/../nimble/mynewt-nimble/porting/npl/wise/include
ccflags-y += -I$(src)/../nimble/mynewt-nimble/porting/nimble/include
ccflags-y += -I$(src)/../nimble/mynewt-core/encoding/base64/include
ccflags-y += -I$(src)/../nimble/mynewt-core/sys/config/include
ccflags-y += -I$(srctree)/lib/tinycrypt/lib/include

# FreeRTOS
ccflags-y += -I$(srctree)/include/FreeRTOS

# defines
ccflags-y += -DSCM2010=1
ccflags-y += -DNIMBLE_CFG_HOST

# static assert
ccflags-y += -Dstatic_assert=_Static_assert
# some of nimble code has this
ccflags-y += -Wno-maybe-uninitialized
# for quick build
ccflags-y += -Wno-unused-function -Wno-unused-const-variable -Wno-unused-variable
# for gcov
ccflags-$(CONFIG_SUPPORT_GCOV_LIB_NIMBLE) += -fprofile-arcs -ftest-coverage

obj-$(CONFIG_BLE_NIMBLE_HOST) += \
	../nimble/mynewt-nimble/nimble/host/services/bas/src/ble_svc_bas.o \
	../nimble/mynewt-nimble/nimble/host/services/tps/src/ble_svc_tps.o \
	../nimble/mynewt-nimble/nimble/host/services/gap/src/ble_svc_gap.o \
	../nimble/mynewt-nimble/nimble/host/services/lls/src/ble_svc_lls.o \
	../nimble/mynewt-nimble/nimble/host/services/ias/src/ble_svc_ias.o \
	../nimble/mynewt-nimble/nimble/host/services/dis/src/ble_svc_dis.o \
	../nimble/mynewt-nimble/nimble/host/services/gatt/src/ble_svc_gatt.o \
	../nimble/mynewt-nimble/nimble/host/services/ans/src/ble_svc_ans.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_store_util.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_sm_alg.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_misc.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_startup.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_gatts.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_mbuf.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_sm.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_store.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_l2cap_sig_cmd.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_log.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_ibeacon.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_gattc.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_att.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_id.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_adv.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_sm_cmd.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_att_svr.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_l2cap_coc.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_l2cap_sig.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_l2cap.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_stop.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_mqueue.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_gap.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_sm_lgcy.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_att_clt.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_atomic.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_cfg.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_hci_util.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_flow.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_att_cmd.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_sm_sc.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_hci.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_pvcy.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_hci_cmd.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_uuid.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_eddystone.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_periodic_sync.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_conn.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_shutdown.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_hs_hci_evt.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_gatts_lcl.o \
	../nimble/mynewt-nimble/nimble/host/src/ble_dtm.o \
	../nimble/mynewt-nimble/nimble/host/store/ram/src/ble_store_ram.o \
	../nimble/mynewt-nimble/nimble/host/store/config/src/ble_store_config.o \
	../nimble/mynewt-nimble/nimble/host/store/config/src/ble_store_config_conf.o \
	../nimble/mynewt-nimble/nimble/host/util/src/addr.o \

obj-$(CONFIG_BLE_NIMBLE_HOST) += \
	../nimble/mynewt-core/encoding/base64/src/base64.o \
	../nimble/mynewt-core/encoding/base64/src/hex.o \

ifeq ($(CONFIG_LINK_TO_ROM),y)
# When using the ROM, rebuild these to add the missing functions
obj-$(CONFIG_BLE_NIMBLE_HOST) += \
	../nimble/mynewt-nimble/porting/nimble/src/mem.o \
	../nimble/mynewt-nimble/porting/nimble/src/endian.o \
	../nimble/mynewt-nimble/porting/nimble/src/os_mbuf.o \
	../nimble/mynewt-nimble/porting/nimble/src/os_mempool.o \
	../nimble/mynewt-nimble/porting/nimble/src/os_msys_init.o \
	../nimble/mynewt-nimble/porting/nimble/src/os_cputime.o \
	../nimble/mynewt-nimble/porting/npl/wise/src/npl_os_freertos.o
endif
