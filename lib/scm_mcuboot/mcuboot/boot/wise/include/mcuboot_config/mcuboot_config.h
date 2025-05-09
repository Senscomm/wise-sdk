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

#ifndef _MCUBOOT_CONFIG_H_
#define _MCUBOOT_CONFIG_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/


#ifdef CONFIG_SCM_MCUBOOT_WATCHDOG
#  include "watchdog/watchdog.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Signature types
 *
 * You must choose exactly one signature type.
 */

/* Uncomment for RSA signature support */

/* #define MCUBOOT_SIGN_RSA */

/* Uncomment for ECDSA signatures using curve P-256. */

/* #define MCUBOOT_SIGN_EC256 */

#ifdef CONFIG_SCM_MCUBOOT_SIGN_AUTH_TYPE_RSA_2048
#  error "Not yet implement"
#elif defined(CONFIG_SCM_MCUBOOT_SIGN_AUTH_TYPE_RSA_3072)
#  error "Not yet implement"
#elif defined(CONFIG_SCM_MCUBOOT_SIGN_AUTH_TYPE_ECDSA_P256)
#  define MCUBOOT_SIGN_EC256
#elif defined(CONFIG_SCM_MCUBOOT_SIGN_AUTH_TYPE_ED25519)
#  error "Not yet implement"
#endif

/* Upgrade mode
 *
 * The default is to support A/B image swapping with rollback.  Other modes
 * with simpler code path, which only supports overwriting the existing image
 * with the update image or running the newest image directly from its flash
 * partition, are also available.
 *
 * You can enable only one mode at a time from the list below to override
 * the default upgrade mode.
 */

/* Enable the overwrite-only code path. */

#ifdef CONFIG_SCM_MCUBOOT_OVERWRITE_ONLY
#  define MCUBOOT_OVERWRITE_ONLY
#endif

/* Only erase and overwrite those primary slot sectors needed
 * to install the new image, rather than the entire image slot.
 */

#ifdef CONFIG_SCM_MCUBOOT_OVERWRITE_ONLY_FAST
#  define MCUBOOT_OVERWRITE_ONLY_FAST
#endif

/* Enable the direct-xip code path. */

#ifdef CONFIG_SCM_MCUBOOT_DIRECT_XIP
#  define MCUBOOT_DIRECT_XIP
#endif

/* Enable the revert mechanism in direct-xip mode. */

#ifdef CONFIG_SCM_MCUBOOT_DIRECT_XIP_REVERT
#  define MCUBOOT_DIRECT_XIP_REVERT
#endif

/* Enable the ram-load code path. */

#ifdef CONFIG_SCM_MCUBOOT_RAM_LOAD
#  define MCUBOOT_RAM_LOAD
#endif

/* Enable bootstrapping the erased primary slot from the secondary slot */

#ifdef CONFIG_SCM_MCUBOOT_BOOTSTRAP
#  define MCUBOOT_BOOTSTRAP
#endif

/* Cryptographic settings
 *
 * You must choose between mbedTLS and Tinycrypt as source of
 * cryptographic primitives. Other cryptographic settings are also
 * available.
 */

#ifdef CONFIG_SCM_MCUBOOT_USE_MBED_TLS
#  define MCUBOOT_USE_MBED_TLS
#endif

#ifdef CONFIG_SCM_MCUBOOT_USE_TINYCRYPT
#  define MCUBOOT_USE_TINYCRYPT
#endif

/* Always check the signature of the image in the primary slot before
 * booting, even if no upgrade was performed. This is recommended if the boot
 * time penalty is acceptable.
 */

#define MCUBOOT_VALIDATE_PRIMARY_SLOT

/* Flash abstraction */

/* Uncomment if your flash map API supports flash_area_get_sectors().
 * See the flash APIs for more details.
 */

#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS

/* Default maximum number of flash sectors per image slot; change
 * as desirable.
 */

#define MCUBOOT_MAX_IMG_SECTORS 512

/* Default number of separately updateable images; change in case of
 * multiple images.
 */

#define MCUBOOT_IMAGE_NUMBER 1

/* Logging */

/* If logging is enabled the following functions must be defined by the
 * platform:
 *
 *    MCUBOOT_LOG_MODULE_REGISTER(domain)
 *      Register a new log module and add the current C file to it.
 *
 *    MCUBOOT_LOG_MODULE_DECLARE(domain)
 *      Add the current C file to an existing log module.
 *
 *    MCUBOOT_LOG_ERR(...)
 *    MCUBOOT_LOG_WRN(...)
 *    MCUBOOT_LOG_INF(...)
 *    MCUBOOT_LOG_DBG(...)
 *
 * The function priority is:
 *
 *    MCUBOOT_LOG_ERR > MCUBOOT_LOG_WRN > MCUBOOT_LOG_INF > MCUBOOT_LOG_DBG
 */

#ifdef CONFIG_SCM_MCUBOOT_ENABLE_LOGGING
#  define MCUBOOT_HAVE_LOGGING
#endif

/* Assertions */

/* Uncomment if your platform has its own mcuboot_config/mcuboot_assert.h.
 * If so, it must provide an ASSERT macro for use by bootutil. Otherwise,
 * "assert" is used.
 */

#define MCUBOOT_HAVE_ASSERT_H 1

/* Watchdog feeding */

/* This macro might be implemented if the OS / HW watchdog is enabled while
 * doing a swap upgrade and the time it takes for a swapping is long enough
 * to cause an unwanted reset. If implementing this, the OS main.c must also
 * enable the watchdog (if required)!
 */

#ifdef CONFIG_SCM_MCUBOOT_WATCHDOG
#  define MCUBOOT_WATCHDOG_FEED()       do                           \
                                          {                          \
                                            mcuboot_watchdog_feed(); \
                                          }                          \
                                        while (0)

#else
#  define MCUBOOT_WATCHDOG_FEED()       do                           \
                                          {                          \
                                          }                          \
                                        while (0)
#endif

#endif /* _MCUBOOT_CONFIG_H_ */
