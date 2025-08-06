# path for ayla components
ccflags-y += -I$(src)/components/ada/include
ccflags-y += -I$(src)/components/ayla/include
ccflags-y += -I$(src)/components/al/include
ccflags-y += -I$(src)/components/al/lwip/include
ccflags-y += -I$(src)/components/al/wise/include
ccflags-y += -I$(src)/components/al/mbedtls/include
ccflags-y += -I$(src)/components/adw
ccflags-y += -I$(src)/components/adw/include
ccflags-y += -I$(src)/components/adb/include
ccflags-y += -I$(src)/components/jsmn
ccflags-y += -I$(src)/components/paho/MQTTPacket/src
ccflags-y += -I$(src)/ayla_demo/main

# path for platform
ccflags-y += -I$(srctree)/api/include
ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/mbedtls-2.28.10/include
ccflags-y += -I$(srctree)/lib/net
ccflags-y += -I$(srctree)/lib/net/http_parser
ccflags-y += -I$(srctree)/lib/net/http_server/include
ccflags-y += -I$(srctree)/lib/scm_mcuboot/mcuboot/boot/bootutil/include
ccflags-y += -I$(srctree)/lib/scm_mcuboot/mcuboot/boot/wise/include
ccflags-y += -I$(srctree)/lib/ble/nimble_host/mynewt-nimble/porting/nimble/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/porting/nimble/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/transport/include/
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/porting/npl/wise/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/services/bas/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/services/tps/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/services/gap/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/services/lls/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/services/ias/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/services/dis/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/services/gatt/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/services/ans/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/util/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/store/ram/include
ccflags-y += -I$(srctree)/lib/ble/nimble/mynewt-nimble/nimble/host/store/config/include

# avoid warning to error temporarily
ccflags-y += -Wno-error
ccflags-y += -Wno-unused-function
ccflags-y += -Wno-address
ccflags-y += -Wno-missing-field-initializers

# config for components
ccflags-y += -DSCM_PORT 		# modification for components/al/wise, and demo
ccflags-y += -DAYLA_SCM_SUPPORT # modification for components/al/* except wise
ifdef CONFIG_AYLA_WIFI_SUPPORT
ccflags-y += -DAYLA_WIFI_SUPPORT
endif
ifdef CONFIG_AYLA_WIFI_SETUP_CLEARTEXT_SUPPORT
ccflags-y += -DAYLA_WIFI_SETUP_CLEARTEXT_SUPPORT
endif
ifdef CONFIG_AYLA_BLUETOOTH_SUPPORT
ccflags-y += -DAYLA_BLUETOOTH_SUPPORT
endif
ifdef CONFIG_AYLA_LOCAL_CONTROL_SUPPORT
ccflags-y += -DAYLA_LOCAL_CONTROL_SUPPORT
ccflags-y += -DAYLA_LAN_SUPPORT
endif
ccflags-y += -DAYLA_LOCAL_SERVER
ccflags-y += -DAYLA_LOG_BUF_ALIGN4
ccflags-y += -DAYLA_METRICS_SUPPORT
ccflags-y += -DAYLA_LOG_COREDUMP
ccflags-y += -DCONF_NO_ID_FILE
ccflags-y += -DAYLA_NET_LWIP
ccflags-y += -DAYLA_STRERROR_R_SUPPORT
ccflags-y += -DCLIENT_MT
ccflags-y += -DLOG_LOCK
ccflags-y += -DLOG_SEV_SHORT
ccflags-y += -DAYLA_LOG_SNAPSHOTS
ccflags-y += -DLWIP_1_5_0_SUPPORT
ccflags-y += -DSNTP_STARTUP_DELAY=0
ccflags-y += -DSTATIC_WEB_CONTENT_IN_MEMORY
ccflags-y += -D_HAS_ASSERT_F_
ccflags-y += -DXXD_BIN_TO_C
ccflags-y += -DMBEDTLS_CONFIG_FILE='$(CONFIG_MBEDTLS_CONFIG_FILE)'

# config for example demo
ccflags-y += -DAYLA_BATCH_SUPPORT
ccflags-y += -DAYLA_TEST_SERVICE_SUPPORT
ccflags-y += -DAYLA_FILE_PROP_SUPPORT
ccflags-y += -DAYLA_BATCH_PROP_SUPPORT
#ccflags-y += -DAYLA_MSG_PROP_SUPPORT
#ccflags-y += -DDEMO_MSG_PROP
ccflags-y += -DAYLA_LOG_THREAD_CT=12

# ayla component json
obj-$(CONFIG_AYLA) += \
	components/jsmn/jsmn.o \

# ayla component mqtt
obj-$(CONFIG_AYLA) += \
	components/paho/MQTTPacket/src/MQTTConnectClient.o \
	components/paho/MQTTPacket/src/MQTTConnectServer.o \
	components/paho/MQTTPacket/src/MQTTDeserializePublish.o \
	components/paho/MQTTPacket/src/MQTTFormat.o \
	components/paho/MQTTPacket/src/MQTTPacket.o \
	components/paho/MQTTPacket/src/MQTTSerializePublish.o \
	components/paho/MQTTPacket/src/MQTTSubscribeClient.o \
	components/paho/MQTTPacket/src/MQTTSubscribeServer.o \
	components/paho/MQTTPacket/src/MQTTUnsubscribeClient.o \
	components/paho/MQTTPacket/src/MQTTUnsubscribeServer.o \

# ayla component ayla device agent
obj-$(CONFIG_AYLA) += \
	components/ada/ada_conf.o \
	components/ada/ada_err.o \
	components/ada/ada_mbuf.o \
	components/ada/ada_timer.o \
	components/ada/ame.o \
	components/ada/ame_json.o \
	components/ada/batch.o \
	components/ada/client.o \
	components/ada/client_ame.o \
	components/ada/client_auth.o \
	components/ada/client_batch.o \
	components/ada/client_cli.o \
	components/ada/client_conf.o \
	components/ada/client_ifp.o \
	components/ada/client_metrics.o \
	components/ada/client_ota.o \
	components/ada/client_prop.o \
	components/ada/client_task.o \
	components/ada/clientv2_msg.o \
	components/ada/conf_json.o \
	components/ada/http2mqtt_client.o \
	components/ada/http_client.o \
	components/ada/lan.o \
	components/ada/local_control.o \
	components/ada/log_client.o \
	components/ada/log_page.o \
	components/ada/metrics.o \
	components/ada/metrics_page.o \
	components/ada/mqtt_client.o \
	components/ada/mqtt_packet.o \
	components/ada/mqttv1_client.o \
	components/ada/oem.o \
	components/ada/onboard_msg.o \
	components/ada/prop_mgr.o \
	components/ada/prop_page.o \
	components/ada/sched.o \
	components/ada/schedeval.o \
	components/ada/server.o \
	components/ada/server_alt.o \
	components/ada/server_req.o \
	components/ada/server_test.o \
	components/ada/sprop.o \
	components/ada/sys_conf.o \
	components/ada/test_svc.o \
	components/ada/certs/ca_certs_der_txt.o \

# ayla component ayla device wifi
obj-$(CONFIG_AYLA_WIFI_SUPPORT) += \
	components/adw/wifi.o \
	components/adw/wifi_conf.o \
	components/adw/wifi_page.o \

# ayla component ayla device bluetooth
obj-$(CONFIG_AYLA_BLUETOOTH_SUPPORT) += \
	components/adb/adb.o \
	components/adb/adb_ayla_svc.o \
	components/adb/adb_conn_svc.o \
	components/adb/adb_mbox_svc.o \
	components/adb/adb_wifi_cfg_svc.o \

# ayla component ayla
obj-$(CONFIG_AYLA) += \
	components/ayla/ayla_base64.o \
	components/ayla/callback.o \
	components/ayla/clock_fmt.o \
	components/ayla/clock_sw.o \
	components/ayla/clock_utils.o \
	components/ayla/conf.o \
	components/ayla/crc.o \
	components/ayla/crc16.o \
	components/ayla/crc8.o \
	components/ayla/endian.o \
	components/ayla/format_mac.o \
	components/ayla/format_string.o \
	components/ayla/hostname_valid.o \
	components/ayla/http.o \
	components/ayla/ipaddr_fmt.o \
	components/ayla/jsmn_get.o \
	components/ayla/jsmn_iterate.o \
	components/ayla/json_format_string.o \
	components/ayla/log.o \
	components/ayla/log_buf.o \
	components/ayla/log_cmd.o \
	components/ayla/log_conf.o \
	components/ayla/log_io.o \
	components/ayla/lookup_by_name.o \
	components/ayla/lookup_by_val.o \
	components/ayla/parse_argv.o \
	components/ayla/parse_date.o \
	components/ayla/parse_hex.o \
	components/ayla/parse_mac.o \
	components/ayla/parse_url.o \
	components/ayla/snprintf.o \
	components/ayla/time_cli.o \
	components/ayla/timer.o \
	components/ayla/tlv.o \
	components/ayla/uri_arg_allowed_map.o \
	components/ayla/uri_decode.o \
	components/ayla/uri_encode.o \
	components/ayla/utf8.o \
	components/ayla/vsnprintf.o \

# ayla component adaptation layer freertos
obj-$(CONFIG_AYLA) += \
	components/al/cmsis_os/al_err.o \
	components/al/cmsis_os/al_os_lock.o \
	components/al/cmsis_os/al_os_thread.o \

# ayla component adaptation layer mbedtls
obj-$(CONFIG_AYLA) += \
	components/al/mbedtls/al_aes.o \
	components/al/mbedtls/al_hash_sha1.o \
	components/al/mbedtls/al_hash_sha256.o \
	components/al/mbedtls/al_hmac_sha256.o \
	components/al/mbedtls/al_net_stream.o \
	components/al/mbedtls/al_random.o \
	components/al/mbedtls/al_rsa.o \

# ayla component adaptation layer lwip
obj-$(CONFIG_AYLA) += \
	components/al/lwip/al_ada_thread.o \
	components/al/lwip/al_net_addr.o \
	components/al/lwip/pfm_net_socket.o \

# ayla component adaptation layer wise
obj-$(CONFIG_AYLA) += \
	components/al/wise/al_assert.o \
	components/al/wise/al_clock.o \
	components/al/wise/al_log.o \
	components/al/wise/al_log_snap.o \
	components/al/wise/al_os_reboot.o \
	components/al/wise/al_os_mem.o \
	components/al/wise/al_persist.o \
	components/al/wise/al_bt.o \
	components/al/wise/al_bt_cli.o \
	components/al/wise/al_wifi.o \
	components/al/wise/al_net_if.o \
	components/al/wise/al_net_dns.o \
	components/al/wise/al_net_mdnss.o \
	components/al/wise/al_net_sntp.o \
	components/al/wise/al_httpd.o \
	components/al/wise/pfm_random.o \
	components/al/wise/pfm_ada_thread.o \
	components/al/wise/pfm_net_socket_init.o \

# ayla demo
obj-$(CONFIG_AYLA) += \
	ayla_demo/main/conf.o \
	ayla_demo/main/command.o \
	ayla_demo/main/sched_conf.o \
	ayla_demo/main/demo_bt.o \
	ayla_demo/main/demo_wifi.o \
	ayla_demo/main/demo_ledevb.o \
	ayla_demo/main/demo_common.o \
	ayla_demo/main/ota.o \
	ayla_demo/main/bp5758d.o \

# main for alya test
obj-$(CONFIG_AYLA) += \
	main.o
