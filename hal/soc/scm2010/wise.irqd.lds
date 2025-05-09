#include "wise.lds.h"

MEMORY
{
    ILM (rx) : ORIGIN = 0x00000000, LENGTH = 3K
	SHM (rx) : ORIGIN = 0x00000c00, LENGTH = 3K
}

_STACK_SIZE = 1024;

ENTRY(_start)

SECTIONS
{
    .text : ALIGN(4)
    {
        __stext = .;
        KEEP(*(.init))
        *(.text)
        *(.text*)
        __etext = .;
        . = ALIGN(4);

    } > ILM

    .exec.itable : ALIGN(4)
    {
        *(.exec.itable )
        . = ALIGN(4);
    } > ILM

    .rodata : ALIGN(4)
    {
        *(.srodata*)
        *(.srodata)
        *(.rodata*)
        *(.rodata)

        KEEP(*(.eh_frame*))
        . = ALIGN(4);
    } > ILM

    .data : ALIGN(4)
    {
        _sdata = ABSOLUTE(.);
        *(.data)
        *(.data*)
        . = ALIGN(4);
        *(.sdata)
        *(.sdata*)
        . = ALIGN(4);

        *(.data.impure_data);
        *(.data._impure_ptr);
        . = ALIGN(4);
        _edata = ABSOLUTE(.);
    } > ILM

    .bssreloc (NOLOAD) : ALIGN(4)
    {
        _sbss = ABSOLUTE(.);
        *(.bss*)
        *(.sbss*)
        . = ALIGN(4);
        *(COMMON)
        _ebss = ABSOLUTE(.);
    } > ILM

    .stack (NOLOAD) : ALIGN(16)
    {
        . += _STACK_SIZE;
    } > ILM

    .heap (NOLOAD) : ALIGN(16)
    {
        *(.heap)
    } > ILM

	.fifo (NOLOAD) : ALIGN(32)
	{
        . = ORIGIN(SHM) + LENGTH(SHM);
	} > SHM

    EXPORT_SECTION_INFO(fifo, .fifo);

    _stack = ADDR(.stack) + SIZEOF(.stack);
    _ldata = LOADADDR(.data);

}
