#
# Copyright 2022 Ayla Networks, Inc.  All rights reserved.
#
set(SOURCES
		ayla_base64.c
		callback.c
		clock_fmt.c
		clock_sw.c
		clock_utils.c
		# conf.c - doesn't pass cstyle, added below
		crc.c
		crc16.c
		crc8.c
		endian.c
		format_mac.c
		format_string.c
		hostname_valid.c
		http.c
		ipaddr_fmt.c
		jsmn_get.c
		jsmn_iterate.c
		json_format_string.c
		log.c
		log_buf.c
		log_cmd.c
		log_conf.c
		log_io.c
		lookup_by_name.c
		lookup_by_val.c
		parse_argv.c
		parse_date.c
		parse_hex.c
		parse_mac.c
		parse_url.c
		snprintf.c
		tlv.c
		time_cli.c
		timer.c
		uri_arg_allowed_map.c
		uri_decode.c
		uri_encode.c
		utf8.c
		vsnprintf.c
)
set(CSTYLE_SOURCES
		${SOURCES}
		include/ayla/assert.h
		include/ayla/ayla_proto_mcu.h
		include/ayla/ayla_spi_mcu.h
		include/ayla/base64.h
		include/ayla/bootinfo.h
		include/ayla/callback.h
		include/ayla/clock.h
		include/ayla/cmd.h
		include/ayla/compiler_gcc.h
		include/ayla/compiler_iar.h
		include/ayla/conf.h
		# include/ayla/conf_token.h
		include/ayla/conf_tokens.h
		include/ayla/country_codes.h
		include/ayla/crc.h
		include/ayla/dns.h
		include/ayla/endian.h
		include/ayla/http.h
		include/ayla/ipaddr_fmt.h
		include/ayla/jsmn_get.h
		include/ayla/json.h
		include/ayla/log.h
		include/ayla/mod_log.h
		include/ayla/nameval.h
		include/ayla/parse.h
		include/ayla/patch.h
		include/ayla/patch_state.h
		include/ayla/random.h
		include/ayla/serial.h
		include/ayla/snprintf.h
		include/ayla/spi.h
		include/ayla/spi_flash.h
		include/ayla/stm32_crc.h
		include/ayla/timer.h
		include/ayla/tlv.h
		include/ayla/tlv_access.h
		include/ayla/uri_code.h
		include/ayla/utf8.h
		include/ayla/utypes.h
		include/ayla/wifi_error.h
		include/ayla/wifi_status.h
	)

if(${ADA_BUILD_IN_MODULE_REPO})
	set(JSMN_PATH ${ADA_REPO_ROOT}/ext/jsmn)
	set(REQUIRES_LIST "")
else()
	set(JSMN_PATH ../jsmn)
	set(REQUIRES_LIST
			ada
			al
		)
endif()

idf_component_register(SRCS
		${SOURCES}
		conf.c	# TODO doesn't pass cstyle
		${JSMN_PATH}/jsmn.c
	INCLUDE_DIRS
		include
		${JSMN_PATH}
	REQUIRES
		${REQUIRES_LIST}
	PRIV_INCLUDE_DIRS
		.
	PRIV_REQUIRES
		ada		
		al
	)

add_cstyle_checks(${COMPONENT_LIB} "${CSTYLE_SOURCES}")
