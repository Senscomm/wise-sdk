#
# Copyright 2023 Ayla Networks, Inc.  All rights reserved.
#
cmake_minimum_required(VERSION 3.5)

set(APP_NAME ayla_matter_demo)
set(APP_VER 1.0.1)

set(AYLA_MATTER_SUPPORT 1)

set(ADM_INCLUDE_LIST
	"${CMAKE_CURRENT_LIST_DIR}/main"
)

set(ADM_CLUSTER_LIST
		access-control-server
		administrator-commissioning-server
		basic-information
		bindings
		descriptor
		diagnostic-logs-server
		fixed-label-server
		general-commissioning-server
		general-diagnostics-server
		group-key-mgmt-server
		groups-server
		identify-server
		network-commissioning
		on-off-server
		operational-credentials-server
		software-diagnostics-server
		user-label-server
		wifi-network-diagnostics-server
	)

include(../../tools/cmake/ayla_matter_project.cmake)

project(${APP_NAME})

idf_build_set_property(COMPILE_OPTIONS "-DAPP_NAME=\"${APP_NAME}\"" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAPP_VER=\"${APP_VER}\"" APPEND)

idf_build_set_property(COMPILE_OPTIONS "-DCHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR=1234" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_MATTER_QRGEN_SECRET=\"QXlsYU1hdHRlckRlbW8=\"" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DCHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME=\"Aylanetworks\"" APPEND)

idf_build_set_property(COMPILE_OPTIONS "-DAYLA_LAN_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_LOCAL_CONTROL_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_ESP32_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DLOG_SEV_SHORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_LOG_THREAD_CT=12" APPEND)
