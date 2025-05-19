/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __SCM_CRYPTO_H__
#define __SCM_CRYPTO_H__

#include <stdint.h>

#include "wise_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read TRNG value
 *
 * @param[in] val trng read buffer
 * @param[in] len len of trng buffer
 */
int scm_crypto_trng_read(uint8_t *val, int len);

#ifdef __cplusplus
}
#endif

#endif //__SCM_CRYPTO_H__
