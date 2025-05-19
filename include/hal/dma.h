#ifndef __DMA_H__
#define __DMA_H__

#include <hal/device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	DMA_STATUS_NONE = 0, 	 /* Spurious ? */
	DMA_STATUS_COMPLETE = 1, /* Completion of a transfer list */
	DMA_STATUS_BLOCK = 2,	 /* Completion of a single transfer block in a transfer list */
	DMA_STATUS_ABORTED = 3,
	DMA_STATUS_ERROR = -1
} dma_isr_status;

enum dma0_hw_req {
	DMA0_HW_REQ_SPI0_TX		= 0,
	DMA0_HW_REQ_SPI0_RX		= 1,
	DMA0_HW_REQ_SPI2_TX		= 2,
	DMA0_HW_REQ_SPI2_RX		= 3,
	DMA0_HW_REQ_UART0_TX	= 4,
	DMA0_HW_REQ_UART0_RX	= 5,
	DMA0_HW_REQ_UART1_TX	= 6,
	DMA0_HW_REQ_UART1_RX	= 7,
	DMA0_HW_REQ_I2C0		= 8,
	DMA0_HW_REQ_SDC			= 9,
};

enum dma1_hw_req {
	DMA1_HW_REQ_SPI1_TX		= 0,
	DMA1_HW_REQ_SPI1_RX		= 1,
	DMA1_HW_REQ_I2S_RX 		= 2,
	DMA1_HW_REQ_I2S_TX		= 3,
	DMA1_HW_REQ_UART2_TX	= 4,
	DMA1_HW_REQ_UART2_RX	= 5,
	DMA1_HW_REQ_I2C1		= 8,
	DMA1_HW_REQ_SDC			= 9,
};

enum dma_mode {
	DMA_MODE_NORMAL,
	DMA_MODE_HANDSHAKE,
};

enum dma_src_width {
	DMA_WIDTH_BYTE = 0x0,
	DMA_WIDTH_HWORD,
	DMA_WIDTH_WORD,
	DMA_WIDTH_DWORD,
};

enum dma_addr_ctrl {
	DMA_ADDR_CTRL_INCREMENT,
	DMA_ADDR_CTRL_DECREMENT,
	DMA_ADDR_CTRL_FIXED,
};

enum dma_intr_mask {
	DMA_INTR_TC_MASK	= (1 << 0),
	DMA_INTR_ERR_MASK	= (1 << 1),
	DMA_INTR_ABT_MASK	= (1 << 2),
};

enum dma_src_burst_size {
	DMA_SRC_BURST_SIZE_1 = 0x0,
	DMA_SRC_BURST_SIZE_2,
	DMA_SRC_BURST_SIZE_4,
	DMA_SRC_BURST_SIZE_8,
	DMA_SRC_BURST_SIZE_16,
	DMA_SRC_BURST_SIZE_32,
	DMA_SRC_BURST_SIZE_64,
	DMA_SRC_BURST_SIZE_128,
};

struct dma_desc_chain {
	u32 src_addr;
	u32 dst_addr;
	u32 len;
};

struct dma_ctrl {
	u8 src_mode;
	u8 dst_mode;
	u8 src_req;
	u8 dst_req;
	u8 src_addr_ctrl;
	u8 dst_addr_ctrl;
	u8 src_width;
	u8 dst_width;
	u8 intr_mask;
	u8 src_burst_size;
	u32 src_addr;
	u32 dst_addr;
};

typedef int (*dma_done_handler)(void *priv, dma_isr_status status);

struct dma_ops {

	/* Allocate a dma channel and copy data by dma controller*/
	int (*dma_copy)(struct device *dma_dev, bool block, bool wait, struct dma_desc_chain *dma_desc, int desc_num, u8 int_mask, int remainder, dma_done_handler handler, void *priv_data);
	int (*dma_copy_hw)(struct device *dma_dev, bool keep, struct dma_ctrl *dma_ctrl, struct dma_desc_chain *dma_desc, int desc_num, dma_done_handler handler, void *priv_data, int *dma_ch);
    /* Reload will only be available for MEM2PERI or PERI2MEM DMA. */
	int (*dma_reload)(struct device *dma_dev, int dma_ch, struct dma_desc_chain *dma_desc, int desc_num);
	int (*dma_ch_rel)(struct device *dma_dev, int dma_ch);
	int (*dma_ch_abort)(struct device *dma_dev, int dma_ch);
	bool (*dma_ch_is_busy)(struct device *dma_dev, int dma_ch);
	int (*dma_ch_get_trans_size)(struct device *dma_dev, int dma_ch);
};

#define dma_ops(x)	((struct dma_ops *)(x)->driver->ops)

__ilm__ static __inline__ u32 dma_copy(struct device *dma_dev, bool block, bool wait, struct dma_desc_chain *dma_desc, int desc_num, u8 int_mask, int remainder, dma_done_handler handler, void *priv_data)
{
	if (!dma_dev || !dma_ops(dma_dev)->dma_copy)
		return -1;

	return dma_ops(dma_dev)->dma_copy(dma_dev, block, wait, dma_desc, desc_num, int_mask, remainder, handler, priv_data);
}

static __inline__ int dma_copy_hw(struct device *dma_dev, bool keep, struct dma_ctrl *dma_ctrl, struct dma_desc_chain *dma_desc, int desc_num, dma_done_handler handler, void *priv_data, int *dma_ch)
{
	if (!dma_dev || !dma_ops(dma_dev)->dma_copy_hw) {
		return -1;
	}

	return dma_ops(dma_dev)->dma_copy_hw(dma_dev, keep, dma_ctrl, dma_desc, desc_num, handler, priv_data, dma_ch);
}

static __inline__ int dma_reload(struct device *dma_dev, int dma_ch, struct dma_desc_chain *dma_desc, int desc_num)
{
	if (!dma_dev || !dma_ops(dma_dev)->dma_reload) {
		return -1;
	}

	return dma_ops(dma_dev)->dma_reload(dma_dev, dma_ch, dma_desc, desc_num);
}


static __inline__ int dma_ch_rel(struct device *dma_dev, int dma_ch)
{
	if (!dma_dev || !dma_ops(dma_dev)->dma_ch_rel) {
		return -1;
	}

	return dma_ops(dma_dev)->dma_ch_rel(dma_dev, dma_ch);
}

static __inline__ int dma_ch_abort(struct device *dma_dev, int dma_ch)
{
	if (!dma_dev || !dma_ops(dma_dev)->dma_ch_abort) {
		return -1;
	}

	return dma_ops(dma_dev)->dma_ch_abort(dma_dev, dma_ch);
}

static __inline__ bool dma_ch_is_busy(struct device *dma_dev, int dma_ch)
{
	if (!dma_dev || !dma_ops(dma_dev)->dma_ch_is_busy) {
		return -1;
	}

	return dma_ops(dma_dev)->dma_ch_is_busy(dma_dev, dma_ch);
}

static __inline__ int dma_ch_get_trans_size(struct device *dma_dev, int dma_ch)
{
	if (!dma_dev || !dma_ops(dma_dev)->dma_ch_get_trans_size) {
		return -1;
	}

	return dma_ops(dma_dev)->dma_ch_get_trans_size(dma_dev, dma_ch);
}

#ifdef __cplusplus
}
#endif

#endif /* __DMA_H__ */
