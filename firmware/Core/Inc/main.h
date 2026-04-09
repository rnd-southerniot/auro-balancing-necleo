/**
 * @file main.h
 * @brief Main application header — global peripheral handles and state.
 */

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "config.h"
#include "comm_protocol.h"
#include "encoder.h"
#include "motor_driver.h"
#include "pid.h"
#include "safety.h"
#include "telemetry.h"
#include "autotune.h"
#include "odometry.h"
#include "imu_mpu6050.h"

/* ── Motor A state ─────────────────────────────────────────── */
extern Autotune_t            g_autotune_a;
extern Encoder_t             g_enc_a;
extern Motor_t               g_motor_a;
extern PID_t                 g_pid_rpm;
extern PID_t                 g_pid_pos;
extern volatile ControlMode_t g_mode;
extern volatile uint32_t     g_last_cmd_ms;
extern volatile uint16_t     g_applied_duty;

/* ── Motor B state ─────────────────────────────────────────── */
extern Autotune_t            g_autotune_b;
extern Encoder_t             g_enc_b;
extern Motor_t               g_motor_b;
extern PID_t                 g_pid_rpm_b;
extern PID_t                 g_pid_pos_b;
extern volatile ControlMode_t g_mode_b;
extern volatile uint32_t     g_last_cmd_ms_b;
extern volatile uint16_t     g_applied_duty_b;

/* ── Shared state ──────────────────────────────────────────── */
extern Telemetry_t           g_telem;
extern CommRx_t              g_comm_rx;
extern volatile uint32_t     g_tick_ms;

/* ── Peripheral handles ────────────────────────────────────── */
extern TIM_HandleTypeDef     htim1;   /* PWM motor A (PA8/PA9)        */
extern TIM_HandleTypeDef     htim2;   /* Encoder motor A (PA0/PA1)    */
extern TIM_HandleTypeDef     htim3;   /* Encoder motor B (PA6/PA7)    */
extern TIM_HandleTypeDef     htim4;   /* PWM motor B (PB6/PB7)        */
extern TIM_HandleTypeDef     htim10;  /* PID loop tick 1 kHz          */
extern TIM_HandleTypeDef     htim11;  /* Telemetry tick 50 Hz         */
extern UART_HandleTypeDef    huart2;  /* ST-LINK VCP (USART2)         */
extern ADC_HandleTypeDef     hadc1;   /* CT_A + battery + CT_B        */
extern DMA_HandleTypeDef     hdma_adc1;
extern DMA_HandleTypeDef     hdma_usart2_tx;
extern IWDG_HandleTypeDef    hiwdg;
extern I2C_HandleTypeDef     hi2c1;   /* MPU6050 IMU (PB8/PB9)        */

/* ── IMU state ─────────────────────────────────────────────── */
extern IMU_t                 g_imu;
extern volatile uint8_t      g_imu_init_err;

/* ── ADC DMA buffers ───────────────────────────────────────── */
#define ADC_NUM_CHANNELS     3U      /* CT_A (PC3), batt (PB1), CT_B (PC4) */
extern volatile uint16_t    g_adc_dma_buf[ADC_NUM_CHANNELS];

/* ── UART single-byte RX buffer ────────────────────────────── */
extern volatile uint8_t     g_uart_rx_byte;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
