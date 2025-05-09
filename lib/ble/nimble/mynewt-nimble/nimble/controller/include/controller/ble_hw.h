/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef H_BLE_HW_
#define H_BLE_HW_

#ifdef __cplusplus
extern "C" {
#endif

#include "hal/compiler.h"

#include "syscfg/syscfg.h"

#if defined(ARCH_sim)
#define BLE_USES_HW_WHITELIST   (0)
#else
#define BLE_USES_HW_WHITELIST   MYNEWT_VAL(BLE_HW_WHITELIST_ENABLE)
#endif

/* Returns the number of hw whitelist elements */
uint8_t _ble_hw_whitelist_size(void);
extern __func_tab__ uint8_t (*ble_hw_whitelist_size)(void);

/* Clear the whitelist */
void _ble_hw_whitelist_clear(void);
extern __func_tab__ void (*ble_hw_whitelist_clear)(void);

/* Remove a device from the hw whitelist */
void _ble_hw_whitelist_rmv(const uint8_t *addr, uint8_t addr_type);
extern __func_tab__ void (*ble_hw_whitelist_rmv)(const uint8_t *addr, uint8_t addr_type);

/* Add a device to the hw whitelist */
int _ble_hw_whitelist_add(const uint8_t *addr, uint8_t addr_type);
extern __func_tab__ int (*ble_hw_whitelist_add)(const uint8_t *addr, uint8_t addr_type);

/* Enable hw whitelisting */
void _ble_hw_whitelist_enable(void);
extern __func_tab__ void (*ble_hw_whitelist_enable)(void);

/* Enable hw whitelisting */
void _ble_hw_whitelist_disable(void);
extern __func_tab__ void (*ble_hw_whitelist_disable)(void);

/* Boolean function returning true if address matches a whitelist entry */
int _ble_hw_whitelist_match(void);
extern __func_tab__ int (*ble_hw_whitelist_match)(void);

/* Encrypt data */
struct ble_encryption_block;
int _ble_hw_encrypt_block(struct ble_encryption_block *ecb);
extern __func_tab__ int (*ble_hw_encrypt_block)(struct ble_encryption_block *ecb);

#if (SCM2010)
/* AES128 CCM ecnryption */
int _ble_hw_ccm_encryption(struct ble_encryption_block *ecb, uint8_t *out, int out_len, uint8_t *in, uint8_t in_len, uint8_t aad);
extern __func_tab__ int (*ble_hw_ccm_encryption)(struct ble_encryption_block *ecb, uint8_t *out, int out_len, uint8_t *in, uint8_t in_len, uint8_t aad);

/* AES128 CCM decryption */
int _ble_hw_ccm_decryption(struct ble_encryption_block *ecb, uint8_t *out, int out_len, uint8_t *in, uint8_t in_len, uint8_t aad);
extern __func_tab__ int (*ble_hw_ccm_decryption)(struct ble_encryption_block *ecb, uint8_t *out, int out_len, uint8_t *in, uint8_t in_len, uint8_t aad);

/* generate private & public pair */
int _ble_hw_ecc_p256_make_key(uint8_t *private_key, uint8_t *public_key);
extern __func_tab__ int (*ble_hw_ecc_p256_make_key)(uint8_t *private_key, uint8_t *public_key);

/* generate dhkey */
int _ble_hw_ecc_p256_make_dhkey(uint8_t *private_key, uint8_t *public_key, uint8_t *dhkey);
extern __func_tab__ int (*ble_hw_ecc_p256_make_dhkey)(uint8_t *private_key, uint8_t *public_key, uint8_t *dhkey);
#endif

/* Random number generation */
typedef void (*ble_rng_isr_cb_t)(uint8_t rnum);
int _ble_hw_rng_init(ble_rng_isr_cb_t cb, int bias);
extern __func_tab__ int (*ble_hw_rng_init)(ble_rng_isr_cb_t cb, int bias);

/**
 * Start the random number generator
 *
 * @return int
 */
int _ble_hw_rng_start(void);
extern __func_tab__ int (*ble_hw_rng_start)(void);

/**
 * Stop the random generator
 *
 * @return int
 */
int _ble_hw_rng_stop(void);
extern __func_tab__ int (*ble_hw_rng_stop)(void);

/**
 * Read the random number generator.
 *
 * @return uint8_t
 */
extern __func_tab__ uint8_t (*ble_hw_rng_read)(void);

/*  Clear the resolving list*/
extern __func_tab__ void (*ble_hw_resolv_list_clear)(void);

/* Add a device to the hw resolving list */
extern __func_tab__ int (*ble_hw_resolv_list_add)(uint8_t *irk);

/* Remove a device from the hw resolving list */
extern __func_tab__ void (*ble_hw_resolv_list_rmv)(int index);

/* Returns the size of the whitelist in HW */
extern __func_tab__ uint8_t (*ble_hw_resolv_list_size)(void);

/* Returns index of resolved address; -1 if not resolved */
extern __func_tab__ int (*ble_hw_resolv_list_match)(uint8_t *addr);

#if (SCM2010)
extern __func_tab__ void (*ble_hw_resolv_list_set_addr)(uint8_t *addr);
#endif

/* Returns public device address or -1 if not present */
extern __func_tab__ int (*ble_hw_get_public_addr)(ble_addr_t *addr);

/* Returns random static address or -1 if not present */
extern __func_tab__ int (*ble_hw_get_static_addr)(ble_addr_t *addr);

#ifdef __cplusplus
}
#endif

#endif /* H_BLE_HW_ */
