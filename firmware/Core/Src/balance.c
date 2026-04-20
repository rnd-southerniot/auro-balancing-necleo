/**
 * balance.c — self-balance angle PID controller
 *
 * g_imu.pitch_deg → angle PID → g_diff_linear (±1.0)
 * ISR: g_diff_linear * DIFF_MAX_RPM → motor RPM setpoint
 *
 * Single-loop angle PID only. Velocity loop deferred until
 * angle loop is confirmed stable on hardware.
 */

#include "balance.h"
#include "config.h"
#include "imu_mpu6050.h"     /* IMU_t */
#include "stm32f4xx_hal.h"
#include <math.h>
#include <string.h>

/* ── Externals — exact names from main.c:40-41, main.h:67 ────────────────── */
extern IMU_t            g_imu;           /* main.h:67 */
extern volatile float   g_diff_linear;   /* main.c:40 — NOT in main.h */
extern volatile float   g_diff_angular;  /* main.c:41 — NOT in main.h */

/* ── Private state ────────────────────────────────────────────────────────── */
static BalanceState_t g_bal;

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void Balance_Init(void)
{
    memset(&g_bal, 0, sizeof(g_bal));
    g_bal.mode         = BALANCE_OFF;
    g_bal.setpoint_deg = BALANCE_SETPOINT_DEG;
    g_bal.kp           = BALANCE_ANGLE_KP;
    g_bal.ki           = BALANCE_ANGLE_KI;
    g_bal.kd           = BALANCE_ANGLE_KD;
    g_bal.cmd_vel_tick = HAL_GetTick();
}

void Balance_Enable(void)
{
    g_bal.integral     = 0.0f;
    g_bal.prev_error   = 0.0f;
    g_bal.fall_detected = false;
    g_bal.mode         = BALANCE_ON;
}

void Balance_Disable(void)
{
    g_bal.mode = BALANCE_OFF;
    g_diff_linear  = 0.0f;
    g_diff_angular = 0.0f;
    g_bal.diff_linear_out  = 0.0f;
    g_bal.diff_angular_out = 0.0f;
}

bool Balance_IsOn(void)
{
    return g_bal.mode == BALANCE_ON;
}

void Balance_SetCmdVel(float linear_ms, float angular_rads)
{
    g_bal.cmd_linear_ms    = linear_ms;
    g_bal.cmd_angular_rads = angular_rads;
    g_bal.cmd_vel_tick     = HAL_GetTick();
}

const BalanceState_t *Balance_GetState(void)
{
    return &g_bal;
}

void Balance_SetGains(float kp, float ki, float kd)
{
    g_bal.kp = kp;
    g_bal.ki = ki;
    g_bal.kd = kd;
    g_bal.integral   = 0.0f;
    g_bal.prev_error = 0.0f;
}

bool Balance_Tick(void)
{
    if (g_bal.mode != BALANCE_ON)
        return true;

    const float dt = (float)BALANCE_LOOP_MS * 0.001f;  /* 0.020 s */

    /* 1. Snapshot pitch — single volatile read, atomic on Cortex-M4 */
    float pitch = g_imu.pitch_deg;
    g_bal.pitch_deg = pitch;

    /* 2. Fall detection */
    float deviation = fabsf(pitch - g_bal.setpoint_deg);
    if (deviation > BALANCE_FALL_THRESHOLD_DEG) {
        g_bal.fall_detected = true;
        g_bal.mode          = BALANCE_FAULT;
        g_diff_linear       = 0.0f;
        g_diff_angular      = 0.0f;
        return false;
    }
    g_bal.fall_detected = false;

    /* 3. Lean-to-drive: cmd_vel shifts setpoint */
    float cmd_linear  = 0.0f;
    float cmd_angular = 0.0f;
    if ((HAL_GetTick() - g_bal.cmd_vel_tick) < BALANCE_CMDVEL_TIMEOUT_MS) {
        cmd_linear  = g_bal.cmd_linear_ms;
        cmd_angular = g_bal.cmd_angular_rads;
    }

    float setpoint = g_bal.setpoint_deg
        + clampf(cmd_linear * BALANCE_LEAN_SCALE,
                 -BALANCE_LEAN_MAX_DEG, BALANCE_LEAN_MAX_DEG);
    g_bal.setpoint_shifted = setpoint;

    /* 4. Angle PID */
    float error = setpoint - pitch;  /* forward tilt (pitch more negative) → positive output → forward motor */
    g_bal.pitch_error_deg = error;

    float p = g_bal.kp * error;

    g_bal.integral += g_bal.ki * error * dt;
    g_bal.integral  = clampf(g_bal.integral,
                             -BALANCE_ANGLE_IMAX, BALANCE_ANGLE_IMAX);

    float d = g_bal.kd * (error - g_bal.prev_error) / dt;
    g_bal.prev_error = error;

    float output = clampf(p + g_bal.integral + d,
                          -BALANCE_ANGLE_OUT_MAX, BALANCE_ANGLE_OUT_MAX);
    g_bal.diff_linear_out = output;

    /* 5. Turn from cmd_vel angular.z */
    float angular_out = clampf(cmd_angular * BALANCE_TURN_SCALE,
                               -BALANCE_TURN_MAX, BALANCE_TURN_MAX);
    g_bal.diff_angular_out = angular_out;

    /* 6. Write to motor globals — ISR reads these on next tick */
    g_diff_linear  = output;
    g_diff_angular = angular_out;

    return true;
}
