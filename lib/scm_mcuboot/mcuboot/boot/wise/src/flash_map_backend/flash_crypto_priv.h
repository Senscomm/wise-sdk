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

#ifndef _FLASH_CRYPTO_PRIVE_H_
#define _FLASH_CRYPTO_PRIVE_H_

#include <stdint.h>

struct flash_crypto {
    uint8_t enable;
    uint8_t user_disable;
    uint32_t key[4];
    uint32_t iv[4];
};

void flash_cyprot_init(struct flash_crypto *crypto);

void flash_crypto_data_encrypt(struct flash_crypto *crypto, uint32_t offset, uint8_t *in, uint8_t *out, uint32_t size);

#endif //_FLASH_CRYPTO_PRIVE_H_
