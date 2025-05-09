/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>

#include "hal/io.h"
#include "mmap.h"
#include "flash_crypto_priv.h"

#define SKE_CTRL_OFFSET             0x00
#define SKE_CFG_OFFSET              0x04
#define SKE_SR1_OFFSET              0x08
#define SKE_SR2_OFFSET              0x0C
#define SKE_KEY_OFFSET              0x10
#define SKE_IV_OFFSET               0x70
#define SKE_DIN_OFFSET              0x90
#define SKE_DOUT_OFFSET             0xB0

#define CRYPTO_CIPHER_EN            (1 << 31)

static void ske_config(void)
{
    uint32_t cfg;

    cfg = 6 << 28 |
          1 << 0;

    writel(cfg, SKE_BASE_ADDR + SKE_CFG_OFFSET);
}

static void ske_set_cfg(uint8_t en)
{
    uint32_t cfg;

    if (en) {
        cfg = readl(SKE_BASE_ADDR + SKE_CFG_OFFSET);
        cfg |= 1 << 12;
        writel(cfg, SKE_BASE_ADDR + SKE_CFG_OFFSET);
    } else {
        cfg = readl(SKE_BASE_ADDR + SKE_CFG_OFFSET);
        cfg &= ~(1 << 12);
        writel(cfg, SKE_BASE_ADDR + SKE_CFG_OFFSET);
    }
}

static void ske_start_and_wait(void)
{
    uint32_t ctr;

    while ((readl(SKE_BASE_ADDR + SKE_SR1_OFFSET) & 1) != 0);

    ctr = readl(SKE_BASE_ADDR + SKE_CTRL_OFFSET);
    ctr |= 1;
    writel(ctr, SKE_BASE_ADDR + SKE_CTRL_OFFSET);

    while ((readl(SKE_BASE_ADDR + SKE_SR2_OFFSET) & 1) != 1);

    writel(0, SKE_BASE_ADDR + SKE_SR2_OFFSET);
}

static void ske_set_data(uint32_t *data, uint32_t offset)
{
    int i;

    for (i = 3; i >= 0; i--) {
        writel(data[i], SKE_BASE_ADDR + offset + (i * 4));
    }
}

static void ske_get_data(uint32_t *data, uint32_t offset)
{
    int i;

    for (i = 3; i >= 0; i--) {
        data[i] = readl(SKE_BASE_ADDR + offset + (i * 4));
    }
}

void flash_cyprot_init(struct flash_crypto *crypto)
{
    uint32_t crypto_cfg = readl(SYS(CRYPTO_CFG));

    if (crypto_cfg & CRYPTO_CIPHER_EN) {
        uint32_t reg_iv[4];

        crypto->enable = 1;
        crypto->user_disable = 0;

        crypto->key[3] = readl(SYS(KEY_CFG(0)));
        crypto->key[2] = readl(SYS(KEY_CFG(1)));
        crypto->key[1] = readl(SYS(KEY_CFG(2)));
        crypto->key[0] = readl(SYS(KEY_CFG(3)));

        reg_iv[0] = readl(SYS(IV_CFG(0)));
        reg_iv[1] = readl(SYS(IV_CFG(1)));
        reg_iv[2] = readl(SYS(IV_CFG(2)));
        reg_iv[3] = readl(SYS(IV_CFG(3)));

        crypto->iv[0] = (reg_iv[3] & 0x0f) << 28;
        crypto->iv[0] |= (reg_iv[2] >> 4);
        crypto->iv[1] = (reg_iv[2] & 0x0f) << 28;
        crypto->iv[1] |= (reg_iv[1] >> 4);
        crypto->iv[2] = (reg_iv[1] & 0x0f) << 28;
        crypto->iv[2] |= (reg_iv[0] >> 4);
        crypto->iv[3] = (reg_iv[0] & 0x0f) << 28;
    } else {
        crypto->enable = 0;
    }
}

void flash_crypto_data_encrypt(struct flash_crypto *crypto, uint32_t offset, uint8_t *in, uint8_t *out, uint32_t size)
{
    uint32_t *counter;
    uint32_t data_offset = 0;
    uint32_t tmp[4];
    int enc_size;

    counter = &crypto->iv[3];

    /* configure AES128-CTR */
    ske_config();

    /* set key */
    ske_set_cfg(1);
    ske_set_data(crypto->key, SKE_KEY_OFFSET);
    ske_start_and_wait();
    ske_set_cfg(0);

    if (offset % 16) {
        *counter &= 0xf0000000;
        *counter |= (offset >> 4);

        enc_size = min(offset % 16, size);

        /* set iv */
        ske_set_cfg(1);
        ske_set_data((uint32_t *)crypto->iv, SKE_IV_OFFSET);
        ske_start_and_wait();
        ske_set_cfg(0);

        /* encryption */
        memcpy(tmp, in + data_offset, enc_size);
        ske_set_data(tmp, SKE_DIN_OFFSET);
        ske_start_and_wait();
        ske_get_data(tmp, SKE_DOUT_OFFSET);
        memcpy(out + data_offset, tmp, enc_size);

        data_offset += enc_size;
        offset += enc_size;
        size -= enc_size;
    }

    while (size > 0) {
        *counter &= 0xf0000000;
        *counter |= (offset >> 4);

        enc_size = min(size, 16);

        /* set iv */
        ske_set_cfg(1);
        ske_set_data(crypto->iv, SKE_IV_OFFSET);
        ske_start_and_wait();
        ske_set_cfg(0);

        /* encryption */
        memcpy(tmp, in + data_offset, enc_size);
        ske_set_data(tmp, SKE_DIN_OFFSET);
        ske_start_and_wait();
        ske_get_data(tmp, SKE_DOUT_OFFSET);
        memcpy(out + data_offset, tmp, enc_size);

        data_offset += enc_size;
        offset += enc_size;
        size -= enc_size;
    }
}
