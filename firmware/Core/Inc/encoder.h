/**
 * @file encoder.h
 * @brief Quadrature encoder interface using STM32 timer encoder mode.
 */

#ifndef ENCODER_H
#define ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct {
    TIM_HandleTypeDef *htim;
    int32_t           count_prev;
    int32_t           count_abs; /* absolute position accumulator */
    int32_t           delta;    /* raw count delta from last update (for odometry) */
    float             rpm_raw;
    float             rpm_filt;  /* IIR filtered */
    float             position_deg;
    uint8_t           dir;       /* 0=CW, 1=CCW */
    uint8_t           is_16bit;  /* 1 = TIM3 16-bit, 0 = TIM2 32-bit */
    uint32_t          cpr_x4;   /* 60000 for both motors (30:1 gearbox) */
} Encoder_t;

void  Encoder_Init(Encoder_t *enc, TIM_HandleTypeDef *htim, uint8_t is_16bit, uint32_t cpr_x4);
void  Encoder_Update(Encoder_t *enc);   /* call at 1 kHz */
float Encoder_GetRPM(const Encoder_t *enc);
float Encoder_GetPositionDeg(const Encoder_t *enc);
void  Encoder_ZeroPosition(Encoder_t *enc);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H */
