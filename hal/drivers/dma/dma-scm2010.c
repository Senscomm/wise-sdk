/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "systm.h"  /* codespell:ignore */
#include <cmsis_os.h>
#include <hal/kernel.h>
#include <hal/kmem.h>
#include <hal/console.h>
#include <hal/dma.h>


/***************************************************
 * ATCDMAC110_DS159_V1.3 registers
 ****************************************************/

#define DMA_CH_PRIORITY_HIGH 0x1
#define DMA_CH_PRIORITY_LOW 0x0

#define DMAC_REVISION	0x0
#define DMAC_CFG			0x10
#define DMAC_SW_RESET	0x20
#define DMAC_CH_ABORT	0x24
#define DMAC_INT_STATUS	0x30
#define     DMA_INTR_STATUS_TC_DONE_MASK   0xff0000
#define     DMA_INTR_STATUS_TC_DONE_SHIFT  16
#define     DMA_INTR_STATUS_ABORT_MASK     0xff00
#define     DMA_INTR_STATUS_ABORT_SHIFT    8
#define     DMA_INTR_STATUS_ERROR_MASK     0xff
#define     DMA_INTR_STATUS_ERROR_SHIFT    0

#define DMAC_CH_EN_STATUS	0x34
#define DMAC_CH_CTRL 0x40
#define DMAC_CH_N_CTRL(n) DMAC_CH_CTRL + (n*0x20)
#define DMAC_CH_TRAN_SIZE	0x44
#define DMAC_CH_N_TRAN_SIZE(n)	DMAC_CH_TRAN_SIZE + (n*0x20)
#define DMAC_CH_SRC_ADDR_L	0x48
#define DMAC_CH_SRC_N_ADDR_L(n)	DMAC_CH_SRC_ADDR_L + (n*0x20)
#define DMAC_CH_SRC_ADDR_H	0x4c
#define DMAC_CH_SRC_N_ADDR_H(n)	DMAC_CH_SRC_ADDR_H + (n*0x20)
#define DMAC_CH_DST_ADDR_L	0x50
#define DMAC_CH_DST_N_ADDR_L(n)	DMAC_CH_DST_ADDR_L + (n*0x20)
#define DMAC_CH_DST_ADDR_H	0x54
#define DMAC_CH_DST_N_ADDR_H(n)	DMAC_CH_DST_ADDR_H + (n*0x20)
#define DMAC_CH_LL_ADDR_L	0x58
#define DMAC_CH_N_LL_ADDR_L(n)	DMAC_CH_LL_ADDR_L + (n*0x20)
#define DMAC_CH_LL_ADDR_H	0x5c
#define DMAC_CH_N_LL_ADDR_H(n)	DMAC_CH_LL_ADDR_H + (n*0x20)

/***************************************************
 * SCM2010 DMA private definitions
 ****************************************************/


#define DMAC_NUM CONFIG_SCM2010_DMAC_NUM
#define DMA_MAX_CH_NUM 8
#define DMAC0_CH_NUM CONFIG_SCM2010_DMA0_CH_NUM
#define DMA_DESC_NUM CONFIG_SCM2010_DMA_DESC_NUM

#ifndef CONFIG_SCM2010_DMA1_CH_NUM
#define CONFIG_SCM2010_DMA1_CH_NUM 8
#endif
#define DMAC1_CH_NUM CONFIG_SCM2010_DMA1_CH_NUM

#if DMAC0_CH_NUM > 8
    #error  message ( "dma channels must <= 8" )
#endif

#if DMAC1_CH_NUM > 8
    #error  message ( "dma channels must <= 8" )
#endif


#define DMA_NORMAL_MODE 0x0
#define DMA_HANDSHAKE_MODE 0x1

#define CH_IDLE	0
#define CH_BUSY	1

#define DMA_CH_N_IDLE(dmac, ch_no) (((dmac->ch_status) & BIT(ch_no)) == CH_IDLE)
#define SET_DMA_CH_N_BUSY(dmac, ch_no) ((dmac->ch_status) |= BIT(ch_no))
#define SET_DMA_CH_N_IDLE(dmac, ch_no) ((dmac->ch_status) &= ~BIT(ch_no))

/* register bit field */
#define RBFM(r, f) 		(r##_##f##_MASK) 	/* mask */
#define RBFS(r, f) 		(r##_##f##_SHIFT) 	/* shift */
#define RBFGET(v, r, f) 	(((v) & RBFM(r, f)) >> RBFS(r, f))

struct scm2010_dma_ch_cfg
{
	u32 enable:1;			/*Enable: 0   */
	u32 int_tc_mask:1;		/*IntTCMask: 1   */
	u32 int_err_mask:1;		/*IntErrMask: 2   */
	u32 int_abt_mask:1;		/*IntAbtMask: 3   */
	u32 dst_req_sel:4;		/*DstReqSel: 7:4   */
	u32 src_req_sel:4;		/*SrcReqSel: 11:8   */
	u32 dst_addr_ctrl:2;	/*DstAddrCtrl: 13:12   */
	u32 src_addr_ctrl:2;	/*SrcAddrCtrl: 15:14   */
	u32 dst_mode:1;			/*DstMode: 16   */
	u32 src_mode:1;			/*SrcMode: 17   */
	u32 dst_width:3;		/*DstWidth: 20:18   */
	u32 src_width:3;		/*SrcWidth: 23:21   */
	u32 src_bust_size:4;	/*SrcBurstSize: 27:24   */
	u32 reserved0:1;
	u32 priority:1;			/*priority: 29   */
	u32 reserved1:1;	/*reserved: 30   */
	u32 reserved2:1;	/*reserved: 31   */
};

struct scm2010_dma_desc
{
	struct scm2010_dma_ch_cfg dmac_cfg;
	volatile u32 tran_size;
	volatile u32 src_addr_l;
	volatile u32 src_addr_h;
	volatile u32 dst_addr_l;
	volatile u32 dst_addr_h;
	volatile u32 llptr_l;
	volatile u32 llptr_h;
} __attribute__ ((aligned (8)));

enum dma_src_brust_size_e
{
	TRANSFERx1 = 0x0,
	TRANSFERx2,
	TRANSFERx4,
	TRANSFERx8,
	TRANSFERx16,
	TRANSFERx32,
	TRANSFERx64,
	TRANSFERx128 = 0x7,
};

enum dma_src_width_e
{
	BYTE = 0x0,
	HALF_WORD,
	WORD,
	D_WORD = 0x3,
};

enum dma_addr_ctrl_e
{
	INCREMENT_ADDR = 0x0,
	DECREMENT_ADDR,
	FIX_ADDR,
};

enum scm2010_dma_ch_state_t {
	CH_STATE_DESC_CONFIG = 1,
	CH_STATE_TRANSMITTING = 2,
};

struct dma_channel
{
	struct scm2010_dma_desc *dma_desc;
	dma_done_handler cb;
	void *priv_data;
};

struct scm2010_dmac_t
{
	u32 inst;
	u8 ch_status;
    u8 ch_aborted;
    u8 ch_keep; /* Subject to reload. */
	struct dma_channel channel[DMA_MAX_CH_NUM];
	struct device *dev;
} scm2010_dmac[DMAC_NUM];

static struct scm2010_dma_ch_cfg default_dma_ch_cfg =
{
	.enable = 1,
	.int_tc_mask = 0,
	.int_err_mask = 0,
	.int_abt_mask = 0,
	.dst_req_sel = 0,
	.src_req_sel = 0,
	.dst_addr_ctrl = INCREMENT_ADDR,
	.src_addr_ctrl = INCREMENT_ADDR,
	.dst_mode = DMA_NORMAL_MODE,
	.src_mode = DMA_NORMAL_MODE,
	.dst_width = WORD,
	.src_width = WORD,
	.src_bust_size = TRANSFERx128,
	.reserved0 = 0,
	.priority = DMA_CH_PRIORITY_HIGH,
	.reserved1 = 0,
	.reserved2 = 0,
};

static struct scm2010_dma_ch_cfg default_hw_dma_ch_cfg =
{
	.enable = 1,
	.int_tc_mask = 0,
	.int_err_mask = 0,
	.int_abt_mask = 0,
	.dst_req_sel = 0,
	.src_req_sel = 0,
	.dst_addr_ctrl = INCREMENT_ADDR,
	.src_addr_ctrl = INCREMENT_ADDR,
	.dst_mode = DMA_NORMAL_MODE,
	.src_mode = DMA_NORMAL_MODE,
	.dst_width = BYTE,
	.src_width = BYTE,
	.src_bust_size = TRANSFERx1,
	.reserved0 = 0,
	.priority = DMA_CH_PRIORITY_HIGH,
	.reserved1 = 0,
	.reserved2 = 0,
};

#ifndef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
__ram_dma_desc__ struct scm2010_dma_desc scm2010_dma0_desc[DMAC0_CH_NUM][DMA_DESC_NUM] = {0};

#if DMAC_NUM > 1
__ram_dma_desc__ struct scm2010_dma_desc scm2010_dma1_desc[DMAC1_CH_NUM][DMA_DESC_NUM] = {0};
#endif
#endif

__ilm__
static void dma_write(struct device *dma, u32 val, u32 offset)
{
	writel(val, dma->base[0] + offset);
}

static u32 dma_read(struct device *dma, u32 oft)
{
	return readl(dma->base[0] + oft);
}

int scm2010_dmac_isr(int irq, void *data)
{
	struct scm2010_dmac_t *dmac = (struct scm2010_dmac_t *) data;
	struct device *dev = dmac->dev;
	u32 trmc, abrt, error;
	int ch;
	u32 int_status = dma_read(dev, DMAC_INT_STATUS);
	int dma_ch_num = dmac->inst == 0 ? (DMAC0_CH_NUM - 1): (DMAC1_CH_NUM - 1);
	dma_isr_status status;

	if (int_status == 0) {
		printk("DMA: spurious interrupt.\n");
		return 0;
	}

	dma_write(dev, int_status, DMAC_INT_STATUS);

	trmc = RBFGET(int_status, DMA_INTR_STATUS, TC_DONE);
	abrt = RBFGET(int_status, DMA_INTR_STATUS, ABORT);
	error = RBFGET(int_status, DMA_INTR_STATUS, ERROR);

	for (ch = dma_ch_num; ch >= 0; ch--) {
	    status = DMA_STATUS_NONE;
		int chk = 1 << ch;
		if (error & chk) {
			status = DMA_STATUS_ERROR;
		}
		else if (abrt & chk) {
			status = DMA_STATUS_ABORTED;
		}
		else if (trmc & chk) {
			status = DMA_STATUS_COMPLETE;
		}
		if (status != DMA_STATUS_NONE && dmac->channel[ch].cb) {
            if (status == DMA_STATUS_ABORTED) {
                dmac->ch_aborted |= BIT(ch);
            } else if (dmac->ch_aborted & BIT(ch)) {
                /* XXX: This is a spurious COMPLETED interrupt
                 *      from an aborted channel, which should be ignored
                 *      not to confuse the upper layer.
                 */
                return 0;
            }
			dmac->channel[ch].cb(dmac->channel[ch].priv_data, status);
		}
        if (status != DMA_STATUS_NONE
                && !(dmac->ch_keep & BIT(ch))) { /* Kept channel needs to be 'released' by a user. */
		    SET_DMA_CH_N_IDLE(dmac, ch);
        }
	}

	return 0;
}

__ilm__
static int scm2010_dma_kick (struct device *dma, int channel_no, struct scm2010_dma_desc *dma_desc)
{
	if (!dma)
		return -1;

	dma_write(dma, dma_desc->src_addr_l, DMAC_CH_SRC_N_ADDR_L(channel_no));
	dma_write(dma, dma_desc->src_addr_h, DMAC_CH_SRC_N_ADDR_H(channel_no));
	dma_write(dma, dma_desc->dst_addr_l, DMAC_CH_DST_N_ADDR_L(channel_no));
	dma_write(dma, dma_desc->dst_addr_h, DMAC_CH_DST_N_ADDR_H(channel_no));
	dma_write(dma, dma_desc->llptr_l, DMAC_CH_N_LL_ADDR_L(channel_no));
	dma_write(dma, dma_desc->llptr_h, DMAC_CH_N_LL_ADDR_H(channel_no));

	dma_write(dma, dma_desc->tran_size, DMAC_CH_N_TRAN_SIZE(channel_no));

	/* after this register setting DMA start to work immediately */
	dma_write(dma, *((u32 *) &dma_desc->dmac_cfg), DMAC_CH_N_CTRL(channel_no));

	return 0;
}

__ilm__
static u8 scm2010_dma_alloc_ch(struct scm2010_dmac_t *dmac)
{
	u8 max_ch_num = (dmac->inst == 0 ? DMAC0_CH_NUM : DMAC1_CH_NUM);
	u8 ch = 0xff;
	int i = 0;
    u32 flags;

    local_irq_save(flags);

	for (i = 0; i < max_ch_num; i++) {
		if (DMA_CH_N_IDLE(dmac, i)) {
			ch = i;
            dmac->ch_aborted &= ~BIT(i);
            dmac->ch_keep &= ~BIT(i);
			SET_DMA_CH_N_BUSY(dmac, i);
			break;
		}

	}

    local_irq_restore(flags);

	return ch;
}

__ilm__
static void scm2010_dma_ch_release(struct scm2010_dmac_t *dmac, u8 ch)
{
    u32 flags;

    local_irq_save(flags);

	SET_DMA_CH_N_IDLE(dmac, ch);

    local_irq_restore(flags);
}

__ilm__
static bool scm2010_dma_ch_is_busy(struct device *dma_dev, int dma_ch)
{
	assert(dma_dev != NULL);

	return ((dma_read(dma_dev, DMAC_CH_N_CTRL(dma_ch)) & BIT(0)) != 0);
}

static int scm2010_dma_ch_get_trans_size(struct device *dma_dev, int dma_ch)
{
	assert(dma_dev != NULL);

	return (int)dma_read(dma_dev, DMAC_CH_N_TRAN_SIZE(dma_ch));
}

__ilm__
static int scm2010_dma_copy (struct device *dma_dev, bool block, bool wait, struct dma_desc_chain dma_desc[], int desc_num, u8 int_mask, int remainder, dma_done_handler handler, void *priv_data)
{
	struct scm2010_dmac_t *dmac = (struct scm2010_dmac_t *) dma_dev->priv;
	u8 ch;
	struct scm2010_dma_desc *desc = NULL;
	int i;
	u32 rem_src;
	u32 rem_dst;
	int rem = 0;

	assert(desc_num <= DMA_DESC_NUM);

	/* 1. alloc channel */
	while ((ch = scm2010_dma_alloc_ch(dmac)) == 0xff) {
		if (block) {
			osDelay(1);
			continue;
		}
		else {
			/* don't wait for dma channel let user to decide next action */
			return -1;
		}
	}

    dmac->channel[ch].cb = handler;
    dmac->channel[ch].priv_data = priv_data;

	/* 2. update dma descriptor */

	desc = dmac->channel[ch].dma_desc;

	if (!desc)
		assert(0);

	for (i = 0; i < desc_num; i++) {
		desc[i].dmac_cfg = default_dma_ch_cfg;
		desc[i].dmac_cfg.int_tc_mask = int_mask;
		desc[i].dmac_cfg.dst_req_sel = ((ch + 1) * 2);
		desc[i].dmac_cfg.src_req_sel = ((ch + 1) * 2) - 1;
		desc[i].dst_addr_l = dma_desc[i].dst_addr;
		desc[i].dst_addr_h = 0;
		desc[i].src_addr_l = dma_desc[i].src_addr;
		desc[i].src_addr_h = 0;

		if (i < desc_num -1) {
			desc[i].llptr_l = (u32) &desc[i+1];
			desc[i].llptr_h = 0;
		} else {
			desc[i].llptr_l = 0;
			desc[i].llptr_h = 0;
		}

		if (remainder == 0 && (dma_desc[i].len >= 4)) {
			rem = (dma_desc[i].len & 0x3);
			desc[i].dmac_cfg.src_width = desc[i].dmac_cfg.dst_width = WORD;
			desc[i].tran_size = dma_desc[i].len/4;
		}
		else if (remainder == 0x2 && (dma_desc[i].len >= 2)) {
			rem = (dma_desc[i].len & 0x1);
			desc[i].dmac_cfg.src_width = desc[i].dmac_cfg.dst_width = HALF_WORD;
			desc[i].tran_size = (dma_desc[i].len/2);
		}
		else {
			desc[i].dmac_cfg.src_width = desc[i].dmac_cfg.dst_width = BYTE;
			desc[i].tran_size = dma_desc[i].len;
		}

		if (rem) {
			rem_src = dma_desc[i].src_addr + dma_desc[i].len - rem;
			rem_dst = dma_desc[i].dst_addr + dma_desc[i].len - rem;
			bcopy((caddr_t) rem_src, (caddr_t)rem_dst, rem);
		}
	}

	/* 3. kick dma */
	scm2010_dma_kick(dma_dev, ch, desc);

	if (int_mask) {
		while (scm2010_dma_ch_is_busy(dma_dev, ch)) {
			if (wait) {
				osDelay(1);
			}
		}
        /* XXX: MEM2MEM DMA will not be allowed to keep a channel. */
		/* 4. release channel */
		scm2010_dma_ch_release(dmac, ch);
	}

	return 0;
}

int scm2010_dma_copy_hw(struct device *dma_dev, bool keep, struct dma_ctrl *dma_ctrl, struct dma_desc_chain *dma_desc, int desc_num, dma_done_handler handler, void *priv_data, int *dma_ch)
{
	struct scm2010_dmac_t *dmac = (struct scm2010_dmac_t *) dma_dev->priv;
	struct scm2010_dma_desc *desc = NULL;
	struct scm2010_dma_ch_cfg dma_cf_cfg;
	u8 ch;
	int i;

	if ((ch = scm2010_dma_alloc_ch(dmac)) == 0xff) {
		printk("DMA ch alloc failure\n");
		return -EBUSY;
	}

    if (keep) {
        dmac->ch_keep |= BIT(ch);
    } else {
        dmac->ch_keep &= ~BIT(ch);
    }

	*dma_ch = (int)ch;
	desc = dmac->channel[ch].dma_desc;

	dma_cf_cfg = default_hw_dma_ch_cfg;
	dma_cf_cfg.src_mode = dma_ctrl->src_mode;
	dma_cf_cfg.dst_mode = dma_ctrl->dst_mode;
	dma_cf_cfg.dst_addr_ctrl = dma_ctrl->dst_addr_ctrl;
	dma_cf_cfg.src_addr_ctrl = dma_ctrl->src_addr_ctrl;
	dma_cf_cfg.src_width = dma_ctrl->src_width;
	dma_cf_cfg.dst_width = dma_ctrl->dst_width;
	dma_cf_cfg.dst_req_sel = dma_ctrl->dst_req;
	dma_cf_cfg.src_req_sel = dma_ctrl->src_req;
	dma_cf_cfg.src_bust_size = dma_ctrl->src_burst_size;
	if (dma_ctrl->intr_mask & DMA_INTR_TC_MASK) {
		dma_cf_cfg.int_tc_mask = 1;
	}
	if (dma_ctrl->intr_mask & DMA_INTR_ERR_MASK) {
		dma_cf_cfg.int_err_mask = 1;
	}
	if (dma_ctrl->intr_mask & DMA_INTR_ABT_MASK) {
		dma_cf_cfg.int_abt_mask = 1;
	}

	for (i = 0; i < desc_num; i++) {
		desc[i].dmac_cfg = dma_cf_cfg;

		desc[i].dst_addr_l = dma_desc[i].dst_addr;
		desc[i].dst_addr_h = 0;
		desc[i].src_addr_l = dma_desc[i].src_addr;
		desc[i].src_addr_h = 0;

		if (i < (desc_num - 1)) {
			desc[i].llptr_l = (u32)&desc[i+1];
			desc[i].llptr_h = 0;
		} else {
			desc[i].llptr_l = 0;
			desc[i].llptr_h = 0;

		}

		desc[i].tran_size = dma_desc[i].len;

	}

	if (handler) {
		dmac->channel[ch].cb = handler;
		dmac->channel[ch].priv_data = priv_data;
	}

	scm2010_dma_kick(dma_dev, ch, desc);

    /* XXX: a user must wait for completion using dma_ch_is_busy probing
     *      when int_tc_mask is set.
     */

	return 0;
}

int scm2010_dma_reload(struct device *dma_dev, int dma_ch, struct dma_desc_chain *dma_desc, int desc_num)
{
	struct scm2010_dmac_t *dmac = (struct scm2010_dmac_t *) dma_dev->priv;
	struct scm2010_dma_desc *desc = dmac->channel[dma_ch].dma_desc;
	int i;
    u32 flags;

    local_irq_save(flags);

    /* Reloading is only allowed when the channel was reserved to be kept. */
    assert(dmac->ch_keep & BIT(dma_ch));

	SET_DMA_CH_N_BUSY(dmac, dma_ch);
    dmac->ch_aborted &= ~BIT(dma_ch);

	for (i = 0; i < desc_num; i++) {
		desc[i].dst_addr_l = dma_desc[i].dst_addr;
		desc[i].src_addr_l = dma_desc[i].src_addr;
		if (i < (desc_num - 1)) {
			desc[i].llptr_l = (u32)&desc[i+1];
		} else {
			desc[i].llptr_l = 0;

		}
		desc[i].tran_size = dma_desc[i].len;
	}

    local_irq_restore(flags);

	scm2010_dma_kick(dma_dev, dma_ch, desc);

	return 0;
}

static int scm2010_dma_ch_rel(struct device *dma_dev, int dma_ch)
{
	struct scm2010_dmac_t *dmac = (struct scm2010_dmac_t *) dma_dev->priv;
	u32 int_status = dma_read(dma_dev, DMAC_INT_STATUS);
    u32 flags;

    local_irq_save(flags);

	SET_DMA_CH_N_IDLE(dmac, dma_ch);

    local_irq_restore(flags);

	dma_write(dma_dev, int_status, DMAC_INT_STATUS);

	return 0;
}

static int scm2010_dma_ch_abort(struct device *dma_dev, int dma_ch)
{
    struct scm2010_dmac_t *dmac = (struct scm2010_dmac_t *) dma_dev->priv;
    u32 int_status;
    u32 ch_ctrl;
    u32 flags;

    dma_write(dma_dev, 1 << dma_ch, DMAC_CH_ABORT);
    ch_ctrl = dma_read(dma_dev, DMAC_CH_N_CTRL(dma_ch));
    dma_write(dma_dev, ch_ctrl & ~0x1, DMAC_CH_N_CTRL(dma_ch));

    if (!(dmac->ch_keep & BIT(dma_ch))) { /* Kept channel needs to be 'released' by a user. */
        local_irq_save(flags);

        SET_DMA_CH_N_IDLE(dmac, dma_ch);

        local_irq_restore(flags);
    }

    int_status = dma_read(dma_dev, DMAC_INT_STATUS);
    dma_write(dma_dev, int_status, DMAC_INT_STATUS);

    return 0;
}

struct dma_ops scm2010_dma_ops = {
	.dma_copy		= scm2010_dma_copy,
	.dma_copy_hw 	= scm2010_dma_copy_hw,
    .dma_reload     = scm2010_dma_reload,
	.dma_ch_rel		= scm2010_dma_ch_rel,
	.dma_ch_abort	= scm2010_dma_ch_abort,
	.dma_ch_is_busy	= scm2010_dma_ch_is_busy,
    .dma_ch_get_trans_size = scm2010_dma_ch_get_trans_size,
};

static int scm2010_dma_probe(struct device *dev) {
	struct scm2010_dmac_t *dmac;
	int dma_ch_num = 0;
	int i;

	dmac = &scm2010_dmac[dev_id(dev)];
	dmac->inst = dev_id(dev);
	dmac->dev = dev;
	dmac->ch_status = CH_IDLE;
    dmac->ch_aborted = 0;
	dev->priv = (void *) dmac;
	dma_ch_num = dmac->inst == 0 ? (DMAC0_CH_NUM): (DMAC1_CH_NUM);

	for (i = 0; i < dma_ch_num; i++) {
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
		dmac->channel[i].dma_desc = dma_kmalloc(sizeof(struct scm2010_dma_desc));
#else
		if (dmac->inst == 0)
			dmac->channel[i].dma_desc = &scm2010_dma0_desc[i][0];
#if DMAC_NUM > 1
		else if (dmac->inst == 1)
			dmac->channel[i].dma_desc = &scm2010_dma1_desc[i][0];
#endif
#endif
	}

	if (request_irq(dev->irq[0], scm2010_dmac_isr, dev_name(dev), dev->pri[0], dmac)) {
        printk("scm2010_dma_probe irq register failed\n");
        return -1;
    }

	return 0;
}

static declare_driver(dma) = {
	.name  = "dmac",
	.probe = scm2010_dma_probe,
	.ops   = &scm2010_dma_ops
};
