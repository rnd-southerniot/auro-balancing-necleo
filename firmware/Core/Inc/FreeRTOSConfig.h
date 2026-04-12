/**
 * FreeRTOSConfig.h — FreeRTOS configuration for STM32F401RE + micro-ROS
 *
 * Critical settings:
 * - configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
 *   Allows USART2 ISR (priority 0) to preempt FreeRTOS critical sections.
 *   This preserves the HW-BUG-02 fix: USART2 must never be blocked.
 * - configTOTAL_HEAP_SIZE = 50KB for micro-ROS allocations
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ── Cortex-M4 specific ─────────────────────────────────────── */
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0

/* ── Core config ─────────────────────────────────────────────── */
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      84000000UL
#define configSYSTICK_CLOCK_HZ                  84000000UL
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    7
#define configMINIMAL_STACK_SIZE                256   /* words = 1KB */
#define configTOTAL_HEAP_SIZE                   ((size_t)(45 * 1024))
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0

/* ── Memory allocation ───────────────────────────────────────── */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* ── Hook functions ──────────────────────────────────────────── */
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1

/* ── Runtime stats ───────────────────────────────────────────── */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ── Co-routine (not used) ───────────────────────────────────── */
#define configUSE_CO_ROUTINES                   0

/* ── Software timer ──────────────────────────────────────────── */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               2
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* ── Interrupt nesting ───────────────────────────────────────── */
/*
 * CRITICAL: These settings control which ISRs can call FreeRTOS API.
 *
 * STM32 uses 4 priority bits (0-15), 0 = highest.
 * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5 means:
 *   Priorities 0-4: CANNOT call FreeRTOS API (but CAN preempt)
 *   Priorities 5-15: CAN call FreeRTOS API
 *
 * USART2 ISR at priority 0 → runs ABOVE FreeRTOS → never blocked
 * TIM10  ISR at priority 1 → runs ABOVE FreeRTOS → never blocked
 * This is intentional: HW-BUG-02 fix requires USART2 to always preempt.
 *
 * SysTick at priority 15 (lowest) — FreeRTOS scheduler tick
 */
#ifdef __NVIC_PRIO_BITS
  #define configPRIO_BITS  __NVIC_PRIO_BITS
#else
  #define configPRIO_BITS  4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ── Optional includes ───────────────────────────────────────── */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTimerPendFunctionCall          1

/* ── Map FreeRTOS handlers to STM32 IRQ names ────────────────── */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
/* SysTick_Handler is NOT mapped here — we call xPortSysTickHandler
 * from our existing SysTick_Handler in stm32f4xx_it.c to preserve
 * HAL_GetTick() functionality. */

/* ── Assert ──────────────────────────────────────────────────── */
#define configASSERT(x) if ((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

#endif /* FREERTOS_CONFIG_H */
