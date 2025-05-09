#include "wise.lds.h"

MEMORY
{
    ROM (rx)    : ORIGIN = 0x00106000,  LENGTH = 312K
	ILM (rx)    : ORIGIN = 0x00000000, 	LENGTH =  32K
	DLM (rw)    : ORIGIN = 0x0020AC00, 	LENGTH = 192K - 43K
    RAM (rw)    : ORIGIN = 0x40000000, 	LENGTH = 128K
  EXILM (rw) 	: ORIGIN = 0xA0000000,  LENGTH =  32K
  EXDLM (rw) 	: ORIGIN = 0xA0200000,  LENGTH = 192K
}

_STACK_SIZE = 2K;

ENTRY(_start)

SECTIONS
{

#define _STR(x) #x
#define STR(x) _STR(x)
#define M(x) STR(*x/##*.a:*.o)(.text* .iram.text)

    .text.boot :
    {
        *(.init)
        *(.text.reset_handler)
        *(.text.cpu_init)
        *(.text.c_startup_common)
        *(.xip.boot)
        *(.ilm.ble.text)
        *(.ilm.wlan.tx.text)
        *(.ilm.wlan.text)
        *(.ilm.text)

 		M(hal/core)
        M(hal/arch/ndsv5)
		M(hal/soc/scm2010/rom)
		M(hal/soc/scm2010)
		M(hal/soc/scm2010/freertos/V10.2.1)
		M(hal/drivers/core)
		M(hal/drivers/clk)
		M(hal/drivers/systimer)
		M(hal/drivers/timer)
		M(hal/drivers/serial)

		M(lib/fs)
 	} > ILM

	.text.ram1 :
    {
        *(.iram.text)
        *(.xip.text)

   		M(hal/drivers/wlan)
		M(lib/net/freebsd)
		M(lib/tinyusb)
		M(lib/scdc)

        *(.exec.itable )
    } > RAM

    .text.ram2 :
    {
        *(.text)
        *(.text*)
        *(.exec.itable )

		M(hal/drivers/rf)

    } > EXILM

#undef M
    .rodata : ALIGN(4)
    {
    *(.rodata*)
        *(.rodata)
        *(.srodata*)

        KEEP(*(.eh_frame*))
    } > DLM

    .data : ALIGN(4)
    {
        *(.data)
        *(.data*)
        *(.iram.data)

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

    .usb (NOLOAD) : ALIGN(4)
    {
        *(.usb_ram)
    } > RAM

    .bufreloc (NOLOAD) : ALIGN(4)
    {
        *(.buffer_cache*)
        . = ALIGN(32);
        *(.buffer_chunk*)
        . = ALIGN(32);
        *(.buffer_ext*)
        . = ALIGN(32);
        *(.buffer*)
        . = ALIGN(32);
        *(.dma_desc)
        . = ALIGN(64);
    } > EXDLM

    EXPORT_SECTION_INFO(boot,   .text.boot);
    EXPORT_SECTION_INFO(text,   .text.ram1);
    EXPORT_SECTION_INFO(etext,  .text.ram2);
    EXPORT_SECTION_INFO(rodata, .rodata);
    EXPORT_SECTION_INFO(data,   .data);
    EXPORT_SECTION_INFO(bss,    .bssreloc);
#ifdef CONFIG_SUPPORT_NODE_POOL
    EXPORT_SECTION_INFO(nodebss,.node.data);
#endif
    EXPORT_SECTION_INFO(usb,    .usb);
    EXPORT_SECTION_INFO(buffer, .bufreloc);
    EXPORT_SECTION_INFO(heap,   .heap);
    EXPORT_SECTION_INFO(stack,  .stack);

    __vma_start = __boot_start;

    PROVIDE(end = .);
    PROVIDE(_stack = __stack_end);
}
