#include "wise.lds.h"

#ifndef CONFIG_BUILD_ROM
#error "This is only for building ROM library!!"
#endif

#ifdef CONFIG_ROM_SIM_FLASH
#define ROMADDR        CONFIG_ROM_SIM_FLASH_ADDRESS
#define ROMSIZE        CONFIG_ROM_SIM_FLASH_SIZE
#endif

MEMORY
{
#ifdef CONFIG_ROM_SIM_FLASH
    ROM (rx)      : ORIGIN = ROMADDR,    LENGTH = ROMSIZE
#else
#ifdef __rom_original__
    ROM (rx)      : ORIGIN = 0x00106000, LENGTH = 312K
#else
    ROM (rx)      : ORIGIN = 0x00106000, LENGTH = 320K
#endif
#endif
    ILM (rx)      : ORIGIN = 0x00000000, LENGTH = 640K
    DLMROM (rw)   : ORIGIN = 0x00200000, LENGTH = 43K
    DLM (rw)      : ORIGIN = 0x0020AC00, LENGTH = 384K - 43K - 2K
    STACKDLM (rw) : ORIGIN = 0x0025F800, LENGTH = 2K
    SHM1 (rw)     : ORIGIN = 0x40000000, LENGTH = 20K
    SHM2 (rw)     : ORIGIN = 0x40005000, LENGTH = 20K
    RAM (rw)      : ORIGIN = 0x4000A000, LENGTH = 64K
    D25RRAM (rw)  : ORIGIN = 0x4001A000, LENGTH = 960
    N22RRAM (rw)  : ORIGIN = 0x4001A3C0, LENGTH = 960
    PMRRAM (rw)   : ORIGIN = 0x4001A780, LENGTH = 128
    SHMRRAM (rw)  : ORIGIN = 0x4001A800, LENGTH = 21K
    APPRRAM (rw)  : ORIGIN = 0x4001FC00, LENGTH = 1K
}

_STACK_SIZE = 2K;

ENTRY(_start)

SECTIONS
{
#define MT(x)   *x/built-in.o(.text* .rodata* .srodata*)
#define MD(x)   *x/built-in.o(.data* .sdata*)
#define MB(x)   *x/built-in.o(.bss* .sbss*)
#define KT(x)   KEEP(*x/built-in.o(.text* .rodata* .srodata*));
#define KD(x)   KEEP(*x/built-in.o(.data* .sdata*));
#define KB(x)   KEEP(*x/built-in.o(.bss* .sbss*));
#define LT(x,y) *x:y(.text* .rodata* .srodata* .eh_frame*)
#define LD(x,y) *x:y(.data* .sdata*)
#define LB(x,y) *x:y(.bss*  .sbss*)

    .rom.text :
    {
        KEEP(*(.version));

        LT(libc.a, *)
        LT(libm.a, *)
        LT(libgcc.a, *)
        LT(libnosys.a, *)

        MT(kernel/FreeRTOS)
        MT(kernel/cmsis-freertos)
        MT(kernel/freebsd)

        MT(hal/fs)
        MT(hal/drivers/wlan)
        KT(hal/drivers/clk)

        KT(lib/stdlib)
        MT(lib/crypto)
        MT(lib/serial)
        MT(lib/net80211)
        MT(lib/ble/nimble_ctrl)
        KT(lib/net/freebsd/mbuf)
        KT(lib/libifconfig)

        KEEP(*(.rom.text*));
    } > ROM

    .rom.data :
    {
        LD(libc.a, *)
        LD(libm.a, *)
        LD(libgcc.a, *)
        LD(libnosys.a, *)

        MD(kernel/FreeRTOS)
        MD(kernel/cmsis-freertos)
        MD(kernel/freebsd)

        MD(hal/fs)
        MD(hal/drivers/wlan)
        MD(hal/drivers/clk)

        KD(lib/stdlib)
        MD(lib/crypto)
        MD(lib/serial)
        MD(lib/net80211)
        MD(lib/ble/nimble_ctrl)
        KD(lib/net/freebsd/mbuf)
        KB(lib/libifconfig)

        . = ALIGN(4);

        rom_patch_fun_start = .;
        KEEP(*(.rom.patch_func*));
        rom_patch_fun_end = .;
        . = ALIGN(4);

    } > DLMROM AT > ROM

    /*
     * Fixed Address to hold a fuction table
     */
    .func_tab (NOLOAD) :
    {
        KEEP(*(SORT(.func_tab*)));
        . = ALIGN(4);
    } > DLMROM

    .rom.bssreloc (NOLOAD) : ALIGN(8)
    {
        LB(libc.a, *)
        LB(libm.a, *)
        LB(libgcc.a, *)
        LB(libnosys.a, *)

        MB(kernel/FreeRTOS)
        MB(kernel/cmsis-freertos)
        MB(kernel/freebsd)

        MB(hal/fs)
        MB(hal/drivers/wlan)
        MB(hal/drivers/clk)

        KB(lib/stdlib)
        MB(lib/crypto)
        MB(lib/serial)
        MB(lib/net80211)
        MB(lib/ble/nimble_ctrl)
        MB(lib/net/freebsd/mbuf)
        MB(lib/libifconfig)

        . = ALIGN(4);
        *(COMMON)
        . = ALIGN(4);

    } > DLMROM

#undef MT
#undef MD
#undef MB
#undef KT
#undef KD
#undef KB
#undef LT
#undef LD
#undef LB

    .text.rram : {
        *(.rram)
    } > N22RRAM

    /*
      * .text: FLASH text/rodata section
     * - vector table
     */
    .text :
    {
        __stext = .;
        KEEP(*(.init))
        *(.text)
        *(.text*)
        *(*.text*)
        __etext = .;

    } > ILM

    .exec.itable :
    {
        *(.exec.itable )
    } > ILM

    .rodata :
    {
        *(.rodata*)
        *(.rodata)
        *(.srodata*)

        KEEP(*(.eh_frame*))
    } > DLM

    .data :
    {
        *(.data)
        *(.data*)
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
    } > DLM

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
        KEEP(*(.node.data));
        . = ALIGN(4);
    } > DLM
#endif

    .stack (NOLOAD) : ALIGN(16)
    {
        . += _STACK_SIZE;
    } > STACKDLM

    .heap (NOLOAD) : ALIGN(16)
    {
#ifdef CONFIG_HEAP_AUTO_SIZE
        . = ORIGIN(DLM) + LENGTH(DLM);
#else
        *(.heap)
#endif
    } > DLM

#ifdef CONFIG_N22_ONLY

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

    .shm1 (NOLOAD) : {
        *(.usb_ram)
        . = ORIGIN(SHM1) + LENGTH(SHM1);
    } > SHM1

    .shm2 (NOLOAD) : {
        . = ORIGIN(SHM2) + LENGTH(SHM2);
    } > SHM2

    .bufreloc (NOLOAD) : ALIGN(4)
    {
        *(.buffer*)
        . = ALIGN(8);
        *(.dma_desc)
    } > RAM

    .pmrram (NOLOAD) : ALIGN(4)
    {
        *(.pmrram)
    } > PMRRAM

    EXPORT_SECTION_INFO(romt,   .rom.text);
    EXPORT_SECTION_INFO(romd,   .rom.data);
    EXPORT_SECTION_INFO(romb,   .rom.bssreloc);
    EXPORT_SECTION_INFO(funt,   .func_tab);

    EXPORT_SECTION_INFO(rram,   .text.rram);
    EXPORT_SECTION_INFO(text,   .text);
    EXPORT_SECTION_INFO(execit, .exec.itable);
    EXPORT_SECTION_INFO(rodata, .rodata);
    EXPORT_SECTION_INFO(data,   .data);
    EXPORT_SECTION_INFO(bss,    .bssreloc);
#ifdef CONFIG_SUPPORT_NODE_POOL
	EXPORT_SECTION_INFO(nodebss,.node.data);
#endif
    EXPORT_SECTION_INFO(buffer, .bufreloc);
    EXPORT_SECTION_INFO(pmrram, .pmrram);
    EXPORT_SECTION_INFO(heap,   .heap);
    EXPORT_SECTION_INFO(stack,  .stack);
    EXPORT_SECTION_INFO(shm1,   .shm1);
    EXPORT_SECTION_INFO(shm2,   .shm2);

    PROVIDE(end = .);
    PROVIDE(_stack = __stack_end);
}

/* NOCROSSREFS_TO(tosection fromsection …) */
NOCROSSREFS_TO(.text .rom.text .rom.data .rom.bssreloc)
NOCROSSREFS_TO(.iram.text .rom.text .rom.data .rom.bssreloc)
NOCROSSREFS_TO(.iram.data .rom.text .rom.data .rom.bssreloc)
NOCROSSREFS_TO(.xip.text .rom.text .rom.data .rom.bssreloc)
NOCROSSREFS_TO(.rodata .rom.text .rom.data .rom.bssreloc)
NOCROSSREFS_TO(.data .rom.text .rom.data .rom.bssreloc)
NOCROSSREFS_TO(.bssreloc .rom.text .rom.data .rom.bssreloc)
NOCROSSREFS_TO(set_ieee80211_ioctl_getset .rom.text .rom.data .rom.bssreloc)
NOCROSSREFS_TO(set_ieee80211_ioctl_setset .rom.text .rom.data .rom.bssreloc)
