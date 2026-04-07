/**
 * @file motor_driver.h
 * @brief H-bridge motor driver interface for DBH-12V (Motor A).
 */

#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Direction convention — bench verified 2026-04-04
 * Motor A: MOTOR_FWD = CW (looking at gearbox face)
 * Motor B: MOTOR_FWD = CW (looking at gearbox face)
 * Both motors share the same positive direction = CW */
typedef enum {
    MOTOR_COAST = 0,
    MOTOR_FWD,
    MOTOR_REV,
    MOTOR_BRAKE
} MotorDir_t;

typedef struct {
    TIM_HandleTypeDef *htim_pwm;   /* &htim1 (A) or &htim4 (B) */
    uint32_t           ch_fwd;     /* TIM_CHANNEL_1 */
    uint32_t           ch_rev;     /* TIM_CHANNEL_2 */
    GPIO_TypeDef      *en_port;    /* GPIOC */
    uint16_t           en_pin;     /* GPIO_PIN_10 (A) or _11 (B) */
    MotorDir_t         dir;
    uint16_t           duty;
} Motor_t;

void Motor_Init(Motor_t *mot, TIM_HandleTypeDef *htim,
                GPIO_TypeDef *en_port, uint16_t en_pin);
void Motor_Set(Motor_t *mot, MotorDir_t dir, uint16_t duty);
void Motor_Brake(Motor_t *mot);
void Motor_Coast(Motor_t *mot);
void Motor_EmergencyStop(Motor_t *mot);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DRIVER_H */
