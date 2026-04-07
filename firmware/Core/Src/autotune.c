/**
 * @file autotune.c
 * @brief Relay feedback autotune (Astrom-Hagglund method).
 */

#include "autotune.h"
#include <string.h>

#define AT_MIN_HALF_PERIOD_MS  500U   /* Reject crossings faster than 500ms */
#define AT_MIN_AMPLITUDE_RPM   5.0f   /* Reject oscillations smaller than this */

void Autotune_Start(Autotune_t *at, float setpoint_rpm,
                    float relay_amp, uint32_t timeout_ms)
{
    memset(at, 0, sizeof(Autotune_t));
    at->state      = AT_RUNNING;
    at->setpoint   = setpoint_rpm;
    at->relay_amp  = relay_amp;
    at->timeout_ms = timeout_ms;
    at->start_ms   = 0U;
    at->peak_pos   = setpoint_rpm;
    at->peak_neg   = setpoint_rpm;
    /* Initialize prev_error to match expected initial sign (RPM=0, setpoint>0 → error<0) */
    at->prev_error = -setpoint_rpm;
}

float Autotune_Tick(Autotune_t *at, float rpm, uint32_t now_ms)
{
    if (at->state != AT_RUNNING) {
        return 0.0f;
    }

    if (at->start_ms == 0U) {
        at->start_ms = now_ms;
    }

    if ((now_ms - at->start_ms) > at->timeout_ms) {
        at->state = AT_FAILED;
        return 0.0f;
    }

    float error = rpm - at->setpoint;
    float output = (error < 0.0f) ? at->relay_amp : -at->relay_amp;

    /* Track peaks between zero crossings */
    if (rpm > at->peak_pos) at->peak_pos = rpm;
    if (rpm < at->peak_neg) at->peak_neg = rpm;

    /* Zero-crossing detection with minimum half-period filter */
    if ((at->prev_error < 0.0f && error >= 0.0f) ||
        (at->prev_error >= 0.0f && error < 0.0f)) {

        uint32_t elapsed_since_last = now_ms - at->last_cross_ms;

        /* Reject crossings that are too fast (noise) */
        if (at->last_cross_ms != 0U && elapsed_since_last < AT_MIN_HALF_PERIOD_MS) {
            /* Skip — likely noise, don't update prev_error so we re-check next tick */
            at->prev_error = error;
            return output;
        }

        /* Record half-period (skip the very first crossing) */
        if (at->half_count > 0U && at->half_count <= AT_MAX_HALF_PERIODS) {
            at->half_periods[at->half_count - 1U] = elapsed_since_last;
        }
        at->last_cross_ms = now_ms;
        at->half_count++;

        /* Record amplitude after 2+ crossings */
        if (at->half_count >= 2U && at->amp_count < AT_MAX_HALF_PERIODS) {
            float amp = at->peak_pos - at->peak_neg;
            /* Only record if amplitude is meaningful */
            if (amp >= AT_MIN_AMPLITUDE_RPM) {
                at->amp_samples[at->amp_count] = amp;
                at->amp_count++;
            }
            at->peak_pos = rpm;
            at->peak_neg = rpm;
        }

        /* Need 10 half-periods (5 full cycles) with valid amplitudes */
        if (at->half_count >= AT_MAX_HALF_PERIODS && at->amp_count >= 4U) {
            uint32_t period_sum = 0U;
            uint8_t count = 0U;
            for (uint8_t i = 2U; i < AT_MAX_HALF_PERIODS - 1U; i++) {
                period_sum += at->half_periods[i];
                count++;
            }
            float pu_ms = (float)period_sum / (float)count * 2.0f;
            at->pu_s = pu_ms / 1000.0f;

            float amp_sum = 0.0f;
            for (uint8_t i = 1U; i < at->amp_count; i++) {
                amp_sum += at->amp_samples[i];
            }
            float a_avg = (amp_sum / (float)(at->amp_count - 1U)) / 2.0f;

            if (a_avg > 1.0f) {
                at->ku = (4.0f * at->relay_amp) / (3.14159f * a_avg);
                at->kp_result = at->ku / 3.2f;
                at->ki_result = at->kp_result / (2.2f * at->pu_s);
                at->kd_result = at->kp_result * at->pu_s / 6.3f;
                at->state = AT_COMPLETE;
            } else {
                at->state = AT_FAILED;
                return 0.0f;
            }
        }
    }

    at->prev_error = error;
    return output;
}

AutotuneState_t Autotune_GetState(const Autotune_t *at)
{
    return at->state;
}

void Autotune_GetResults(const Autotune_t *at,
                         float *kp, float *ki, float *kd)
{
    *kp = at->kp_result;
    *ki = at->ki_result;
    *kd = at->kd_result;
}
