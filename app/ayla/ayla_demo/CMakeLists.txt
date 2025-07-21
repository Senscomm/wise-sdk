# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(led_strip_list
	esp32c3
	esp32s3
	)

if(IDF_TARGET IN_LIST led_strip_list)
    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/common_components")
endif()

#
# Determine if building inside ADA workspace.  This is normal inside Ayla only.
# ADA_REPO_ROOT is required for building inside Ayla repository
#
execute_process(COMMAND git rev-parse --show-toplevel
			OUTPUT_VARIABLE GIT_TREE
			ERROR_QUIET
		)
string(STRIP "${GIT_TREE}" GIT_TREE)
if((NOT "${GIT_TREE}" STREQUAL "") AND (EXISTS ${GIT_TREE}/bc/lib/ada))
	set(ADA_REPO_ROOT ${GIT_TREE})
endif()

include(../../tools/cmake/ayla_project.cmake)
set(COMPONENTS bootloader esptool_py main)
project(ayla_demo)

idf_build_set_property(COMPILE_OPTIONS "-DAYLA_BATCH_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_LAN_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_BLUETOOTH_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_LOCAL_CONTROL_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_TEST_SERVICE_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_FILE_PROP_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_BATCH_PROP_SUPPORT" APPEND)
#idf_build_set_property(COMPILE_OPTIONS "-DAYLA_MSG_PROP_SUPPORT" APPEND)
#idf_build_set_property(COMPILE_OPTIONS "-DDEMO_MSG_PROP" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_ESP32_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_WIFI_SUPPORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DLOG_SEV_SHORT" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DAYLA_LOG_THREAD_CT=12" APPEND)
