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
#ifndef __IRQD_H__
#define __IRQD_H__

#define IRQD_SW_INT	(1)

/* Struct to carry information associated with each interrupt. */

typedef struct sdvt_interrupt_s
{
	u16 irq_status;		/* Only one bit to be set */
    u16 cmd_info; 		/* CMD_INFO */
    u16 data_blk_sz; 	/* DAT_BLOCK_SIZE */
    u16 data_blk_cnt; 	/* DAT_BLOCK_CNT */
}   sdvt_interrupt_t;

typedef struct sdvt_interrupt_fifo_s
{
	int prod_idx;
	int cons_idx;
	sdvt_interrupt_t *ielems; /* Size should be 2^8, 2^16. etc. */
}	sdvt_interrupt_fifo_t;

typedef struct sdvt_txelem_s
{
	u16 len;
	u32 addr; /* Address of TX element */
} sdvt_txelem_t;

#define SDVT_MAX_TXCHAIN_MAXSZ 64
typedef struct sdvt_tx_chain_s
{
	u8 fn;
	u8 cnt;				/* Total count of txelems */
	u8 cons_idx;		/* TX consumed index */
	u32 dma_addr;		/* The FBAR DMA Address for this fn */
	sdvt_txelem_t txelems[SDVT_MAX_TXCHAIN_MAXSZ];
} sdvt_tx_chain_t;
#endif
