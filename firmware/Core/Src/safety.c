/**
 * @file safety.c
 * @brief Fault monitoring — overcurrent, overspeed, battery, comms, stall.
 */

#include "safety.h"
#include "config.h"
#include "motor_driver.h"
#include "main.h"

static float fabsf_local(float x)
{
    return (x < 0.0f) ? -x : x;
}

static uint8_t  s_fault_flags;
static uint8_t  s_faulted;
static uint32_t s_stall_timer_ms;
static uint32_t s_startup_ms;
static uint32_t s_overcurrent_ticks;

void Safety_Init(void)
{
    s_fault_flags       = 0U;
    s_faulted           = 0U;
    s_stall_timer_ms    = 0U;
    s_startup_ms        = HAL_GetTick();
    s_overcurrent_ticks = 0U;
}

void Safety_Tick(float rpm, float current_ma,
                 float batt_v, uint16_t pwm_duty,
                 uint32_t last_cmd_ms, uint32_t now_ms)
{
    if (s_faulted) {
        /* COMMS_LOSS auto-clears when a new command arrives (watchdog reset) */
        if ((s_fault_flags == (uint8_t)FAULT_COMMS_LOSS) &&
            last_cmd_ms != 0U &&
            (now_ms - last_cmd_ms) <= SAFETY_WATCHDOG_MS) {
            s_fault_flags = 0U;
            s_faulted = 0U;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
        }
        return;
    }

    /* Grace period: skip fault checks until ADC DMA has valid data */
    if ((now_ms - s_startup_ms) < SAFETY_STARTUP_GRACE_MS) {
        return;
    }

    uint8_t new_faults = 0U;

    /* Overcurrent — debounced: N consecutive ticks above threshold */
    if (pwm_duty > 0U && current_ma > SAFETY_MAX_CURRENT_MA) {
        s_overcurrent_ticks++;
        if (s_overcurrent_ticks >= SAFETY_OVERCURRENT_HOLD_TICKS) {
            new_faults |= (uint8_t)FAULT_OVERCURRENT;
        }
    } else {
        s_overcurrent_ticks = 0U;
    }

    /* Overspeed */
    if (fabsf_local(rpm) > SAFETY_MAX_RPM) {
        new_faults |= (uint8_t)FAULT_OVERSPEED;
    }

    /* Low battery */
    if (batt_v < SAFETY_MIN_BATT_V) {
        new_faults |= (uint8_t)FAULT_LOW_BATT;
    }

    /* Over voltage */
    if (batt_v > SAFETY_MAX_BATT_V) {
        new_faults |= (uint8_t)FAULT_OVERVOLT;
    }

    /* Communications watchdog */
    if (last_cmd_ms != 0U &&
        (now_ms - last_cmd_ms) > SAFETY_WATCHDOG_MS) {
        new_faults |= (uint8_t)FAULT_COMMS_LOSS;
    }

    /* Stall detection */
    if (pwm_duty > SAFETY_STALL_PWM_THRESH && fabsf_local(rpm) < 1.0f) {
        s_stall_timer_ms += 1U;
        if (s_stall_timer_ms >= SAFETY_STALL_TIMEOUT_MS) {
            new_faults |= (uint8_t)FAULT_STALL;
        }
    } else {
        s_stall_timer_ms = 0U;
    }

    if (new_faults != 0U) {
        s_fault_flags |= new_faults;
        s_faulted = 1U;

        Motor_EmergencyStop(&g_motor_a);

        /* Fault LED on */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
    }
}

uint8_t Safety_GetFaults(void)
{
    return s_fault_flags;
}

void Safety_ClearFaults(void)
{
    s_fault_flags       = 0U;
    s_faulted           = 0U;
    s_stall_timer_ms    = 0U;
    s_overcurrent_ticks = 0U;

    /* Fault LED off */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
}

uint8_t Safety_IsSafe(void)
{
    return (s_fault_flags == 0U) ? 1U : 0U;
}
