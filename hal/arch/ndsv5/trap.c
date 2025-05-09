/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Copyright (c) 2012-2017 Andes Technology Corporation
 * All rights reserved.
 *
 */
#include <hal/types.h>
#include <hal/console.h>
#include <hal/init.h>
#include <bug.h>
#include "linker.h"

#include <stdio.h>
#include "platform.h"
#include <string.h>
#include <soc.h>
#include <linker.h>
#include <cmsis_os.h>
#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>
#include <hal/ndsv5/core_v5.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/machine.h>
#include <hal/console.h>
#include <hal/irq.h>
#include "bug.h"

#ifdef CONFIG_CORE_DUMP

/* XXX: ugly! */
/* We need to make sure that these macros are consistent with
 * freertos_risc_v_chip_specific_extensions.h.
 */

#if __riscv_flen == 64
	#define portFPWORD_SIZE 8
#elif __riscv_flen == 32
	#define portFPWORD_SIZE 4
#else
	#define portFPWORD_SIZE 0
#endif

/* Additional FPU registers to save and restore (fcsr + 32 FPUs) */
#define portasmFPU_CONTEXT_SIZE        ( 1 + ( 32 * portFPWORD_SIZE ) / 4 )

/* One additional registers to save and restore, as per the #defines above. */
#define portasmADDITIONAL_CONTEXT_SIZE ( 2 + portasmFPU_CONTEXT_SIZE )  /* Must be even number on 32-bit cores. */

#define GP_OFT              portasmADDITIONAL_CONTEXT_SIZE
#define GP(x)               (x + GP_OFT)

/* Processor PC */

#define REG_EPC_NDX         0

/* Machine Extended Status */

#define REG_MXS_NDX         1


#ifdef __riscv_dsp

/* UCODE */

#define REG_UCD_NDX         2

#else

/* MTVAL */

#define REG_MTV_NDX         2

#endif

/* General purpose registers */

/* $0: Zero register does not need to be saved
 * $1: ra (return address)
 */

#define REG_X1_NDX          GP(1)

/* $5-$7 = t0-t3: Temporary registers */

#define REG_X5_NDX          GP(2)
#define REG_X6_NDX          GP(3)
#define REG_X7_NDX          GP(4)

/* $8: s0 / fp Frame pointer */

#define REG_X8_NDX          GP(5)

/* $89 s1 Saved register */

#define REG_X9_NDX          GP(6)

/* $10-$17 = a0-a7: Argument registers */

#define REG_X10_NDX         GP(7)
#define REG_X11_NDX         GP(8)
#define REG_X12_NDX         GP(9)
#define REG_X13_NDX         GP(10)
#define REG_X14_NDX         GP(11)
#define REG_X15_NDX         GP(12)
#define REG_X16_NDX         GP(13)
#define REG_X17_NDX         GP(14)

/* $18-$27 = s2-s11: Saved registers */

#define REG_X18_NDX         GP(15)
#define REG_X19_NDX         GP(16)
#define REG_X20_NDX         GP(17)
#define REG_X21_NDX         GP(18)
#define REG_X22_NDX         GP(19)
#define REG_X23_NDX         GP(20)
#define REG_X24_NDX         GP(21)
#define REG_X25_NDX         GP(22)
#define REG_X26_NDX         GP(23)
#define REG_X27_NDX         GP(24)

/* $28-31 = t3-t6: Temporary (Volatile) registers */

#define REG_X28_NDX         GP(25)
#define REG_X29_NDX         GP(26)
#define REG_X30_NDX         GP(27)
#define REG_X31_NDX         GP(28)

/* Machine Status */

#define REG_MST_NDX         GP(29)

#define REG_EPC             REG_EPC_NDX
#define REG_MXS             REG_MXS_NDX
#define REG_MTV             REG_MTV_NDX
#ifdef __riscv_dsp
#define REG_UCD             REG_UCD_NDX
#else
#define REG_MST             REG_MST_NDX
#endif
#define REG_X1              REG_X1_NDX
#define REG_X5              REG_X5_NDX
#define REG_X6              REG_X6_NDX
#define REG_X7              REG_X7_NDX
#define REG_X8              REG_X8_NDX
#define REG_X9              REG_X9_NDX
#define REG_X10             REG_X10_NDX
#define REG_X11             REG_X11_NDX
#define REG_X12             REG_X12_NDX
#define REG_X13             REG_X13_NDX
#define REG_X14             REG_X14_NDX
#define REG_X15             REG_X15_NDX
#define REG_X16             REG_X16_NDX
#define REG_X17             REG_X17_NDX
#define REG_X18             REG_X18_NDX
#define REG_X19             REG_X19_NDX
#define REG_X20             REG_X20_NDX
#define REG_X21             REG_X21_NDX
#define REG_X22             REG_X22_NDX
#define REG_X23             REG_X23_NDX
#define REG_X24             REG_X24_NDX
#define REG_X25             REG_X25_NDX
#define REG_X26             REG_X26_NDX
#define REG_X27             REG_X27_NDX
#define REG_X28             REG_X28_NDX
#define REG_X29             REG_X29_NDX
#define REG_X30             REG_X30_NDX
#define REG_X31             REG_X31_NDX

/* Now define more user friendly alternative name that can be used either
 * in assembly or C contexts.
 */

/* $1 = ra: Return address */

#define REG_RA              REG_X1

/* $2 = sp:  The value of the stack pointer on return from the exception */

#define REG_SP              REG_X2

/* $3 = gp: Only needs to be saved under conditions where there are
 * multiple, per-thread values for the GP.
 */

#define REG_GP              REG_X3

/* $4 = tp:  Thread Pointer */

#define REG_TP              REG_X4

/* $5-$7 = t0-t2: Caller saved temporary registers */

#define REG_T0              REG_X5
#define REG_T1              REG_X6
#define REG_T2              REG_X7

/* $8 = either s0 or fp:  Depends if a frame pointer is used or not */

#define REG_S0              REG_X8
#define REG_FP              REG_X8

/* $9 = s1: Caller saved register */

#define REG_S1              REG_X9

/* $10-$17 = a0-a7: Argument registers */

#define REG_A0              REG_X10
#define REG_A1              REG_X11
#define REG_A2              REG_X12
#define REG_A3              REG_X13
#define REG_A4              REG_X14
#define REG_A5              REG_X15
#define REG_A6              REG_X16
#define REG_A7              REG_X17

/* $18-$27 = s2-s11: Callee saved registers */

#define REG_S2              REG_X18
#define REG_S3              REG_X19
#define REG_S4              REG_X20
#define REG_S5              REG_X21
#define REG_S6              REG_X22
#define REG_S7              REG_X23
#define REG_S8              REG_X24
#define REG_S9              REG_X25
#define REG_S10             REG_X26
#define REG_S11             REG_X27

/* $28-$31 = t3-t6: Caller saved temporary registers */

#define REG_T3              REG_X28
#define REG_T4              REG_X29
#define REG_T5              REG_X30
#define REG_T6              REG_X31

DECLARE_SECTION_INFO(stack);

static uint32_t ndsv5_get_sp(void)
{
    return __nds__get_current_sp();
}

static uint32_t ndsv5_get_current_stack_base(void)
{
    TaskStatus_t tskSt;

    vTaskGetInfo(xTaskGetCurrentTaskHandle(), &tskSt, pdFALSE, eRunning);

    return (uint32_t)tskSt.pxStackBase;
}

static uint32_t ndsv5_get_current_stack_top(void)
{
    uint32_t *top = (uint32_t *)xTaskGetCurrentTaskHandle();

    return *top;
}

static uint32_t ndsv5_get_current_stack_size(void)
{
    TaskStatus_t tskSt;

    vTaskGetInfo(xTaskGetCurrentTaskHandle(), &tskSt, pdFALSE, eRunning);

    return (uint32_t)((tskSt.pxEndOfStack - tskSt.pxStackBase) * sizeof(StackType_t));
}

static void ndsv5_stackdump(uint32_t sp, uint32_t stack_top)
{
  uint32_t stack;

  for (stack = sp & ~0x1f; stack < stack_top; stack += 32)
    {
      uint32_t *ptr __maybe_unused = (uint32_t *)stack;
      printk("%08x: %08x %08x %08x %08x %08x %08x %08x %08x\n",
             stack, ptr[0], ptr[1], ptr[2], ptr[3],
             ptr[4], ptr[5], ptr[6], ptr[7]);
    }
}

static bool ndsv5_is_irq()
{
	unsigned char typ = (__nds__read_csr(NDS_MXSTATUS) & 0xc0) >> 6;
	/* Is it either an interrupt(1) or exception(2). */
	return (typ != 0);
}

#ifdef CONFIG_BACKTRACE
#define DUMP_DEPTH  16
#define DUMP_NITEM  8
#define DUMP_WIDTH  (int)(2 * sizeof(void *) + 2)
#define DUMP_LINESZ (DUMP_NITEM * (DUMP_WIDTH + 1))

/****************************************************************************
 * Name: getfp
 *
 * Description:
 *  getfp() returns current frame pointer
 *
 ****************************************************************************/

static inline uintptr_t getfp(void)
{
  register uintptr_t fp;

  __asm__
  (
    "\tadd  %0, x0, fp\n"
    : "=r"(fp)
  );

  return fp;
}

/****************************************************************************
 * Name: backtrace
 *
 * Description:
 *  backtrace() parsing the return address through frame pointer
 *
 ****************************************************************************/

static int backtrace(uintptr_t *base, uintptr_t *limit,
                     uintptr_t *fp, uintptr_t *ra,
                     void **buffer, int size, int *skip)
{
  int i = 0;

  if (ra)
    {
      if ((*skip)-- <= 0)
        {
          buffer[i++] = ra;
        }
    }

  for (; i < size; fp = (uintptr_t *)*(fp - 2))
    {
      if (fp > limit || fp < base)
        {
          break;
        }

      ra = (uintptr_t *)*(fp - 1);
      if (ra == NULL)
        {
          break;
        }

      if ((*skip)-- <= 0)
        {
          buffer[i++] = ra;
        }
    }

  return i;
}

/****************************************************************************
 * Name: up_backtrace
 *
 * Description:
 *  up_backtrace()  returns  a backtrace for the TCB, in the array
 *  pointed to by buffer.  A backtrace is the series of currently active
 *  function calls for the program.  Each item in the array pointed to by
 *  buffer is of type void *, and is the return address from the
 *  corresponding stack frame.  The size argument specifies the maximum
 *  number of addresses that can be stored in buffer. If  the backtrace is
 *  larger than size, then the addresses corresponding to the size most
 *  recent function calls are returned; to obtain the complete backtrace,
 *  make sure that buffer and size are large enough.
 *
 * Input Parameters:
 *   tcb    - Address of the task's Status
 *   buffer - Return address from the corresponding stack frame
 *   size   - Maximum number of addresses that can be stored in buffer
 *   skip   - number of addresses to be skipped
 *
 * Returned Value:
 *   up_backtrace() returns the number of addresses returned in buffer
 *
 ****************************************************************************/

int up_backtrace(TaskStatus_t *tcb, void **buffer, int size, int skip)
{
  int ret;

  if (size <= 0 || !buffer)
    {
      return 0;
    }

    /* backtrace current task */
    {
      if (ndsv5_is_irq())
        {
          ret = backtrace((void *)&VMA(stack),
                          (void *)((uint32_t)&VMA(stack) +
                                   (SSIZE(stack) & (~15))),
                          (void *)getfp(), NULL, buffer, size, &skip);
          if (ret < size)
            {
              volatile uint32_t *current_regs = (uint32_t *)ndsv5_get_current_stack_top();
              ret += backtrace((uintptr_t *)tcb->pxStackBase,
                               (uintptr_t *)tcb->pxEndOfStack,
                               (void *)current_regs[REG_FP],
                               (void *)current_regs[REG_EPC],
                               &buffer[ret], size - ret, &skip);
            }
        }
      else
        {
          ret = backtrace((uintptr_t *)tcb->pxStackBase,
                          (uintptr_t *)tcb->pxEndOfStack,
                          (void *)getfp(), NULL, buffer, size, &skip);
        }
    }

  return ret;
}

void ndsv5_dumpstack(TaskStatus_t *tskSt)
{
  int size = DUMP_DEPTH;
  int skip;

  for (skip = 0; size == DUMP_DEPTH; skip += size)
    {
      void *address[DUMP_DEPTH];
      const char *format = " %0*p";
      char line[DUMP_LINESZ + 1];
      int ret = 0;
      int i;

      size = up_backtrace(tskSt, address, DUMP_DEPTH, skip);
      if (size <= 0)
        {
          break;
        }

      for (i = 0; i < size; i++)
        {
          ret += snprintf(line + ret, sizeof(line) - ret,
                          format, DUMP_WIDTH, address[i]);
          if (i == size - 1 || ret % DUMP_LINESZ == 0)
            {
              printk("backtrace|%2d:%s\n", tskSt->xTaskNumber, line);
              ret = 0;
            }
        }
    }
}

void ndsv5_dumpcurrentstack(void)
{
    TaskStatus_t tskSt;

    vTaskGetInfo(xTaskGetCurrentTaskHandle(), &tskSt, pdFALSE, eRunning);

    ndsv5_dumpstack(&tskSt);
}
#endif

#define xstr(s) str(s)
#define str(s) #s

#define FMT "%-" xstr(configMAX_TASK_NAME_LEN) "s"

static void ndsv5_print_task_info(TaskStatus_t *ti)
{
	char state[] __maybe_unused = {'X', 'R', 'B', 'S', 'D', 'I'};
	printk("%4lu%5lu%6d%3c   "FMT" (0x%x-0x%x, 0x%x)""\n",
	       ti->xTaskNumber,
	       ti->uxCurrentPriority,
	       ti->usStackHighWaterMark,
	       state[ti->eCurrentState],
	       ti->pcTaskName,
           ti->pxStackBase,
#if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
		   ti->pxEndOfStack,
#else
		   0,
#endif
		   *(uint32_t *)(ti->xHandle) /* the last stack pointer */
		   );
}

static void ndsv5_showtasks(void)
{
	TaskStatus_t info[20];
    int i, nr_task = sizeof(info)/sizeof(info[0]);

    nr_task = uxTaskGetSystemState(info, nr_task, NULL);

	printk("%4s%5s%6s%3s   %-44s\n",
	       "PID", "PR", "STWM", "S", "TASK");
	for (i = 0; i < nr_task; i++) {
        ndsv5_print_task_info(&info[i]);
    }
}

static void ndsv5_registerdump(void)
{
    volatile uint32_t *current_regs __maybe_unused = (uint32_t *)ndsv5_get_current_stack_top();

    /* Are user registers available from interrupt processing? */

    if (ndsv5_is_irq()) {
        printk("EPC:%08x \n",
        current_regs[REG_EPC]);
        printk("MSTATUS:%08x \n",
        current_regs[REG_MST]);
        printk("MXSTATUS:%08x \n",
        current_regs[REG_MXS]);
        #ifdef __riscv_dsp
        printk("UCODE:%08x \n",
        current_regs[REG_UCD]);
        #else
        printk("MTVAL:%08x \n",
        current_regs[REG_MTV]);
        #endif
        printk("A0:%08x A1:%08x A2:%08x A3:%08x A4:%08x A5:%08x "
        "A6:%08x A7:%08x\n",
        current_regs[REG_A0], current_regs[REG_A1],
        current_regs[REG_A2], current_regs[REG_A3],
        current_regs[REG_A4], current_regs[REG_A5],
        current_regs[REG_A6], current_regs[REG_A7]);
        printk("T0:%08x T1:%08x T2:%08x T3:%08x T4:%08x T5:%08x T6:%08x\n",
        current_regs[REG_T0], current_regs[REG_T1],
        current_regs[REG_T2], current_regs[REG_T3],
        current_regs[REG_T4], current_regs[REG_T5],
        current_regs[REG_T6]);
        printk("S0:%08x S1:%08x S2:%08x S3:%08x S4:%08x S5:%08x "
        "S6:%08x S7:%08x\n",
        current_regs[REG_S0], current_regs[REG_S1],
        current_regs[REG_S2], current_regs[REG_S3],
        current_regs[REG_S4], current_regs[REG_S5],
        current_regs[REG_S6], current_regs[REG_S7]);
        printk("S8:%08x S9:%08x S10:%08x S11:%08x\n",
        current_regs[REG_S8], current_regs[REG_S9],
        current_regs[REG_S10], current_regs[REG_S11]);
        printk("FP:%08x RA:%08x\n",
        current_regs[REG_FP], current_regs[REG_RA]);
    }
}

static void ndsv5_dumpstate(void)
{
    uint32_t sp = ndsv5_get_sp();
    uint32_t istackbase;
    uint32_t istacksize;
    uint32_t ustackbase;
    uint32_t ustacksize;

    /* Show back trace */
#ifdef CONFIG_BACKTRACE
    ndsv5_dumpcurrentstack();
#endif

    ndsv5_registerdump();

    /* Get the limits on the user stack memory */

    ustackbase = ndsv5_get_current_stack_base();
    ustacksize = ndsv5_get_current_stack_size();

    /* XXX: configISR_STACK_SIZE_WORDS should not be defined. */

    istackbase = (uint32_t)VMA(stack);
    istacksize = (uint32_t)SSIZE(stack);
    istacksize &= ~15;

    /* Show interrupt stack info */

    printk("sp:     %08x\n", sp);
    printk("IRQ stack:\n");
    printk("  base: %08x\n", istackbase);
    printk("  size: %08x\n", istacksize);

    /* Does the current stack pointer lie within the interrupt
    * stack?
    */

    if (sp >= istackbase && sp < istackbase + istacksize) {
        /* Yes.. dump the interrupt stack */

        ndsv5_stackdump(sp, istackbase + istacksize);

        /* Extract the user stack pointer which should lie
        * at the base of the interrupt stack.
        */

        sp = (uint32_t)ndsv5_get_current_stack_top();
        printk("sp:     %08x\n", sp);
    } else if (ndsv5_is_irq()) { /* Unhandled exception */
        printk("ERROR: Stack pointer is not within the interrupt stack\n");
        ndsv5_stackdump(istackbase, istackbase + istacksize);

        sp = (uint32_t)ndsv5_get_current_stack_top();
        printk("sp:     %08x\n", sp);
    }

    /* Show user stack info */

    printk("User stack:\n");
    printk("  base: %08x\n", ustackbase);
    printk("  size: %08x\n", ustacksize);

    /* Dump the user stack if the stack pointer lies within the allocated user
    * stack memory.
    */

    if (sp >= ustackbase && sp < ustackbase + ustacksize) {
        ndsv5_stackdump(sp, ustackbase + ustacksize);
    } else {
        printk("ERROR: Stack pointer is not within allocated stack\n");
        ndsv5_stackdump(ustackbase, ustackbase + ustacksize);
    }
}

void ndsv5_dump_core(void)
{
    ndsv5_dumpstate();

    ndsv5_showtasks();

    while (1);
}

void ndsv5_panic(const char *msg)
{
    uint32_t flags __maybe_unused;

    local_irq_save(flags);

#ifdef CONFIG_CMD_DMESG
    console_flush();
#endif

    kernel_enter_panic();

	printk("%s\n", msg);

    ndsv5_dump_core();
}

static int ndsv5_register_panic(void)
{
	panic = ndsv5_panic;
	return 0;
}
__initcall__(arch, ndsv5_register_panic);

static const char *exception_cause[] = {
	[0] = "Instruction address misaligned",
	[1] = "Instruction access fault",
	[2] = "Illegal instruction",
	[3] = "Breakpoint (ebreak)",
	[4] = "Load address misaligned",
	[5] = "Load access fault",
	[6] = "Store address misaligned",
	[7] = "Store access fault",
	[8 ... 15] = "Unknown"
};

__ilm__ void except_handler(uint32_t cause, uint32_t epc)
{
    uint32_t flags __maybe_unused;

	/* Unhandled Trap */

    local_irq_save(flags);

#ifdef CONFIG_CMD_DMESG
    console_flush();
#endif

    kernel_enter_panic();

    printk("Unhandled Trap : %s (mcause = 0x%x), mepc = 0x%x\n", exception_cause[cause],
			(unsigned int)cause, (unsigned int)epc);

    ndsv5_dump_core();
}

#endif

extern void mext_interrupt(void);
extern void mswi_handler(void);

__attribute__((weak)) void mswi_handler(void)
{
	clear_csr(NDS_MIE, MIP_MSIP);
}


/*
 * The portasmHANDLE_INTERRUPT is defined to FreeRTOS_IRQ_handler() to provide handling
 * external interrupts. Since we don't have CLINT, we also handle machine timer and software
 * interrupts.
 */
__ilm__ void FreeRTOS_IRQ_handler(uint32_t mcause)
{
	if ((mcause & MCAUSE_INT) && ((mcause & MCAUSE_CAUSE) == IRQ_M_TIMER)) {
		__asm volatile( "tail FreeRTOS_tick_handler" );
	} else if ((mcause & MCAUSE_INT) && ((mcause & MCAUSE_CAUSE) == IRQ_M_EXT)) {
		__asm volatile( "tail mext_interrupt" );
	} else if ((mcause & MCAUSE_INT) && ((mcause & MCAUSE_CAUSE) == IRQ_M_SOFT)) {
		__asm volatile( "tail mswi_handler" );
	}

	while (1) {
		__asm volatile( "ebreak" );
	}
}
