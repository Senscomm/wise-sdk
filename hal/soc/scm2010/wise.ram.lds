#include <wise.lds.h>

MEMORY
{
    HDR (r)       : ORIGIN = 0x40000000, LENGTH =  160
#ifdef CONFIG_PM_TEST
    RAM (rw)      : ORIGIN = 0x400000a0, LENGTH = 128K - 160 - 24K
    RRAM(rw)      : ORIGIN = 0x4001a000, LENGTH = 1920
#else
    RAM (rw)      : ORIGIN = 0x400000a0, LENGTH = 112K - 160 - 6K
#endif
	DLM (rw)      : ORIGIN = 0x0020AC00, LENGTH = 192K - 43K
#ifdef CONFIG_N22_ONLY
#ifdef CONFIG_SDIO_USE_IRQD
    IRQDBIN(rw)	  : ORIGIN = 0xA0000000, LENGTH = 3K
	IRQDSHM(rw)	  : ORIGIN = 0xA0000c00, LENGTH = 3K
    EXTRAM1(rw)	  : ORIGIN = 0xA0001800, LENGTH = 26K
#else
    EXTRAM1(rw)	  : ORIGIN = 0xA0000000, LENGTH = 32K
#endif
    EXTRAM2(rw)	  : ORIGIN = 0xA0200000, LENGTH = 192K
#endif
}

_STACK_SIZE = 2K;

SECTIONS
{
	.head :
	{
		KEEP(*(.header));
	} > HDR

	.text . :
	{
		__stext = .;
		KEEP(*(.init))
		*(.text)
		*(.text*)
        *(.xip.text)
        *(.iram.text)
		KEEP(*(.version))
		__etext = .;
	} > RAM

#ifdef CONFIG_PM_TEST
    .text.rram : {
        *(.rram)
    } > RRAM
#endif

	.exec.itable :
	{
		*(.exec.itable )
	} > RAM

    .func_tab :
	{
		KEEP(*(SORT(.func_tab*)));

		. = ALIGN(4);
	} > RAM

	.rodata :
	{
		*(.rodata*)
		*(.rodata)
		*(.srodata*)

		KEEP(*(.eh_frame*))
	} > RAM

	/*
 	 * .data: rwdata section
	 */
	.data :
	{
		*(.data)
		*(.data*)
        *(.iram.data)
        *(.rom.data*)
		*(.rom.patch_func*)
		. = ALIGN(4);
		*(.sdata)
		*(.sdata*)
		. = ALIGN(4);

		KEEP(*(SORT(.wise_list_*)));
		. = ALIGN(4);
		ARRAY(initcall)
		ARRAY(device_tab);
		ARRAY(driver_tab)
		. = ALIGN(4);
		*(.data.impure_data);
		*(.data._impure_ptr);
		. = ALIGN(4);
	} > RAM

	.bssreloc (NOLOAD) : ALIGN(4)
	{
		*(.bss*)
		*(.sbss*)
		. = ALIGN(4);
		*(COMMON)
	} > DLM

	.stack (NOLOAD) : ALIGN(16)
	{
   		. += _STACK_SIZE;
	} > DLM

	.heap (NOLOAD) : ALIGN(16)
	{
#ifdef CONFIG_HEAP_AUTO_SIZE
		. = ORIGIN(DLM) + LENGTH(DLM);
#else
		*(.heap)
#endif
	} > DLM

#ifdef CONFIG_N22_ONLY

#ifdef CONFIG_SDIO_USE_IRQD
    .irqd_bin (NOLOAD) : ALIGN(16)
    {
        . = ORIGIN(IRQDBIN) + LENGTH(IRQDBIN);
    } > IRQDBIN

    .irqd_shm (NOLOAD) : ALIGN(32)
    {
        . = ORIGIN(IRQDSHM) + LENGTH(IRQDSHM);
    } > IRQDSHM
#endif
#
    .heap_ext1 (NOLOAD) : ALIGN(16)
    {
#ifdef CONFIG_HEAP_AUTO_SIZE
        . = ORIGIN(EXTRAM1) + LENGTH(EXTRAM1);
#else
        *(.heap_ext1)
#endif
    } > EXTRAM1

    .heap_ext2 (NOLOAD) : ALIGN(16)
    {
#ifdef CONFIG_HEAP_AUTO_SIZE
        . = ORIGIN(EXTRAM2) + LENGTH(EXTRAM2);
#else
        *(.heap_ext2)
#endif
    } > EXTRAM2

#endif

	EXPORT_SECTION_INFO(init, 	 .head);
	EXPORT_SECTION_INFO(text, 	 .text);
#ifdef CONFIG_PM_TEST
    EXPORT_SECTION_INFO(rram,    .text.rram);
#endif
	EXPORT_SECTION_INFO(execit,  .exec.itable);
	EXPORT_SECTION_INFO(rodata,  .rodata);
	EXPORT_SECTION_INFO(data, 	 .data);
	EXPORT_SECTION_INFO(bss, 	 .bssreloc);
	EXPORT_SECTION_INFO(stack,	 .stack);
	EXPORT_SECTION_INFO(heap, 	 .heap);
#ifdef CONFIG_N22_ONLY
#ifdef CONFIG_SDIO_USE_IRQD
    EXPORT_SECTION_INFO(irqdbin, .irqd_bin);
    EXPORT_SECTION_INFO(irqdshm, .irqd_shm);
#endif
    EXPORT_SECTION_INFO(heapext1,.heap_ext1);
    EXPORT_SECTION_INFO(heapext2,.heap_ext2);
#endif

	__vma_start = __text_start;

	PROVIDE(end = .);
	PROVIDE(_stack = __stack_end);
    PROVIDE(__ram_limit = ORIGIN(RAM) + LENGTH(RAM));
}
