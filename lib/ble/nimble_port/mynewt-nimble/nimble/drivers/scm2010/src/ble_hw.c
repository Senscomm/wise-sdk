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

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "sys/ioctl.h"
#include "vfs.h"

#include "syscfg/syscfg.h"
#include "os/os.h"
#include "ble/xcvr.h"
#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "controller/ble_hw.h"
#include "os/os_trace_api.h"

#include "hal/device.h"
#include "hal/crypto.h"
#include "hal/console.h"
#include "hal/rom.h"
#include "controller/ble_phy.h"
#include "controller/ble_ll.h"
#include "controller/ble_ll_conn.h"

#include "hal/efuse.h"

#include "mmap.h"

#define SKE_INLINE static __attribute__((__always_inline__)) inline

#if (USE_HW_CRYPTO == 1)

#define SKE_CTRL         (*((volatile uint32_t *)(SKE_BASE_ADDR+0x00)))
#define SKE_CFG          (*((volatile uint32_t *)(SKE_BASE_ADDR+0x04)))
#define SKE_SR1          (*((volatile uint32_t *)(SKE_BASE_ADDR+0x08)))
#define SKE_SR2          (*((volatile uint32_t *)(SKE_BASE_ADDR+0x0C)))
#define SKE_KEY          (((volatile uint32_t *)(SKE_BASE_ADDR+0x10)))
#define SKE_AAD          (((volatile uint32_t *)(SKE_BASE_ADDR+0x60)))
#define SKE_CLEN         (((volatile uint32_t *)(SKE_BASE_ADDR+0x68)))
#define SKE_IV           (((volatile uint32_t *)(SKE_BASE_ADDR+0x70)))
#define SKE_DIN_CR       (*((volatile uint32_t *)(SKE_BASE_ADDR+0x80)))
#define SKE_DIN          (((volatile uint32_t *)(SKE_BASE_ADDR+0x90)))
#define SKE_DOUT         (((volatile uint32_t *)(SKE_BASE_ADDR+0xB0)))
#define SKE_DMA_SADDR    (*((volatile uint32_t *)(SKE_BASE_ADDR+0xC0)))
#define SKE_DMA_DADDR    (*((volatile uint32_t *)(SKE_BASE_ADDR+0xC4)))
#define SKE_DMA_RLEN     (*((volatile uint32_t *)(SKE_BASE_ADDR+0xC8)))
#define SKE_DMA_WLEN     (*((volatile uint32_t *)(SKE_BASE_ADDR+0xCC)))
#define SKE_VERSION      (*((volatile uint32_t *)(SKE_BASE_ADDR+0xFC)))

#define _attribute_ram_sec_     __attribute__((section(".dma_buffer")))

uint8_t ccm_dma_in[288] __attribute__((section(".dma_buffer")));
uint8_t ccm_dma_out[288] __attribute__((section(".dma_buffer")));

#else
/* If we are using s/w, include tinycrypt */
#include "tinycrypt/aes.h"
#include "tinycrypt/constants.h"
#include "tinycrypt/ecc.h"
#include "tinycrypt/ecc_dh.h"
#endif

void
swap_copy(uint8_t *dst, uint8_t *src, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++) {
        dst[len - 1 - i] = src[i];
    }
}


#if (USE_HW_CRYPTO == 1)

SKE_INLINE void
ske_set_cfg(uint8_t en)
{
    if (en) {
        SKE_CFG |= (0x01 << 12);
    } else {
        SKE_CFG &= ~(0x01 << 12);
    }
}

SKE_INLINE void
ske_set_dma(void)
{
    SKE_CFG |= (0x01 << 16);
}

SKE_INLINE void
ske_config_aes128_encrypt(void)
{
    SKE_CFG = \
              (0x01 << 0) | \
              (0x00 << 11) | \
              (0x02 << 24) | \
              (0x01 << 28);
}

SKE_INLINE void
ske_config_ae128_ccm(uint8_t is_enc)
{
    SKE_CFG = \
              (0x01 << 0) | \
              (0x02 << 24) | \
              (0x0A << 28);

    if (!is_enc) {
        SKE_CFG |= 1 << 11;
    }
}

SKE_INLINE void
ske_start_and_wait(void)
{
    while ((SKE_SR1 & 1) != 0);

    SKE_CTRL |= 1;

    while ((SKE_SR2 & 1) != 1);

    SKE_SR2 = 0;
}

SKE_INLINE void
ske_set_key(uint32_t *buf)
{
    int i;
    for (i = 3; i >= 0; i--) {
        SKE_KEY[i] = buf[i];
    }
}

SKE_INLINE void
ske_set_iv(uint32_t *buf)
{
    int i;
    for (i = 3; i >= 0; i--) {
        SKE_IV[i] = buf[i];
    }
}

SKE_INLINE void
ske_set_input(uint32_t *buf)
{
    int i;
    for (i = 3; i >= 0; i--) {
        SKE_DIN[i] = buf[i];
    }
}

SKE_INLINE void
ske_get_output(uint32_t *buf)
{
    int i;
    for (i = 3; i >= 0; i--) {
        buf[i] = SKE_DIN[i];
    }
}

int
private_key_valid(uint8_t *private_key)
{
    int i;
	uint8_t max_private_key[32] = {0xA8,0x92,0x31,0x7E,0x61,0xe5,0xdc,0x79,
                                   0x42,0xcf,0x8b,0xd3,0x56,0x7d,0x73,0xde,
                                   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f,
                                   0x00,0x00,0x00,0x80,0xff,0xff,0xff,0x7f};

    for (i = 31; i > 0; i--) {
        if (private_key[i] != 0) {
            break;
        }
    }

    if ((i == -1) && (private_key[0] == 0)) {
        return 0;
    }

    for (i = 31; i >= 0; i--) {
        if (private_key[i] > max_private_key[i]) {
            return 0;
        } else if (private_key[i] < max_private_key[i]) {
            return 1;
        }
    }

    return 1;
}


#else

int
ecc_rand(unsigned char *dest, unsigned int size)
{
    uint32_t i;

    for (i = 0; i < size; i++) {
        dest[i] = rand() & 0xFF;
    }

    return 1;
}

#endif

/* Total number of resolving list elements */
#define BLE_HW_RESOLV_LIST_SIZE     (2)

/* We use this to keep track of which entries are set to valid addresses */
#if !defined(SCM2010)
static uint8_t g_ble_hw_whitelist_mask;
#endif

/* Random number generator isr callback */
static ble_rng_isr_cb_t g_ble_rng_isr_cb;
static bool g_ble_rng_started;

/* If LL privacy is enabled, allocate memory for AAR */
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)

#define SW_IRK_LIST_ENTRIES    (MYNEWT_VAL(BLE_LL_RESOLV_LIST_SIZE))

/* NOTE: each entry is 16 bytes long. */
uint32_t g_sw_irk_list[SW_IRK_LIST_ENTRIES * 4];

/* Current number of IRK entries */
uint8_t g_sw_num_irks;

#endif

static int
ble_hw_get_rng(uint8_t *val, int len)
{
	struct device *trng_dev;
	int ret;

	trng_dev = device_get_by_name("trng");
	if (!trng_dev) {
		printk("No trng crypto device\n");
		assert(0);
	}

	ret = crypto_trng_get_rand(trng_dev, val, len);

	return ret;
}

/* Returns public device address or -1 if not present */
int
_ble_hw_get_public_addr(ble_addr_t *addr)
{
	u32 addrl;
	u32 addrh;
	int fd;
	int ret;
	int val[6];
	struct efuse_rw_data e_rw_data;

	addr->type = BLE_ADDR_PUBLIC;

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		printk("Can't open /dev/efuse: %s\n", strerror(errno));
		goto fixed;
	}

	e_rw_data.row = 19;
	e_rw_data.val = &addrl;
	ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &e_rw_data);
	if (ret < 0) {
		printk("ioctl error: %s\n", strerror(errno));
		close(fd);
		goto fixed;
	}

	e_rw_data.row = 20;
	e_rw_data.val = &addrh;
	ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &e_rw_data);
	if (ret < 0) {
		printk("ioctl error: %s\n", strerror(errno));
		close(fd);
		goto fixed;
	}

	close(fd);

	addr->val[5] = ((uint8_t *)&addrh)[3];
	addr->val[4] = ((uint8_t *)&addrh)[2];
	addr->val[3] = ((uint8_t *)&addrh)[1];
	addr->val[2] = ((uint8_t *)&addrh)[0];
	addr->val[1] = ((uint8_t *)&addrl)[3];
	addr->val[0] = ((uint8_t *)&addrl)[2];

	if (ble_addr_cmp(addr, BLE_ADDR_ANY)) {
		printk("Use eFuse BLE public address: %02x.%02x.%02x.%02x.%02x.%02x\n",
				addr->val[5],
				addr->val[4],
				addr->val[3],
				addr->val[2],
				addr->val[1],
				addr->val[0]);

		return 0;
	}

fixed:
	sscanf(CONFIG_DEFAULT_BLE_MACADDR, "%x:%x:%x:%x:%x:%x",
			&val[5], &val[4], &val[3], &val[2], &val[1], &val[0]);

	addr->val[5] = (uint8_t)val[5];
	addr->val[4] = (uint8_t)val[4];
	addr->val[3] = (uint8_t)val[3];
	addr->val[2] = (uint8_t)val[2];
	addr->val[1] = (uint8_t)val[1];
	addr->val[0] = (uint8_t)val[0];

	printk("Use fixed BLE public address: %02x.%02x.%02x.%02x.%02x.%02x\n",
			addr->val[5],
			addr->val[4],
			addr->val[3],
			addr->val[2],
			addr->val[1],
			addr->val[0]);

	return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_get_public_addr, &ble_hw_get_public_addr, &_ble_hw_get_public_addr);
#else
__func_tab__ int (*ble_hw_get_public_addr)(ble_addr_t *addr)
= _ble_hw_get_public_addr;
#endif

/* Returns random static address or -1 if not present */
int
_ble_hw_get_static_addr(ble_addr_t *addr)
{
    /* this is called from the vendor specific hci, we do not support it */
    return -1;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_get_static_addr, &ble_hw_get_static_addr, &_ble_hw_get_static_addr);
#else
__func_tab__ int (*ble_hw_get_static_addr)(ble_addr_t *addr)
= _ble_hw_get_static_addr;
#endif

/**
 * Clear the whitelist
 *
 * @return int
 */
void
_ble_hw_whitelist_clear(void)
{
    /* we are not using hardware whitelist */
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_whitelist_clear, &ble_hw_whitelist_clear, &_ble_hw_whitelist_clear);
#else
__func_tab__ void (*ble_hw_whitelist_clear)(void)
= _ble_hw_whitelist_clear;
#endif

/**
 * Add a device to the hw whitelist
 *
 * @param addr
 * @param addr_type
 *
 * @return int 0: success, BLE error code otherwise
 */
int
_ble_hw_whitelist_add(const uint8_t *addr, uint8_t addr_type)
{
    /* we are not using hardware whitelist */
    return BLE_ERR_SUCCESS;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_whitelist_add, &ble_hw_whitelist_add, &_ble_hw_whitelist_add);
#else
__func_tab__ int (*ble_hw_whitelist_add)(const uint8_t *addr, uint8_t addr_type)
= _ble_hw_whitelist_add;
#endif

/**
 * Remove a device from the hw whitelist
 *
 * @param addr
 * @param addr_type
 *
 */
void
_ble_hw_whitelist_rmv(const uint8_t *addr, uint8_t addr_type)
{
    /* we are not using hardware whitelist */
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_whitelist_rmv, &ble_hw_whitelist_rmv, &_ble_hw_whitelist_rmv);
#else
__func_tab__ void (*ble_hw_whitelist_rmv)(const uint8_t *addr, uint8_t addr_type)
= _ble_hw_whitelist_rmv;
#endif

/**
 * Returns the size of the whitelist in HW
 *
 * @return int Number of devices allowed in whitelist
 */
uint8_t
_ble_hw_whitelist_size(void)
{
    /* we are not using hardware whitelist */
    return BLE_HW_WHITE_LIST_SIZE;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_whitelist_size, &ble_hw_whitelist_size, &_ble_hw_whitelist_size);
#else
__func_tab__ uint8_t (*ble_hw_whitelist_size)(void)
= _ble_hw_whitelist_size;
#endif

/**
 * Enable the whitelisted devices
 */
void
_ble_hw_whitelist_enable(void)
{
    /* we are not using hardware whitelist */
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_whitelist_enable, &ble_hw_whitelist_enable, &_ble_hw_whitelist_enable);
#else
__func_tab__ void (*ble_hw_whitelist_enable)(void)
= _ble_hw_whitelist_enable;
#endif

/**
 * Disables the whitelisted devices
 */
void
_ble_hw_whitelist_disable(void)
{
    /* we are not using hardware whitelist */
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_whitelist_disable, &ble_hw_whitelist_disable, &_ble_hw_whitelist_disable);
#else
__func_tab__ void (*ble_hw_whitelist_disable)(void)
= _ble_hw_whitelist_disable;
#endif

/**
 * Boolean function which returns true ('1') if there is a match on the
 * whitelist.
 *
 * @return int
 */
int
_ble_hw_whitelist_match(void)
{
    /* we are not using hardware whitelist */
    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_whitelist_match, &ble_hw_whitelist_match, &_ble_hw_whitelist_match);
#else
__func_tab__ int (*ble_hw_whitelist_match)(void)
= _ble_hw_whitelist_match;
#endif

/* Encrypt data */
__ilm_ble__ int
_ble_hw_encrypt_block(struct ble_encryption_block *ecb)
{
#if (USE_HW_CRYPTO == 1)

    /* configure for aes128 */
    ske_config_aes128_encrypt();

    /* set key */
    ske_set_cfg(1);
    ske_set_key((uint32_t *)ecb->key);
    ske_start_and_wait();
    ske_set_cfg(0);

    /* set input */
    ske_set_input((uint32_t *)ecb->plain_text);

    /* start for data */
    ske_start_and_wait();

    /* get output */
    ske_get_output((uint32_t *)ecb->cipher_text);
#else

    struct tc_aes_key_sched_struct s;

    if (tc_aes128_set_encrypt_key(&s, ecb->key) == TC_CRYPTO_FAIL) {
        return -1;
    }

    if (tc_aes_encrypt(ecb->cipher_text, ecb->plain_text, &s) == TC_CRYPTO_FAIL) {
        return -1;
    }
#endif

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_encrypt_block, &ble_hw_encrypt_block, &_ble_hw_encrypt_block);
#else
__func_tab__ int (*ble_hw_encrypt_block)(struct ble_encryption_block *ecb)
= _ble_hw_encrypt_block;
#endif

__ilm_ble__ int
_ble_hw_ccm_encryption(struct ble_encryption_block *ecb, uint8_t *out, int out_len, uint8_t *in, uint8_t in_len, uint8_t aad)
{
    int ret = 0;
#if (USE_HW_CRYPTO == 1)
    uint8_t *b0 = &ccm_dma_in[0];
    uint8_t *b1 = &ccm_dma_in[16];
    uint8_t *text = &ccm_dma_in[32];
    uint8_t a0[16];
    uint32_t dma_len;

    b0[0] = 0x40 | 1 << 3 | 1;
    memcpy(&b0[1], ecb->nonce, 13);
	b0[14] = (uint8_t)(in_len >> 8);
	b0[15] = (uint8_t)(in_len);

    memset(b1, 0, 16);
    b1[0] = 0x00;
    b1[1] = 0x01;
    b1[2] = aad;

    memcpy(text , in, in_len);
    if (in_len % 16) {
        memset(text + in_len, 0, 16 - (in_len % 16));
    }

    SKE_AAD[0] = 0x100;
    SKE_AAD[1] = 0;

    SKE_CLEN[0] = in_len << 3;
    SKE_CLEN[1] = 0;

    a0[0] = 1;
    memcpy(&a0[1], ecb->nonce, 13);
    a0[14] = 0;
    a0[15] = 0;

    ske_config_ae128_ccm(1);
    /* set key */
    ske_set_cfg(1);
    ske_set_key((uint32_t *)ecb->cipher_text);
    ske_set_iv((uint32_t *)a0);
    ske_start_and_wait();
    ske_set_dma();
    ske_set_cfg(0);

    dma_len = (((in_len + 15) / 16 ) * 4);

    SKE_DMA_SADDR = (uint32_t)ccm_dma_in;
    SKE_DMA_DADDR = (uint32_t)ccm_dma_out;
    SKE_DMA_RLEN = (dma_len + 8) << 2;
    SKE_DMA_WLEN = (dma_len + 4) << 2;

    ske_start_and_wait();

    memcpy(out, ccm_dma_out, in_len);
    memcpy(&out[in_len], &ccm_dma_out[dma_len * 4], 4);
#else
    tc_ccm_generation_encryption(out, out_len,
                                 (const uint8_t *)&aad, 1,
                                 (const uint8_t *)in, in_len,
                                 &ecb->ccm);
#endif
    return ret;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_ccm_encryption, &ble_hw_ccm_encryption, &_ble_hw_ccm_encryption);
#else
__func_tab__ int (*ble_hw_ccm_encryption)(struct ble_encryption_block *ecb, uint8_t *out, int out_len, uint8_t *in, uint8_t in_len, uint8_t aad)
= _ble_hw_ccm_encryption;
#endif

__ilm_ble__ int
_ble_hw_ccm_decryption(struct ble_encryption_block *ecb, uint8_t *out, int out_len, uint8_t *in, uint8_t in_len, uint8_t aad)
{
    int ret = 0;
#if (USE_HW_CRYPTO == 1)
    uint8_t *b0 = &ccm_dma_in[0];
    uint8_t *b1 = &ccm_dma_in[16];
    uint8_t *text = &ccm_dma_in[32];
    uint8_t a0[16];
    uint32_t dma_len;

    in_len -= 4;

    b0[0] = 0x40 | 1 << 3 | 1;
    memcpy(&b0[1], ecb->nonce, 13);
	b0[14] = (uint8_t)(in_len >> 8);
	b0[15] = (uint8_t)(in_len);

    memset(b1, 0, 16);
    b1[0] = 0x00;
    b1[1] = 0x01;
    b1[2] = aad;

    memcpy(text , in, in_len);
    if (in_len % 16) {
        memset(text + in_len, 0, 16 - (in_len % 16));
    }

    SKE_AAD[0] = 0x100;
    SKE_AAD[1] = 0;

    SKE_CLEN[0] = in_len << 3;
    SKE_CLEN[1] = 0;

    a0[0] = 1;
    memcpy(&a0[1], ecb->nonce, 13);
    a0[14] = 0;
    a0[15] = 0;

    ske_config_ae128_ccm(0);
    /* set key */
    ske_set_cfg(1);
    ske_set_key((uint32_t *)ecb->cipher_text);
    ske_set_iv((uint32_t *)a0);
    ske_start_and_wait();
    ske_set_dma();
    ske_set_cfg(0);

    dma_len = (((in_len + 15) / 16 ) * 4);

    SKE_DMA_SADDR = (uint32_t)ccm_dma_in;
    SKE_DMA_DADDR = (uint32_t)ccm_dma_out;
    SKE_DMA_RLEN = (dma_len + 8) << 2;
    SKE_DMA_WLEN = (dma_len + 4) << 2;

    ske_start_and_wait();


    if (!memcmp(&in[in_len], &ccm_dma_out[dma_len * 4], 4)) {
        memcpy(out, ccm_dma_out, in_len);
        ret = 1;
    }

#else

    ret = tc_ccm_decryption_verification(out, out_len,
                                         (const uint8_t *)&aad, 1,
                                         (const uint8_t *)in, in_len,
                                         &ecb->ccm);
#endif
    return ret;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_ccm_decryption, &ble_hw_ccm_decryption, &_ble_hw_ccm_decryption);
#else
__func_tab__ int (*ble_hw_ccm_decryption)(struct ble_encryption_block *ecb, uint8_t *out, int out_len, uint8_t *in, uint8_t in_len, uint8_t aad)
= _ble_hw_ccm_decryption;
#endif

int
_ble_hw_ecc_p256_make_key(uint8_t *private_key, uint8_t *public_key)
{
#if (USE_HW_CRYPTO == 1)
    int ret = 0;
    uint32_t k[8] = {0};
	uint32_t *x;
	uint32_t *y;
	uint32_t const secp256r1_gx[8] = {0xD898C296,0xF4A13945,0x2DEB33A0,0x77037D81,
		                              0x63A440F2,0xF8BCE6E5,0xE12C4247,0x6B17D1F2};
	uint32_t const secp256r1_gy[8] = {0x37BF51F5,0xCBB64068,0x6B315ECE,0x2BCE3357,
		                              0x7C0F9E16,0x8EE7EB4A,0xFE1A7F9B,0x4FE342E2};
	struct device *pke_dev;

    /*
     * HW calculate time 42.57mesc.
     */

    do {
		ret = ble_hw_get_rng(private_key, 32);
		if (ret) {
			printk("failed trng generate : %d\n", ret);
			return ret;
		}
    } while (!private_key_valid(private_key));

	pke_dev = device_get_by_name("pke");
	if (!pke_dev) {
		printk("No pke crypto device\n");
		assert(0);
	}

    swap_copy((uint8_t *)k, private_key, 32);

	ret = crypto_eccp_point_mul(pke_dev,
			(uint32_t *)k,
			(uint32_t *)secp256r1_gx,
			(uint32_t *)secp256r1_gy,
			(uint32_t *)public_key, (uint32_t  *)(public_key + 32));


	if (ret) {
		printk("failed public key generate : \n", ret);
	}

	return ret;
#else
    uint8_t pk[64];

    /*
     * software calculate time 1.15s.
     */

    uECC_set_rng(ecc_rand);

    if (uECC_make_key(pk, private_key, &curve_secp256r1) != TC_CRYPTO_SUCCESS) {
        return -1;
    }

    swap_copy(public_key, pk, 32);
    swap_copy(&public_key[32], &pk[32], 32);

    return 0;
#endif
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_ecc_p256_make_key, &ble_hw_ecc_p256_make_key, &_ble_hw_ecc_p256_make_key);
#else
__func_tab__ int (*ble_hw_ecc_p256_make_key)(uint8_t *private_key, uint8_t *public_key)
= _ble_hw_ecc_p256_make_key;
#endif

int
_ble_hw_ecc_p256_make_dhkey(uint8_t *private_key, uint8_t *public_key, uint8_t *dhkey)
{
#if (USE_HW_CRYPTO == 1)
    int ret = 0;
    uint32_t k[8];
    uint32_t px[8];
    uint32_t py[8];
	struct device *pke_dev;

    /*
     * HW calculate time 42.51mesc.
     */

	pke_dev = device_get_by_name("pke");
	if (!pke_dev) {
		printk("No pke crypto device\n");
		assert(0);
	}

    memcpy((uint8_t *)px, public_key, 32);
    memcpy((uint8_t *)py, &public_key[32], 32);

	ret = crypto_eccp_point_verify(pke_dev, px, py);
	if (ret) {
		printk("failed public key verify : %d\n", ret);
		return ret;
	}

    swap_copy((uint8_t *)k, private_key, 32);

	ret = crypto_eccp_point_mul(pke_dev, k,
			(uint32_t *)px,
			(uint32_t *)py,
			(uint32_t *)px,
			(uint32_t *)py);
    if (ret) {
        printk("failed dhkey generation : %d\n", ret);
        return ret;
    } else {
		memcpy(dhkey, (uint8_t *)px, 32);
	}

    return ret;
#else
    uint8_t pk[64];
    uint8_t dh[32];

    swap_copy(pk, public_key, 32);
    swap_copy(&pk[32], &public_key[32], 32);

    /*
     * software calculate time 1.15s.
     * if supervision timeout is less than this time,
     * may be happened connection timeout
     */

    if (uECC_valid_public_key(pk, &curve_secp256r1) < 0) {
        return -1;
    }

    if (uECC_shared_secret(pk, private_key, dh, &curve_secp256r1) != TC_CRYPTO_SUCCESS) {
        return -1;
    }

    swap_copy(dhkey, dh, 32);

    return 0;
#endif
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_ecc_p256_make_dhkey, &ble_hw_ecc_p256_make_dhkey, &_ble_hw_ecc_p256_make_dhkey);
#else
__func_tab__ int (*ble_hw_ecc_p256_make_dhkey)(uint8_t *private_key, uint8_t *public_key, uint8_t *dhkey)
= _ble_hw_ecc_p256_make_dhkey;
#endif


/**
 * Random number generator ISR.
 */
static void
ble_rng_isr(void)
{
}

/**
 * Initialize the random number generator
 *
 * @param cb
 * @param bias
 *
 * @return int
 */
int
_ble_hw_rng_init(ble_rng_isr_cb_t cb, int bias)
{
	uint32_t seed;

    g_ble_rng_isr_cb = cb;

    (void)ble_rng_isr;


    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_rng_init, &ble_hw_rng_init, &_ble_hw_rng_init);
#else
__func_tab__ int (*ble_hw_rng_init)(ble_rng_isr_cb_t cb, int bias)
= _ble_hw_rng_init;
#endif

/**
 * Start the random number generator
 *
 * @return int
 */
int
_ble_hw_rng_start(void)
{
	int ret;

    g_ble_rng_started = true;

    if (g_ble_rng_isr_cb) {
        while (g_ble_rng_started) {
#if (USE_HW_CRYPTO == 1)
            uint8_t val;

			ret = ble_hw_get_rng(&val, 1);
			if (ret) {
				assert(0);
				return ret;
			}

            g_ble_rng_isr_cb(val);
#else
            g_ble_rng_isr_cb(rand());
#endif
        }
    }

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_rng_start, &ble_hw_rng_start, &_ble_hw_rng_start);
#else
__func_tab__ int (*ble_hw_rng_start)(void)
= _ble_hw_rng_start;
#endif

/**
 * Stop the random generator
 *
 * @return int
 */
int
_ble_hw_rng_stop(void)
{
    g_ble_rng_started = false;

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_rng_stop, &ble_hw_rng_stop, &_ble_hw_rng_stop);
#else
__func_tab__ int (*ble_hw_rng_stop)(void)
= _ble_hw_rng_stop;
#endif

/**
 * Read the random number generator.
 *
 * @return uint8_t
 */
uint8_t
_ble_hw_rng_read(void)
{
    uint8_t rng = (uint8_t)(rand() & 0xFF);

    return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_rng_read, &ble_hw_rng_read, &_ble_hw_rng_read);
#else
__func_tab__ uint8_t (*ble_hw_rng_read)(void)
= _ble_hw_rng_read;
#endif

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
/**
 * Clear the resolving list
 *
 * @return int
 */
void
_ble_hw_resolv_list_clear(void)
{
    g_sw_num_irks = 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_resolv_list_clear, &ble_hw_resolv_list_clear, &_ble_hw_resolv_list_clear);
#else
__func_tab__ void (*ble_hw_resolv_list_clear)(void)
= _ble_hw_resolv_list_clear;
#endif

/**
 * Add a device to the hw resolving list
 *
 * @param irk   Pointer to IRK to add
 *
 * @return int 0: success, BLE error code otherwise
 */
int
_ble_hw_resolv_list_add(uint8_t *irk)
{
    uint32_t *sw_entry;

    /* Find first unused device address match element */
    if (g_sw_num_irks == SW_IRK_LIST_ENTRIES) {
        return BLE_ERR_MEM_CAPACITY;
    }

    /* Copy into irk list */
    sw_entry = &g_sw_irk_list[4 * g_sw_num_irks];
    memcpy(sw_entry, irk, 16);

    /* Add to total */
    ++g_sw_num_irks;
    return BLE_ERR_SUCCESS;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_resolv_list_add, &ble_hw_resolv_list_add, &_ble_hw_resolv_list_add);
#else
__func_tab__ int (*ble_hw_resolv_list_add)(uint8_t *irk)
= _ble_hw_resolv_list_add;
#endif

/**
 * Remove a device from the hw resolving list
 *
 * @param index Index of IRK to remove
 */
void
_ble_hw_resolv_list_rmv(int index)
{
    uint32_t *irk_entry;

    if (index < g_sw_num_irks) {
        --g_sw_num_irks;
        irk_entry = &g_sw_irk_list[index];
        if (g_sw_num_irks > index) {
            memmove(irk_entry, irk_entry + 4, 16 * (g_sw_num_irks - index));
        }
    }
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_resolv_list_rmv, &ble_hw_resolv_list_rmv, &_ble_hw_resolv_list_rmv);
#else
__func_tab__ void (*ble_hw_resolv_list_rmv)(int index)
= _ble_hw_resolv_list_rmv;
#endif

/**
 * Returns the size of the resolving list. NOTE: this returns the maximum
 * allowable entries in the HW. Configuration options may limit this.
 *
 * @return int Number of devices allowed in resolving list
 */
uint8_t
_ble_hw_resolv_list_size(void)
{
    return BLE_HW_RESOLV_LIST_SIZE;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_resolv_list_size, &ble_hw_resolv_list_size, &_ble_hw_resolv_list_size);
#else
__func_tab__ uint8_t (*ble_hw_resolv_list_size)(void)
= _ble_hw_resolv_list_size;
#endif

/**
 * Called to determine if the address received was resolved.
 *
 * @return int  Negative values indicate unresolved address; positive values
 *              indicate index in resolving list of resolved address.
 */
__ilm_ble__ int
_ble_hw_resolv_list_match(uint8_t *addr)
{
    uint32_t index;
    uint8_t aes_key[16];
    uint8_t aes_in[16];
    uint8_t aes_out[16];

    if (ble_phy_resolv_list_enabled() == 0) {
        return -1;
    }

    memset(aes_in, 0, 16);
    aes_in[13] = addr[5];
    aes_in[14] = addr[4];
    aes_in[15] = addr[3];

    for (index = 0; index < g_sw_num_irks; index++) {
        /* try resolving for each irks against the address */

        uint8_t *irk = (uint8_t *)(&g_sw_irk_list[index * 4]);

#if (USE_HW_CRYPTO == 1)
        ske_config_aes128_encrypt();

        /* set key */
        ske_set_cfg(1);
        ske_set_key((uint32_t *)irk);
        ske_start_and_wait();
        ske_set_cfg(0);

        /* set input */
        ske_set_input((uint32_t *)aes_in);

        /* start for data */
        ske_start_and_wait();

        /* get output */
        ske_get_output((uint32_t *)aes_out);
#else
        struct tc_aes_key_sched_struct s;

        if (tc_aes128_set_encrypt_key(&s, aes_key) == TC_CRYPTO_FAIL) {
            return -1;
        }
        if (tc_aes_encrypt(aes_out, aes_in, &s) == TC_CRYPTO_FAIL) {
            return -1;
        }
#endif

        if (aes_out[13] == addr[2] &&
            aes_out[14] == addr[1] &&
            aes_out[15] == addr[0]) {

            /* RPA matched and return index of the irk */
            return index;
        }
    }

    return -1;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_hw_resolv_list_match, &ble_hw_resolv_list_match, &_ble_hw_resolv_list_match);
#else
__func_tab__ int (*ble_hw_resolv_list_match)(uint8_t *addr)
= _ble_hw_resolv_list_match;
#endif

#endif
