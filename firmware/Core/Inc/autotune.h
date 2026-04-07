/**
 * @file autotune.h
 * @brief Relay feedback autotune (Astrom-Hagglund) for PID gain calculation.
 */

#ifndef AUTOTUNE_H
#define AUTOTUNE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define AT_MAX_HALF_PERIODS  10U

typedef enum {
    AT_IDLE     = 0,
    AT_RUNNING  = 1,
    AT_COMPLETE = 2,
    AT_FAILED   = 3,
} AutotuneState_t;

typedef struct {
    AutotuneState_t state;

    float    relay_amp;
    float    setpoint;

    float    peak_pos;
    float    peak_neg;
    float    prev_error;
    uint32_t last_cross_ms;
    uint32_t half_periods[AT_MAX_HALF_PERIODS];
    float    amp_samples[AT_MAX_HALF_PERIODS];
    uint8_t  half_count;
    uint8_t  amp_count;

    float    ku;
    float    pu_s;
    float    kp_result;
    float    ki_result;
    float    kd_result;

    uint32_t start_ms;
    uint32_t timeout_ms;
} Autotune_t;

void            Autotune_Start(Autotune_t *at, float setpoint_rpm,
                               float relay_amp, uint32_t timeout_ms);
float           Autotune_Tick(Autotune_t *at, float rpm, uint32_t now_ms);
AutotuneState_t Autotune_GetState(const Autotune_t *at);
void            Autotune_GetResults(const Autotune_t *at,
                                    float *kp, float *ki, float *kd);

#ifdef __cplusplus
}
#endif

#endif /* AUTOTUNE_H */
