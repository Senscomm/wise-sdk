libble=lib/ble/nimble
ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -Dstatic_assert=_Static_assert
ccflags-y += -Wno-maybe-uninitialized
ccflags-y += -Wno-unused-function -Wno-unused-const-variable -Wno-unused-variable

# porting
# porting
ccflags-y += -I$(srctree)/$(libble)/../nimble_host/mynewt-nimble/porting/nimble/include

# nimble
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/services/bas/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/services/tps/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/services/gap/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/services/lls/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/services/ias/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/services/dis/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/services/gatt/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/services/ans/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/util/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/store/ram/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/host/store/config/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/transport/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/nimble/transport/common/hci_h4/include/
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/porting/npl/wise/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-nimble/porting/nimble/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-core/encoding/base64/include
ccflags-y += -I$(srctree)/$(libble)/mynewt-core/sys/config/include

obj-y += main.o
obj-y += misc.o
obj-$(CONFIG_NIMBLE_BLE_SIMPLE)    += blecent/main.o blecent/peer.o
obj-$(CONFIG_NIMBLE_BLE_SIMPLE)    += bleprph/main.o bleprph/gatt_svr.o
obj-$(CONFIG_NIMBLE_BLECLI) 	   += btshell/main.o btshell/cmd.o btshell/cmd_gatt.o btshell/cmd_l2cap.o btshell/gatt_svr.o btshell/parse.o btshell/misc.o
obj-$(CONFIG_NIMBLE_PTS)		   += btshell/gatt_svr_small_db.o btshell/gatt_svr_large_db1.o
obj-$(CONFIG_NIMBLE_BLEHOGP) 	   += blehogp/main.o blehogp/gatt_svr.o \
									  blehogp/blehogp_svc_dis.o blehogp/blehogp_svc_bas.o blehogp/blehogp_svc_hids.o blehogp/blehogp_svc_scps.o
obj-$(CONFIG_NIMBLE_BLECOMP)	   += blecomp/main.o blecomp/gatt_svr.o
