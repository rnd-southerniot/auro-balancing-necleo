/**
 * @file stm32f4xx_it.c
 * @brief Interrupt service routines.
 *
 * PORTED FROM: STM32F429ZI → STM32F401RE (NUCLEO)
 * Changes: TIM6/TIM7 → TIM10/TIM11, USART1 → USART2,
 *          DMA2_Stream7 → DMA1_Stream6, fault LED PG14 → PB2
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* ── Cortex-M4 system exceptions ───────────────────────────── */

void NMI_Handler(void)
{
}

/* Fault diagnostics — read via GDB after crash */
volatile uint32_t g_fault_pc   = 0xDEADBEEF;
volatile uint32_t g_fault_lr   = 0xDEADBEEF;
volatile uint32_t g_fault_cfsr = 0xDEADBEEF;
volatile uint32_t g_fault_bfar = 0xDEADBEEF;
volatile uint32_t g_fault_sp   = 0xDEADBEEF;

void HardFault_Handler(void)
{
    /* Extract stacked PC/LR from exception frame */
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "ldr r1, [r0, #24]   \n"  /* stacked PC */
        "ldr r2, [r0, #20]   \n"  /* stacked LR */
        "ldr r3, =g_fault_pc \n"
        "str r1, [r3]        \n"
        "ldr r3, =g_fault_lr \n"
        "str r2, [r3]        \n"
        "ldr r3, =g_fault_sp \n"
        "str r0, [r3]        \n"
    );
    g_fault_cfsr = *(volatile uint32_t *)0xE000ED28U;  /* CFSR */
    g_fault_bfar = *(volatile uint32_t *)0xE000ED38U;  /* BFAR */

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
    while (1) {
    }
}

void MemManage_Handler(void)
{
    while (1) {
    }
}

void BusFault_Handler(void)
{
    while (1) {
    }
}

void UsageFault_Handler(void)
{
    while (1) {
    }
}

/* SVC_Handler and PendSV_Handler are provided by FreeRTOS portable layer
 * (mapped via FreeRTOSConfig.h: vPortSVCHandler, xPortPendSVHandler) */

void DebugMon_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
#if defined(INCLUDE_xTaskGetSchedulerState) && defined(configUSE_PREEMPTION)
    extern void xPortSysTickHandler(void);
    /* Only call FreeRTOS tick if scheduler is running */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
#endif
}

/* ── Peripheral interrupts ─────────────────────────────────── */

void TIM1_UP_TIM10_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim10);
}

void TIM1_TRG_COM_TIM11_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim11);
}

void USART2_IRQHandler(void)
{
#if !defined(MICROROS_ENABLED)
    /* micro-ROS uses blocking HAL_UART_Transmit/Receive on USART2.
     * The HAL IRQ handler must NOT run — it corrupts the blocking
     * state machine and prevents transport from working. */
    HAL_UART_IRQHandler(&huart2);
#endif
}

void I2C1_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

void I2C1_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c1);
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}
