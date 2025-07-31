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

#ifndef _SYSFLASH_H_
#define _SYSFLASH_H_

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PRIMARY_ID      0
#define SECONDARY_ID    1
#define SCRATCH_ID      2

#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ?        \
                                         PRIMARY_ID :       \
                                         PRIMARY_ID)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ?        \
                                         SECONDARY_ID :     \
                                         SECONDARY_ID)
#define FLASH_AREA_IMAGE_SCRATCH       SCRATCH_ID

#ifdef __cplusplus
}
#endif
#endif /* _SYSFLASH_H_ */
