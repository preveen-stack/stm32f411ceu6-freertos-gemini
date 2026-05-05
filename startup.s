.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global Reset_Handler
.global vPortSVCHandler
.global xPortPendSVHandler
.global xPortSysTickHandler

.section .isr_vector, "a", %progbits
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word 0                /* NMI */
    .word 0                /* HardFault */
    .word 0                /* MemManage */
    .word 0                /* BusFault */
    .word 0                /* UsageFault */
    .word 0, 0, 0, 0       /* Reserved */
    .word vPortSVCHandler  /* SVCall */
    .word 0                /* DebugMonitor */
    .word 0                /* Reserved */
    .word xPortPendSVHandler   /* PendSV */
    .word xPortSysTickHandler  /* SysTick */
    
    /* External Interrupts */
    .word 0,0,0,0,0,0,0,0  /* 0-7 */
    .word 0,0,0,0,0,0,0,0  /* 8-15 */
    .word 0,0,0,0,0,0,0,0  /* 16-23 */
    .word 0,0,0,0,0,0,0,0  /* 24-31 */
    .word 0,0,0,0,0        /* 32-36 */
    .word USART1_IRQHandler /* 37 */

.section .text.Reset_Handler
.thumb_func
Reset_Handler:
    /* init data */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
copy_data:
    cmp r1, r2
    ittt lt
    ldrlt r3, [r0], #4
    strlt r3, [r1], #4
    blt copy_data

    /* zero bss */
    ldr r0, =_sbss
    ldr r1, =_ebss
    mov r2, #0
zero_bss:
    cmp r0, r1
    itt lt
    strlt r2, [r0], #4
    blt zero_bss

    /* FPU Enable */
    ldr r0, =0xE000ED88
    ldr r1, [r0]
    orr r1, r1, #(0xF << 20)
    str r1, [r0]

    bl main
    b .
