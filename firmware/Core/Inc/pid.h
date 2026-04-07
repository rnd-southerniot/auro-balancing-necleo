/**
 * @file pid.h
 * @brief Position-form PID controller with anti-windup and derivative filter.
 */

#ifndef PID_H
#define PID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    float kp, ki, kd;
    float integral;
    float integral_limit;
    float d_filter_alpha;
    float prev_error;
    float prev_derivative;
    float out_min, out_max;
    float setpoint;
    float setpoint_target;   /* Requested value — slew walks toward this */
    float slew_limit;        /* Max |change| per tick; 0.0f = disabled */
    uint8_t enabled;
} PID_t;

float PID_Compute(PID_t *pid, float measured, float dt);
void  PID_SetSetpoint(PID_t *pid, float setpoint);
void  PID_Reset(PID_t *pid);
void  PID_SetGains(PID_t *pid, float kp, float ki, float kd);
void  PID_SetLimits(PID_t *pid, float out_min, float out_max);

#ifdef __cplusplus
}
#endif

#endif /* PID_H */
