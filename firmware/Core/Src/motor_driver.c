/**
 * @file motor_driver.c
 * @brief DBH-12V H-bridge motor driver control (Motor A).
 */

#include "motor_driver.h"
#include "config.h"

void Motor_Init(Motor_t *mot, TIM_HandleTypeDef *htim,
                GPIO_TypeDef *en_port, uint16_t en_pin)
{
    mot->htim_pwm = htim;
    mot->ch_fwd   = TIM_CHANNEL_1;
    mot->ch_rev   = TIM_CHANNEL_2;
    mot->en_port  = en_port;
    mot->en_pin   = en_pin;
    mot->dir      = MOTOR_COAST;
    mot->duty     = 0U;

    /* Start with motor disabled */
    HAL_GPIO_WritePin(en_port, en_pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, 0U);
}

void Motor_Set(Motor_t *mot, MotorDir_t dir, uint16_t duty)
{
    /* Clamp duty */
    if (duty > PWM_MAX_DUTY) {
        duty = PWM_MAX_DUTY;
    }

    /* Dead-time on direction change */
    if (dir != mot->dir && mot->dir != MOTOR_COAST) {
        __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_fwd, 0U);
        __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_rev, 0U);
        mot->duty = 0U;
        mot->dir = dir;
        return;  /* Zero duty for 1 PID tick, next call applies new dir */
    }

    switch (dir) {
    case MOTOR_FWD:
        HAL_GPIO_WritePin(mot->en_port, mot->en_pin, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_fwd, duty);
        __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_rev, 0U);
        break;

    case MOTOR_REV:
        HAL_GPIO_WritePin(mot->en_port, mot->en_pin, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_fwd, 0U);
        __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_rev, duty);
        break;

    case MOTOR_BRAKE:
        Motor_Brake(mot);
        return;

    case MOTOR_COAST:
    default:
        Motor_Coast(mot);
        return;
    }

    mot->dir  = dir;
    mot->duty = duty;
}

void Motor_Brake(Motor_t *mot)
{
    HAL_GPIO_WritePin(mot->en_port, mot->en_pin, GPIO_PIN_SET);
    __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_fwd, PWM_ARR);
    __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_rev, PWM_ARR);
    mot->dir  = MOTOR_BRAKE;
    mot->duty = 0U;
}

void Motor_Coast(Motor_t *mot)
{
    __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_fwd, 0U);
    __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_rev, 0U);
    HAL_GPIO_WritePin(mot->en_port, mot->en_pin, GPIO_PIN_RESET);
    mot->dir  = MOTOR_COAST;
    mot->duty = 0U;
}

void Motor_EmergencyStop(Motor_t *mot)
{
    HAL_GPIO_WritePin(mot->en_port, mot->en_pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_fwd, 0U);
    __HAL_TIM_SET_COMPARE(mot->htim_pwm, mot->ch_rev, 0U);
    mot->dir  = MOTOR_COAST;
    mot->duty = 0U;
}
