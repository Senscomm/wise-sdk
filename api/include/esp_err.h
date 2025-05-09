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
 * Translation from wise_err.h to ease importing components.
 */

#ifndef __ESP_ERR_H__
#define __ESP_ERR_H__

#include "esp_compiler.h"
#include <wise_err.h>

#define esp_err_t	wise_err_t

#define ESP_OK				WISE_OK
#define ESP_FAIL			WISE_FAIL

#define ESP_ERR_NO_MEM          	WISE_ERR_NO_MEM
#define ESP_ERR_INVALID_ARG     	WISE_ERR_INVALID_ARG
#define ESP_ERR_INVALID_STATE   	WISE_ERR_INVALID_STATE
#define ESP_ERR_INVALID_SIZE    	WISE_ERR_INVALID_SIZE
#define ESP_ERR_NOT_FOUND       	WISE_ERR_NOT_FOUND
#define ESP_ERR_NOT_SUPPORTED   	WISE_ERR_NOT_SUPPORTED
#define ESP_ERR_TIMEOUT         	WISE_ERR_TIMEOUT
#define ESP_ERR_INVALID_RESPONSE    WISE_ERR_INVALID_RESPONSE
#define ESP_ERR_INVALID_CRC     	WISE_ERR_INVALID_CRC
#define ESP_ERR_INVALID_VERSION     WISE_ERR_INVALID_VERSION
#define ESP_ERR_INVALID_MAC     	WISE_ERR_INVALID_MAC

#define ESP_ERR_WIFI_BASE       	WISE_ERR_WIFI_BASE
#define ESP_ERR_MESH_BASE       	WISE_ERR_MESH_BASE
#define ESP_ERR_ESP_TLS_BASE       	WISE_ERR_SCM_TLS_BASE
#define ESP_ERR_TCP_TRANSPORT_BASE 	WISE_ERR_TCP_TRANSPORT_BASE
#define ESP_ERR_HTTP_BASE           WISE_ERR_SCM_HTTP_BASE

#define ESP_ERROR_CHECK(x)		WISE_ERROR_CHECK(x)

#endif /* __ESP_ERR_H__ */
