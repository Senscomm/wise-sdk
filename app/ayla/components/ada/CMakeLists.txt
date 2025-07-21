#
# Copyright 2022 Ayla Networks, Inc.  All rights reserved.
#
set(SOURCES
		ada_conf.c
		ada_err.c
		ada_mbuf.c
		ada_timer.c
		ame.c
		ame_json.c
		batch.c
		client.c
		client_ame.c
		clientv2_msg.c
		client_auth.c
		client_batch.c
		client_cli.c
		client_conf.c
		client_ifp.c
		client_metrics.c
		client_ota.c
		client_prop.c
		client_task.c
		conf_json.c
		http2mqtt_client.c
		http_client.c
		lan.c
		local_control.c
		log_client.c
		log_page.c
		metrics.c
		metrics_page.c
		mqtt_client.c
		mqtt_packet.c
		mqttv1_client.c
		oem.c
		onboard_msg.c
		prop_mgr.c
		prop_page.c
		sched.c
		schedeval.c
		server.c
		server_alt.c
		server_req.c
		server_test.c
		sprop.c
		sys_conf.c
		test_svc.c
	)
set(CSTYLE_SOURCES
		${SOURCES}
		ada_lock.h
		ada_mbuf.h
		ame.h
		ame_json.h
		batch_int.h
		ca_cert.h
		client_ame.h
		client_batch.h
		client_common.h
		client_int.h
		client_lock.h
		client_req.h
		client_timer.h
		clientv2_msg.h
		http2mqtt_client.h
		http_client.h
		include/ada/ada_conf.h
		include/ada/ada_lan_conf.h
		include/ada/ada_wifi.h
		include/ada/batch.h
		include/ada/client.h
		include/ada/client_ota.h
		include/ada/dnss.h
		include/ada/err.h
		include/ada/generic_session.h
		include/ada/lan_state.h
		include/ada/libada.h
		include/ada/linker_text.h
		include/ada/local_control.h
		include/ada/log_page.h
		include/ada/metrics.h
		include/ada/onboard_msg.h
		include/ada/prop.h
		include/ada/prop_mgr.h
		include/ada/sched.h
		include/ada/server_req.h
		include/ada/sprop.h
		include/ada/task_label.h
		lan_int.h
		metrics_int.h
		mqtt_client.h
		mqtt_packet.h
		mqttv1_client.h
		oem_int.h
		prop_int.h
		schedeval.h
		test_svc_int.h
	)

set(PRIV_REQUIRES_LIST
		paho
	)

if(NOT DEFINED AYLA_BUILD_WITHOUT_ADW)
	list(APPEND PRIV_REQUIRES_LIST adw)
endif()

idf_component_register(SRCS
		${SOURCES}
	INCLUDE_DIRS
		include
	PRIV_INCLUDE_DIRS
		.
	REQUIRES
		al
		ayla
	PRIV_REQUIRES
		${PRIV_REQUIRES_LIST}
		adb
	)

#
# Compile options for this component.
#
if(NOT DEFINED AYLA_BUILD_WITHOUT_ADW)
	target_compile_options(${COMPONENT_LIB} PRIVATE
			-DAYLA_WIFI_SUPPORT
		)
endif()

#
# For internal builds at Ayla, create build.h and perform source file checks.
#
if(${ADA_BUILD_IN_MODULE_REPO})
	#
	# Get ADA version
	#
	file(READ "${COMPONENT_DIR}/ada_version.mk" ADA_VERSION)
	string(STRIP "${ADA_VERSION}" ADA_VERSION)
	string(REGEX REPLACE ".*= " "" ADA_VERSION "${ADA_VERSION}")

	set(BUILD_H_IN build.h.in)
	set(BUILD_H build.h)
	include(${ADA_VERSION_CMAKE})

	add_cstyle_checks(${COMPONENT_LIB} "${CSTYLE_SOURCES}")
endif()
