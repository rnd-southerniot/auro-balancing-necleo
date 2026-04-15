#pragma once
/**
 * balance.h — self-balance controller
 *
 * Reads:  g_imu.pitch_deg   (IMU_t, written by 1kHz ISR at 200Hz)
 * Writes: g_diff_linear     (volatile float ±1.0, read by ISR)
 *         g_diff_angular    (volatile float ±1.0, read by ISR)
 *
 * Called from microros_task at 50Hz via timestamp gate.
 * No FreeRTOS tasks. No CMSIS-OS. No mutexes needed:
 *   volatile float is atomic on Cortex-M4 (single aligned word).
 *
 * State machine:
 *   OFF   → default at boot (g_diff driven by cmd_vel only)
 *   ON    → balance owns g_diff_linear; cmd_vel shifts setpoint
 *   FAULT → fall detected; g_diff zeroed; re-enable required
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BALANCE_OFF   = 0,
    BALANCE_ON    = 1,
    BALANCE_FAULT = 2,
} BalanceMode_t;

typedef struct {
    BalanceMode_t mode;
    float  setpoint_deg;       /* default BALANCE_SETPOINT_DEG = -28.4 */
    float  pitch_deg;          /* last snapshot of g_imu.pitch_deg */
    float  pitch_error_deg;    /* setpoint_shifted - pitch_deg */
    float  setpoint_shifted;   /* after lean-to-drive adjustment */
    float  integral;
    float  prev_error;
    float  diff_linear_out;    /* what was written to g_diff_linear */
    float  diff_angular_out;   /* what was written to g_diff_angular */
    bool   fall_detected;

    /* Runtime-tunable gains */
    float  kp, ki, kd;

    /* cmd_vel influence (written by cmd_vel path) */
    volatile float    cmd_linear_ms;
    volatile float    cmd_angular_rads;
    volatile uint32_t cmd_vel_tick;
} BalanceState_t;

void Balance_Init(void);
void Balance_Enable(void);
void Balance_Disable(void);
bool Balance_IsOn(void);
bool Balance_Tick(void);
void Balance_SetCmdVel(float linear_ms, float angular_rads);
const BalanceState_t *Balance_GetState(void);
void Balance_SetGains(float kp, float ki, float kd);
