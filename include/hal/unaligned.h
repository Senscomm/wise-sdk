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
#ifndef __UNALIGNED_H__
#define __UNALIGNED_H__

#include <hal/types.h>
#include <hal/compiler.h>

/*
 * LE: LSB at lower byte
 * BE: LSB at higher byte
 *
 * uint32_t x = 0x12345678;
 * uint8_t *p = (uint8_t) &x;
 *
 *       p[0]   p[1]   p[2]   p[3]
 *  LE   0x78   0x56   0x34   0x12
 *  BE   0x12   0x34   0x56   0x78
 *
 * Conversely,
 * const u8 buf[] = "\x12\x34\x56\x78" __attribute__((aligned(4)));
 * uint32_t val = *(uint32_t)buf;
 *
 * val  [31:24] [23:16] [15: 8] [ 7: 0]
 * LE:   0x78    0x56    0x34    0x12   (=0x78563412)
 * BE:   0x12    0x34    0x56    0x78   (=0x12345678)
 *
 * More generally,
 *
 * val  [31:24] [23:16] [15: 8] [ 7: 0]
 * LE:   b[3]    b[2]    b[1]    b[0]
 * BE:   b[0]    b[1]    b[2]    b[3]
 */

static const u32 pattern = 0x12345678;

static __inline__ char arch_endianness(void)
{
	u8 *p = (u8 *) &pattern;

	if (p[0] == 0x12)
		return 'B'; /* big endian */
	else
		return 'L'; /* little endian */
}

static __inline__ int arch_is_big_endian(void)
{
	u8 *p = (u8 *) &pattern;

	return p[0] == 0x12;
}

static __inline__ u32 get_unaligned_le32(const void *ptr)
{
	const u8 *x = ptr;

	return (x[3] << 24 | x[2] << 16 | x[1] << 8 | x[0]);
}

static __inline__ u64 get_unaligned_le64(void *p)
{
	u8 *ptr = p;
	u32 v[2];
	v[0] = ptr[0] | ptr[1] << 8 | ptr[2] << 16 | ptr[3] << 24;
	v[1] = ptr[4] | ptr[5] << 8 | ptr[6] << 16 | ptr[7] << 24;
	return v[0] | (u64) v[1] << 32;
}

static __inline__ u16 get_unaligned_le16(const void *ptr)
{
	const u8 *x = ptr;

	return (x[1] << 8 | x[0]);
}

static __inline__ void put_unaligned_le32(u32 val, void *ptr)
{
	u8 *x = ptr;

	x[0] = val & 0xff;
	x[1] = (val >> 8) & 0xff;
	x[2] = (val >> 16) & 0xff;
	x[3] = (val >> 24) & 0xff;
}

static __inline__ void put_unaligned_le16(u16 val, void *ptr)
{
	u8 *x = ptr;

	x[0] = val & 0xff;
	x[1] = (val >> 8) & 0xff;
}

static __inline__ u32 get_unaligned_be32(const void *ptr)
{
	const u8 *x = ptr;

	return (x[0] << 24 | x[1] << 16 | x[2] << 8 | x[3]);
}

static __inline__ u16 get_unaligned_be16(const void *ptr)
{
	const u8 *x = ptr;
	return (x[0] << 8 | x[1]);
}

static __inline__ void put_unaligned_be32(u32 val, void *ptr)
{
	u8 *x = ptr;

	x[3] = val & 0xff;
	x[2] = (val >> 8) & 0xff;
	x[1] = (val >> 16) & 0xff;
	x[0] = (val >> 24) & 0xff;
}

static __inline__ void put_unaligned_be16(u16 val, void *ptr)
{
	u8 *x = ptr;

	x[1] = val & 0xff;
	x[0] = (val >> 8) & 0xff;
}

#endif /* __UNALIGNED_H__ */
