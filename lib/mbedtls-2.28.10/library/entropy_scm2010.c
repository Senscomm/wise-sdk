/**
 * \file entropy_scm2010.h
 *
 * \brief Hardware entropy collector implementation
 */
/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
#include <hal/device.h>
#include <hal/crypto.h>
#include <hal/rom.h>
#include "mbedtls/entropy.h"
#include "mbedtls/entropy_poll.h"

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(mbedtls_hardware_poll, &mbedtls_hardware_poll, &_mbedtls_hardware_poll);
#else
__func_tab__ int (*mbedtls_hardware_poll)( void *data,
	unsigned char *output, size_t len, size_t *olen ) = _mbedtls_hardware_poll;
#endif

__iram__ int _mbedtls_hardware_poll( void *data,
                           unsigned char *output, size_t len, size_t *olen )
{
	uint32_t ret;
	struct device *trng_dev = device_get_by_name("trng");

	if (trng_dev == NULL) {
		return( MBEDTLS_ERR_ENTROPY_SOURCE_FAILED );
	}

    if (olen)
	    *olen = 0;

	ret = crypto_trng_get_rand(trng_dev, output, len);
	if (ret != 0)
		return( MBEDTLS_ERR_ENTROPY_SOURCE_FAILED );

    if (olen)
	    *olen = len;

	return( 0 );
}
#endif
