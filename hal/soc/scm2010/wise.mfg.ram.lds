#include "wise.lds.h"

MEMORY
{
#ifdef CONFIG_SUPPORT_GCOV
    ILM (rx)      : ORIGIN = 0x00000000, LENGTH = 10K
    GCOV (rw)     : ORIGIN = 0x00002800, LENGTH = 640K -10K
#else
    ILM (rx)      : ORIGIN = 0x00000000, LENGTH = 32K
#endif
    DLMROM (rw)   : ORIGIN = 0x00200000, LENGTH = 43K
    DLM (rw)      : ORIGIN = 0x0020AC00, LENGTH = 192K - 43K
#ifdef CONFIG_N22_ONLY
    BUFFER (rw)   : ORIGIN = 0x40000000, LENGTH = 104K
#else
    SHM1 (rw)     : ORIGIN = 0x40000000, LENGTH = 20K
    SHM2 (rw)     : ORIGIN = 0x40005000, LENGTH = 20K
    BUFFER (rw)   : ORIGIN = 0x4000A000, LENGTH = 64K
#endif
    N22RRAM (rw)  : ORIGIN = 0x4001A000, LENGTH = 1920
    PMRRAM (rw)   : ORIGIN = 0x4001A780, LENGTH = 128
    SHMWRAM (rw)  : ORIGIN = 0x4001A800, LENGTH = 19K + 640
    SHMRRAM (rw)  : ORIGIN = 0x4001F680, LENGTH =  3K - 640
    FLASH (rw)    : ORIGIN = CONFIG_FLASH_IMAGE_OFFSET, LENGTH = CONFIG_FLASH_IMAGE_SIZE
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

ENTRY(_start)

SECTIONS
{
    .text.boot :
    {
        __stext = .;
        *(.init)
        *(.text.reset_handler)
        *(.text.cpu_init)
        *(.text.c_startup_common)
        *(.xip.boot)
        KEEP(*(.version))
    } > EXTRAM2

    .text.rram : {
        *(.rram)
    } > N22RRAM AT > EXTRAM2

    /*
     * .text.ram : ram code
     * - vector table
     * - SPI flash driver
     * - kernel
     * - performance critical components
     */
     .text.ram : ALIGN(4)
     {

#define _STR(x) #x
#define STR(x) _STR(x)
#define M(x) STR(*x/##*.a:*.o)(.text* .iram.text)
#define L(x,y) *x:y(.text* .iram.text .rodata* .srodata*)

#ifdef CONFIG_BOOTLOADER
        M(hal/arch/ndsv5)
        M(hal/soc/scm2010)

        M(hal/drivers/core)
        M(hal/drivers/timer)
        M(hal/drivers/serial)
        M(hal/drivers/spi-flash)

        M(lib/serial)

        M(kernel/FreeRTOS)
        M(kernel/cmsis-freertos)
        *(.text.ktime)
#endif

        L(libc.a,lib_a-memcpy.o)
        L(libc.a,lib_a-memcmp.o)
        L(libc.a,lib_a-memset.o)
        L(libc.a,lib_a-strcmp.o)
        L(libc.a,lib_a-strncmp.o)
        L(libc.a,lib_a-strcpy.o)
        L(libc.a,lib_a-strlen.o)
        L(libc.a,lib_a-ctype_.o)

#ifdef CONFIG_SUPPORT_GCOV
        M(hal/arch/ndsv5)
        M(kernel)
#else
        *(.ilm.text)
#endif
        *(.exec.itable )

#ifndef CONFIG_LINK_TO_ROM
        KEEP(*(SORT(.func_tab*)));
#endif

        . = ALIGN(4);

        __etext = .;

#undef M
#undef L
    } > ILM AT > EXTRAM2

    .text.xip : ALIGN(4)
    {
        __stext1 = .;
        *(.text)
        *(.text*)
        *(.xip.text)
        *(.iram.text)
#ifdef CONFIG_SUPPORT_GCOV
        *(.ilm.text)
#endif
        __etext1 = .;
    } > EXTRAM2 AT > EXTRAM2

#ifdef CONFIG_SUPPORT_GCOV
    .init_array : ALIGN(4)
    {
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        . = ALIGN(4);
    } > GCOV AT > EXTRAM2

    .gcovdata : ALIGN(4)
    {
        *(.data.__gcov*)
        . = ALIGN(4);

        *(.sdata.__gcov*)
        . = ALIGN(4);
    } > GCOV AT > EXTRAM2

    .gcovbss : ALIGN(4)
    {
        *(.bss.__gcov*)
        . = ALIGN(4);

        *(.sbss.__gcov*)
        . = ALIGN(4);
    } > GCOV AT > EXTRAM2
#endif

    .rodata : ALIGN(4)
    {
        *(.rodata*)
        *(.rodata)
        *(.srodata*)

        KEEP(*(.eh_frame*))
    } > EXTRAM2 AT > EXTRAM2

    .data : ALIGN(4)
    {
        *(.data)
        *(.data*)
        *(.iram.data)

#ifndef CONFIG_LINK_TO_ROM
        *(.rom.data*)
        *(.rom.patch_func*)
#endif


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
    } > DLM AT > EXTRAM2

    .bssreloc (NOLOAD) : ALIGN(4)
    {
        *(.bss*)
        *(.sbss*)
        . = ALIGN(4);
        *(COMMON)
    } > DLM

#ifdef CONFIG_SUPPORT_NODE_POOL
    .node.data (NOLOAD) : ALIGN(4)
    {
        *(.node.data)
        . = ALIGN(4);
    } > DLM
#endif

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

    .heap_ext1 (NOLOAD) : ALIGN(16)
    {
#ifdef CONFIG_HEAP_AUTO_SIZE
        . = ORIGIN(EXTRAM1) + LENGTH(EXTRAM1);
#else
        *(.heap_ext1)
#endif
    } > EXTRAM1

    .dma (NOLOAD) : ALIGN(16)
    {
        *(.dma_buffer)
        . = ALIGN(32);
        *(.dma_desc)
    } > EXTRAM2

    .heap_ext2 (NOLOAD) : ALIGN(16)
    {
#ifdef CONFIG_HEAP_AUTO_SIZE
        . = ORIGIN(EXTRAM2) + LENGTH(EXTRAM2);
#else
        *(.heap_ext2)
#endif
    } > EXTRAM2

#endif

#ifndef CONFIG_N22_ONLY

    .shm1 (NOLOAD) : {
        . = ORIGIN(SHM1) + LENGTH(SHM1);
    } > SHM1

    .shm2 (NOLOAD) : {
        . = ORIGIN(SHM2) + LENGTH(SHM2);
    } > SHM2

#endif

    .bufreloc (NOLOAD) : ALIGN(4)
    {
        *(.buffer_ext*)
        . = ALIGN(32);
        *(.buffer_chunk*)
        . = ALIGN(32);
        *(.buffer_cache*)
        . = ALIGN(32);

#ifdef CONFIG_TINYUSB
        *(.usb_ram)
        . = ALIGN(32);
#endif

#ifdef CONFIG_SDIO
        *(.sdio_dma_desc)
#endif
    } > BUFFER

    .pmrram (NOLOAD) : ALIGN(4)
    {
        *(.pmrram)
    } > PMRRAM

    .shrram (NOLOAD) : ALIGN(4)
    {
        *(.waon)
    } > SHMRRAM

#if defined(CONFIG_MCUBOOT_BL) || defined(CONFIG_BOOTLOADER)
    EXPORT_SECTION_INFO(init,    .head);
    EXPORT_SECTION_INFO(boot,    .text.boot);
#else
    EXPORT_SECTION_INFO(init,    .text.boot);
#endif
    EXPORT_SECTION_INFO(rram,    .text.rram);
    EXPORT_SECTION_INFO(text,    .text.ram);
    EXPORT_SECTION_INFO(rodata,  .rodata);
    EXPORT_SECTION_INFO(data,    .data);
    EXPORT_SECTION_INFO(bss,     .bssreloc);
#ifdef CONFIG_SUPPORT_NODE_POOL
    EXPORT_SECTION_INFO(nodebss, .node.data);
#endif
    EXPORT_SECTION_INFO(buffer,  .bufreloc);
    EXPORT_SECTION_INFO(pmrram,  .pmrram);
    EXPORT_SECTION_INFO(waon,    .shrram);
    EXPORT_SECTION_INFO(heap,    .heap);
#ifdef CONFIG_N22_ONLY
#ifdef CONFIG_SDIO_USE_IRQD
    EXPORT_SECTION_INFO(irqdbin, .irqd_bin);
    EXPORT_SECTION_INFO(irqdshm, .irqd_shm);
#endif
    EXPORT_SECTION_INFO(heapext1,.heap_ext1);
    EXPORT_SECTION_INFO(heapext2,.heap_ext2);
#else
    EXPORT_SECTION_INFO(shm1,    .shm1);
    EXPORT_SECTION_INFO(shm2,    .shm2);
#endif
    EXPORT_SECTION_INFO(stack,   .stack);

#ifdef CONFIG_SUPPORT_GCOV
    EXPORT_SECTION_INFO(init_array, .init_array);
    EXPORT_SECTION_INFO(gcovdata, .gcovdata);
    EXPORT_SECTION_INFO(gcovbss,  .gcovbss);
#endif

#if defined(CONFIG_MCUBOOT_BL) || defined(CONFIG_BOOTLOADER)
    __vma_start = __boot_start;
#else
    __vma_start = __init_start;
#endif

    PROVIDE(end = .);
    PROVIDE(_stack = __stack_end);
}
