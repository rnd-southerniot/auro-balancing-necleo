/**
 * @file freertos_hooks.c
 * @brief FreeRTOS hook functions (stack overflow, malloc failed).
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal.h"

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* Fault LED on (PB2), then halt */
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
