/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.
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
 * Originated from esp_err.h of ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and modified to provide wise Wi-Fi API as being ESP8266 style
 */


#ifndef __WISE_ERR_H__
#define __WISE_ERR_H__

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int wise_err_t;

/* Definitions for error constants. */

#define WISE_OK          	0
#define WISE_FAIL        	-1

#define WISE_ERR_NO_MEM          	0x101
#define WISE_ERR_INVALID_ARG     	0x102
#define WISE_ERR_INVALID_STATE   	0x103
#define WISE_ERR_INVALID_SIZE    	0x104
#define WISE_ERR_NOT_FOUND       	0x105
#define WISE_ERR_NOT_SUPPORTED   	0x106
#define WISE_ERR_TIMEOUT         	0x107
#define WISE_ERR_INVALID_RESPONSE    	0x108
#define WISE_ERR_INVALID_CRC     	0x109
#define WISE_ERR_INVALID_VERSION     	0x10A
#define WISE_ERR_INVALID_MAC     	0x10B
#define WISE_ERR_IOCTL			0x10C  /*!< for iotcl error, see errno */

#define WISE_ERR_WIFI_BASE       	0x3000 /*!< Starting number of WiFi error codes */
#define WISE_ERR_MESH_BASE       	0x4000 /*!< Starting number of MESH error codes */
#define WISE_ERR_SCM_HTTP_BASE		0x7000 /*!< Starting number of SCM-HTTP error codes */
#define WISE_ERR_SCM_TLS_BASE       0x8000 /*!< Starting number of SCM-TLS error codes */
#define WISE_ERR_TCP_TRANSPORT_BASE 0xe000 /*!< Starting number of TCP Transport error codes */

/**
 * Macro which can be used to check the error code,
 * and terminate the program in case the code is not WISE_OK.
 * Prints the error code, error location, and the failed statement to serial output.
 *
 * Disabled if assertions are disabled.
 */
#ifdef NDEBUG
#define WISE_ERROR_CHECK(x) do {                                         \
        wise_err_t __err_rc = (x);                                       \
        (void) sizeof(__err_rc);                                        \
    } while(0)
#else
#define WISE_ERROR_CHECK(x) do {                                        \
        wise_err_t __err_rc = (x);                                      \
        if (__err_rc != WISE_OK) {                                      \
            hal_assert_fail(#x" != WISE_OK",				\
			    __FILE__, __LINE__, __func__);		\
        }                                                               \
    } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __WISE_ERR_H__ */
