OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    ROM (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    EWRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
}

SECTIONS
{
    . = ORIGIN(ROM);
    .gba.header : { KEEP(*(.gba.header)) } > ROM
    . = ORIGIN(ROM) + 0xc0;
    .text : { KEEP(*(.text.startup)) *(.text*) *(.rodata*) } > ROM
    .data : { *(.data*) } > ROM
    .bss (NOLOAD) :
    {
        __bss_start__ = .;
        *(.bss*) *(COMMON)
        __bss_end__ = .;
    } > EWRAM
    /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.attributes*) }
}