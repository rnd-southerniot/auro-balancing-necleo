/**
 * @file encoder.c
 * @brief Quadrature encoder reading via STM32 timer encoder mode (TIM2).
 */

#include "encoder.h"
#include "config.h"

void Encoder_Init(Encoder_t *enc, TIM_HandleTypeDef *htim, uint8_t is_16bit, uint32_t cpr_x4)
{
    enc->htim         = htim;
    enc->count_prev   = 0;
    enc->count_abs    = 0;
    enc->rpm_raw      = 0.0f;
    enc->rpm_filt     = 0.0f;
    enc->position_deg = 0.0f;
    enc->dir          = 0U;
    enc->is_16bit     = is_16bit;
    enc->cpr_x4       = cpr_x4;
    __HAL_TIM_SET_COUNTER(htim, 0U);
}

void Encoder_Update(Encoder_t *enc)
{
    int32_t delta;

    if (enc->is_16bit) {
        int32_t raw = (int32_t)(uint16_t)__HAL_TIM_GET_COUNTER(enc->htim);
        delta = raw - enc->count_prev;
        if (delta >  32767) delta -= 65536;
        if (delta < -32768) delta += 65536;
        enc->count_prev = raw;
    } else {
        int32_t now = (int32_t)__HAL_TIM_GET_COUNTER(enc->htim);
        delta = now - enc->count_prev;
        enc->count_prev = now;
    }

    enc->delta = delta;
    enc->count_abs += delta;
    enc->rpm_raw = ((float)delta / (float)enc->cpr_x4) / PID_DT_S * 60.0f;
    enc->rpm_filt = ENCODER_RPM_FILTER_ALPHA * enc->rpm_raw
                  + (1.0f - ENCODER_RPM_FILTER_ALPHA) * enc->rpm_filt;
    enc->position_deg = ((float)enc->count_abs / (float)enc->cpr_x4) * 360.0f;
    enc->dir = __HAL_TIM_IS_TIM_COUNTING_DOWN(enc->htim) ? 1U : 0U;
}

float Encoder_GetRPM(const Encoder_t *enc)
{
    return enc->rpm_filt;
}

float Encoder_GetPositionDeg(const Encoder_t *enc)
{
    return enc->position_deg;
}

void Encoder_ZeroPosition(Encoder_t *enc)
{
    __disable_irq();
    enc->count_abs    = 0;
    enc->position_deg = 0.0f;
    __enable_irq();
}
