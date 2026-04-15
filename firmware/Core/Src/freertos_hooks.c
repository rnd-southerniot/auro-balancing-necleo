/**
 * @file freertos_hooks.c
 * @brief FreeRTOS hook functions (stack overflow, malloc failed).
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal.h"

volatile char g_overflow_task[16] = {0};

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    for (int i = 0; i < 15 && pcTaskName[i]; i++)
        g_overflow_task[i] = pcTaskName[i];
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    /* Fault LED on (PB2), then halt */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
