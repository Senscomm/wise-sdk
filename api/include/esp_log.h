/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.
 */

// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Originated from esp_log.h of ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and modified to provide wise Wi-Fi API as being ESP8266 style
 */

/*
 * Translation from wise_log.h to ease importing components.
 */

#ifndef __ESP_LOG_H__
#define __ESP_LOG_H__

#include <wise_log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LOG_NONE		WISE_LOG_NONE
#define ESP_LOG_ERROR		WISE_LOG_ERROR
#define ESP_LOG_WARN		WISE_LOG_WARN
#define ESP_LOG_INFO		WISE_LOG_INFO
#define ESP_LOG_DEBUG		WISE_LOG_DEBUG
#define ESP_LOG_VERBOSE		WISE_LOG_VERBOSE

#define esp_log_level_t		wise_log_level_t

#define esp_log_write		wise_log_write

#define ESP_LOGE		WISE_LOGE
#define ESP_LOGW		WISE_LOGW
#define ESP_LOGI		WISE_LOGI
#define ESP_LOGD		WISE_LOGD
#define ESP_LOGV		WISE_LOGV

#define ESP_LOG_LEVEL	WISE_LOG_LEVEL

#define ESP_LOG_LEVEL_LOCAL	WISE_LOG_LEVEL_LOCAL

#ifdef __cplusplus
}
#endif

#endif /* __ESP_LOG_H__ */
