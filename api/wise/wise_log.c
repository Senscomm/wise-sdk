/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.
 */
// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Inspired by log.c of ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and will provide wise Wi-Fi API as being ESP8266 style
 */

#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <cmsis_os.h>
#include <assert.h>
#include <mutex.h>

#include "wise_log.h"

#define LOG_COLOR           "\033[0;%dm"
#define LOG_BOLD            "\033[1;%dm"
#define LOG_RESET_COLOR     "\033[0m"
#define MAX_LOG_LENGTH CONFIG_LOG_BUF_SIZE // or any size you deem appropriate

#ifdef CONFIG_LOG_COLORS
static const uint32_t s_log_color[WISE_LOG_MAX] = {
	0,  //  WISE_LOG_NONE
	31, //  WISE_LOG_ERROR
	33, //  WISE_LOG_WARN
	32, //  WISE_LOG_INFO
	0,  //  WISE_LOG_DEBUG
	0,  //  WISE_LOG_VERBOSE
};
#endif

static const char s_log_prefix[WISE_LOG_MAX] = {
	'N', //  WISE_LOG_NONE
	'E', //  WISE_LOG_ERROR
	'W', //  WISE_LOG_WARN
	'I', //  WISE_LOG_INFO
	'D', //  WISE_LOG_DEBUG
	'V', //  WISE_LOG_VERBOSE
};

mutex wifi_log_mtx;
static char log_buf[MAX_LOG_LENGTH];

static int wise_log_write_str(const char *s, bool color)
{
	int len = strlen(s);
	int pos = -1;

	if (color)
		pos -= strlen(LOG_RESET_COLOR);

	if (s[len + pos] != '\n'
			&& s[len + pos] != '\r')
		len = printf("%s\n\r", s);
	else
		len = printf("%s", s);

	return len;
}

static uint32_t wise_log_timestamp()
{
	return osKernelGetTickCount();
}

/**
 * @brief Write message into the log
 */
void wise_log_write(wise_log_level_t level, const char *tag,  const char *fmt, ...)
{
	va_list va;
	char prefix = level >= WISE_LOG_MAX ? 'N' : s_log_prefix[level];
#ifdef CONFIG_LOG_COLORS
	uint32_t color = level >= WISE_LOG_MAX ? 0 : s_log_color[level];
#else
	uint32_t color = 0;
#endif

	uint32_t ts = wise_log_timestamp();
	int offset = 0;

	if(wifi_log_mtx.mid == NULL)
		mtx_init(&wifi_log_mtx, NULL, NULL, MTX_DEF);

	mtx_lock(&wifi_log_mtx);

	if (color) {
		offset += snprintf(log_buf + offset, MAX_LOG_LENGTH - offset, LOG_COLOR, color);
	}


	offset += snprintf(log_buf + offset, MAX_LOG_LENGTH - offset, "%c (%d) %s: ", prefix, ts, tag);

	va_start(va, fmt);
	offset += vsnprintf(log_buf + offset, MAX_LOG_LENGTH - offset, fmt, va);
	va_end(va);

	if (color && (MAX_LOG_LENGTH - offset > strlen(LOG_RESET_COLOR))) {
		offset += snprintf(log_buf + offset, MAX_LOG_LENGTH - offset, LOG_RESET_COLOR);
	}

	if (offset >= MAX_LOG_LENGTH) {
		offset = MAX_LOG_LENGTH - 1;
	}

	log_buf[offset] = '\0';

	wise_log_write_str(log_buf, color == 0 ? false : true);

	mtx_unlock(&wifi_log_mtx);

}
