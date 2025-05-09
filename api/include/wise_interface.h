/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.
 */

// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
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
 * Originated from esp_interface.h of ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and modified to provide wise Wi-Fi API as being ESP8266 style
 */

#ifndef __WISE_INTERFACE_H__
#define __WISE_INTERFACE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WISE_IF_WIFI_STA = 0,     /**< station interface */
    WISE_IF_WIFI_AP,          /**< soft-AP interface */
    WISE_IF_MAX
} wise_interface_t;

#ifdef __cplusplus
}
#endif


#endif /* __WISE_INTERFACE_TYPES_H__ */
