/**
 * @file pid.c
 * @brief Position-form PID controller with anti-windup and derivative filter.
 */

#include "pid.h"

static float clampf(float val, float lo, float hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static float fabsf_local(float x) { return (x < 0.0f) ? -x : x; }

float PID_Compute(PID_t *pid, float measured, float dt)
{
    if (!pid->enabled) {
        return 0.0f;
    }

    /* Slew rate limiter — walk setpoint toward target */
    if (pid->slew_limit > 0.0f) {
        float delta = pid->setpoint_target - pid->setpoint;
        if (fabsf_local(delta) <= pid->slew_limit) {
            pid->setpoint = pid->setpoint_target;
        } else {
            pid->setpoint += (delta > 0.0f) ? pid->slew_limit : -pid->slew_limit;
        }
    }

    float error = pid->setpoint - measured;

    /* Derivative (on error, IIR filtered) */
    float derivative = (error - pid->prev_error) / dt;
    float d_filt = pid->d_filter_alpha * derivative
                 + (1.0f - pid->d_filter_alpha) * pid->prev_derivative;

    /* Integral with anti-windup clamp */
    pid->integral += error * dt;
    pid->integral = clampf(pid->integral,
                           -pid->integral_limit,
                            pid->integral_limit);

    /* Output */
    float output = pid->kp * error
                 + pid->ki * pid->integral
                 + pid->kd * d_filt;
    output = clampf(output, pid->out_min, pid->out_max);

    /* Store state */
    pid->prev_error      = error;
    pid->prev_derivative = d_filt;

    return output;
}

void PID_SetSetpoint(PID_t *pid, float setpoint)
{
    pid->setpoint_target = setpoint;
    /* If no slew limit, jump immediately */
    if (pid->slew_limit <= 0.0f) {
        pid->setpoint = setpoint;
    }
}

void PID_Reset(PID_t *pid)
{
    pid->integral        = 0.0f;
    pid->prev_error      = 0.0f;
    pid->prev_derivative = 0.0f;
}

void PID_SetGains(PID_t *pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    PID_Reset(pid);
}

void PID_SetLimits(PID_t *pid, float out_min, float out_max)
{
    pid->out_min = out_min;
    pid->out_max = out_max;
}
