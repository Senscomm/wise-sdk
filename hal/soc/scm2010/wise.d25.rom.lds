#include "wise.lds.h"

MEMORY
{
    ROM (rx)    : ORIGIN = 0x00100000, LENGTH = 48K
	DLM (rw)    : ORIGIN = 0x00200000, LENGTH = 192K
    HIF (rw)    : ORIGIN = 0x40000000, LENGTH = 20K - 64
}

_STACK_SIZE = 2K;

ENTRY(_start)

SECTIONS
{
	/*
 	 * .text: text/rodata section
	 * - vector table
	 */
	.text :
	{
		__stext = .;
		KEEP(*(.init))
		*(.text)
		*(.text*)
		__etext = .;

	} > ROM

	.exec.itable :
	{ 
		*(.exec.itable )
	} > ROM

	.func_tab :
	{
		KEEP(*(SORT(.func_tab*)));

		. = ALIGN(4);
	} > ROM

	.rodata :
	{
		*(.rodata*)
		*(.rodata)
		*(.srodata*)

		KEEP(*(.eh_frame*))
	} > ROM

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
	} > DLM AT > ROM

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

    .hif  (NOLOAD) : {
		*(.usb_ram)
	} > HIF

	EXPORT_SECTION_INFO(text, 	.text);
	EXPORT_SECTION_INFO(execit, .exec.itable);
	EXPORT_SECTION_INFO(rodata, .rodata);
	EXPORT_SECTION_INFO(data, 	.data);
	EXPORT_SECTION_INFO(bss, 	.bssreloc);
	EXPORT_SECTION_INFO(heap, 	.heap);
	EXPORT_SECTION_INFO(stack,	.stack);

	PROVIDE(end = .);
	PROVIDE(_stack = __stack_end);
}
