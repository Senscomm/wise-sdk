#include "pm_rram.h"

#ifdef CONFIG_PM_SCM2010

#if __riscv_xlen == 64
	#define portWORD_SIZE 8
	#define store_x sd
	#define load_x ld
#elif __riscv_xlen == 32
	#define store_x sw
	#define load_x lw
	#define portWORD_SIZE 4
#else
	#error Assembler did not define __riscv_xlen
#endif

#define NUM_REGS_TO_SAVE ( \
		30 /* standard registers */ + \
		1 /* CSR mtvec */ + \
		1 /* CSR mstatus (MIE) */ + \
		1 /* CSR mie (MIE) */ + \
		1 /* CSR mmisc_ctl */ + \
		1 /* CSR mxstatus */ + \
		1 /* CSR m_cachectl */ + \
		1 /* dummy */ \
	)
#define portCONTEXT_SIZE ( NUM_REGS_TO_SAVE * portWORD_SIZE )

/*
 * N22 ILM
 * no need to save, but restored from the lma on flash without DMA
 */
#define N22_ILM_SIZE				0x8000
#define N22_ILM_ADDR_START			0x00000000
#define N22_ILM_ADDR_END			(N22_ILM_ADDR_START + N22_ILM_SIZE)

/*
 * D25 ILM
 * need to save in stand alone mode, restored using DMA
 * ipce      : no need to save, but restored from the lma on flash with DMA
 * standalone: save to ram, restored with DMA
 */
#define D25_ILM_SIZE				0x8000
#define D25_ILM_ADDR_START			0xA0000000
#define D25_ILM_ADDR_END			(D25_ILM_ADDR_START + D25_ILM_SIZE)
#define D25_ILM_ADDR_SAVE_START		(CONFIG_PM_HIBERNATION_PARTITION_ADDR)
#define D25_ILM_ADDR_SAVE_END		(D25_ILM_ADDR_SAVE_START + D25_ILM_SIZE)
#define D25_ILM_DMA_SIZE			(D25_ILM_SIZE / 4)

/*
 * N22 DLM
 * hibernation: save to flash, restored without DMA
 */
#define N22_DLM_SIZE				0x30000
#define N22_DLM_ADDR_START			0x00200000
#define N22_DLM_ADDR_END			(N22_DLM_ADDR_START + N22_DLM_SIZE)
#define N22_DLM_ADDR_SAVE_START		(D25_ILM_ADDR_SAVE_END)
#define N22_DLM_ADDR_SAVE_END		(N22_DLM_ADDR_SAVE_START + N22_DLM_SIZE)

/*
 * D25 DLM
 * hibernation: save to flash, restored with DMA
 */
#define D25_DLM_SIZE				0x30000
#define D25_DLM_ADDR_START			0xA0200000
#define D25_DLM_ADDR_END			(D25_DLM_ADDR_START + D25_DLM_SIZE)
#define D25_DLM_ADDR_SAVE_START		(N22_DLM_ADDR_SAVE_END)
#define D25_DLM_ADDR_SAVE_END		(D25_DLM_ADDR_SAVE_START + D25_DLM_SIZE)
#define D25_DLM_DMA_SIZE			(D25_DLM_SIZE / 4)


/* return address */
#define RA_WAIT						s2
#define RA_CLEAR_FIFO				s3
#define RA_EXEC_CMD					s4
#define RA_READ_STATUS				s5
#define RA_WRITE_ENABLE				s6
#define RA_WIP						s7

/* register for the dma */
#define DMA_STATUS_REG				0xf1600030
#define DMA_CTRL_REG				0xf1600040
#define DMA_TRANS_SIZE_REG			0xf1600044
#define DMA_SRC_ADDR_REG			0xf1600048
#define DMA_DST_ADDR_REG			0xf1600050

#define DMA_CTRL_DATA				0x2748000F
#define DMA_COMPLETE				0x10000

/* register for the spi flash */
#define SPI_INTF_TIMING_REG			0xf0900040
#define SPI_MEM_ACCESS_CTRL_REG		0xf0900050
#define SPI_MEM_CTRL_CHANGE			0x00000010
#define SYS_SPI0_CFG				0xf1700224

/* register used by pm */
#define SMU_D25_RESET_VECTOR		0xF0100200
#define SMU_N22_RESET_VECTOR		0xF0100204
#define SMU_LOWPOWER				0xF010020C

/* register for the crypto */
#define SYS_CRYPTO_CFG				0xF17002A4
#define SYS_CRYPTO_IV				0xF17002B8
#define SYS_CRYPTO_KEY				0xF17002C8

/* register for core reset */
#define SYS_CORE_RESET_CTRL			0xf1700218

.section .rram, "ax"

.extern pxCurrentTCB
.extern __text_lma
.extern __d25_ilm_save
.extern __enc_buffer

.align 2

#ifdef CONFIG_PM_GPIO_DBG
/****************************************************************************
 * Name: pm_gpio_dbg_set
 ****************************************************************************/

.func
pm_gpio_dbg_set:
	li		a1, 0xf0700014
	li		a2, 1
	sll		a2, a2, a0
	lw		a3, 0(a1)
	or		a3, a3, a2
	sw		a3, 0(a1)
	ret
.endfunc

/****************************************************************************
 * Name: pm_gpio_dbg_clr
 ****************************************************************************/

.func
pm_gpio_dbg_clr:
	li		a1, 0xf0700014
	li		a2, 1
	sll		a2, a2, a0
	not		a2, a2
	lw		a3, 0(a1)
	and		a3, a3, a2
	sw		a3, 0(a1)
	ret
.endfunc
#endif

/****************************************************************************
 * Name: pm_save_context
 ****************************************************************************/

.macro pm_save_context

	/* backup all the cpu registers */
	addi    sp, sp, -portCONTEXT_SIZE
	store_x x1, 1 * portWORD_SIZE( sp )
	store_x x5, 2 * portWORD_SIZE( sp )
	store_x x6, 3 * portWORD_SIZE( sp )
	store_x x7, 4 * portWORD_SIZE( sp )
	store_x x8, 5 * portWORD_SIZE( sp )
	store_x x9, 6 * portWORD_SIZE( sp )
	store_x x10, 7 * portWORD_SIZE( sp )
	store_x x11, 8 * portWORD_SIZE( sp )
	store_x x12, 9 * portWORD_SIZE( sp )
	store_x x13, 10 * portWORD_SIZE( sp )
	store_x x14, 11 * portWORD_SIZE( sp )
	store_x x15, 12 * portWORD_SIZE( sp )
#ifndef __riscv_32e
	store_x x16, 13 * portWORD_SIZE( sp )
	store_x x17, 14 * portWORD_SIZE( sp )
	store_x x18, 15 * portWORD_SIZE( sp )
	store_x x19, 16 * portWORD_SIZE( sp )
	store_x x20, 17 * portWORD_SIZE( sp )
	store_x x21, 18 * portWORD_SIZE( sp )
	store_x x22, 19 * portWORD_SIZE( sp )
	store_x x23, 20 * portWORD_SIZE( sp )
	store_x x24, 21 * portWORD_SIZE( sp )
	store_x x25, 22 * portWORD_SIZE( sp )
	store_x x26, 23 * portWORD_SIZE( sp )
	store_x x27, 24 * portWORD_SIZE( sp )
	store_x x28, 25 * portWORD_SIZE( sp )
	store_x x29, 26 * portWORD_SIZE( sp )
	store_x x30, 27 * portWORD_SIZE( sp )
	store_x x31, 28 * portWORD_SIZE( sp )
#endif
	store_x x3, 29 * portWORD_SIZE( sp )
	store_x x4, 30 * portWORD_SIZE( sp )

	/* backup csrs */
	csrr t0, mtvec
	store_x t0, 31 * portWORD_SIZE( sp )
	csrr t0, mstatus
	store_x t0, 32 * portWORD_SIZE( sp )
	csrr t0, mie
	store_x t0, 33 * portWORD_SIZE( sp )
	csrr t0, mmisc_ctl
	store_x t0, 34 * portWORD_SIZE( sp )
	csrr t0, mxstatus
	store_x t0, 35 * portWORD_SIZE( sp )
#if defined(CONFIG_ICACHE_ENABLE) \
		|| defined(CONFIG_DCACHE_ENABLE)
	csrr t0, mcache_ctl
	store_x t0, 36 * portWORD_SIZE( sp )
#endif

	/* backup sp */
	load_x t0, pxCurrentTCB
	store_x sp, 0( t0 )

	/* invalidate cache */
	fence.i

.endm

/****************************************************************************
 * Name: pm_restore_context
 ****************************************************************************/

.macro pm_restore_context
	/* restore sp */
	load_x  t0, pxCurrentTCB
	load_x  sp, 0( t0 )

	/* restore csrs - need more? */
	load_x  t0, 31 * portWORD_SIZE( sp )
	csrw mtvec, t0
	load_x  t0, 32 * portWORD_SIZE( sp )
	csrw mstatus, t0
	load_x  t0, 33 * portWORD_SIZE( sp )
	csrw mie, t0
	load_x  t0, 34 * portWORD_SIZE( sp )
	csrw mmisc_ctl, t0
	load_x  t0, 35 * portWORD_SIZE( sp )
	csrw mxstatus, t0
#if defined(CONFIG_ICACHE_ENABLE) \
		|| defined(CONFIG_DCACHE_ENABLE)
	load_x  t0, 36 * portWORD_SIZE( sp )
	csrw mcache_ctl, t0
#endif

	/* restore all the cpu registers */
	load_x  x1, 1 * portWORD_SIZE( sp )		/* ra */
	load_x  x5, 2 * portWORD_SIZE( sp )		/* t0 */
	load_x  x6, 3 * portWORD_SIZE( sp )		/* t1 */
	load_x  x7, 4 * portWORD_SIZE( sp )		/* t2 */
	load_x  x8, 5 * portWORD_SIZE( sp )		/* s0/fp */
	load_x  x9, 6 * portWORD_SIZE( sp )		/* s1 */
	load_x  x10, 7 * portWORD_SIZE( sp )	/* a0 */
	load_x  x11, 8 * portWORD_SIZE( sp )	/* a1 */
	load_x  x12, 9 * portWORD_SIZE( sp )	/* a2 */
	load_x  x13, 10 * portWORD_SIZE( sp )	/* a3 */
	load_x  x14, 11 * portWORD_SIZE( sp )	/* a4 */
	load_x  x15, 12 * portWORD_SIZE( sp )	/* a5 */
#ifndef __riscv_32e
	load_x  x16, 13 * portWORD_SIZE( sp )	/* a6 */
	load_x  x17, 14 * portWORD_SIZE( sp )	/* a7 */
	load_x  x18, 15 * portWORD_SIZE( sp )	/* s2 */
	load_x  x19, 16 * portWORD_SIZE( sp )	/* s3 */
	load_x  x20, 17 * portWORD_SIZE( sp )	/* s4 */
	load_x  x21, 18 * portWORD_SIZE( sp )	/* s5 */
	load_x  x22, 19 * portWORD_SIZE( sp )	/* s6 */
	load_x  x23, 20 * portWORD_SIZE( sp )	/* s7 */
	load_x  x24, 21 * portWORD_SIZE( sp )	/* s8 */
	load_x  x25, 22 * portWORD_SIZE( sp )	/* s9 */
	load_x  x26, 23 * portWORD_SIZE( sp )	/* s10 */
	load_x  x27, 24 * portWORD_SIZE( sp )	/* s11 */
	load_x  x28, 25 * portWORD_SIZE( sp )	/* t3 */
	load_x  x29, 26 * portWORD_SIZE( sp )	/* t4 */
	load_x  x30, 27 * portWORD_SIZE( sp )	/* t5 */
	load_x  x31, 28 * portWORD_SIZE( sp )	/* t6 */
#endif
	load_x  x3, 29 * portWORD_SIZE( sp )	/* gp */
	load_x  x4, 30 * portWORD_SIZE( sp )	/* tp */
	addi	sp, sp, portCONTEXT_SIZE
.endm

/****************************************************************************
 * Name: pm_copy
 ****************************************************************************/

.func

pm_copy:
	lw		t0, (a0)
	sw		t0, (a1)
	addi	a0, a0, 4
	addi	a1, a1, 4
	blt		a1, a2, pm_copy
	ret

.endfunc

/****************************************************************************
 * Name: pm_dma_copy
 ****************************************************************************/

.func

pm_dma_copy:
	li		t0, DMA_SRC_ADDR_REG
	sw		a0, (t0)
	li		t0, DMA_DST_ADDR_REG
	sw		a1, (t0)
	li		t0, DMA_TRANS_SIZE_REG
	sw		a2, (t0)
	li		t0, DMA_CTRL_REG
	li		a0, DMA_CTRL_DATA
	sw		a0, (t0)
pm_wait_dma_done:
	lw		a0, (t0)
	andi	a0, a0, 0x1
	bnez	a0, pm_wait_dma_done

	li		t0, DMA_STATUS_REG
	lw		a0, (t0)
	sw		a0, (t0)

	ret

.endfunc

/****************************************************************************
 * flash operation
 ****************************************************************************/

wait:
	li		a1, 0xf0900034
	lw		a2, 0(a1)
	and		a2, a2, t2
	bnez	a2, wait
	jalr	x0, RA_WAIT, 0x0

clear_fifo:
	li		a1, 0xf0900030
	lw		a2, 0(a1)
	ori		a2, a2, 0x2
	ori		a2, a2, 0x4
	sw		a2, 0(a1)
	jalr	x0, RA_CLEAR_FIFO, 0x0

exec_cmd:
	li		t2, 0x1
	jal		RA_WAIT, wait
	jal		RA_CLEAR_FIFO, clear_fifo
	li		a2, 0xf0900020
	sw		t1, 0(a2)
	li		a2, 0xf0900024
	sw		t0, 0(a2)
	jalr	x0, RA_EXEC_CMD, 0x0

read_status:
	li		t0, 0x5
	li		t1, 0x42000000
	jal		RA_EXEC_CMD, exec_cmd
	li		t2, 0x4000
	jal		RA_WAIT, wait
	li		a1, 0xf090002c
	lw		a2, 0(a1)
	jalr	x0, RA_READ_STATUS, 0x0

write_enable:
	li		t0, 0x6
	li		t1, 0x47000000
	jal		RA_EXEC_CMD, exec_cmd
	jal		RA_READ_STATUS, read_status
	andi	a2, a2, 0x2
	beqz	a2, write_enable
	jalr	x0, RA_WRITE_ENABLE, 0x0

wip:
	jal		RA_READ_STATUS, read_status
	andi	a2, a2, 0x1
	bnez	a2, wip
	jalr	x0, RA_WIP, 0x0


/****************************************************************************
 * Name: pm_flash_check_cancel
 ****************************************************************************/

.macro pm_flash_check_cancel check_continue

	/* 10-6 check if saving data is canceled. */
	la		a0, SCM2010_PM_RRAM_INFO_ADDR
	lbu		a1, SCM2010_PM_RRAM_SYNC_FLAG( a0 )
	andi	a1, a1, SCM2010_PM_RRAM_SAVE_CANCEL
	beqz	a1, \check_continue
	j		restore_context

.endm

/****************************************************************************
 * Name: ske_start_and_wait
 ****************************************************************************/

ske_start_and_wait:
	li		a0, 0xe0800008
wait_sr1:
	lw		a1, 0(a0)
	andi	a1, a1, 1
	bnez	a1, wait_sr1

	li		a0, 0xe0800000
	lw		a1, 0(a0)
	ori		a1, a1, 1
	sw		a1, 0(a0)

	li		a0, 0xe080000c
wait_sr2:
	lw		a1, 0(a0)
	andi	a1, a1, 1
	beqz	a1, wait_sr2

	li		a1, 0
	sw		a1, 0(a0)

	jalr	x0, t4, 0x00

/****************************************************************************
 * Name: ske_iv_convert
 ****************************************************************************/

.macro ske_iv_convert
	la		a0, SCM2010_PM_RRAM_INFO_ADDR
	lw		t0, SCM2010_PM_RRAM_CRYPTO_IV0( a0 )
	lw		t1, SCM2010_PM_RRAM_CRYPTO_IV1( a0 )
	lw		t2, SCM2010_PM_RRAM_CRYPTO_IV2( a0 )
	lw		t3, SCM2010_PM_RRAM_CRYPTO_IV3( a0 )

	andi	t3, t3, 0xF
	slli	s8, t3, 28
	srli	t3, t2, 4
	or		s8, s8, t3

	andi	t2, t2, 0xF
	slli	s9, t2, 28
	srli	t2, t1, 4
	or		s9, s9, t2

	andi	t1, t1, 0xF
	slli	s10, t1, 28
	srli	t1, t0, 4
	or		s10, s10, t1

	andi	t0, t0, 0xF
	slli	s11, t0, 28
.endm

/****************************************************************************
 * Name: ske_set_cfg_en
 ****************************************************************************/

.macro ske_set_cfg_en
	li		a0, 0xe0800004
	lw		a1, 0(a0)
	li		a2, 1 << 12
	or		a1, a1, a2
	sw		a1, 0(a0)
.endm

/****************************************************************************
 * Name: ske_set_cfg_dis
 ****************************************************************************/

.macro ske_set_cfg_dis
	li		a0, 0xe0800004
	lw		a1, 0(a0)
	li		a2, ~(1 << 12)
	and		a1, a1, a2
	sw		a1, 0(a0)
.endm

/****************************************************************************
 * Name: ske_set_key
 ****************************************************************************/

.macro ske_set_key
	la		a0, SCM2010_PM_RRAM_INFO_ADDR
	li		a1, 0xe0800010
	lw		a2, SCM2010_PM_RRAM_CRYPTO_KEY0(a0)
	sw		a2, 12(a1)
	lw		a2, SCM2010_PM_RRAM_CRYPTO_KEY1(a0)
	sw		a2, 8(a1)
	lw		a2, SCM2010_PM_RRAM_CRYPTO_KEY2(a0)
	sw		a2, 4(a1)
	lw		a2, SCM2010_PM_RRAM_CRYPTO_KEY3(a0)
	sw		a2, 0(a1)
.endm

/****************************************************************************
 * Name: ske_set_iv
 ****************************************************************************/

.macro ske_set_iv
	li		a1, 0xe0800070
	sw		s11, 12(a1)
	sw		s10, 8(a1)
	sw		s9, 4(a1)
	sw		s8, 0(a1)
.endm

/****************************************************************************
 * Name: ske_set_input
 ****************************************************************************/

.macro ske_set_input
	li		a1, 0xe0800090
	lw		a2, 12(t0)
	sw		a2, 12(a1)
	lw		a2, 8(t0)
	sw		a2, 8(a1)
	lw		a2, 4(t0)
	sw		a2, 4(a1)
	lw		a2, 0(t0)
	sw		a2, 0(a1)
.endm

/****************************************************************************
 * Name: ske_set_output
 ****************************************************************************/

.macro ske_set_output
	li		a1, 0xe08000B0
	lw		a2, 12(a1)
	sw		a2, 12(t3)
	lw		a2, 8(a1)
	sw		a2, 8(t3)
	lw		a2, 4(a1)
	sw		a2, 4(t3)
	lw		a2, 0(a1)
	sw		a2, 0(t3)
.endm

/****************************************************************************
 * Name: pm_enc
 ****************************************************************************/

pm_enc:
	mv		t0, a3
	add		t1, a3, 256
	mv		t2, a4
	lw		t3, __enc_buffer

ecn_cont:
	li		a2, 0xf0000000
	and 	s11, s11, a2
	srli	a2, t2, 4
	or		s11, s11, a2

	ske_set_cfg_en
	ske_set_iv
	jal 	t4, ske_start_and_wait
	ske_set_cfg_dis

	ske_set_input
	jal 	t4, ske_start_and_wait
	ske_set_output

	addi	t0, t0, 16
	addi	t2, t2, 16
	addi	t3, t3, 16
	bne		t0, t1, ecn_cont

	jalr	x0, t5, 0x00

/****************************************************************************
 * Name: pm_write_flash_page
 ****************************************************************************/

pm_write_flash_page:
	li		t4, 0x0
	li		t5, 256
tx_word:
	li		t2, 0x800000
	jal		RA_WAIT, wait
	lw		a1, 0(t3)
	addi	t3, t3, 4
	li		a2, 0xf090002c
	sw		a1, 0(a2)
	addi	t4, t4, 4
	bltu	t4, t5, tx_word
	jal		RA_WIP, wip
	addi	a3, a3, 256
	addi	a4, a4, 256
	jalr	x0, t6, 0x00

/****************************************************************************
 * Name: pm_save_to_flash
 ****************************************************************************/

.func

pm_save_to_flash:

	/* save flash start address */
	mv		t3, a4

	/* partition size check */
	li		t0, 0x8000
	add		t0, t0, a4
	beq		t0, a5, erase_2_cmd

	/* erase DLM save area */
erase_1_cmd:
	la		t0, SCM2010_PM_RRAM_INFO_ADDR
	lbu		t4, SCM2010_PM_RRAM_FLASH_ER1_CMD( t0 )
	j		erase_loop

erase_2_cmd:
	/* erase ILM save area */
	la		t0, SCM2010_PM_RRAM_INFO_ADDR
	lbu		t4, SCM2010_PM_RRAM_FLASH_ER2_CMD( t0 )

erase_loop:
	pm_flash_check_cancel erase_continue
erase_continue:

	jal		RA_WRITE_ENABLE, write_enable
	mv		t0, t4
	li		t1, 0x67000000
	li		a2, 0xf0900028
	sw		t3, 0(a2)
	jal		RA_EXEC_CMD, exec_cmd
	jal		RA_WIP, wip

	/* partition size check */
	li		t0, 0x8000
	add		t0, t0, a4
	beq		t0, a5, erase_2_size

erase_1_size:
	la		t0, SCM2010_PM_RRAM_INFO_ADDR
	lw		t0, SCM2010_PM_RRAM_FLASH_ER1_SIZE( t0 )
	j		erase_next_block

erase_2_size:
	la		t0, SCM2010_PM_RRAM_INFO_ADDR
	lw		t0, SCM2010_PM_RRAM_FLASH_ER2_SIZE( t0 )

erase_next_block:
	add		t3, t3, t0
	blt		t3, a5, erase_loop

	/* spi transfer format */
	li		a1, 0xf0900010
	lw		a2, 0(a1)
	ori		a2, a2, 0x80 /* word unit */
	sw		a2, 0(a1)

write_loop:
	pm_flash_check_cancel write_continue

write_continue:
	jal		RA_WRITE_ENABLE, write_enable
	li		t0, 0x32
	li		t1, 0x618ff000
	li		a2, 0xf0900028
	sw		a4, 0(a2)
	jal		RA_EXEC_CMD, exec_cmd

	la		t0, SCM2010_PM_RRAM_INFO_ADDR
	lw		t0, SCM2010_PM_RRAM_CRYPTO_CFG( t0 )
	li		t1, 0x80000000
	and		t0, t0, t1
	beqz	t0, skip_ecnryption

	jal 	t5, pm_enc

	lw		t3,	__enc_buffer
	jal		t6, pm_write_flash_page
	bne		a4, a5, write_loop

	j		write_done

skip_ecnryption:

	mv		t3,	a3
	jal		t6, pm_write_flash_page
	bne		a4, a5, write_loop

write_done:
	/* spi transfer format */
	li		a1, 0xf0900010
	lw		a2, 0(a1)
	andi	a2, a2, 0xFFFFFF7F
	sw		a2, 0(a1)

	ret

.endfunc

#ifdef CONFIG_PM_HIBERNATION_VERIFY
/****************************************************************************
 * Name: pm_save_verify
 ****************************************************************************/
.extern pm_assert

.func

pm_save_verify:
	lw		t0, (a3)
	lw		t1, (a4)
	bne		t0, t1, verify_failed
	addi	a3, a3, 4
	addi	a4, a4, 4
	bne 	a4, a5, pm_save_verify

	ret

verify_failed:
	call	pm_assert

.endfunc
#endif

/****************************************************************************
 * Name: pm_save_d25_ilm_to_flash
 ****************************************************************************/

.macro pm_save_d25_ilm_to_flash

	li		a3, D25_ILM_ADDR_START
	li		a4, D25_ILM_ADDR_SAVE_START
	li		a5, D25_ILM_ADDR_SAVE_END
	call    pm_save_to_flash

#ifdef CONFIG_PM_HIBERNATION_VERIFY
	li		a3, D25_ILM_ADDR_START
	li		a4, D25_ILM_ADDR_SAVE_START
	li		a5, D25_ILM_ADDR_SAVE_END
	call    pm_save_verify
#endif

.endm

/****************************************************************************
 * Name: pm_save_n22_dlm_to_flash
 ****************************************************************************/

.macro pm_save_n22_dlm_to_flash

	/* save N22 DLM */
	li		a3, N22_DLM_ADDR_START
	li		a4, N22_DLM_ADDR_SAVE_START
	li		a5, N22_DLM_ADDR_SAVE_END
	call	pm_save_to_flash

#ifdef CONFIG_PM_HIBERNATION_VERIFY
	li		a3, N22_DLM_ADDR_START
	li		a4, N22_DLM_ADDR_SAVE_START
	li		a5, N22_DLM_ADDR_SAVE_END
	call    pm_save_verify
#endif

.endm

/****************************************************************************
 * Name: pm_save_d25_dlm_to_flash
 ****************************************************************************/

.macro pm_save_d25_dlm_to_flash

	/* save D25 DLM */
	li		a3, D25_DLM_ADDR_START
	li		a4, D25_DLM_ADDR_SAVE_START
	li		a5, D25_DLM_ADDR_SAVE_END
	call	pm_save_to_flash

#ifdef CONFIG_PM_HIBERNATION_VERIFY
	li		a3, D25_DLM_ADDR_START
	li		a4, D25_DLM_ADDR_SAVE_START
	li		a5, D25_DLM_ADDR_SAVE_END
	call    pm_save_verify
#endif

.endm


/****************************************************************************
 * Name: pm_restore_n22_ilm_from_flash
 ****************************************************************************/

.macro pm_restore_n22_ilm_from_flash

	/* restore N22 ILM */
	la		a0, __text_lma
	li		a1, N22_ILM_ADDR_START
	li		a2, N22_ILM_ADDR_END
	call	pm_copy

.endm

/****************************************************************************
 * Name: pm_restore_n22_dlm_from_flash
 ****************************************************************************/

.macro pm_restore_n22_dlm_from_flash

	/* restore N22 DLM */
	li		a0, N22_DLM_ADDR_SAVE_START
	li		a1, N22_DLM_ADDR_START
	li		a2, N22_DLM_ADDR_END
	call	pm_copy

.endm

/****************************************************************************
 * Name: pm_restore_d25_dlm_from_flash
 ****************************************************************************/

.macro pm_restore_d25_dlm_from_flash

	/* restore D25 DLM */
	li		a0, D25_DLM_ADDR_SAVE_START
	li		a1, D25_DLM_ADDR_START
	li		a2, D25_DLM_DMA_SIZE
	call	pm_dma_copy

.endm

/****************************************************************************
 * Name: pm_restore_flash
 ****************************************************************************/

.macro pm_restore_flash

	/* Memory Mapped fastet read enable */
	la		a0, SCM2010_PM_RRAM_INFO_ADDR
	lw		a2, SCM2010_PM_RRAM_FLASH_RD_CMD( a0 )
	li		a1, SPI_MEM_ACCESS_CTRL_REG
	sw		a2, 0(a1)
flash_wait_spi_ready:
	lw		a2, 0(a1)
	andi	a2, a2, SPI_MEM_CTRL_CHANGE
	bnez	a2, flash_wait_spi_ready

	la		a0, SCM2010_PM_RRAM_INFO_ADDR
	lw		a2, SCM2010_PM_RRAM_FLASH_TIMING( a0 )
	li		a1, SPI_INTF_TIMING_REG
	sw		a2, 0(a1)

	lw		a2, SCM2010_PM_RRAM_FLASH_CLK( a0 )
	li		a1, SYS_SPI0_CFG
	sw		a2, 0(a1)

.endm

/****************************************************************************
 * Name: pm_restore_d25_ilm_from_flash
 ****************************************************************************/

.macro pm_restore_d25_ilm_from_flash

	/* restore D25 ILM */
	li		a0, D25_ILM_ADDR_SAVE_START
	li		a1, D25_ILM_ADDR_START
	li		a2, D25_ILM_DMA_SIZE
	call	pm_dma_copy

.endm

/****************************************************************************
 * Name: pm_save_d25_ilm_to_ram
 ****************************************************************************/

.macro pm_save_d25_ilm_to_ram

	/* save D25 ILM to RAM*/
	li		a0, D25_ILM_ADDR_START
	lw		a1, __d25_ilm_save
	li		a2, D25_ILM_DMA_SIZE
	call	pm_dma_copy

.endm

/****************************************************************************
 * Name: pm_restore_d25_ilm_from_ram
 ****************************************************************************/

.macro pm_restore_d25_ilm_from_ram

	lw		a0, __d25_ilm_save
	li		a1, D25_ILM_ADDR_START
	li		a2, D25_ILM_DMA_SIZE
	call	pm_dma_copy

.endm

/****************************************************************************
 * Public Functions
 ****************************************************************************/

.global pm_lowpower_start
.global pm_wakeup_entry
.global pm_wakeup_wfi
.global pm_gpio_dbg_set
.global pm_gpio_dbg_clr
.global pm_test_wakeup_wfi

/****************************************************************************
 * Name: pm_lowpower_start
 ****************************************************************************/

.func

pm_lowpower_start:

	pm_save_context

	la		s0, SCM2010_PM_RRAM_INFO_ADDR

	lbu		a1, SCM2010_PM_RRAM_SYNC_FLAG( s0 )
	andi	a1, a1, SCM2010_PM_RRAM_SAVE_FLASH
	beqz	a1, skip_save_to_flash

	lw		t0, SCM2010_PM_RRAM_CRYPTO_CFG( s0 )
	li		t1, 0x80000000
	and		t0, t0, t1
	beqz	t0, skip_enc_configure

	/* configure AES128-CTR */
	li		a0, 0x60000001
	li		a1, 0xe0800004
	sw		a0, 0(a1)

	/* set key */
	ske_set_cfg_en
	ske_set_key
	ske_set_cfg_dis
	jal		t4, ske_start_and_wait

	/* iv convert */
	ske_iv_convert

skip_enc_configure:

	pm_save_d25_ilm_to_flash
	pm_save_n22_dlm_to_flash
	pm_save_d25_dlm_to_flash

	/* clear save flash flag */
	lb		a1, SCM2010_PM_RRAM_SYNC_FLAG( s0 )
	andi	a1, a1, 0xfe
	sb		a1, SCM2010_PM_RRAM_SYNC_FLAG( s0 )

	pm_flash_check_cancel skip_save_to_ram

skip_save_to_flash:
	pm_save_d25_ilm_to_ram

skip_save_to_ram:

#ifdef CONFIG_PM_GPIO_DBG
	li		a0, 6
	call	pm_gpio_dbg_clr
#endif

	wfi
	/*
	 * entering either deep sleep or hibernation
	 * code should not reach here, and wakeup should start from the reset vector
	 */

hang:
	j		hang

restore_context:
	pm_restore_context

	ret

.endfunc

/****************************************************************************
 * Name: pm_wakeup_wfi
 ****************************************************************************/

.func
pm_wakeup_wfi:
1:
	wfi
	j		1b

.endfunc

/****************************************************************************
 * Name: pm_wakeup_entry
 ****************************************************************************/

.func

pm_wakeup_entry:

	pm_restore_flash

	/* for the full wakeup, skip restoring if it was not in hibernation */
	la		a0, SCM2010_PM_RRAM_INFO_ADDR
	lbu		a1, SCM2010_PM_RRAM_MODE( a0 )
	li		a2, SCM2010_PM_RRAM_MODE_HIBERNATION
	bne		a1, a2, skip_restore_from_flash

	pm_restore_n22_ilm_from_flash
	pm_restore_n22_dlm_from_flash
	pm_restore_d25_dlm_from_flash
	pm_restore_d25_ilm_from_flash
    j       skip_restore_from_ram


skip_restore_from_flash:
    pm_restore_d25_ilm_from_ram

skip_restore_from_ram:
	/* Resume so that we continue as if we are returning from the sleep */
	tail	restore_context

.endfunc

#ifdef CONFIG_PM_TEST

/****************************************************************************
 * Name: pm_test_wakeup_wfi
 ****************************************************************************/

.section .text, "ax"

#if (CONFIG_SERIAL_CONSOLE_PORT == 0)
#define UART_TEST_BASE 0xf0300000
#else
#define UART_TEST_BASE 0xf0200000
#endif
.func
test_uart_tx:
	li		a1, UART_TEST_BASE + 0x20
	sw		a0, 0(a1)

	li		a1, UART_TEST_BASE + 0x34
	li		a2, 0x60
uart_cmpl:
	lw		a0, 0(a1)
	and		a0, a2, a0
	bne		a0, a2, uart_cmpl

	ret

.endfunc

.func
pm_test_wakeup_wfi:

	li		t0, 1 << 11
	csrw	mie, t0

1:
	wfi

    /* FIXME: can we remov this? */
	/* uart lossy enable */
#if (CONFIG_SEIRAL_CONSOLE_PORT == 0)
	li		a1, 0xf1700230
#else
	li		a1, 0xf170022c
#endif
	li		a0, ~(1 << 11)
	lw		a2, 0(a1)
	and		a2, a2, a0
	sw		a2, 0(a1)

	/* plic claim */
	li		a3, 0xe4200004
	lw		a4, 0(a3)

	/* read 0x28 IIR */
	li		a1, UART_TEST_BASE + 0x28
	lw		a0, 0(a1)

	/* read 0x20 rx buffer */
	li		a1, UART_TEST_BASE + 0x20
	lw		a0, 0(a1)

	/* write 0x28 FIFO */
	li		a1, UART_TEST_BASE + 0x28
	li		a2, 5
	lw		a2, 0(a1)

	/* plic complete */
	sw		a4, 0(a3)

	/* clear wakeup source */
	li		a1, 0xf010020c
	li		a2, ~(3 << 13)
	li		a3, ~(0x7ff)
	lw		a0, 0(a1)
	and		a0, a0, a2
	and		a0, a0, a3
	sw		a0, 0(a1)

	/* print WKUP\r\n */
	li		a0, 0x57 //'W'
	call	test_uart_tx
	li		a0, 0x4B //'K'
	call	test_uart_tx
	li		a0, 0x55 //'U'
	call	test_uart_tx
	li		a0, 0x50 //'P'
	call	test_uart_tx
	li		a0, 0x0A //LF
	call	test_uart_tx
	li		a0, 0x0D //CR
	call	test_uart_tx

	j		1b

.endfunc

#endif

#endif
