#
# Copyright 2022 Ayla Networks, Inc.  All rights reserved.
#
set(SOURCES
		adb.c
		adb_ayla_svc.c
		adb_conn_svc.c
		adb_mbox_svc.c
		adb_wifi_cfg_svc.c
	)
set(CSTYLE_SOURCES
		${SOURCES}
		include/adb/adb_ayla_svc.h
		include/adb/adb_conn_svc.h
		include/adb/adb_mbox_svc.h
		include/adb/adb_wifi_cfg_svc.h
		include/adb/adb.h
	)

idf_component_register(SRCS
		${SOURCES}
	REQUIRES
		ada
		adw
		al
		ayla
		bt
	INCLUDE_DIRS
		include
	PRIV_INCLUDE_DIRS
		../adw 		# TODO allows include of wifi_int.h.
	)

add_cstyle_checks(${COMPONENT_LIB} "${CSTYLE_SOURCES}")
