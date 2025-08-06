#
# Copyright 2022 Ayla Networks, Inc.  All rights reserved.
#
set(SOURCES_LIST
		esp-idf/al_assert.c
		esp-idf/al_bt_cli_esp32.c
		esp-idf/al_bt_esp32.c
		esp-idf/al_clock.c
		esp-idf/al_httpd.c
		esp-idf/al_log.c
		esp-idf/al_log_core.c
		esp-idf/al_log_core_sum.c
		esp-idf/al_log_snap.c
		esp-idf/al_net_dns.c
		esp-idf/al_net_if.c
		esp-idf/al_net_mdnss.c
		esp-idf/al_net_sntp.c
		esp-idf/al_os_mem.c
		esp-idf/al_os_reboot.c
		esp-idf/al_persist.c
		esp-idf/al_wifi.c
		esp-idf/pfm_ada_thread.c
		esp-idf/pfm_net_socket_init.c
		esp-idf/pfm_persist.c
		esp-idf/pfm_persist_v1.c
		esp-idf/pfm_random.c

		freertos/al_err.c
		freertos/al_os_lock.c
		freertos/al_os_thread.c

		lwip/al_ada_thread.c
		lwip/al_net_addr.c
		lwip/pfm_net_socket.c

		mbedtls/al_aes.c
		mbedtls/al_hash_sha1.c
		mbedtls/al_hash_sha256.c
		mbedtls/al_hmac_sha256.c
		mbedtls/al_net_stream.c
		mbedtls/al_random.c
		mbedtls/al_rsa.c
	)

set(CSTYLE_SOURCES
		${SOURCES_LIST}
		esp-idf/include/al/al_compiler.h
		esp-idf/include/platform/pfm_ada_thread.h
		esp-idf/include/platform/pfm_bt.h
		esp-idf/include/platform/pfm_clock.h
		esp-idf/include/platform/pfm_const.h
		esp-idf/include/platform/pfm_endian.h
		esp-idf/include/platform/pfm_net_addr.h
		esp-idf/include/platform/pfm_net_if.h
		esp-idf/include/platform/pfm_os_mem.h
		esp-idf/include/platform/pfm_os_thread.h
		esp-idf/pfm_log.h
		esp-idf/pfm_log_core.h
		esp-idf/pfm_log_core_int.h
		esp-idf/pfm_persist.h
		esp-idf/pfm_persist_map.h
		esp-idf/pfm_persist_map_v1.h

		include/al/al_ada_thread.h
		include/al/al_aes.h
		include/al/al_assert.h
		include/al/al_bt.h
		include/al/al_clock.h
		include/al/al_err.h
		include/al/al_hash_sha1.h
		include/al/al_hash_sha256.h
		include/al/al_hmac_sha256.h
		include/al/al_httpd.h
		include/al/al_log.h
		include/al/al_net_addr.h
		include/al/al_net_dns.h
		include/al/al_net_if.h
		include/al/al_net_mdnss.h
		include/al/al_net_sntp.h
		include/al/al_net_stream.h
		include/al/al_os_lock.h
		include/al/al_os_mem.h
		include/al/al_os_reboot.h
		include/al/al_os_thread.h
		include/al/al_persist.h
		include/al/al_random.h
		include/al/al_rsa.h
		# include/al/al_utypes.h
		include/al/al_wifi.h

		lwip/include/platform/pfm_net_socket.h
		mbedtls/pfm_certs.h
		mbedtls/pfm_linker_text.h
	)

idf_component_register(SRCS
		${SOURCES_LIST}
	EMBED_FILES
		mbedtls/ca_certs_der.txt
	INCLUDE_DIRS
		"include"
		"esp-idf/include"
	PRIV_INCLUDE_DIRS
		"."
		lwip/include
		mbedtls/include
		$ENV{IDF_PATH}/components/espcoredump/include_core_dump
		$ENV{IDF_PATH}/components/freertos/include/freertos
	PRIV_REQUIRES
		adb
		adw
		ayla
		bt
		esp_http_server
		esp_wifi
		espcoredump
		freertos
		lwip
		mbedtls
		mdns
		wpa_supplicant
		${FIREDOME_COMPONENT}
	)

#
# Compile options for all components.
# This is where ADA options required for the ESP-IDF should be.
# Other CMakeList.txt files do not need to specify these.
#
idf_build_set_property(COMPILE_OPTIONS -DAYLA_ESP32_SUPPORT APPEND)
idf_build_set_property(COMPILE_OPTIONS -DAYLA_FreeRTOS APPEND)
idf_build_set_property(COMPILE_OPTIONS -DAYLA_LOCAL_SERVER APPEND)
idf_build_set_property(COMPILE_OPTIONS -DAYLA_LOG_BUF_ALIGN4 APPEND)
idf_build_set_property(COMPILE_OPTIONS -DAYLA_LOG_SNAPSHOTS APPEND)
idf_build_set_property(COMPILE_OPTIONS -DAYLA_METRICS_SUPPORT APPEND)
idf_build_set_property(COMPILE_OPTIONS -DAYLA_NET_LWIP APPEND)
idf_build_set_property(COMPILE_OPTIONS -DAYLA_STRERROR_R_SUPPORT APPEND)
idf_build_set_property(COMPILE_OPTIONS -DCLIENT_MT APPEND)
idf_build_set_property(COMPILE_OPTIONS -DLOG_LOCK APPEND)
idf_build_set_property(COMPILE_OPTIONS -DLOG_SEV_SHORT APPEND)
idf_build_set_property(COMPILE_OPTIONS -DAYLA_LOG_SNAPSHOTS APPEND)
idf_build_set_property(COMPILE_OPTIONS -DLWIP_1_5_0_SUPPORT APPEND)
idf_build_set_property(COMPILE_OPTIONS -DSNTP_STARTUP_DELAY=0 APPEND)
idf_build_set_property(COMPILE_OPTIONS -DSTATIC_WEB_CONTENT_IN_MEMORY APPEND)
idf_build_set_property(COMPILE_OPTIONS -D_HAS_ASSERT_F_ APPEND)
idf_build_set_property(COMPILE_OPTIONS -Wno-address APPEND)
idf_build_set_property(COMPILE_OPTIONS -Wno-missing-field-initializers APPEND)

add_cstyle_checks(${COMPONENT_LIB} "${CSTYLE_SOURCES}")

#
# Configuration checks
#
if(CONFIG_MBEDTLS_SSL_PROTO_TLS1 OR CONFIG_MBEDTLS_SSL_PROTO_TLS1_1)
	message(FATAL_ERROR
		"Use menuconfig to disable TLS 1.0 and TLS 1.1 support. "
		"Use TLS >= 1.2.")
endif()
if(NOT CONFIG_MBEDTLS_HAVE_TIME_DATE)
	message(FATAL_ERROR
		"Use menuconfig to enable TLS certificate expiry check")
endif()
if(NOT CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC)
	message(WARNING
		"menuconfig CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC recommended")
endif()
if(CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS LESS 2)
	message(FATAL_ERROR "Use menuconfig "
		"to set CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS to "
		"at least 2")
endif()
