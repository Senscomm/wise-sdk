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

#ifndef __EFUSEMAP_H__
#define __EFUSEMAP_H__

#include <bitfield.h>

#include <hal/kernel.h>
#include <hal/device.h>


/*
 * scm2010 eFuse map
 *
 */

typedef struct _efusemap {
    /* R00 - R03 */
    u32 ROOT_KEY[4];
    /* R4 */
    bf(0,  0, PARITY);
    bf(1,  1, HARD_KEY);
    bf(2,  2, FLASH_PROT);
    bf(3,  3, SECURE_BOOT);
    bf(4, 31, RSVD1);
    /* R05 - R08 */
    u32 CUST_ID[4];
    /* R09 - R12 */
    u32 CHIP_ID[4];
    /* R13 - R16*/
    u32 PK_HASH[4];
    /* R17 - R31 */
    u32 RSVD[15];
} efusemap;

#endif
