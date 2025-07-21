#
# Copyright 2023 Ayla Networks, Inc.  All rights reserved.
#
set(REQUIRES_LIST
		ada
		ayla
		mbedtls
		adm
		console
		driver
		app_update
		nvs_flash
	)

set(SOURCES
		"app_common.c"
		"app_main.c"
		"demo_matter.c"
		"demo_ota.c"
	)

set(CSTYLE_SOURCES
		"${SOURCES}"
	)

idf_component_register(SRC_DIRS
		"."
		"zap-generated"
		"app"
	INCLUDE_DIRS
		"."
	REQUIRES
		"${REQUIRES_LIST}"
	)

add_cstyle_checks(${COMPONENT_LIB} "${CSTYLE_SOURCES}")
