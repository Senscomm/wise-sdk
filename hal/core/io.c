/*
 * Copyright (c) 2020-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <hal/compiler.h>
#include <hal/kernel.h>
#include <hal/io.h>

__weak void memset_io(void __iomem *to, int c, size_t len)
{
	size_t size = len;
	int pos = 0;

	if (PTR_IS_ALIGNED(to, 4)) {

		while (size >= 4) {
			writel(c, to);
			to += 4;
			size -= 4;
		}
	}

	if (size >= 2 || PTR_IS_ALIGNED(to, 2)) {
		pos = 0;
		while (size >= 2) {
			u16 val16;

			val16 = (c >> (pos * 8)) & 0xffff;
			writew(val16, to);

			to += 2;
			size -= 2;
			pos ^= 2;
		}
	}

	while (size > 0) {
		u8 val8;

		val8 = (c >> (pos * 8)) & 0xff;
		writeb(val8, to);
		to += 1;
		size -= 1;
	}
}

__weak void memcpy_toio(void __iomem *to, const void *from, size_t len)
{
	size_t size = len;

	if (PTR_IS_ALIGNED(from, 4) && PTR_IS_ALIGNED(to, 4)) {

		while (size >= 4) {
			u32 *src4 = (u32 *) from;

			writel(*src4, to);

			from += 4;
			to += 4;
			size -= 4;
		}
	}

	if (size >= 2 || PTR_IS_ALIGNED(from, 2) || PTR_IS_ALIGNED(to, 2)) {

		while (size >= 2) {
			u16 *src2 = (u16 *) from;

			writew(*src2, to);
			from += 2;
			to += 2;
			size -= 2;
		}
	}

	while (size > 0) {
		u8 *src1 = (u8 *) from;

		writeb(*src1, to);
		from += 1;
		to += 1;
		size -= 1;
	}
}

__weak void memcpy_fromio(void * to, void __iomem *from, size_t len)
{
	size_t size = len;

	if (PTR_IS_ALIGNED(from, 4) && PTR_IS_ALIGNED(to, 4)) {

		while (size >= 4) {
			u32 *dst4 = (u32 *) to;

			*dst4 = readl(from);
			from += 4;
			to += 4;
			size -= 4;
		}
	}

	if (size >= 2 || PTR_IS_ALIGNED(from, 2) || PTR_IS_ALIGNED(to, 2)) {

		while (size >= 2) {
			u16 *dst2 = (u16 *) to;

			*dst2 = readw(from);
			from += 2;
			to += 2;
			size -= 2;
		}
	}

	while (size > 0) {
		u8 *dst1 = (u8 *) to;

		*dst1 = readb(from);
		from += 1;
		to += 1;
		size -= 1;
	}
}
