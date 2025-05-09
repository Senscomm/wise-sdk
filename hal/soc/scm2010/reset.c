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

#include "platform.h"
#include "linker.h"
#include "mmap.h"
#include <hal/types.h>
#include "asm/io.h"
#ifdef CONFIG_LINK_TO_ROM
#include <hal/rom.h>
#endif
#include "version.h"


__xip_boot__ static void
__memcpy(void *ptr2, void *ptr1, unsigned len)
{
	u8 *dst = ptr2, *src = ptr1;

	if (src == dst)
		return;

	while (src < ((u8 *)ptr1 + len))
		*dst++ = *src++;
}

__xip_boot__ static void
__memset(void *ptr, int value, size_t size)
{
    u8 *dest = ptr;

    while (size--)
        *dest++ = (u8)value;
}

#pragma weak c_startup = c_startup_common

void c_startup(void);

/*
 * Default c_startup() function which used for those relocation from LMA to VMA.
 */
static void c_startup_common(void)
{
	__memset(VMA(bss), 0, SSIZE(bss));
#ifdef CONFIG_SUPPORT_NODE_POOL
	__memset(VMA(nodebss), 0, SSIZE(nodebss));
#endif
	__memcpy(VMA(text), LMA(text), SSIZE(text));
	__memcpy(VMA(rodata), LMA(rodata), SSIZE(rodata));
	__memcpy(VMA(data), LMA(data), SSIZE(data));

#ifdef CONFIG_PM_SCM2010
	__memcpy(VMA(rram), LMA(rram), SSIZE(rram));
#endif

#ifdef CONFIG_SUPPORT_GCOV
	__memset(VMA(gcovbss), 0, SSIZE(gcovbss));
	__memcpy(VMA(init_array), LMA(init_array), SSIZE(init_array));
	__memcpy(VMA(gcovdata), LMA(gcovdata), SSIZE(gcovdata));

#endif

#ifdef CONFIG_LINK_TO_ROM
	__memset(VMA(romb), 0, SSIZE(romb));
	__memcpy(VMA(romd), LMA(romd), SSIZE(romd));
#endif
}

static void cpu_init(void)
{
	/* Enable Misaligned access */
	set_csr(NDS_MMISC_CTL, (1 << 6));

    /* Initialize CSRs */

    /* Clear Machine Status */
    clear_csr(NDS_MSTATUS, MSTATUS_MIE);

    /* Clear Machine Exception PC */
    write_csr(NDS_MEPC, 0);

    /* Clear Machine Extended Status */
    clear_csr(NDS_MXSTATUS, (0x3 << 6));
}

/*
 * Platform specific initialization
 */
extern void reset_vector(void);
static void plf_init(void)
{
#ifdef CONFIG_HW_PRIO_SUPPORT
    /* Enable preemptive interrupt */
	__nds__plic_set_feature(NDS_PLIC_FEATURE_PREEMPT);
#endif

#ifdef CONFIG_BOOTLOADER
    /* Boot loader will run directly from AHB-SRAM.
     * We need to invalidate I-Cache because refill operations
     * might have been compromised.
     */
    __nds__fencei();
#endif
}

/*
 * All scm2010 hardware initialization
 */
void hardware_init(void)
{
	plf_init();
}

/*
 * Reset handler to reset all devices sequentially and call application
 * entry function.
 */
void reset_handler(void)
{
	extern void start_kernel(void);

#if defined(CONFIG_MCUBOOT_BL) || defined(CONFIG_UART_BOOT)
    extern void _start(void);
    if (__nds__mfsr(NDS_MHARTID) == 0) {
        writel((uint32_t)_start, SMU(RESET_VECTOR(1)));
        writel(0x10, SYS(CORE_RESET_CTRL));
        while (1) {
            __asm("wfi");
        }
    }
#endif

	/*
	 * Initialize CPU to a post-reset state, ensuring the ground doesn't
	 * shift under us while we try to set things up.
	*/
	cpu_init();

	/*
	 * Initialize LMA/VMA sections.
	 * Relocation for any sections that need to be copied from LMA to VMA.
	 */
	c_startup();

#ifdef CONFIG_LINK_TO_ROM
    /*
     * Make sure that it is linked to the proper ROM version
     * If not, do not proceed further.
     */
    if (verify_rom_version()) {
        while (1) {
        }
    }
	/* XXX: should be after c_startup(). */
	patch();
    provide();
#endif

	/* Platform specific hardware initialization */
	hardware_init();

	start_kernel();

	/* Never go back here! */
	while(1);
}
