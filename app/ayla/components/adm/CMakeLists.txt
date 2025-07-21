#
# Copyright 2022 Ayla Networks, Inc.  All rights reserved.
#

if(AYLA_MATTER_SUPPORT)
set(SOURCES
	adm_cli.cpp
	adm_data_provider.cpp
	adm_data_provider.h
	adm_int.h
	adm.cpp
	ayla-lc-server.cpp
	ayla-server.cpp
    )

set(CSTYLE_SOURCES
	${SOURCES}
	include/adm/adm.h
	include/adm/adm_ayla_base.h
	include/adm/adm_ayla_numbers.h
	include/adm/adm_cli.h
	include/adm/adm_csa_numbers.h
	include/adm/adm_local_control.h
    )

set(CHIP_APP_CLUSTERS "${CHIP_ROOT}/src/app/clusters")

foreach(cluster IN LISTS ADM_CLUSTER_LIST)
	list(APPEND CHIP_CLUSTER_DIR_LIST ${CHIP_APP_CLUSTERS}/${cluster})
endforeach()

idf_component_register(
	SRC_DIRS
        	.
	        "${CHIP_ROOT}/src/app/server"
	        "${CHIP_ROOT}/src/app/util"
	        "${CHIP_ROOT}/src/app/reporting"
	        "${CHIP_ROOT}/zzz_generated/app-common/app-common/zap-generated/attributes"
	        "${CHIP_ROOT}/zzz_generated/app-common/app-common/zap-generated"
        	"${CHIP_CLUSTER_DIR_LIST}"
	INCLUDE_DIRS
		include
		"${ADM_INCLUDE_LIST}"
	PRIV_INCLUDE_DIRS
		.
		../ada
	REQUIRES
		chip
		esp_event
		esp_netif
		esp_wifi
		bt
		al
		ayla
	)

add_cstyle_checks(${COMPONENT_LIB} "${CSTYLE_SOURCES}")

target_compile_options(${COMPONENT_LIB} PUBLIC
		"-DCHIP_ADDRESS_RESOLVE_IMPL_INCLUDE_HEADER=<lib/address_resolve/AddressResolve_DefaultImpl.h>"
	)

endif()