#
# Copyright 2022 Ayla Networks, Inc.  All rights reserved.
#
set(SOURCES
		wifi.c
		wifi_conf.c
		wifi_page.c
	)

set(CSTYLE_SOURCES
		${SOURCES}
		include/adw/wifi.h
		include/adw/wifi_conf.h
		wifi_int.h
		wifi_page.h
		wifi_timer.h
	)

idf_component_register(SRCS
		${SOURCES}
	REQUIRES
		ada
		ayla
	INCLUDE_DIRS
		include
	PRIV_REQUIRES
		al
	)

target_compile_options(${COMPONENT_LIB} PRIVATE
		-DAYLA_WIFI_SUPPORT
		-DWIFI_CONCURRENT_AP_STA_MODE
	)

add_cstyle_checks(${COMPONENT_LIB} "${CSTYLE_SOURCES}")
