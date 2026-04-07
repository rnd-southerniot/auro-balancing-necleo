/**
 * @file stm32f4xx_hal_conf.h
 * @brief HAL configuration — enable only required modules.
 */

#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Module selection ──────────────────────────────────────── */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED

/* ── Oscillator values ─────────────────────────────────────── */
#if !defined(HSE_VALUE)
#define HSE_VALUE    8000000U   /* 8 MHz HSE from ST-LINK MCO */
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT    100U
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE    16000000U
#endif

#if !defined(LSI_VALUE)
#define LSI_VALUE    32000U
#endif

#if !defined(LSE_VALUE)
#define LSE_VALUE    32768U
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT    5000U
#endif

#if !defined(EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE    12288000U
#endif

/* ── System configuration ──────────────────────────────────── */
#define VDD_VALUE                  3300U  /* mV */
#define TICK_INT_PRIORITY          15U
#define USE_RTOS                   0U
#define PREFETCH_ENABLE            1U
#define INSTRUCTION_CACHE_ENABLE   1U
#define DATA_CACHE_ENABLE          1U

/* ── Ethernet (not used) ───────────────────────────────────── */
#define MAC_ADDR0   2U
#define MAC_ADDR1   0U
#define MAC_ADDR2   0U
#define MAC_ADDR3   0U
#define MAC_ADDR4   0U
#define MAC_ADDR5   0U
#define DP83848_PHY_ADDRESS  0x01U

/* ── Module includes ───────────────────────────────────────── */
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f4xx_hal_rcc.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f4xx_hal_gpio.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f4xx_hal_dma.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f4xx_hal_cortex.h"
#endif

#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32f4xx_hal_adc.h"
#endif

#ifdef HAL_IWDG_MODULE_ENABLED
#include "stm32f4xx_hal_iwdg.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f4xx_hal_pwr.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f4xx_hal_flash.h"
#endif

#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32f4xx_hal_tim.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f4xx_hal_uart.h"
#endif

/* ── Assert ────────────────────────────────────────────────── */
#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif /* STM32F4XX_HAL_CONF_H */
