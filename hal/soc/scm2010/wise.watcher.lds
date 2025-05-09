#include <wise.lds.h>

MEMORY
{
#ifndef CONFIG_WATCHER_MEM_EXTEND
    RRAMDATA (rx) : ORIGIN = 0x4001A780, LENGTH = 128
    RAMTEXT (rx) : ORIGIN = 0x4001A800, LENGTH = 19K + 640
    RAMDATA (rw) : ORIGIN = 0x4001F680, LENGTH = 1K
    RAMRESV (rw) : ORIGIN = 0x4001FA80, LENGTH = 2K - 640
#else
    RRAMDATA (rx) : ORIGIN = 0x40019780, LENGTH = 128
    RAMTEXT (rx) : ORIGIN = 0x40019800, LENGTH = 23K
    RAMDATA (rw) : ORIGIN = 0x4001F400, LENGTH = 1K + 128
    RAMRESV (rw) : ORIGIN = 0x4001F880, LENGTH = 2K - 128
#endif
}

_STACK_SIZE = 832;

ENTRY(_start)

SECTIONS
{
    .pmrram :
    {
        *(.pmrram)
        . = ORIGIN(RRAMDATA) + LENGTH(RRAMDATA);
    } > RRAMDATA

    .text :
    {
        __stext = .;
        KEEP(*(.init))
        *(.text)
        *(.text*)
        __etext = .;

    } > RAMTEXT

    .exec.itable :
    {
        *(.exec.itable )
    } > RAMTEXT

    .rodata :
    {
        *(.srodata*)
        *(.srodata)
        *(.rodata*)
        *(.rodata)
        *(.iram.text)

        KEEP(*(.eh_frame*))
    } > RAMTEXT

    .data :
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
    } > RAMDATA AT > RAMTEXT

    .bssreloc (NOLOAD) : ALIGN(4)
    {
        _sbss = ABSOLUTE(.);
        *(.bss*)
        *(.sbss*)
        . = ALIGN(4);
        *(COMMON)
        _ebss = ABSOLUTE(.);
    } > RAMDATA

    .stack (NOLOAD) : ALIGN(16)
    {
        . += _STACK_SIZE;
    } > RAMDATA

    .heap (NOLOAD) : ALIGN(16)
    {
        *(.heap)
    } > RAMDATA

    .waon (NOLOAD) : ALIGN(4)
    {
        _waon = .;
        *(.waon)
    } > RAMRESV

    EXPORT_SECTION_INFO(pmrram, .pmrram);

    _stack = ADDR(.stack) + SIZEOF(.stack);
    _ldata = LOADADDR(.data);
}
