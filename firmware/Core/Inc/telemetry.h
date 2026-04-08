/**
 * @file telemetry.h
 * @brief Telemetry accumulation and packet building for UART DMA TX.
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct {
    uint32_t timestamp_ms;
    float    m1_rpm, m1_position_deg, m1_current_ma, m1_pid_output;
    float    m2_rpm, m2_position_deg, m2_current_ma, m2_pid_output;
    float    batt_voltage;
    uint8_t  m1_fault_flags, m2_fault_flags;
    uint8_t  m1_mode, m2_mode;

    UART_HandleTypeDef *huart;
    uint8_t tx_buf[64];
    volatile uint8_t tx_busy;
    uint8_t pose_divider;   /* sends pose every 5th call = 10 Hz */
    uint8_t imu_divider;    /* sends IMU every 2nd call = 25 Hz  */
} Telemetry_t;

void Telemetry_Init(Telemetry_t *telem, UART_HandleTypeDef *huart);
void Telemetry_BuildAndSend(Telemetry_t *telem);
void Telemetry_TxCompleteCallback(Telemetry_t *telem);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
