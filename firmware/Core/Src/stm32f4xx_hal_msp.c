/**
 * @file stm32f4xx_hal_msp.c
 * @brief HAL MSP (MCU Support Package) init callbacks.
 *
 * Called internally by HAL_*_Init() functions. Configures GPIO,
 * DMA, and NVIC for each peripheral.
 *
 * PORTED FROM: STM32F429ZI → STM32F401RE (NUCLEO)
 */

#include "main.h"

/* ── UART MSP ──────────────────────────────────────────────── */

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        /* PA2 = TX, PA3 = RX, AF7 (NUCLEO VCP via SB13/SB14) */
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &gpio);

        /* USART2 TX DMA: DMA1 Stream6 Channel4 */
        hdma_usart2_tx.Instance                 = DMA1_Stream6;
        hdma_usart2_tx.Init.Channel             = DMA_CHANNEL_4;
        hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart2_tx.Init.Mode                = DMA_NORMAL;
        hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;
        hdma_usart2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_usart2_tx);

        __HAL_LINKDMA(huart, hdmatx, hdma_usart2_tx);

        /* NVIC */
        HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 3, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

        /* USART2 must preempt TIM10 (priority 0) to avoid byte loss
         * during blocking I2C reads inside the 1 kHz control ISR. */
        HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);
    }
}

/* ── ADC MSP ───────────────────────────────────────────────── */

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();

        /* PC3 = ADC1_IN13 (Motor A current)
         * PB1 = ADC1_IN9  (battery voltage)
         * PC4 = ADC1_IN14 (Motor B current) */
        GPIO_InitTypeDef gpio = {0};
        gpio.Mode = GPIO_MODE_ANALOG;
        gpio.Pull = GPIO_NOPULL;

        gpio.Pin = GPIO_PIN_3 | GPIO_PIN_4;
        HAL_GPIO_Init(GPIOC, &gpio);

        gpio.Pin = GPIO_PIN_1;
        HAL_GPIO_Init(GPIOB, &gpio);

        /* ADC1 DMA: DMA2 Stream0 Channel0 */
        hdma_adc1.Instance                 = DMA2_Stream0;
        hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
        hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        hdma_adc1.Init.Mode                = DMA_CIRCULAR;
        hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;
        hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_adc1);

        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);

        HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    }
}

/* ── I2C MSP ──────────────────────────────────────────────── */

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB8 = I2C1_SCL, PB9 = I2C1_SDA, AF4, open-drain with pull-up */
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
        gpio.Mode      = GPIO_MODE_AF_OD;
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &gpio);

        HAL_NVIC_SetPriority(I2C1_EV_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
        HAL_NVIC_SetPriority(I2C1_ER_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
    }
}

/* ── TIM Encoder MSP ───────────────────────────────────────── */

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA0 = TIM2_CH1, PA1 = TIM2_CH2, AF1 */
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF1_TIM2;
        HAL_GPIO_Init(GPIOA, &gpio);
    } else if (htim->Instance == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA6 = TIM3_CH1, PA7 = TIM3_CH2, AF2 */
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF2_TIM3;
        HAL_GPIO_Init(GPIOA, &gpio);
    }
}

/* ── TIM PWM MSP ───────────────────────────────────────────── */

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA8 = TIM1_CH1, PA9 = TIM1_CH2, AF1 */
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF1_TIM1;
        HAL_GPIO_Init(GPIOA, &gpio);
    } else if (htim->Instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB6 = TIM4_CH1 = IN1_B, PB7 = TIM4_CH2 = IN2_B, AF2 */
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF2_TIM4;
        HAL_GPIO_Init(GPIOB, &gpio);
    }
}

/* ── TIM Base MSP (TIM10, TIM11) ───────────────────────────── */

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM10) {
        __HAL_RCC_TIM10_CLK_ENABLE();
        /* Priority 1: lower than USART2 (priority 0) so UART RX can
         * preempt during blocking I2C reads inside the control ISR. */
        HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
    } else if (htim->Instance == TIM11) {
        __HAL_RCC_TIM11_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
    }
}
