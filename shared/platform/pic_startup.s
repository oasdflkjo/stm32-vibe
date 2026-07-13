.syntax unified
.cpu cortex-m3
.thumb

.equ BOOT_HANDOFF_ADDR, 0x20013E00
.equ APP_MANIFEST_OFFSET, 0x200
.equ DATA_LOAD_FIELD_OFFSET, 52

.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr r3, =BOOT_HANDOFF_ADDR
    ldr r4, [r3, #12]
    add r3, r4, #APP_MANIFEST_OFFSET
    ldr r2, [r3, #DATA_LOAD_FIELD_OFFSET]
    add r2, r2, r4
    ldr r0, =_sdata
    ldr r1, =_edata
1:
    cmp r0, r1
    bcs 2f
    ldr r3, [r2], #4
    str r3, [r0], #4
    b 1b
2:
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
3:
    cmp r0, r1
    bcs 4f
    str r2, [r0], #4
    b 3b
4:
    bl main
5:
    b 5b
.size Reset_Handler, .-Reset_Handler
