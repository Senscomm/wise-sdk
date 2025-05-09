/*
 * Copyright (c) 2018-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __SCM2020_SHMEM_H__
#define __SCM2020_SHMEM_H__

struct txdesc {
	u32 ptable_l; /* pointer to MPDU table (lower 32 bits) */
	u32 ptable_h; /* pointer to MPDU table (higher 32 bits) */
	union {
		struct {
			bf( 0,  5, num);
			bf( 6,  7, rsvd);
			bf( 8,  8, ok);
			bf( 9,  9, nak);
			bf(10, 13, fail);
			bf(14, 14, tb);
			bf(15, 15, uora);
			bf(16, 16, cw);
			bf(17, 31, rsvd1);
		};
		u32 word;
	};
	u32 htc; /* HT Control field value for the received control response frame */
};

struct rxdesc {
	u32 pbuffer_l; /* pointer to a rx buffer (lower 32 bits) */
	u32 pbuffer_h; /* pointer to a rx buffer (higher 32 bits) */
	union {
		struct {
			bf( 0, 15, size);
			bf(16, 31, rsvd);
		};
		u32 word1;
	};
	union {
		struct {
			bf( 0, 15, len);
			bf(16, 17, frag);
			bf(18, 31, rsvd1);
		};
		u32 word2;
	};
};

struct scm_mac_shmem_map {
	struct txdesc tx[10];
	struct rxdesc rx[128];
};

#endif /* __SCM2020_SHMEM_H__ */
