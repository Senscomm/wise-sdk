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
#ifndef __HEAD_H__
#define __HEAD_H__

/* SCM2010 flash boot record */
typedef struct {
	unsigned magic;
	unsigned hcrc32;
	unsigned ver;

	unsigned size;
	unsigned lma;
	unsigned vma;
	unsigned dcrc32;

	unsigned char os;
	unsigned char arch;
	unsigned char type; /* 0 : bootable, 1 : non-bootable */
	unsigned char comp;

	/* secure boot */
	unsigned char signature[64]; /* [32:95] */
	unsigned char pk[64]; /* [96:159] */
} boot_hdr_t;

typedef union {
	boot_hdr_t bhdr;
	char buffer[160];
} img_hdr_t;

#define uswap_32(x) \
	((((x) & 0xff000000) >> 24) | \
	 (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | \
	 (((x) & 0x000000ff) << 24))

#endif /* __HEAD_H__ */
