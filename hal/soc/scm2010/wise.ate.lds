#include <wise.lds.h>

MEMORY
{
    HDR (r)       : ORIGIN = 0x40018000, LENGTH = 160
    RAM (rw)      : ORIGIN = 0x400180a0, LENGTH = 24K - 160
	DATA(rw)	  : ORIGIN = 0x4001E0a0, LENGTH = 8K - 160
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
		__etext = .;
	} > RAM 

	.exec.itable :
	{
		*(.exec.itable )
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
		. = ALIGN(4);
		*(.sdata)
		*(.sdata*)
		. = ALIGN(4);
	} > RAM

	.bssreloc (NOLOAD) : ALIGN(4)
	{
		*(.bss*)
		*(.sbss*)
		. = ALIGN(4);
		*(COMMON)
	} > DATA

	.stack (NOLOAD) : ALIGN(16)
	{
   		. += _STACK_SIZE;
	} > DATA

	EXPORT_SECTION_INFO(init, 	 .head);
	EXPORT_SECTION_INFO(text, 	 .text);

	EXPORT_SECTION_INFO(execit,  .exec.itable);
	EXPORT_SECTION_INFO(rodata,  .rodata);
	EXPORT_SECTION_INFO(data, 	 .data);
	EXPORT_SECTION_INFO(bss, 	 .bssreloc);
	EXPORT_SECTION_INFO(stack,	 .stack);

	__vma_start = __text_start;

	PROVIDE(end = .);
	PROVIDE(_stack = __stack_end);
    PROVIDE(__ram_limit = ORIGIN(RAM) + LENGTH(RAM));
}
