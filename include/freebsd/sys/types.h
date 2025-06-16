/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __WISE_SYS_TYPES_H__
#define __WISE_SYS_TYPES_H__

#include_next <sys/types.h>

/* XXX: always needed to build against wise */
#ifdef __WISE__ /*ndef __USE_NATIVE_HEADER__*/

#ifndef __BIT_TYPES_DEFINED__
/* NDS32 does not defines the following types */
typedef __uint8_t u_int8_t;
typedef __uint16_t u_int16_t;
typedef __uint32_t u_int32_t;
typedef __uint64_t u_int64_t;

#endif /* __BIT_TYPES_DEFINED__ */

#include <freebsd/byteorder.h>

#ifndef BYTE_ORDER
#define BYTE_ORDER 	__BYTE_ORDER__
#define LITTLE_ENDIAN 	__ORDER_LITTLE_ENDIAN__
#define BIG_ENDIAN	__ORDER_BIG_ENDIAN__
#endif

#endif /* __USE_NATIVE_HEADER__ */

#endif /* __WISE_SYS_TYPES_H__ */
