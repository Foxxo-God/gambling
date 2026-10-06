.syntax unified
.cpu arm7tdmi
.arm

.section .gba.header, "a", %progbits
.global gba_header
gba_header:
    .word 0xea00002e
    .space 156, 0
    .ascii "LUCKY CIRCU"
    .byte 0
    .ascii "ALUC"
    .ascii "01"
    .byte 0x96
    .byte 0
    .byte 0
    .space 7, 0
    .byte 0
    .byte 0xad
    .byte 0

.section .text.startup, "ax", %progbits
.global _start
.type _start, %function
_start:
    ldr sp, =0x03007f00
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
1:
    cmp r0, r1
    strlo r2, [r0], #4
    blo 1b
    bl main
2:
    b 2b
.size _start, . - _start