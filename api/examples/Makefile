ccflags-y += -I$(srctree)/include/freebsd -I$(srctree)/lib/net80211 -I$(srctree)/lib/lwip/src/include -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/include/FreeRTOS

obj-$(CONFIG_DEMO_API_WIFI) += wifi/
obj-$(CONFIG_DEMO_PROTOS)    += protocols/
obj-$(CONFIG_DEMO_PERIPHERALS) += peripherals/
obj-$(CONFIG_DEMO_POWERSAVE) += power_save/
obj-$(CONFIG_DEMO_BLE) += ble/
obj-$(CONFIG_TEST_PERIPHERALS) += test/peripherals/
