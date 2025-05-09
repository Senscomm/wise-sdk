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

#include <stdint.h>
#include <hal/kernel.h>
#include <hal/console.h>

#include "soc.h"
#include "mmap.h"
#include "pmu.h"

/* direct registers */
#define PMU_DLCPD_MSR_OFFSET		0x00
#define PMU_DLCPD_MPACR_OFFSET		0x04
#define PMU_DLCPD_MPADR_OFFSET		0x08
#define PMU_DLCPD_IMR_OFFSET		0x0C
#define PMU_DLCPD_IFR_OFFSET		0x10
#define PMU_DLCPD_IOK_IFR_OFFSET	0x14
#define PMU_DLCPD_IDL_IFR_OFFSET	0x18
#define PMU_DLCPD_IDN_IFR_OFFSET	0x1C
#define PMU_DLCPD_IUP_IFR_OFFSET	0x20

#define PMU_DLCPD_MPACR_PAS			(1 << 28)
#define PMU_DLCPD_MPACR_PAD_WRITE	(0 << 24)
#define PMU_DLCPD_MPACR_PAD_READ	(1 << 24)

#define PMU_DLCPD_IMR_SCU_FL_M		(1 << 8)
#define PMU_DLCPD_IMR_SCU_OK_M		(1 << 7)
#define PMU_DLCPD_IMR_PICL_OK_M		(1 << 6)

#define PMU_DLCPD_IFR_SCU_FL_F		(1 << 8)
#define PMU_DLCPD_IFR_SCU_OK_F		(1 << 7)
#define PMU_DLCPD_IFR_PICL_OK_F		(1 << 6)


/* indirect registers */
#define PMU_BLOCK_ADDR(block)		((block) << 8)
#define PMU_BLOCK_ADDR_WIU			0x01
#define PMU_BLOCK_ADDR_ICU			0x10
#define PMU_BLOCK_ADDR_DMU			0x30

#define PMU_WIU_ISPMR				0x00
#define PMU_WIU_IFR					0x02
#define PMU_WIU_ICR_0				0x04
#define PMU_WIU_ICR_1				0x06
#define PMU_WIU_ICR_2				0x08
#define PMU_WIU_ICR_3				0x0A
#define PMU_WIU_ICR_4				0x0C
#define PMU_WIU_ICR_5				0x0E
#define PMU_WIU_ICR_6				0x10
#define PMU_WIU_ICR_7				0x12
#define PMU_WIU_ICR_8				0x14
#define PMU_WIU_ICR_9				0x16
#define PMU_WIU_ICR_10				0x18
#define PMU_WIU_ICR_11				0x1A
#define PMU_WIU_ICR_12				0x1C
#define PMU_WIU_ICR_13				0x1E
#define PMU_WIU_ICR_14				0x20
#define PMU_WIU_ICR_15				0x22

#define PMU_ICU_MT_SR				0x00
#define PMU_ICU_MT_CR				0x02
#define PMU_ICU_IM_SR				0x04
#define PMU_ICU_CGM_SR				0x06
#define PMU_ICU_RM_SR				0x08
#define PMU_ICU_BBM_SR				0x0A
#define PMU_ICU_BBM_CR				0x0C
#define PMU_ICU_CGO_CR				0x0E
#define PMU_ICU_RO_CR				0x10
#define PMU_ICU_BBGO_CR				0x12
#define PMU_ICU_RPC_CR				0x14
#define PMU_ICU_EPC_CR				0x16
#define PMU_ICU_ABB_SR				0x18
#define PMU_ICU_ABB_CFG_CR			0x1C
#define PMU_ICU_ABB_ANA_CFG_CR		0x1E
#define PMU_ICU_ABB_AG_CFG_CR		0x20

#define PMU_DMU_AONLDO				0x00
#define PMU_DMU_DCDC				0x02
#define PMU_DMU_DLDO				0x04

#define PMU_DMU_RSR					0x00
#define PMU_DMU_MINOR				0x02
#define PMU_DMU_MAXOR				0x04
#define PMU_DMU_OCR					0x06
#define PMU_DMU_CGNO_CR				0x08
#define PMU_DMU_CGMO_CR				0x0A
#define PMU_DMU_CGLO_CR				0x0C
#define PMU_DMU_RNO_CR				0x0E
#define PMU_DMU_RMO_CR				0x10
#define PMU_DMU_RLO_CR				0x12
#define PMU_DMU_RRO_CR				0x14
#define PMU_DMU_BBGOO_CR			0x16
#define PMU_DMU_TCR					0x18
#define PMU_DMU_TRR					0x1A

/* Senscomm specific */
#define PMU_WIU_SLEEP				PMU_WIU_ICR_1
#define PMU_WIU_WAKEUP				PMU_WIU_ICR_2
#define PMU_WIU_IDLE				PMU_WIU_ICR_3

/* only for debug */
#define PMU_CHECK_WRITE_VALUE		0
#define PMU_CHECK_STATUS			1

#if PMU_CHECK_STATUS
static struct {
	enum pmu_aon_volt aon;
	enum pmu_dldo_volt dldo;
	enum pmu_dcdc_volt dcdc;
} pmu_status;
#endif

void pmu_indirect_write(uint16_t addr, uint32_t val)
{
	uint32_t v;

	/* mask the PICL_OK interrupt */
	v = readl(PMU_BASE_ADDR + PMU_DLCPD_IMR_OFFSET);
	v |= PMU_DLCPD_IMR_PICL_OK_M;
	writel(v, PMU_BASE_ADDR + PMU_DLCPD_IMR_OFFSET);

	/* if remain, clear PICL_OK_F */
	v = readl(PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);
	if (v & PMU_DLCPD_IFR_PICL_OK_F) {
		v |= PMU_DLCPD_IFR_PICL_OK_F;
		writel(v, PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);
	}

	/* write PICL access data */
	writel(val, PMU_BASE_ADDR + PMU_DLCPD_MPADR_OFFSET);

	/* set PICL access control */
	v = PMU_DLCPD_MPACR_PAS | \
		PMU_DLCPD_MPACR_PAD_WRITE | \
		addr;
	writel(v, PMU_BASE_ADDR + PMU_DLCPD_MPACR_OFFSET);

	/* wait PICL_OK */
	do {
		v = readl(PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);
	} while (!(v & PMU_DLCPD_IFR_PICL_OK_F));

	/* clear PICL_OK_F */
	writel(v, PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);
}

uint32_t pmu_indirect_read(uint16_t addr)
{
	uint32_t v;
	uint32_t val;

	/* mask the PICL_OK interrupt */
	v = readl(PMU_BASE_ADDR + PMU_DLCPD_IMR_OFFSET);
	v |= PMU_DLCPD_IMR_PICL_OK_M;
	writel(v, PMU_BASE_ADDR + PMU_DLCPD_IMR_OFFSET);

	/* if remain, clear PICL_OK_F */
	v = readl(PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);
	if (v & PMU_DLCPD_IFR_PICL_OK_F) {
		v |= PMU_DLCPD_IFR_PICL_OK_F;
		writel(v, PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);
	}

	/* set PICL access control */
	v = PMU_DLCPD_MPACR_PAS | \
		PMU_DLCPD_MPACR_PAD_READ | \
		addr;
	writel(v, PMU_BASE_ADDR + PMU_DLCPD_MPACR_OFFSET);

	/* wait PICL_OK */
	do {
		v = readl(PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);
	} while (!(v & PMU_DLCPD_IFR_PICL_OK_F));

	/* clear PICL_OK_F */
	writel(v, PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);

	/* read PICL access data */
	val = readl(PMU_BASE_ADDR + PMU_DLCPD_MPADR_OFFSET);

	return val;
}

static int pmu_irq(int irq, void *data)
{
	uint32_t ifr;

	ifr = readl(PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);

	if (ifr & ((1 << 7) | (1 << 6))) {
		writel(ifr , PMU_BASE_ADDR + PMU_DLCPD_IFR_OFFSET);
	} else if (ifr & ((1 << 8) | (1 << 2))) {
#if PMU_CHECK_STATUS
		printk("PMU status\n");
		printk("IFR: 0x%08x\n", ifr);
		printk("IOK: 0x%08x\n", readl(PMU_BASE_ADDR + PMU_DLCPD_IOK_IFR_OFFSET));
		printk("IDL: 0x%08x\n", readl(PMU_BASE_ADDR + PMU_DLCPD_IDL_IFR_OFFSET));
		printk("IDN: 0x%08x\n", readl(PMU_BASE_ADDR + PMU_DLCPD_IDN_IFR_OFFSET));
		printk("IUO: 0x%08x\n", readl(PMU_BASE_ADDR + PMU_DLCPD_IUP_IFR_OFFSET));
		printk("aon : %d\n", pmu_status.aon);
		printk("dldo: %d\n", pmu_status.dldo);
		printk("dcdc: %d\n", pmu_status.dcdc);
#endif
		assert(0);
	}

	return 0;
}

void pmu_irq_enable(void)
{
	uint32_t v;

	/* clear the mask to enable the interrupt */
	v = readl(PMU_BASE_ADDR + PMU_DLCPD_IMR_OFFSET);
	v &= ~PMU_DLCPD_IMR_SCU_OK_M;
	writel(v, PMU_BASE_ADDR + PMU_DLCPD_IMR_OFFSET);
}

void pmu_irq_disable(void)
{
	uint32_t v;

	/* set the mask to disable the interrupt */
	v = readl(PMU_BASE_ADDR + PMU_DLCPD_IMR_OFFSET);
	v |= PMU_DLCPD_IMR_SCU_OK_M;
	writel(v, PMU_BASE_ADDR + PMU_DLCPD_IMR_OFFSET);
}

void pmu_init(void)
{
	uint32_t addr;

	request_irq(IRQn_PMU, pmu_irq, "pmu", 1, NULL);
	pmu_irq_disable();

	/* LDO max/min setting */
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DLDO) | PMU_DMU_MINOR;
	pmu_indirect_write(addr, PMU_DLDO_0V7);

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DLDO) | PMU_DMU_MAXOR;
	pmu_indirect_write(addr, PMU_DLDO_0V9);

	/* DCDC max/min setting */
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DCDC) | PMU_DMU_MINOR;
	pmu_indirect_write(addr, PMU_DCDC_0V9);

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DCDC) | PMU_DMU_MAXOR;
	pmu_indirect_write(addr, PMU_DCDC_1V4);

	/* AON max/min setting */
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_AONLDO) | PMU_DMU_MINOR;
	pmu_indirect_write(addr, PMU_AON_0V6);

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_AONLDO) | PMU_DMU_MAXOR;
	pmu_indirect_write(addr, PMU_AON_1V0);

#ifdef CONFIG_PM_AON_VOLTAGE_CTRL_OCR
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_AONLDO) | PMU_DMU_RNO_CR;
	pmu_indirect_write(addr, PMU_AON_0V7);
#endif
}

#if 0
void pmu_set_wakeup_mode(enum pmu_mode mode)
{
	uint32_t addr;

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | PMU_WIU_WAKEUP;
	pmu_indirect_write(addr, mode);
}

void pmu_set_sleep_mode(enum pmu_mode mode)
{
	uint32_t addr;

	if (mode == ACTIVE_TO_IDLE || mode == ACTIVE_TO_IDLE_IO_OFF) {
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | PMU_WIU_IDLE;
	} else {
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | PMU_WIU_SLEEP;
	}
	pmu_indirect_write(addr, mode);
}
#endif

void pmu_set_mode(enum pmu_mode mode)
{
	uint32_t addr;

	if (mode == ACTIVE_TO_IDLE || mode == ACTIVE_TO_IDLE_IO_OFF) {
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | PMU_WIU_IDLE;
	} else if (mode == WAKEUP_TO_ACTIVE || mode == WAKEUP_TO_ACTIVE_IO_OFF) {
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | PMU_WIU_WAKEUP;
	} else {
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | PMU_WIU_SLEEP;
	}

	pmu_indirect_write(addr, mode);
}

void pmu_set_dldo_volt(enum pmu_dldo_volt volt)
{
	uint32_t addr;

#if PMU_CHECK_STATUS
	pmu_status.dldo = volt;
#endif

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DLDO) | PMU_DMU_RNO_CR;
	pmu_indirect_write(addr, volt);

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DLDO) | PMU_DMU_OCR;
	pmu_indirect_write(addr, 0x01);

#if PMU_CHECK_WRITE_VALUE
	printk("DLDO\n");
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DLDO) | PMU_DMU_MINOR;
	printk("[%08x]: %08x\n", addr, pmu_indirect_read(addr));
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DLDO) | PMU_DMU_RNO_CR;
	printk("[%08x]: %08x\n", addr, pmu_indirect_read(addr));
#endif
}

void pmu_set_dcdc_volt(enum pmu_dcdc_volt volt)
{
	uint32_t addr;

#if PMU_CHECK_STATUS
	pmu_status.dcdc = volt;
#endif

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DCDC) | PMU_DMU_RNO_CR;
	pmu_indirect_write(addr, volt);

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DCDC) | PMU_DMU_OCR;
	pmu_indirect_write(addr, 0x01);

#if PMU_CHECK_WRITE_VALUE
	printk("DCDC\n");
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DCDC) | PMU_DMU_MINOR;
	printk("[%08x]: %08x\n", addr, pmu_indirect_read(addr));
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_DCDC) | PMU_DMU_RNO_CR;
	printk("[%08x]: %08x\n", addr, pmu_indirect_read(addr));
#endif
}


void pmu_set_aon_qLR(enum pmu_aon_volt volt)
{
	uint16_t addr;
	uint32_t val;

#if PMU_CHECK_STATUS
	pmu_status.aon = volt;
#endif

	/*
	 * AON power is fixed to qLR,
	 * which is used in low power mode.
	 */

	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_AONLDO) | PMU_DMU_TRR;
	pmu_indirect_write(addr, 0x100 | volt);

	/* iLR to qLR */
	addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_AONLDO) | PMU_DMU_TCR;
	pmu_indirect_write(addr, 0x81);
	pmu_indirect_write(addr, 0x81);

	while (1) {
		val = pmu_indirect_read(addr);
		if (val == 0x82) {
			break;
		}
	}
}

void pmu_ctlr_aon(uint8_t flag)
{
#ifdef CONFIG_PM_AON_VOLTAGE_CTRL_OCR
	return;
#else
	uint16_t addr;

	if (flag == PMU_AON_VOLTAGE_qLR_0V7) {
		/*
		 * AON voltage change iLR 0.8v -> qLR 0.75v -> qLR 0.7v
		 */
		pmu_set_aon_qLR(PMU_AON_0V75);
		pmu_set_aon_qLR(PMU_AON_0V7);
	} else if (flag == PMU_AON_VOLTAGE_qLR_0V8) {
		/*
		 * AON voltage change iLR 0.8v -> qLR 0.8v
		 */
		pmu_set_aon_qLR(PMU_AON_0V8);
	} else {
		/*
		 * AON voltage change qLR 0.7v -> qLR 0.8v -> iLR 0.8v
		 */
		pmu_set_aon_qLR(PMU_AON_0V8);
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU + PMU_DMU_AONLDO) | PMU_DMU_TRR;
		pmu_indirect_write(addr, 0x200 | PMU_AON_0V8);

	}
#endif
}

#ifdef CONFIG_CMD_PMU

#include <cli.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int do_pmu_read(int argc, char *argv[])
{
	uint16_t addr;
	uint32_t val;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	addr = strtoul(argv[1], NULL, 16);

	val = pmu_indirect_read(addr);
	printf("[0x%04x]: 0x%08x\n", addr, val);

	return CMD_RET_SUCCESS;
}

static int do_pmu_write(int argc, char *argv[])
{
	uint16_t addr;
	uint32_t val;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	addr = strtoul(argv[1], NULL, 16);
	val = strtoul(argv[2], NULL, 16);

	pmu_indirect_write(addr, val);

	return CMD_RET_SUCCESS;
}

static int do_pmu_dump(int argc, char *argv[])
{
	uint32_t addr;
	uint32_t i;

	printf("WIU ISPMR :%08x\n", pmu_indirect_read(PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | PMU_WIU_ISPMR));
	printf("WIU IFR   :%08x\n", pmu_indirect_read(PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | PMU_WIU_IFR));
	for (i = 0; i < 16; i++) {
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_WIU) | (PMU_WIU_ICR_0 + i * 2);
		printf("WIU ICR_%02d:%08x\n", i, pmu_indirect_read(addr));
	}

	for (i = 0; i < 16; i++) {
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_ICU) | (i * 2);
		printf("ICU addr 0x%02x:%08x\n", addr, pmu_indirect_read(addr));
	}

	for (i = 0; i < 16; i++) {
		addr = PMU_BLOCK_ADDR(PMU_BLOCK_ADDR_DMU) | (i * 2);
		printf("DMU addr 0x%02x:%08x\n", addr, pmu_indirect_read(addr));
	}

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd pmu_cmd[] = {
	CMDENTRY(read, do_pmu_read, "", ""),
	CMDENTRY(write, do_pmu_write, "", ""),
	CMDENTRY(dump, do_pmu_dump, "", ""),
};

static int do_pmu(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], pmu_cmd, ARRAY_SIZE(pmu_cmd));
	if (cmd == NULL) {
		return CMD_RET_USAGE;
	}

	return cmd->handler(argc, argv);
}

CMD(pmu, do_pmu,
		"CLI commands for PMU",
		"pmu read [addr]" OR
		"pmu write [addr] [val]" OR
		"pmu dump"
	);

#endif
