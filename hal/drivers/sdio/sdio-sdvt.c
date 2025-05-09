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

#include <string.h>

#include <soc.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/device.h>
#include <hal/clk.h>
#include <hal/pinctrl.h>
#include <hal/sdio.h>
#include <hal/timer.h>
#include <hal/arm/asm/barriers.h>
#include <hal/unaligned.h>
#include <cmsis_os.h>
#include <sdioif.h>
#include "hal/pm.h"


#ifdef CONFIG_SDIO_USE_IRQD
#include <hal/sw-irq.h>
#include "irqd.h"
#endif

/*
 * Note
 * CSR registers and and FBARs are supposed
 * to be wired into SoC.
 * If relevant part of SoC may change in a future
 * revision, or on a new chipset, this driver will
 * break unless it manage to keep the same APB
 * registers.
 * It's inevitable because Smart DV has designed
 * interfaces as such.
 */

/*
 * CSR registers
 *
 * These are read only registers in CSR,
 * which SDIO host can read from SDIO device.
 *
 */

/* CFG0 */
#define OCR_SHFT					(0)
#define OCR_MASK					(0x00ffffff)
#define RDFIFO_THLD_SHFT			(24)
#define RDFIFO_THLD_MASK			(0xff000000)

/* CFG1 */
#define CCCR_SDIO_REV_SHFT			(0)
#define CCCR_SDIO_REV_MASK			(0x000000ff)
#define FUNC0_CIS_PTR_SHFT			(8)
#define FUNC0_CIS_PTR_MASK			(0x01ffff00)

/* CFG2 - CFG8 */
#define FUNCn_CIS_PTR_SHFT			(0)
#define FUNCn_CIS_PTR_MASK			(0x0001ffff)
#define FUNCn_EXT_CODE_SHFT			(17)
#define FUNCn_EXT_CODE_MASK			(0x01fe0000)

/* CFG9 */
#define SPEC_REV_SHFT				(0)
#define SPEC_REV_MASK				(0x000000ff)

/*
 * Function BARs.
 * XXX: only taking care of lower 32bits.
 */

/* FBAR BASE CFGn */
#define IO_FUNC_BAR_L_SHFT			(0)
#define IO_FUNC_BAR_L_MASK			(0xffffffff)

/*
 * Software registers
 *
 * These are software registers to support
 * command and data passing.
 *
 */

#define SREG_CONTROL 				(0x00)

#define SSPI_SHFT					(0)
#define SSPI_MASK					(0x00000001)
#define ESPI_SHFT					(1)
#define ESPI_MASK					(0x00000002)
#define MEM_MODE_SHFT				(2)
#define MEM_MODE_MASK				(0x00000004)
#define VHS_SHFT					(3)
#define VHS_MASK					(0x00000078)

#define SREG_CCCR0 					(0x04)

#define IO_ENABLE_SHFT				(0)
#define IO_ENABLE_MASK				(0x000000ff)
#define INT_ENABLE_SHFT				(8)
#define INT_ENABLE_MASK				(0x0000ff00)
#define IO_ABORT_SHFT				(16)
#define IO_ABORT_MASK				(0x00ff0000)
#define BUS_IF_CTRL_SHFT			(24)
#define BUS_IF_CTRL_MASK			(0xff000000)

#define SREG_CCCR1_BLK_SIZE 		(0x08)

#define SREG_CCCR2 					(0x0c)

#define CARD_CAPABILITY_SHFT		(0)
#define CARD_CAPABILITY_MASK		(0x000000ff)
#define BUS_SPEED_SHFT				(8)
#define BUS_SPEED_MASK				(0x0000ff00)

#define SREG_CCCR_BUS_SUSPEND 		(0x10)

#define BUS_SUSPEND_SHFT			(0)
#define BUS_SUSPEND_MASK			(0x000000ff)
#define FUNC_SELECT_SHFT			(8)
#define FUNC_SELECT_MASK			(0x0000ff00)
#define POWER_CONTROL_SHFT			(16)
#define POWER_CONTROL_MASK			(0x00ff0000)

#define SREG_CCCR_STATUS 			(0x14)

#define IO_READY_SHFT				(0)
#define IO_READY_MASK				(0x000000ff)
#define INT_PENDING_SHFT			(8)
#define INT_PENDING_MASK			(0x0000ff00)
#define EXEC_FLAG_SHFT				(16)
#define EXEC_FLAG_MASK				(0x00ff0000)
#define READY_FLAG_SHFT				(24)
#define READY_FLAG_MASK				(0xff000000)

#define SREG_VENDOR_UNIQUE0 		(0x18)
/* |  FIFO min linear len  |  FIFO depth  |
   |          2 bytes      |    2 bytes   | */

#define SREG_VENDOR_UNIQUE1 		(0x1c)
/* | FIFO interrupt avail len | Recovery[2bits] MBU EXT avail nodes[14bits] | FIFO Read idx |
   |           1 bytes        |                 1 bytes                     |    2 bytes    | */


#define SREG_VENDOR_UNIQUE2					(0x20)
#define SREG_VENDOR_UNIQUE2_SLEEP			(0x1) << 0
#define SREG_VENDOR_UNIQUE2_REENUM_DONE		(0x1) << 1

#define SREG_VENDOR_UNIQUE3 		(0x24)

#define SREG_FN_FBR0(f)				(0x28 + (f - 1) * 8)

#define FN_FBR_IF_TYPE_SHFT			(0)
#define FN_FBR_IF_TYPE_MASK			(0x000000ff)
#define FN_FBR_PWR_SEL_SHFT			(8)
#define FN_FBR_PWR_SEL_MASK			(0x0000ff00)
#define FN_FBR_BLK_SZ_SHFT			(16)
#define FN_FBR_BLK_SZ_MASK			(0xffff0000)

#define SREG_FN_FBR1(f) 			(0x2c + (f - 1) * 8)

#define FN_FBR_ACC_WND_CSA_SHFT		(0)
#define FN_FBR_ACC_WND_CSA_MASK		(0x000000ff)
#define FN_FBR_CSA_SHFT				(8)
#define FN_FBR_CSA_MASK				(0xffffff00)

#define SREG_FUNC_READY_DRUATION 	(0x60)

#define SREG_SD_MODE_TIMING0 		(0x64)

#define NCR_SHFT					(0)
#define NCR_MASK					(0x000000ff)
#define NCC_SHFT					(8)
#define NCC_MASK					(0x0000ff00)
#define NRC_SHFT					(16)
#define NRC_MASK					(0x00ff0000)
#define NID_SHFT					(24)
#define NID_MASK					(0x0f000000)
#define NWR_SHFT					(28)
#define NWR_MASK					(0xf0000000)

#define SREG_SD_MODE_TIMING1 		(0x68)

#define NSD_SHFT					(0)
#define NSD_MASK					(0x0000000f)
#define NSB_D_SHFT					(4)
#define NSB_D_MASK					(0x000000f0)
#define NSB_R_SHFT					(8)
#define NSB_R_MASK					(0x00000f00)
#define NSB_T_SHFT					(12)
#define NSB_T_MASK					(0x0000f000)
#define R1B_START_TIME_SHFT			(16)
#define R1B_START_TIME_MASK			(0x000f0000)
#define R1B_DURATION_SHFT			(20)
#define R1B_DURATION_MASK			(0x0ff00000)

#define SREG_T_INIT_PERIOD 			(0x6c)

#define SREG_SPI_TIMING 			(0x70)

#define SPI_NCS_SHFT				(0)
#define SPI_NCS_MASK				(0x0000000f)
#define SPI_NCR_SHFT				(4)
#define SPI_NCR_MASK				(0x000000f0)
#define SPI_NRC_SHFT				(8)
#define SPI_NRC_MASK				(0x00000f00)
#define SPI_NWR_SHFT				(12)
#define SPI_NWR_MASK				(0x0000f000)
#define SPI_NEC_SHFT				(16)
#define SPI_NEC_MASK				(0x000f0000)
#define SPI_NBR_SHFT				(20)
#define SPI_NBR_MASK				(0x00f00000)
#define SPI_NCX_SHFT				(24)
#define SPI_NCX_MASK				(0x0f000000)

#define SREG_NAC_TIMING 			(0x74)

#define SREG_PULLUP_EN 				(0x78)

#define CMD_P_EN_SHFT				(0)
#define CMD_P_EN_MASK				(0x00000001)
#define DAT_P_EN_SHFT				(1)
#define DAT_P_EN_MASK				(0x0000001e)

#define SREG_IRQ_ENABLE 			(0x7c)

#define IRQ_WR_CMD_RECVD 			(1 << 0)
#define IRQ_RD_CMD_RECVD 			(1 << 1)
#define IRQ_RD_BLOCK_DONE			(1 << 2)
#define IRQ_RD_NEXT_BLOCK			(1 << 3)
#define IRQ_WR_BLOCK_READY			(1 << 4)
#define IRQ_XFER_FULL_DONE			(1 << 5)
#define IRQ_CMD_UPDATE				(1 << 6)
#define IRQ_CMD_ABORT				(1 << 7)
#define IRQ_CMD_CRC_ERROR			(1 << 8)
#define IRQ_DAT_CRC_ERROR			(1 << 9)
#define IRQ_FUNC0_CIS				(1 << 10)
#define IRQ_SUSPEND_ACCEPT			(1 << 11)
#define IRQ_RESUME_ACCEPT			(1 << 12)
#define IRQ_OUT_OF_RANGE			(1 << 13)
#define IRQ_ERASE_RESET				(1 << 14)

#define SREG_IRQ_STATUS 			(0x80)

#define SREG_SOC_TIMEOUT 			(0x84)

#define SREG_CMD_INFO 				(0x88)

#define CMD_BLOCK_MODE_SHFT			(0)
#define CMD_BLOCK_MODE_MASK			(0x00000001)
#define CMD_RW_SHFT					(1)
#define CMD_RW_MASK					(0x00000002)
#define CMD_IDX_SHFT				(2)
#define CMD_IDX_MASK				(0x000000fc)
#define CMD_FUNC_NUM_SHFT			(8)
#define CMD_FUNC_NUM_MASK			(0x00000700)
#define CMD_ADDR_MODE_SHFT			(11)
#define CMD_ADDR_MODE_MASK			(0x00000800)
#define CMD_RAW_SHFT				(12)
#define CMD_RAW_MASK				(0x00001000)
#define CMD_OPCODE_SHFT				(13)
#define CMD_OPCODE_MASK				(0x00002000)

#define SREG_DAT_ADDR 				(0x8c)

#define SREG_DAT_BLOCK_SIZE 		(0x90)

#define SREG_DAT_BLOCK_CNT 			(0x94)

#define SREG_INT_INIT_PROCESS 		(0x98)

#define SREG_ONE_SEC_TIMNEOUT 		(0x9c)

#define SREG_RCA 					(0xa0)

#define SREG_OCR 					(0xa4)

#define SWITCH_S18A_SHFT			(0)
#define SWITCH_S18A_MASK			(0x00000001)
#define CCS_SHFT					(1)
#define CCS_MASK					(0x00000002)
#define CARD_BUSY_SHFT				(2)
#define CARD_BUSY_MASK				(0x00000004)

#define SREG_CARD_STATUS 			(0xa8)

#define OUT_OF_RANGE_SHFT			(0)
#define OUT_OF_RANGE_MASK			(0x00000001)
#define COM_CRC_ERROR_SHFT			(1)
#define COM_CRC_ERROR_MASK			(0x00000002)
#define ILLEGAL_CMD_SHFT			(2)
#define ILLEGAL_CMD_MASK			(0x00000004)
#define ERROR_SHFT					(3)
#define ERROR_MASK					(0x00000008)
#define CURRENT_STATUS_SHFT			(4)
#define CURRENT_STATUS_MASK			(0x000000f0)

#define rbfm(f) 					(f##_MASK) 	/* mask */
#define rbfs(f) 					(f##_SHFT) 	/* shift */
#define rbfset(v, f, x) 			((v) |= ((x) << rbfs(f)) & rbfm(f))
#define rbfclr(v, f) 				((v) &= ~rbfm(f))
#define rbfmod(v, f, x) 			{rbfclr(v, f); rbfset(v, f, x);}
#define rbfget(v, f) 				(((v) & rbfm(f)) >> rbfs(f))
#define rbfzero(v) 					((v) = 0)

#define SCM_VID 					0x3364
#define SCM_PID 					0x4010

__sdio_dma_desc__ static u8 cis_ram[512];

#define SDIO_FN_MAX					(7 + 1)

#define SDIO_CMD53_CMD_IDX			(0x35) /* IO_RW_EXTENDED Command(CMD53) 110101b */
struct sdio_cmd53_info
{
	bool cmd53;
	bool write;			/* Command Write(true)/Read(false) */
	bool block;			/* Block mode */
	bool block_cmdup;
	uint32_t nblocks;	/* block=1, number of blocks to be transferred */
};

struct sdvt_sdio_pm
{
	osMutexId_t reenum_mutex; /* Lock for host reeum */
	bool need_host_reenum; /* True: notfiy the host reeum */
	bool host_assumes_device_sleep; /* False: need the host approve to sleep */
};

struct sdvt_sdio_ctx
{
	struct device   *dev;

	struct list_head rx_queue;
	uint8_t rx_req_num;
	struct sdio_req *cur_rx_req;
	struct sdio_cmd53_info transfer_info;

	filled_cb filled;
	getbuf_cb getbuf;
	resetrxbuf_cb resetrxbuf;

#ifdef CONFIG_SDIO_USE_IRQD
	volatile sdvt_tx_chain_t *g_txchain;
	volatile sdvt_interrupt_fifo_t *global_int_fifo;
	sdvt_interrupt_fifo_t int_fifo;  /* A local copy */

	int swi_irq; /* SW IRQ to be raised by irq-dispatcher */
#endif

	bool busy;
#ifdef CONFIG_SDIO_PM
	struct sdvt_sdio_pm pmcfg;
#endif
};

struct sdvt_sdio_ctx sdio_ctx;

#define RX_DESC_BUF_NUM 100 /* number of RX Desc in Func WiFi */

static struct sdio_req rx_desc_buffer[RX_DESC_BUF_NUM];

/* These variables are required for using command */
#ifdef CONFIG_CMD_SDIO
struct sdio_dbg_irq_info {
	u16 irq_status;
	u16 cmd_info;
};

static struct sdio_dbg_irq_info sdio_dbg_irq[100] = {0};
static u32 dbg_irq_idx = 0;
#endif

#ifdef CONFIG_SDIO_USE_IRQD
extern const u8  irqd_bin[];
extern const u32 irqd_bin_size;
extern char  __irqdbin_start[];
extern char  __irqdshm_start[];

#define INT_FIFO_DEPTH  255
#endif

// #define GPIO_DBG
#if defined GPIO_DBG || defined CONFIG_SDIO_OOB_GPIO_INT
static void enable_gpio_out(uint8_t gpio)
{
	uint8_t  bank, shift;
	uint32_t addr, reg;

	/* Set pinmux to GPIO. */
	bank  = gpio / 8;
	addr  = IOMUX_BASE_ADDR + bank * 4;
	shift = (gpio - bank * 8) * 4;
	reg   = readl(addr);
	reg &= ~(0xf << shift);
	reg |= (0x8 << shift);
	writel(reg, addr);

	/* Disable GPIO input. */
	addr = GPIO_BASE_ADDR + 0x0c; /* GPIO_IE */
	reg  = readl(addr);
	reg &= ~(1 << gpio);
	writel(reg, addr);

	/* Enable GPIO output. */
	addr = GPIO_BASE_ADDR + 0x10; /* GPIO_OEN */
	reg  = readl(addr);
	reg |= (1 << gpio);
	writel(reg, addr);
}

__ilm__ void control_gpio_out(uint8_t gpio, uint8_t high)
{
	uint32_t addr, reg;

	addr = GPIO_BASE_ADDR + 0x14; /* GPIO_OUT */
	reg  = readl(addr);
	reg &= ~(1 << gpio);
	if (high)
		reg |= (1 << gpio);
	writel(reg, addr);
}

uint8_t g_sdio_cmdupdate_init = 0;

#define SDIO_DEBUG_CMDUPDATE  \
		control_gpio_out(23, 1);  \
		if (g_sdio_cmdupdate_init != 0) {  \
			control_gpio_out(24, 1);  \
			printk("irq_status=%08x\n", irq_status);  \
			assert(0);  \
		}  \
		g_sdio_cmdupdate_init = 1;

#define SDIO_DEBUG_FULLDONE  \
	control_gpio_out(23, 0);  \
	if (g_sdio_cmdupdate_init != 1) {  \
		control_gpio_out(24, 1);  \
		printk("irq_status=%08x\n", irq_status);  \
		assert(0);  \
	}  \
	g_sdio_cmdupdate_init = 0;
#else
#define SDIO_DEBUG_CMDUPDATE
#define SDIO_DEBUG_FULLDONE
#endif

static u32 read_csrr(struct device *dev, u32 cfg)
{
	u32 addr = (u32)dev->base[1] + cfg * 4;
	u32 v = readl(addr);

	return v;
}

static void write_csrr(struct device *dev, u32 val, u32 cfg)
{
	u32 addr = (u32)dev->base[1] + cfg * 4;
	writel(val, addr);
}

static u32 read_fbar(struct device *dev, u8 func) __maybe_unused;
static u32 read_fbar(struct device *dev, u8 func)
{
	u32 addr = (u32)dev->base[2] + func * 8;
	u32 v = readl(addr);

	return v;
}

static void write_fbar(struct device *dev, u32 val, u8 func)
{
	u32 addr = (u32)dev->base[2] + func * 8;
	writel(val, addr);
}

__ilm__ static u32 read_sreg(struct device *dev, u32 reg)
{
	u32 addr = (u32)dev->base[0] + reg;
	u32 v = readl(addr);

	return v;
}

__ilm__ static void write_sreg(struct device *dev, u32 val, u32 reg)
{
	u32 addr = (u32)dev->base[0] + reg;
	writel(val, addr);
}

static u8 sdvt_sdio_clk_cfg(void)
{
	u8 v;

	/*
	 * Refer to Table 16-7 :
	 * TPLFID_FUNCTION Field Descriptions for
	 * Function 0 (common)
	 *
	 * Part E1 SDIO Specification
	 *
	 */

    switch (CONFIG_SDIO_MAX_TRAN_SPEED) {
	case 400000:
		v = 0x48;
		break;
	case 15000000:
		v = 0x22;
		break;
	case 25000000:
		v = 0x32;
		break;
	case 40000000:
		v = 0x4A;
		break;
	case 50000000:
		v = 0x5A;
		break;
	default:
		v = 0x22;
		break;
	}

	return v;
}

static void sdvt_sdio_init(struct device *dev)
{
	/* CIS 0 */

	u8 cis0[] = {

	[0x00 + 0x00] = 0x21,                   // TPL_CODE_CISTPL_FUNCID
	[0x00 + 0x01] = 0x02,                   // Link to next tuple
	[0x00 + 0x02] = 0x0C,                   // Card function code
	[0x00 + 0x03] = 0x00,                   // Not used

	[0x04 + 0x00] = 0x22,                	// TPL_CODE_CISTPL_FUNCE
	[0x04 + 0x01] = 0x04,                	// Link to next tuple
	[0x04 + 0x02] = 0x00,                	// Extended data
	[0x04 + 0x03] = 0x00,                	// Only block size function 0 can support (512)
	[0x04 + 0x04] = 0x02,                	// Together with previous byte
	[0x04 + 0x05] = 0x22,                	// Transfer rate (15 Mbit/sec)

	[0x0a + 0x00] = 0x20,                	// TPL_CODE_CISTPL_MANFID
	[0x0a + 0x01] = 0x04,                	// Link to next tuple
	[0x0a + 0x02] = SCM_VID & 0xff,      	// SDIO manufacturer code 0x0296
	[0x0a + 0x03] = (SCM_VID >> 8) & 0xff, 	// Used with previous byte
	[0x0a + 0x04] = SCM_PID & 0xff,        	// Part number/revision number OEM ID = 0x5347
	[0x0a + 0x05] = (SCM_PID >> 8) & 0xff, 	// Used with previous byte

	[0x10] = 0xFF,                       	// End of Tuple Chain

	};

	u8 cis[] = {

	/* fnunction CIS */
	[0x00 + 0x00] = 0x21, // TPL_CODE_CISTPL_FUNCID
	[0x00 + 0x01] = 0x02, // Link to next tuple
	[0x00 + 0x02] = 0x0C, // Card function type
	[0x00 + 0x03] = 0x00, // Not used

	[0x04 + 0x00] = 0x22, // TPL_CODE_CISTPL_FUNCE
	[0x04 + 0x01] = 0x2A, // Link to next tuple
	[0x04 + 0x02] = 0x01, // Type of extended data
	[0x04 + 0x03] = 0x01, // Wakeup support
	[0x04 + 0x04] = 0x20, // X.Y revision
	[0x04 + 0x05] = 0x00, // No serial number
	[0x04 + 0x06] = 0x00, // No serial number
	[0x04 + 0x07] = 0x00, // No serial number
	[0x04 + 0x08] = 0x00, // No serial number
	[0x04 + 0x09] = 0x00, // Size of the CSA space available for this function in bytes (0)
	[0x04 + 0x0a] = 0x00, // Used with previous
	[0x04 + 0x0b] = 0x00, // Used with previous
	[0x04 + 0x0c] = 0x00, // Used with previous
	[0x04 + 0x0d] = 0x03, // CSA property: Bit 0 - 0 implies R/W capability in CSA
	[0x04 + 0x0e] = 0x00, // Maximum block size (512 bytes)
	[0x04 + 0x0f] = 0x02, // Used with previous
	[0x04 + 0x10] = 0x00, // OCR value of the function
	[0x04 + 0x11] = 0x80, // Used with previous
	[0x04 + 0x12] = 0xFF, // Used with previous
	[0x04 + 0x13] = 0x00, // Used with previous
	[0x04 + 0x14] = 0x08, // Minimum power required by this function (8 mA)
	[0x04 + 0x15] = 0x0A, // ADDED => Average power required by this function when operating (10 mA)
	[0x04 + 0x16] = 0x0F, // ADDED => Maximum power required by this function when operating (15 mA)
	[0x04 + 0x17] = 0x00, // Stand by is not supported
	[0x04 + 0x18] = 0x00, // Used with previous
	[0x04 + 0x19] = 0x00, // Used with previous
	[0x04 + 0x1a] = 0x00, // Minimum BW
	[0x04 + 0x1b] = 0x00, // Used with previous
	[0x04 + 0x1c] = 0x00, // Optional BW
	[0x04 + 0x1d] = 0x00, // Used with previous
	[0x04 + 0x1e] = 0x64, // Card required timeout in 10ms unit: 1000ms
	[0x04 + 0x1f] = 0x00, // Used with previous
	[0x04 + 0x20] = 0x0A, // Average Power required by this function when operating. (10 mA)
	[0x04 + 0x21] = 0x00, // Used with previous
	[0x04 + 0x22] = 0x0F, // Maximum Power required by this function when operating. (15 mA)
	[0x04 + 0x23] = 0x00, // Used with previous
	[0x04 + 0x24] = 0x00, // High Powermode average(High power function is not supported)
	[0x04 + 0x25] = 0x00, // Used with previous
	[0x04 + 0x26] = 0x00, // High power mode - peak power(High power function is not supported)
	[0x04 + 0x27] = 0x00, // Used with previous
	[0x04 + 0x28] = 0x00, // Low Power Mode average(Low power function is not supported)
	[0x04 + 0x29] = 0x00, // Used with previous
	[0x04 + 0x2a] = 0x00, // Low Power Peak(Low power function is not supported)
	[0x04 + 0x2b] = 0x00,

	[0x30] = 0xFF // End of Tuple Chain

	};

	u32 v;
	u32 cis_offset;
	u8 bus_speed;
	u8 func;
	u32 int_enable;

	/* OCR
	 * If set to 0xff8000, there is no response to CMD5 with OCR selected.
	 */
	v = read_csrr(dev, 0);
	rbfmod(v, OCR, 0x200000);
	write_csrr(dev, v, 0);

	/* func0 cis ptr */
	v = read_csrr(dev, 1);
	rbfmod(v, FUNC0_CIS_PTR, 0x1000);
	write_csrr(dev, v, 1);

	/* configure max speed */
	cis0[9] = sdvt_sdio_clk_cfg();

	/* cis0 data copy to ram */
	memcpy(cis_ram, cis0, sizeof(cis0));
	cis_offset = sizeof(cis0);

	for (func = 1; func <= 7; func++) {
		/* func(n) cis ptr */
		v = read_csrr(dev, func + 1);
		rbfmod(v, FUNCn_CIS_PTR, (0x1000 + cis_offset));
		write_csrr(dev, v, func + 1);

		/* function cis data copy to ram */
		memcpy(cis_ram + cis_offset, cis, sizeof(cis));
		cis_offset += sizeof(cis);
	}

	/* func0 io base address
	 * Not includes CCCR and FBR locations.
	 */

	v = (u32)cis_ram - 0x1000;
	write_fbar(dev, v, 0);

	/* write RCA value 1 */
	write_sreg(dev, 1, SREG_RCA);

	/*
	 * set bus width 4bit mode
	 * IO_ENABLE[7:0] and INT_ENABLE[15:8] will be set by Host
	 */

	v = read_sreg(dev, SREG_CCCR0);
#ifdef CONFIG_SDIO_1BIT_MODE
	rbfmod(v, BUS_IF_CTRL, 0);
#else
	rbfmod(v, BUS_IF_CTRL, 0x2);
#endif
	write_sreg(dev, v, SREG_CCCR0);

	/*
	 * Set card capability and bus Speed
	 */

	/*
	 * Card capability
	 * - SMB (Support Multi Block mode)
	 * - SRW (Support Read Wait)
	 * - SBS (Support Suspend / Resume)
	 * - S4MI (Support Interrupt Period at Data Block Gap in 4-bit mode)
	 */
	v = read_sreg(dev, SREG_CCCR2);
	rbfmod(v, CARD_CAPABILITY, 0x1E);
#ifdef CONFIG_SDIO_1BIT_MODE
	/*
	 * Full-Speed: support 1-bit SD and the 4-bit SD transfer modes.
	 * Low-Speed: 1-bit SD transfer mode, 4-bit support is optional.
	 * 6th bit: LowSpeed Card.
	 * 7th bit: 4-bit Mode Support for Low-Speed Card.
	 * Some Linux Host only automatically switch to 1bit mode in the Low-Speed Mode.
	 */
	v |= 0x40;
#endif
	bus_speed = rbfget(v, BUS_SPEED);
	/* Default speed mode: up to 25 MHz.
	 * High-speed mode: up to 50 MHz.
	 * Compatibility: Support High Speed > 25MHz.
	 */
	if (CONFIG_SDIO_MAX_TRAN_SPEED > 25000000) {
		/* SHS (Support High Speed) */
		rbfmod(v, BUS_SPEED, bus_speed | 0x1);
	} else {
		rbfmod(v, BUS_SPEED, bus_speed & ~0x1);
	}
	write_sreg(dev, v, SREG_CCCR2);

    /* set NRC timing */
	v = read_sreg(dev, SREG_SD_MODE_TIMING0);
	rbfmod(v, NRC, 0x08);
	write_sreg(dev, v, SREG_SD_MODE_TIMING0);

    /* set block size */
	for (func = 1; func <= 7; func++) {
		v = read_sreg(dev, SREG_FN_FBR0(func));
		rbfmod(v, FN_FBR_BLK_SZ, SDIO_BLOCK_SIZE);
		write_sreg(dev, v, SREG_FN_FBR0(func));
	}

	/* Enable interrupts to be used. */
	/* Enable the ERR IRQ, and SW can get the IRQ then clear them. */
	int_enable = (
			IRQ_RD_BLOCK_DONE |\
			IRQ_XFER_FULL_DONE |\
			IRQ_CMD_UPDATE |\
			IRQ_CMD_ABORT |\
			IRQ_CMD_CRC_ERROR |\
			IRQ_DAT_CRC_ERROR |\
			/*
			IRQ_RD_CMD_RECVD |\
			IRQ_RD_NEXT_BLOCK |\
			IRQ_WR_BLOCK_READY |\
			IRQ_CMD_CRC_ERROR |\
			IRQ_DAT_CRC_ERROR |\
			IRQ_FUNC0_CIS |\
			IRQ_SUSPEND_ACCEPT |\
			IRQ_RESUME_ACCEPT |\
			IRQ_OUT_OF_RANGE |\
			IRQ_ERASE_RESET |\
			*/
			0);

	write_sreg(dev, int_enable, SREG_IRQ_ENABLE);
}

__ilm__ static void sdvt_sdio_setup_dma(struct device *dev, struct sdio_req *req)
{
	write_fbar(dev, (u32)req->buf, req->fn);
}

__ilm__
static struct sdio_req *next_rx_req(struct sdvt_sdio_ctx *ctx)
{
	struct sdio_req *req;

	assert(ctx->rx_req_num >= 1);

	req = list_first_entry(&ctx->rx_queue, struct sdio_req, entry);
	list_del(&req->entry);
	ctx->rx_req_num--;
	return req;
}

__ilm__
static void sdvt_sdio_next_rx_req(struct device *dev)
{
	u32 v;
	u16 mbuf_num;
	struct sdvt_sdio_ctx *ctx = dev->priv;

	ctx->cur_rx_req = next_rx_req(ctx);

	v = read_sreg(ctx->dev, SREG_VENDOR_UNIQUE1);
	mbuf_num = (v & 0x3f0000) >> 16;
	if (mbuf_num > 0) {
		mbuf_num--;
		v &= 0xffc0ffff;
		v |= (mbuf_num << 16);
		write_sreg(dev, v, SREG_VENDOR_UNIQUE1);
	}
}

void sdvt_sdio_release_rx_req(struct device *dev, struct sdio_req *req)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;
	u32 flags;

	req->len = 0;

	local_irq_save(flags);
	list_add_tail(&req->entry, &ctx->rx_queue);
	ctx->rx_req_num++;
	local_irq_restore(flags);
}

void sdvt_sdio_write_flowctrl_info(struct device *dev, u16 rd_idx, u16 mbuf_num)
{
	u32 v;
	u32 flags;
	struct sdvt_sdio_ctx *ctx = dev->priv;
	u16 rxleft_pktnum = RX_DESC_BUF_NUM - ctx->rx_req_num;

	mbuf_num = (rxleft_pktnum < mbuf_num) ? (mbuf_num - rxleft_pktnum) : 0;

	local_irq_save(flags);
	v = read_sreg(dev, SREG_VENDOR_UNIQUE1);
	v &= 0xffc00000;
	v |= (mbuf_num << 16);
	v |= rd_idx;
	write_sreg(dev, v, SREG_VENDOR_UNIQUE1);
	local_irq_restore(flags);
}

/* RX desc initialize and attach rx queue list */
static void sdio_init_rx_desc(struct device *dev, u8 fn)
{
	struct sdio_req      *req;
	struct sdvt_sdio_ctx *ctx = dev->priv;

	for (int i = 0; i < RX_DESC_BUF_NUM; i++) {
		req = &rx_desc_buffer[i];
		req->fn = fn;
		req->len = 0;
		req->priv = dev;
		list_add_tail(&req->entry, &ctx->rx_queue);
	}

	ctx->rx_req_num = RX_DESC_BUF_NUM;

	return;
}

static int sdvt_sdio_start(struct device *dev, u8 fn_rx, u16 depth_rx,
		u16 min_linear, u16 mbufext_num)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;

	sdio_init_rx_desc(dev, fn_rx);

	ctx->cur_rx_req = next_rx_req(ctx);
	/* setup DMA addr for FIFO base addr */
	/* Better way? to get FIFO base and set DMA */
	ctx->getbuf(ctx->cur_rx_req);
	sdvt_sdio_setup_dma(dev, ctx->cur_rx_req);

	write_sreg(dev, (min_linear  << 16) | depth_rx, SREG_VENDOR_UNIQUE0);
	write_sreg(dev, (mbufext_num << 16) | (INT_FIFO_DEPTH << 24), SREG_VENDOR_UNIQUE1);
	return 0;
}

#ifdef CONFIG_SDIO_PM
static void sdvt_sdio_resumerxbuf(struct device *dev, u16 depth_rx,
		u16 min_linear, u16 mbufext_num)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;

	/* setup DMA addr for FIFO base addr */
	/* Better way? to get FIFO base and set DMA */
	ctx->getbuf(ctx->cur_rx_req);
	sdvt_sdio_setup_dma(dev, ctx->cur_rx_req);

	write_sreg(dev, (min_linear  << 16) | depth_rx, SREG_VENDOR_UNIQUE0);
	write_sreg(dev, (mbufext_num << 16) | (INT_FIFO_DEPTH << 24), SREG_VENDOR_UNIQUE1);
}
#endif

static int sdvt_sdio_stop(struct device *dev)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;

	ctx->resetrxbuf();
	return 0;
}

static int sdvt_sdio_tx(struct device *dev, u8 fn, u8 *buf, u32 len)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;
	u32 flags;
	u32 v __maybe_unused;

	/* block until previous tx done */
	local_irq_save(flags);
	while (ctx->busy == true) {
		local_irq_restore(flags);
		osDelay(1);
		local_irq_save(flags);
	}
	ctx->busy = true;
	local_irq_restore(flags);

	ctx->g_txchain->fn = fn;
	ctx->g_txchain->cnt = 1;
	ctx->g_txchain->cons_idx = 0;
	ctx->g_txchain->dma_addr = (u32)dev->base[2] + fn * 8;
	ctx->g_txchain->txelems[0].addr = (u32)buf;
	ctx->g_txchain->txelems[0].len = len;

	local_irq_save(flags);
#ifdef CONFIG_SDIO_VERIFICATION_TEST /* Need this for loopback for now */
	/* write tx len */
	write_sreg(dev, len, SREG_VENDOR_UNIQUE0);
#endif
	/* setup DMA */
	write_fbar(dev, (u32)buf, fn);
	dmb();
#ifdef CONFIG_SDIO_OOB_GPIO_INT
	control_gpio_out(CONFIG_SDIO_OOB_GPIO_INT_PIN, 1);
#else
	/* INT pending */
	v = read_sreg(dev, SREG_CCCR_STATUS);
	rbfset(v, INT_PENDING, (1 << fn));
	write_sreg(dev, v, SREG_CCCR_STATUS);
#endif

	local_irq_restore(flags);

	/* block until tx done */
	while (ctx->busy == true) {
		osDelay(1);
	}

	return 0;
}

#ifdef CONFIG_SDIO_TXCHAIN
static void sdvt_sdio_acquire_tx(struct device *dev)
{
	u32 flags;
	struct sdvt_sdio_ctx *ctx = dev->priv;

	/* block until previous tx done */
	local_irq_save(flags);
	while (ctx->busy == true) {
		local_irq_restore(flags);
		osDelay(1);
		local_irq_save(flags);
	}
	ctx->busy = true;
	local_irq_restore(flags);

	return;
}

static void sdvt_sdio_release_tx(struct device *dev)
{
	u32 flags;
	struct sdvt_sdio_ctx *ctx = dev->priv;

	local_irq_save(flags);
	ctx->busy = false;
	local_irq_restore(flags);
}

static void sdvt_sdio_txchain_addelem(struct device *dev, u8 *buf, u16 len, u8 idx)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;

	ctx->g_txchain->txelems[idx].addr = (u32)buf;
	ctx->g_txchain->txelems[idx].len = len;
}

static void sdvt_sdio_txchain_kick(struct device *dev, u8 fn, u8 cnt)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;
	u32 flags;
	u32 v __maybe_unused;

	ctx->g_txchain->fn = fn;
	ctx->g_txchain->cnt = cnt;
	ctx->g_txchain->cons_idx = 0;
	ctx->g_txchain->dma_addr = (u32)dev->base[2] + fn * 8;

	local_irq_save(flags);
	/* setup DMA */
	write_fbar(dev, ctx->g_txchain->txelems[0].addr, fn);
	dmb();
#ifdef CONFIG_SDIO_OOB_GPIO_INT
	control_gpio_out(CONFIG_SDIO_OOB_GPIO_INT_PIN, 1);
#else
	/* INT pending */
	v = read_sreg(dev, SREG_CCCR_STATUS);
	rbfset(v, INT_PENDING, (1 << fn));
	write_sreg(dev, v, SREG_CCCR_STATUS);
#endif
	local_irq_restore(flags);

	/* block until tx done */
	while (ctx->busy == true) {
		osDelay(1);
	}
}
#endif

static int sdvt_sdio_register_cb(struct device *dev,
		filled_cb filled, getbuf_cb getbuf, resetrxbuf_cb resetrxbuf)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;

	ctx->filled = filled;
	ctx->getbuf = getbuf;
	ctx->resetrxbuf = resetrxbuf;

	return 0;
}

#ifdef CONFIG_SDIO_RECOVERY
u8 sdvt_recover_info_proc(struct device *dev, bool write, u8 val)
{
	u32 v, flags;

	if (!write) {
		v = read_sreg(dev, SREG_VENDOR_UNIQUE1);
		v &= 0xc00000;
		return (v >> 22);
	}

	local_irq_save(flags);
	v = read_sreg(dev, SREG_VENDOR_UNIQUE1);
	v &= 0xff3f0000;
	v |= (val << 22);
	write_sreg(dev, v, SREG_VENDOR_UNIQUE1);
	local_irq_restore(flags);

	return 0;
}

int sdvt_buffered_rx(struct device *dev)
{
	u8 num;
	struct sdvt_sdio_ctx *ctx = dev->priv;

	num = ctx->global_int_fifo->prod_idx - ctx->global_int_fifo->cons_idx;
	if (num) {
		return 1;
	}

	if (ctx->rx_req_num < (RX_DESC_BUF_NUM - 1)) {
		return 1;
	}

	return 0;
}
#endif

#ifdef CONFIG_SDIO_PM
static int sdvt_sdio_setbit_and_waitclear(struct device *dev, uint32_t reg, uint32_t bitMask, uint32_t maxWait_ms)
{
	uint32_t value = read_sreg(dev, reg);

	write_sreg(dev, (value | bitMask), reg);

	while ((read_sreg(dev, reg) & bitMask) && (maxWait_ms > 0)) {
		udelay(1000);
		maxWait_ms--;
	}

	if (!maxWait_ms) {
		write_sreg(dev, (value & ~bitMask), reg);
	}

	return (maxWait_ms > 0) ? 1 : 0;
}

int sdvt_sdio_reenum_done(void) {
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	struct device *dev = ctx->dev;
	int done;

	done = sdvt_sdio_setbit_and_waitclear(dev, SREG_VENDOR_UNIQUE2, SREG_VENDOR_UNIQUE2_REENUM_DONE, 50);
	if (!done)
		sdio_dbg_log("%s fail\n", __func__);

	return done;
}

static void sdvt_sdio_reenum_host(void) {
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	uint8_t notify_gpio = CONFIG_SDIO_NOTIFY_REENUM_GPIO;

	if (osMutexAcquire(ctx->pmcfg.reenum_mutex, osWaitForever) != osOK)
		assert(false);
	if (ctx->pmcfg.need_host_reenum) {
		gpio_direction_output(notify_gpio, 1);
		sdvt_sdio_reenum_done();
		gpio_direction_output(notify_gpio, 0);
		ctx->pmcfg.need_host_reenum = false;
		ctx->pmcfg.host_assumes_device_sleep = false;
		pm_staytimeout(CONFIG_SDIO_WAKEUP_HOLD_TIME_MS);
		sdio_dbg_log("%s\n", __func__);
	}
	osMutexRelease(ctx->pmcfg.reenum_mutex);
}
#endif

struct sdio_ops sdvt_sdio_ops = {
	.start = sdvt_sdio_start,
	.stop = sdvt_sdio_stop,
	.tx = sdvt_sdio_tx,
	.register_cb = sdvt_sdio_register_cb,
	.release_rx = sdvt_sdio_release_rx_req,
	.write_flowctrl_info = sdvt_sdio_write_flowctrl_info,
#ifdef CONFIG_SDIO_TXCHAIN
	.acquire_tx = sdvt_sdio_acquire_tx,
	.release_tx = sdvt_sdio_release_tx,
	.txchain_addelem = sdvt_sdio_txchain_addelem,
	.txchain_kick = sdvt_sdio_txchain_kick,
#endif
#ifdef CONFIG_SDIO_RECOVERY
	.recover_info_proc = sdvt_recover_info_proc,
	.buffered_rx = sdvt_buffered_rx,
#endif
#ifdef CONFIG_SDIO_PM
	.reenum_host = sdvt_sdio_reenum_host,
#endif
};

/*
(How to Invoke interrupt for sending data to Host)
Bit[15:8] of CCCR0 register is related to INT_ENABLE. It indicates interrupt for which function was enabled.
When interrupt and interrupt pending was enabled, then the DAT LINE(o_sdio_data[1]) of bit will be driven low.
It will take care interrupt timing for write and read

(How to send data to Host)
Once Read command is received, the device will send response for that command.
1. The CMD_INFO, DATA_ADDR, DAT_BLOCK_SIZE, DAT_BLOCK_CNT AND  IRQ_ENBLE registers  will be loaded.
2. IRQ_RD_CMD_RECEIVED IRQ will be HIGH.
3. IRQ_RD_NEXT_BLOCK will be high, when DMA start internal read for data and  IRQ_RD_BLOCK_DONE will be High when that particular read data was sampled in DMA.
4. For next Block of read data, the step 3 repeat.
5. IRQ_XFER_FULL_DONE will be high when Particular read transfer is completed.
*/

#ifdef CONFIG_SDIO_USE_IRQD

__ilm__ static void sdvt_sdio_process_ielem(struct device *dev, sdvt_interrupt_t *ielem)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;
	u16 irq_status = ielem->irq_status;
	u16 cmd_info = ielem->cmd_info;
	u32 dat_blk_sz = ielem->data_blk_sz;
	u32 dat_blk_cnt = ielem->data_blk_cnt;

	if (irq_status & IRQ_CMD_UPDATE) {
		ctx->transfer_info.write = rbfget(cmd_info, CMD_RW) ? true : false;
		ctx->transfer_info.cmd53 = (rbfget(cmd_info, CMD_IDX) == SDIO_CMD53_CMD_IDX) ? true : false;

		if (ctx->transfer_info.write && ctx->transfer_info.cmd53) {
			ctx->transfer_info.block = rbfget(cmd_info, CMD_BLOCK_MODE) == 1 ? true : false;

			SDIO_DEBUG_CMDUPDATE
			if (ctx->transfer_info.block) {
				ctx->transfer_info.nblocks = dat_blk_cnt;
				ctx->transfer_info.block_cmdup = true;
			}
		}
	}

	if (irq_status & IRQ_XFER_FULL_DONE) {
		if (ctx->transfer_info.write) {
			SDIO_DEBUG_FULLDONE
			/* Byte mode: end of this transfer */
			if (!ctx->transfer_info.block) {
				ctx->cur_rx_req->len = ctx->transfer_info.nblocks * SDIO_BLOCK_SIZE + dat_blk_sz;
				ctx->transfer_info.nblocks = 0;

				ctx->filled(ctx->cur_rx_req);
				sdvt_sdio_next_rx_req(dev);
			}
			ctx->transfer_info.block_cmdup = false;
		} else {
			/* TX Done */
			ctx->busy = false;
		}
	}

	if (irq_status & IRQ_CMD_ABORT) {
		/* Move to irq-dispatcher to reduce latency? */
		if (!ctx->transfer_info.write) {
#ifdef CONFIG_SDIO_OOB_GPIO_INT
			control_gpio_out(CONFIG_SDIO_OOB_GPIO_INT_PIN, 0);
#else
			u8 int_pending;
			u32 v;
			v = read_sreg(dev, SREG_CCCR_STATUS);
			int_pending = rbfget(v, INT_PENDING);
			int_pending &= ~(1 << ctx->g_txchain->fn);
			rbfmod(v, INT_PENDING, int_pending);
			write_sreg(dev, v, SREG_CCCR_STATUS);
#endif
			/* IRQ_CMD_ABORT occur means the data wise sent has something wrong
			*  we need to abort the sending data
			*/
			ctx->busy = false;
		}
	}
#if CONFIG_CMD_SDIO
	sdio_dbg_irq[dbg_irq_idx].irq_status = irq_status;
	sdio_dbg_irq[dbg_irq_idx].cmd_info = cmd_info;
	dbg_irq_idx++;
	if (dbg_irq_idx >= 100) {
		dbg_irq_idx = 0;
	}
#endif
}

__ilm__ void sdvt_sdio_update_intfifo_num(struct device *dev, u8 fifo_num)
{
	u32 v;
	u8 fifo_avail;

	fifo_avail = INT_FIFO_DEPTH - fifo_num;

	v = read_sreg(dev, SREG_VENDOR_UNIQUE1);
	v &= 0xffffff;
	v |= (fifo_avail << 24);
	write_sreg(dev, v, SREG_VENDOR_UNIQUE1);
}

/* SDIO Interrupt Service Routine */
__OPT_O1__ __ilm__ static int sdvt_sdio_sw_irq(int irq, void *data)
{
	struct device *dev = data;
	struct sdvt_sdio_ctx *ctx = dev->priv;
	sdvt_interrupt_fifo_t *fifo = &ctx->int_fifo;
	sdvt_interrupt_t *ielem;
	u8 num;

    assert(irq == ctx->swi_irq);

	/* Read producer's status to see if there are interrupts to process. */

	fifo->prod_idx = ctx->global_int_fifo->prod_idx;

	/* No need to worry about wrap-around here because it will because
	 * handled by two's complementing.
	 * ex.) prod_idx = 2, cons_idx = 254 => num = 2 - 254 = -252 (0x4).
	 */

	num = fifo->prod_idx - fifo->cons_idx;

	if (num) {
		sdvt_sdio_update_intfifo_num(dev, num);
		/* Read available interrupt elements. */
		do {
			/* Integer index should be converted to uint8_t. */
			ielem = &fifo->ielems[(u8)fifo->cons_idx];

			sdvt_sdio_process_ielem(dev, ielem);

			fifo->cons_idx++;
			num--;
		} while (num);
		num = ctx->global_int_fifo->prod_idx - fifo->cons_idx;
		sdvt_sdio_update_intfifo_num(dev, num);
	}

	/* Update global fifo header. */

	ctx->global_int_fifo->cons_idx = fifo->cons_idx;

	return 0;
}

#else

/* SDIO Interrupt Service Routine */
__OPT_O1__ __ilm__ static int sdvt_sdio_irq(int irq, void *data)
{
	struct device *dev = data;
	struct sdvt_sdio_ctx *ctx = dev->priv;
	volatile u32 irq_status = 1;
	struct sdio_req *req = NULL;
	u32 v;

	while (irq_status) {
		irq_status = read_sreg(dev, SREG_IRQ_STATUS);
		write_sreg(dev, irq_status, SREG_IRQ_STATUS);

		if (irq_status & IRQ_CMD_UPDATE) {
			v = read_sreg(dev, SREG_CMD_INFO);
			ctx->transfer_info.write = rbfget(v, CMD_RW) ? true : false;
			ctx->transfer_info.cmd53 = (rbfget(v, CMD_IDX) == SDIO_CMD53_CMD_IDX) ? true : false;

			if (ctx->transfer_info.write && ctx->transfer_info.cmd53) {
					ctx->transfer_info.block = rbfget(v, CMD_BLOCK_MODE) == 1 ? true : false;
					/* Workaround[Remove after irqdispatch]
					 * Delayed FullDone of block mode happens, just ignore it
					 */
					if ((ctx->transfer_info.block_cmdup) && (!ctx->transfer_info.block)) {
						ctx->transfer_info.block_cmdup = false;
						irq_status &= ~IRQ_XFER_FULL_DONE;
						SDIO_DEBUG_WORKAROUND();
					}

					SDIO_DEBUG_CMDUPDATE();

					if (ctx->transfer_info.block) {
						ctx->transfer_info.nblocks = read_sreg(dev, SREG_DAT_BLOCK_CNT);
						ctx->transfer_info.block_cmdup = true;
					}
			}
		}

		/* RD_BLOCK_DONE status should be processed before XFER_FULL_DONE */
		if (irq_status & IRQ_RD_BLOCK_DONE) {
			ctx->cur_tx_req->len += read_sreg(dev, SREG_DAT_BLOCK_SIZE);
		}

		if (irq_status & IRQ_XFER_FULL_DONE) {
			if (ctx->transfer_info.write) {
				SDIO_DEBUG_FULLDONE();

				/* Byte mode: end of this transfer */
				if (!ctx->transfer_info.block) {
					v = read_sreg(dev, SREG_DAT_BLOCK_SIZE);  /* residual bytes */
					ctx->cur_rx_req->len = ctx->transfer_info.nblocks * SDIO_BLOCK_SIZE + v;
					ctx->transfer_info.nblocks = 0;

					ctx->filled(ctx->cur_rx_req);
					sdvt_sdio_next_rx_req(dev);
				}
				ctx->transfer_info.block_cmdup = false;
			} else {
				req = ctx->cur_tx_req;
				if (req->req_len <= req->len) {
					u8 int_pending;
					v = read_sreg(dev, SREG_CCCR_STATUS);
					int_pending = rbfget(v, INT_PENDING);
					int_pending &= ~(1 << req->fn);
					rbfmod(v, INT_PENDING, int_pending);
					write_sreg(dev, v, SREG_CCCR_STATUS);

					list_del(&req->entry);
					if (req->cmpl) {
						req->cmpl(req, true);
					}
				}
				/* else not yet complete */
			}
		}

		if (irq_status & IRQ_CMD_ABORT) {
			if (!ctx->transfer_info.write) {
				u8 int_pending;
				req = ctx->cur_tx_req;

				v = read_sreg(dev, SREG_CCCR_STATUS);
				int_pending = rbfget(v, INT_PENDING);
				int_pending &= ~(1 << req->fn);
				rbfmod(v, INT_PENDING, int_pending);
				write_sreg(dev, v, SREG_CCCR_STATUS);

				/* IRQ_CMD_ABORT occur means the data wise sent has something wrong
				*  we need to abort the sending data
				*/
				list_del(&req->entry);
				if (req->cmpl) {
					req->cmpl(req, true);
				}
			}
		}

#if CONFIG_CMD_SDIO_IRQ
		sdio_dbg_irq[dbg_irq_idx].irq_status = irq_status;
		sdio_dbg_irq[dbg_irq_idx].cmd_info = read_sreg(dev, SREG_CMD_INFO);
		dbg_irq_idx++;
		if (dbg_irq_idx >= 100) {
			dbg_irq_idx = 0;
		}
#endif
		irq_status = read_sreg(dev, SREG_IRQ_STATUS);
	}

	return 0;
}

#endif


#define DMAC_RESET				(0xf1600020)
#define DMAC1_SRC_ADDR_REG		(0xf1600048)
#define DMAC1_DST_ADDR_REG		(0xf1600050)
#define DMAC1_STATUS_REG		(0xf1600030)
#define DMAC1_CTRL_REG			(0xf1600040)
#define DMAC1_TRANS_SIZE_REG	(0xf1600044)

#define DMA_CTRL_DATA			(0x27480003)
#define DMA_COMPLETE			(0x10000)

#ifdef CONFIG_SDIO_PM
static int sdvt_sdio_busy(struct device *dev)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;
	u8 num, rx_busy = false;

	/* check sdio rx idle */
	num = ctx->global_int_fifo->prod_idx - ctx->global_int_fifo->cons_idx;

	if (ctx->rx_req_num != RX_DESC_BUF_NUM -1) {
		rx_busy = true;
	}

	return (num || ctx->busy || rx_busy);
}

static int sdvt_sdio_get_sleep_approve(struct device *dev)
{
	int can_sleep = 0;
	struct sdvt_sdio_ctx *ctx = dev->priv;

	if ((can_sleep = sdvt_sdio_setbit_and_waitclear(dev, SREG_VENDOR_UNIQUE2, SREG_VENDOR_UNIQUE2_SLEEP, 40))) {
		ctx->pmcfg.host_assumes_device_sleep = true;
	}
	return can_sleep;
}

static int sdvt_sdio_suspend(struct device *dev, u32 *duration)
{
	struct sdvt_sdio_ctx *ctx = dev->priv;
	int can_sleep = 1;

	if (sdvt_sdio_busy(dev)) {
		return -1;
	}

	if (ctx->pmcfg.host_assumes_device_sleep == false)
		can_sleep = sdvt_sdio_get_sleep_approve(dev);

	if (can_sleep)
		ctx->pmcfg.need_host_reenum = true;
	else
		sdio_dbg_log("Host not allow to sleep.\n");
	return (can_sleep ? 0 : -1);
}

static int sdvt_sdio_resume(struct device *dev)
{
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	sdvt_interrupt_fifo_t *fifo = &ctx->int_fifo;

	const char *pins[] = {"data2", "data3", "clk", "cmd", "data0", "data1"};
	struct pinctrl_pin_map *pmap[6];
	u8 *imem __maybe_unused;
	int i;
	int ret = 0;

	sdio_fifo_resume();

	sdvt_sdio_resumerxbuf(dev, CONFIG_SDIO_RCV_BUFFER_SIZE,
		CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE, CONFIG_MEMP_NUM_MBUF_DYNA_EXT);

	for (i = 0; i < ARRAY_SIZE(pins); i++) {
		pmap[i] = pinctrl_lookup_platform_pinmap(dev, pins[i]);
		if (pmap[i] == NULL) {
			assert(0);
		}

		if (pinctrl_request_pin(dev, pmap[i]->id, pmap[i]->pin) < 0) {
			assert(0);
		}
	}

#ifdef CONFIG_SDIO_USE_IRQD
	ctx->swi_irq = IRQD_SW_INT;
	ret = request_sw_irq(ctx->swi_irq, sdvt_sdio_sw_irq, dev_name(dev), 1, dev);
	if (ret) {
		sdio_dbg_log("%s: sw irq req is failed(%d)\n", __func__, ret);
		return -1;
	}

	/* Enable SW interrupt via PLIC_SW. */

	__nds__plic_sw_enable_interrupt(ctx->swi_irq);

	ctx->g_txchain =  (sdvt_tx_chain_t *)__irqdshm_start;
	ctx->global_int_fifo = (sdvt_interrupt_fifo_t *)(ctx->g_txchain + 1);
	/* prod_idx and cons_ids will be initialized by irqd. */

	ctx->int_fifo.ielems = (sdvt_interrupt_t *)(ctx->global_int_fifo + 1);

	/* Ugly! This assumes a specific SoC platform.
	 * Move this to a better place.
	 */
	/*
	 * Run irq-dispatcher
	 * (1) Download irqd binary onto D25 ILM.
	 * (2) Reset D25 to jump to ILM.
	 */

	imem = (u8 *)__irqdbin_start;

#ifdef CONFIG_USE_SYSDMA_FOR_IRQD_DN
	writel((u32)irqd_bin, DMAC1_SRC_ADDR_REG);
	writel((u32)imem, DMAC1_DST_ADDR_REG);
	writel((u32)(irqd_bin_size / 4), DMAC1_TRANS_SIZE_REG);
	writel(DMA_CTRL_DATA, DMAC1_CTRL_REG);
	while (1) {
		if ((readl(DMAC1_CTRL_REG) & 0x1) == 0)
			break;
	}
#else
	for (i = 0; i < irqd_bin_size; i++)
		imem[i] = irqd_bin[i];
#endif

	writel(ILM_BASE, SMU(RESET_VECTOR(0)));
	writel(0x01, SYS(CORE_RESET_CTRL));

#else
	ret = request_irq(dev->irq[0], sdvt_sdio_irq, dev_name(dev), dev->pri[0], dev);
	if (ret) {
		sdio_dbg_log("%s irq req is failed(%d)\n", __func__, ret);
		return -1;
	}

#endif

	sdvt_sdio_init(dev);

#ifdef CONFIG_SDIO_OOB_GPIO_INT
	enable_gpio_out(CONFIG_SDIO_OOB_GPIO_INT_PIN);
	control_gpio_out(CONFIG_SDIO_OOB_GPIO_INT_PIN, 0);
#endif

	sdio_dbg_log("sdio driver initialized\n");
	/*
	 * set cons_idx and prod_idx after D25 initialize
	 */
	fifo->cons_idx = ctx->global_int_fifo->cons_idx;
	fifo->prod_idx = ctx->global_int_fifo->prod_idx;

	return 0;
}
#endif

static int sdvt_sdio_probe(struct device *dev)
{
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	const char *pins[] = {"data2", "data3", "clk", "cmd", "data0", "data1"};
	struct pinctrl_pin_map *pmap[6];
	u8 *imem __maybe_unused;
	int ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(pins); i++) {
		pmap[i] = pinctrl_lookup_platform_pinmap(dev, pins[i]);
		if (pmap[i] == NULL) {
			goto free_pin;
		}

		if (pinctrl_request_pin(dev, pmap[i]->id, pmap[i]->pin) < 0) {
			goto free_pin;
		}
	}

	memset(ctx, 0, sizeof(struct sdvt_sdio_ctx));
	ctx->dev = dev;

	INIT_LIST_HEAD(&ctx->rx_queue);

	dev->priv = ctx;

#ifdef CONFIG_SDIO_USE_IRQD

	ctx->swi_irq = IRQD_SW_INT;
	ret = request_sw_irq(ctx->swi_irq, sdvt_sdio_sw_irq, dev_name(dev), 1, dev);
	if (ret) {
		sdio_dbg_log("%s: sw irq req is failed(%d)\n", __func__, ret);
		return -1;
	}

    /* Enable SW interrupt via PLIC_SW. */

    __nds__plic_sw_enable_interrupt(ctx->swi_irq);

	ctx->g_txchain =  (sdvt_tx_chain_t *)__irqdshm_start;
	ctx->global_int_fifo = (sdvt_interrupt_fifo_t *)(ctx->g_txchain + 1);
	/* prod_idx and cons_ids will be initialized by irqd. */

	ctx->int_fifo.ielems = (sdvt_interrupt_t *)(ctx->global_int_fifo + 1);

	/* Ugly! This assumes a specific SoC platform.
	 * Move this to a better place.
	 */
	/*
	 * Run irq-dispatcher
	 * (1) Download irqd binary onto D25 ILM.
	 * (2) Reset D25 to jump to ILM.
	 */

	imem = (u8 *)__irqdbin_start;

#ifdef CONFIG_USE_SYSDMA_FOR_IRQD_DN
	writel((u32)irqd_bin, DMAC1_SRC_ADDR_REG);
	writel((u32)imem, DMAC1_DST_ADDR_REG);
	writel((u32)(irqd_bin_size / 4), DMAC1_TRANS_SIZE_REG);
	writel(DMA_CTRL_DATA, DMAC1_CTRL_REG);
	while (1) {
		if ((readl(DMAC1_CTRL_REG) & 0x1) == 0)
			break;
	}
#else
	for (i = 0; i < irqd_bin_size; i++)
		imem[i] = irqd_bin[i];
#endif

	writel(ILM_BASE, SMU(RESET_VECTOR(0)));
	writel(0x01, SYS(CORE_RESET_CTRL));

#else

	ret = request_irq(dev->irq[0], sdvt_sdio_irq, dev_name(dev), dev->pri[0], dev);
	if (ret) {
		sdio_dbg_log("%s irq req is failed(%d)\n", __func__, ret);
		return -1;
	}

#endif

	sdvt_sdio_init(dev);

#ifdef CONFIG_SDIO_OOB_GPIO_INT
	enable_gpio_out(CONFIG_SDIO_OOB_GPIO_INT_PIN);
	control_gpio_out(CONFIG_SDIO_OOB_GPIO_INT_PIN, 0);
#endif
#ifdef CONFIG_SDIO_PM
	ctx->pmcfg.reenum_mutex = osMutexNew(NULL);
#endif

	sdio_dbg_log("sdio driver initialized\n");

#ifdef GPIO_DBG
	enable_gpio_out(23);
	enable_gpio_out(24);

	control_gpio_out(23, 0);
	control_gpio_out(24, 0);
#endif

	return 0;

free_pin:
	sdio_dbg_log("sdio pin err\n");
	for (; i >= 0; i--) {
		if (pmap[i] && pmap[i]->pin != -1)
			gpio_free(dev, pmap[i]->id, pmap[i]->pin);
	}

	return -EBUSY;
}

static declare_driver(sdio) = {
	.name  = "sdio",
	.probe = sdvt_sdio_probe,
	.ops   = &sdvt_sdio_ops,
#ifdef CONFIG_SDIO_PM
	.suspend = sdvt_sdio_suspend,
	.resume = sdvt_sdio_resume,
#endif
};

#ifdef CONFIG_CMD_SDIO
#include <cli.h>
#include <stdio.h>

void sdio_show_status(void)
{
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	struct sdio_req *req;
	int i;

	printf("[SDIO] IRQ Status register[status cmd_info]\n");
	if (sdio_dbg_irq[dbg_irq_idx].irq_status != 0) {
		for (i = dbg_irq_idx; i < 100; i++) {
			printf("%08x  %08x\n", sdio_dbg_irq[i].irq_status, sdio_dbg_irq[i].cmd_info);
		}
	}
	for (i = 0; i < dbg_irq_idx; i++) {
		printf("%08x  %08x\n", sdio_dbg_irq[i].irq_status, sdio_dbg_irq[i].cmd_info);
	}
	dbg_irq_idx = 0;
	memset(sdio_dbg_irq, 0, sizeof(sdio_dbg_irq));

	for (int i = 0; i < RX_DESC_BUF_NUM; i++) {
		req = (struct sdio_req *)(&rx_desc_buffer[i]);
		printf("%d rx_desc(%p) len(%d)\n", i, req, req->len);
	}

	printf("[SDIO] tx busy %u\n", ctx->busy);

	printf("TX fn %u cnt %u cons_idx %u dma_addr %u\n",
		ctx->g_txchain->fn, ctx->g_txchain->cnt, ctx->g_txchain->cons_idx, ctx->g_txchain->dma_addr);
	for (i = 0; i < ctx->g_txchain->cnt; i++) {
		printf("%d len %u addr %u\n", i, ctx->g_txchain->txelems[i].len, ctx->g_txchain->txelems[i].addr);
	}
#ifdef CONFIG_SDIO_USE_IRQD
	u8 num;

	num = ctx->global_int_fifo->prod_idx - ctx->global_int_fifo->cons_idx;

	printf("global int num %u prod_idx %d cons_idx %d\n", num,
		ctx->global_int_fifo->prod_idx, ctx->global_int_fifo->cons_idx);
#endif
}

int do_sdio_read_status(int argc, char *argv[])
{
	u32 v;
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;

	v = read_sreg(ctx->dev, SREG_CARD_STATUS);
	printf("SREG_CARD_STATUS=0x%x\n", v);

	return CMD_RET_SUCCESS;
}

int do_sdio_write_status(int argc, char *argv[])
{
	u32 v, input;
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	v = read_sreg(ctx->dev, SREG_CARD_STATUS);
	input = atoi(argv[1]);

	v = v | input;
	printf("set SREG_CARD_STATUS=0x%x\n", v);

	write_sreg(ctx->dev, v, SREG_CARD_STATUS);

	return CMD_RET_SUCCESS;
}

int sdio_cli_read_flow_ctrl(void)
{
	u32 v;
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;

	v = read_sreg(ctx->dev, SREG_VENDOR_UNIQUE1);
	return v;
}

int do_sdio_read_reg(int argc, char *argv[])
{
	u32 v, reg;
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	reg = atoi(argv[1]);
	v = read_sreg(ctx->dev, reg);
	printf("read reg[0x%x]=0x%x\n", reg, v);

	return CMD_RET_SUCCESS;
}

int do_sdio_write_reg(int argc, char *argv[])
{
	u32 v, reg;
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	reg = atoi(argv[1]);
	v = atoi(argv[2]);

	printf("write reg[0x%x]=0x%x\n", reg, v);
	write_sreg(ctx->dev, v, reg);

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_SDIO_PM
int do_sdio_restore(int argc, char *argv[])
{
	struct device *dev;
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;
	dev = ctx->dev;

	write_sreg(dev, 0x2000706, 0x4);
	//write_sreg(dev, 0x31e, 0xc);
	write_sreg(dev, 0x1e, 0xc);
	write_sreg(dev, 0x6, 0x14);

	write_sreg(dev, 0x21d4, 0x88);
	write_sreg(dev, 0x8, 0x8c);
	write_sreg(dev, 0x24, 0x90);
	write_sreg(dev, 0x2, 0xA0);
	write_sreg(dev, 0x4, 0xA4);
	write_sreg(dev, 0xF0, 0xA8);
	sdio_start(dev, 2, CONFIG_SDIO_RCV_BUFFER_SIZE,
			CONFIG_SDIO_FIFO_MIN_LINEAR_SIZE, CONFIG_MEMP_NUM_MBUF_DYNA_EXT);
	//write_sreg(dev, 0xff170270, 0x1c);

	return 0;
}

int do_sdio_dump_cis_ram(int argc, char *argv[])
{
	int i = 0;
	struct device *dev;
	struct sdvt_sdio_ctx *ctx = &sdio_ctx;

	dev = ctx->dev;

	for (i = 0; i < 0xac; i += 4) {
		/* func(n) cis ptr */
		printf("sreg[%x] 0x%x\n", i, read_sreg(dev, i));
	}

	return 0;
}
#endif
#endif
