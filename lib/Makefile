libs-$(CONFIG_CMDLINE) += lib/cli/
ifneq ($(CONFIG_LINK_TO_ROM),y)
libs-y += lib/stdlib/
endif
libs-y += lib/stdlib-ext/
libs-y += lib/proc/
libs-y += lib/serial/
libs-$(CONFIG_NET) += lib/net/
libs-y += lib/fs/
libs-$(CONFIG_AT) += lib/at/

libs-y += lib/lwip/
libs-$(CONFIG_NET80211) += lib/net80211/
ifneq ($(CONFIG_LINK_TO_ROM),y)
libs-$(CONFIG_LIB_CRYPTO) += lib/crypto/
endif
libs-y += lib/ifconfig/
libs-y += lib/libifconfig/
libs-y += lib/cjson/
libs-$(CONFIG_WPA_SUPPLICANT) += lib/hostap/wpa_supplicant/
libs-$(CONFIG_MBEDTLS) += lib/mbedtls-2.28.10/library/
libs-$(CONFIG_TLS_SUPPORT) += lib/tls-transport/

ifneq ($(CONFIG_LINK_TO_ROM),y)
libs-$(CONFIG_BLE_NIMBLE_CTRL) += lib/ble/nimble_ctrl/
endif
libs-$(CONFIG_BLE_NIMBLE_CTRL) += lib/ble/nimble_port/
libs-$(CONFIG_BLE_NIMBLE_HOST) += lib/ble/nimble_host/
libs-$(CONFIG_BLE_TELINK) += lib/ble/telink/
libs-$(CONFIG_TINYCRYPT) += lib/tinycrypt/
libs-$(CONFIG_TINYUSB) += lib/tinyusb/
libs-$(CONFIG_SUPPORT_SCDC) += lib/scdc/

libs-$(CONFIG_NUTTX) += lib/nuttx/
libs-$(CONFIG_SCM_MCUBOOT) += lib/scm_mcuboot/
libs-$(CONFIG_SDIO) += lib/sdio/
libs-$(CONFIG_SUPPORT_WIFI_REPEATER) += lib/repeater/
