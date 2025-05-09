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
#ifndef __WISE_SPI_FLASH_INTERNAL_H__
#define __WISE_SPI_FLASH_INTERNAL_H__

#include <hal/spi-flash.h>

/* Flash opcodes. */
#define SPI_FLASH_OP_WREN	0x06	/* Write enable */
#define SPI_FLASH_OP_RDSR	0x05	/* Read status register */
#define SPI_FLASH_OP_WRSR	0x01	/* Write status register 1 byte */
#define SPI_FLASH_OP_RDSR2	0x3f	/* Read status register 2 */
#define SPI_FLASH_OP_WRSR2	0x3e	/* Write status register 2 */

#define SPI_FLASH_OP_READ	0x03	/* Read data bytes (low frequency) */
#define SPI_FLASH_OP_READ_FAST	0x0b	/* Read data bytes (high frequency) */
#define SPI_FLASH_OP_READ_1_1_2	0x3b	/* Read data bytes (Dual Output SPI) */
#define SPI_FLASH_OP_READ_1_2_2	0xbb	/* Read data bytes (Dual I/O SPI) */
#define SPI_FLASH_OP_READ_1_1_4	0x6b	/* Read data bytes (Quad Output SPI) */
#define SPI_FLASH_OP_READ_1_4_4	0xeb	/* Read data bytes (Quad I/O SPI) */

#define SPI_FLASH_OP_PP		0x02	/* Page program (up to 256 bytes) */
#define SPI_FLASH_OP_PP_1_1_4	0x32	/* Quad page program */
#define SPI_FLASH_OP_PP_1_4_4	0x38	/* Quad page program */

#define SPI_FLASH_OP_BE_4K	0x20	/* Erase 4KiB block */
#define SPI_FLASH_OP_BE_4K_PMC	0xd7	/* Erase 4KiB block on PMC chips */
#define SPI_FLASH_OP_BE_32K	0x52	/* Erase 32KiB block */
#define SPI_FLASH_OP_CHIP_ERASE	0xc7	/* Erase whole flash chip */

#define SPI_FLASH_OP_SE		0x20 	/* Erase 4KiB sector */
#define SPI_FLASH_OP_BE		0xd8	/* Erase block (usually 64KiB) */

#define SPI_FLASH_OP_RDID	0x9f	/* Read JEDEC ID */
#define SPI_FLASH_OP_RDSFDP	0x5a	/* Read SFDP */
#define SPI_FLASH_OP_RDCR	0x35	/* Read configuration register */
#define SPI_FLASH_OP_RDFSR	0x70	/* Read flag status register */

#define SPI_FLASH_OP_WRDI	0x04	/* Write disable */

/* Used for S3AN flashes only */
#define SPI_FLASH_OP_XSE	0x50	/* Sector erase */
#define SPI_FLASH_OP_XPP	0x82	/* Page program */
#define SPI_FLASH_OP_XRDSR	0xd7	/* Read status register */

#define XSR_PAGESIZE		BIT(0)	/* Page size in Po2 or Linear */
#define XSR_RDY			BIT(7)	/* Ready */

/* Used for Macronix and Winbond flashes. */
#define SPI_FLASH_OP_EN4B	0xb7	/* Enter 4-byte mode */
#define SPI_FLASH_OP_EX4B	0xe9	/* Exit 4-byte mode */

/* Used for Spansion flashes only. */
#define SPI_FLASH_OP_BRWR	0x17	/* Bank register write */
#define SPI_FLASH_OP_CLSR	0x30	/* Clear status register 1 */

/* Used for Micron flashes only. */
#define SPI_FLASH_OP_RD_EVCR      0x65    /* Read EVCR register */
#define SPI_FLASH_OP_WD_EVCR      0x61    /* Write EVCR register */

/* Status Register bits. */
#define SR_WIP			BIT(0)	/* Write in progress */
#define SR_WEL			BIT(1)	/* Write enable latch */
/* meaning of other SR_* bits may differ between vendors */
#define SR_BP0			BIT(2)	/* Block protect 0 */
#define SR_BP1			BIT(3)	/* Block protect 1 */
#define SR_BP2			BIT(4)	/* Block protect 2 */
#define SR_TB			BIT(5)	/* Top/Bottom protect */
#define SR_SRWD			BIT(7)	/* SR write protect */
/* Spansion/Cypress specific status bits */
#define SR_E_ERR		BIT(5)
#define SR_P_ERR		BIT(6)

#define SR_QUAD_EN_MX		BIT(6)	/* Macronix Quad I/O */

/* Enhanced Volatile Configuration Register bits */
#define EVCR_QUAD_EN_MICRON	BIT(7)	/* Micron Quad I/O */

/* Flag Status Register bits */
#define FSR_READY		BIT(7)

/* Configuration Register bits. */
#define CR_QUAD_EN_SPAN		BIT(1)	/* Spansion Quad I/O */

/* Status Register 2 bits. */
#define SR2_QUAD_EN_BIT7	BIT(7)

#define JEDEC_MFR(info)		((info)->id[0])
#define JEDEC_ID(info)		(((info)->id[1]) << 8 | ((info)->id[2]))
#define JEDEC_EXT(info)		(((info)->id[3]) << 8 | ((info)->id[4]))

#define KiB 1024
#define ID(x)				\
{					\
	[0] = ((x) >> 16) & 0xff,	\
	[1] = ((x) >> 8) & 0xff,	\
	[2] = (x) & 0xff,		\
}

#define JDEC_ID(id) ((id)[0] << 16 | (id)[1] << 8 | (id)[2])

#define SPI_FLASH_CMD_LEN	4

struct spi_flash_info {
	const char *name;
	u8 id[6];
	struct erase_method *method;
	struct erase_region region[4];
	int page_size;
	u32 flags;
};

struct spi_flash_driver {
	u8 vid[4];
	int (*probe)(struct spi_flash *);
};

#define SPI_FLASH_DRIVER(name)						\
	ll_entry_declare(struct spi_flash_driver, name, spi_flash_driver)

/* SPI flash chip driver APIs */
int spi_flash_cmd_read(struct spi_flash *flash, const u8 *cmd,
		       size_t cmd_len, size_t dummy_len, void *rx, size_t rx_len);
int spi_flash_cmd_write(struct spi_flash *flash, const u8 *cmd,
			size_t cmd_len, void *tx, size_t tx_len);

int read_status(struct spi_flash *flash);
int write_status(struct spi_flash *flash, u8 status);
int read_config(struct spi_flash *flash);

int spi_flash_write_enable(struct spi_flash *flash);
int spi_flash_write_disable(struct spi_flash *flash);

int spi_flash_sr_ready(struct spi_flash *flash);
int spi_flash_wait_till_ready(struct spi_flash *flash,
			      unsigned long timeout);
void spi_flash_addr(u8 *cmd, u32 addr);
int spi_flash_write_sequence(struct spi_flash *flash,
			     const u8 *cmd, size_t cmd_len,
			     void *tx, size_t tx_len,
			     unsigned long timeout);

/**
 * struct spi_flash_master_ops - SPI flash controller methods
 */
struct spi_flash_master_ops {
	int (*cmd_read)(struct device *device, const u8 *cmd,
			size_t cmd_len, size_t dummy_len, void *rx, size_t data_len);
	int (*cmd_mio_read)(struct device *device, struct fast_read_cmd *cmd,
            u32 addr, void *rx, size_t data_len);
	int (*cmd_write)(struct device *device, const u8 *cmd,
		    size_t cmd_len, void *tx, size_t data_len);
    bool (*set_mm_rcmd)(struct device *device,
            const struct fast_read_cmd *cmd, bool dryrun);
    bool (*is_quad_feasible)(struct device *device);
	size_t max_xfer_size;
};

#define spi_flash_master_ops(x) \
	(struct spi_flash_master_ops *)((x)->driver->ops)

#endif /* __WISE_SPI_FLASH_INTERNAL_H__ */
