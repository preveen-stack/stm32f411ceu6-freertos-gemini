#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configCPU_CLOCK_HZ              16000000
#define configTICK_RATE_HZ              1000

#define configMAX_PRIORITIES            5
#define configMINIMAL_STACK_SIZE        128
#define configTOTAL_HEAP_SIZE           (10 * 1024)

#define configUSE_PREEMPTION            1
#define configUSE_TIME_SLICING          1

#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0

#define configUSE_16_BIT_TICKS          0

#define configUSE_MUTEXES               1
#define configUSE_COUNTING_SEMAPHORES   1

#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION  0

#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191

/* No need to map if we use FreeRTOS names in vector table */

#define INCLUDE_vTaskDelay 1

#endif /* FREERTOS_CONFIG_H */
