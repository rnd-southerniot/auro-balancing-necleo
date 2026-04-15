/**
 * @file config.h
 * @brief All tunable constants for Motor A single-channel PID controller.
 *
 * Every numeric constant used by firmware modules is defined here.
 * Never use inline magic numbers in application code.
 */

#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Mechanical ────────────────────────────────────────────── */
#define MOTOR_A_GEAR_RATIO        30U   /* MG513P30 */
#define MOTOR_B_GEAR_RATIO        30U   /* MG513P30 — GMR transplant, identical to Motor A */
#define ENCODER_PPR_MOTOR_SHAFT   500U
#define ENCODER_CPR_X4_A  (ENCODER_PPR_MOTOR_SHAFT * 4U * MOTOR_A_GEAR_RATIO)  /* 60000 */
#define ENCODER_CPR_X4_B  (ENCODER_PPR_MOTOR_SHAFT * 4U * MOTOR_B_GEAR_RATIO)  /* 60000 */

/* ── PWM — TIM1, APB2, 84 MHz (F401RE) ────────────────────── */
#define PWM_TIMER_CLOCK_HZ        84000000UL
#define PWM_FREQ_HZ               20000UL
#define PWM_ARR   ((PWM_TIMER_CLOCK_HZ / PWM_FREQ_HZ) - 1U)  /* 4199 */
#define PWM_MAX_DUTY              4115U   /* 98% of 4199 */
#define PWM_MIN_DUTY              0U
#define PWM_DEADBAND_DUTY         100U    /* Min duty before motor moves */

/* ── PID Loop — TIM10 (no TIM6 on F401RE) ─────────────────── */
#define PID_LOOP_RATE_HZ          1000U
#define PID_DT_S                  (1.0f / (float)PID_LOOP_RATE_HZ)

/* ── PID default gains — Motor A (30:1, GMR encoder) ──────── */
/* RPM PID — autotune relay feedback 2026-04-04, 50% Tyreus-Luyben
 * Ku=60.63  Pu=1.017s  relay_amp=250  setpoint=30 RPM
 * Rise time: ~3s to 50 RPM, SS error <1 RPM */
#define PID_RPM_KP                9.5f
#define PID_RPM_KI                4.2f
#define PID_RPM_KD                1.5f
/* Motor B RPM PID — autotuned after GMR transplant 2026-04-04
 * MG513P30 30:1, transplanted GMR, Ku=49.29 Pu=1.013s
 * 50% Tyreus-Luyben. Ku delta from Motor A: 18.7% */
#define PID_RPM_KP_B              7.70f
#define PID_RPM_KI_B              3.45f
#define PID_RPM_KD_B              1.24f
/* Position PID — placeholder, not yet tuned */
#define PID_POS_KP                2.5f
#define PID_POS_KI                0.0f
#define PID_POS_KD                0.08f
#define PID_DEFAULT_INTEGRAL_LIM  500.0f
#define PID_DEFAULT_D_ALPHA       0.1f

/* ── Chassis Dimensions ────────────────────────────────────── */
#define CHASSIS_WHEEL_DIAM_M     0.065f  /* 65mm rubber wheel */
#define CHASSIS_TRACK_M          0.180f  /* 180mm between wheel contact patches */
/* Motor A = left wheel, Motor B = right wheel */

/* ── Differential Drive ────────────────────────────────────── */
#define DIFF_TRACK_FACTOR   1.0f    /* Angular/linear ratio — tune after chassis */
#define DIFF_MAX_RPM        150.0f  /* Per-motor clamp within validated range */

/* ── Encoder IIR Filter ────────────────────────────────────── */
#define ENCODER_RPM_FILTER_ALPHA  0.15f   /* Lower = more filtering */

/* ── Safety Limits ─────────────────────────────────────────── */
#define SAFETY_MAX_RPM            300.0f  /* 30:1 output shaft */
#define SAFETY_MAX_CURRENT_MA     7500.0f /* Below 10A cell limit */
#define SAFETY_MIN_BATT_V         7.0f    /* Allow for voltage sag under load */
#define SAFETY_MAX_BATT_V         13.2f   /* 3 x 4.4V — hardware fault guard */
#define SAFETY_WATCHDOG_MS        500U    /* Production: 500ms */
#define SAFETY_STALL_PWM_THRESH   200U    /* PWM above this = expect motion */
#define SAFETY_STALL_TIMEOUT_MS   500U
#define SAFETY_STARTUP_GRACE_MS   500U    /* Skip fault checks for first N ms */
#define SAFETY_OVERCURRENT_HOLD_TICKS 5U  /* Consecutive ticks at 1kHz = 5ms debounce */

/* ── PID Slew Rate Limits ──────────────────────────────────── */
#define PID_POS_SLEW_DEG_PER_TICK  15.0f  /* Max deg/tick at 1kHz ISR */
#define PID_RPM_SLEW_RPM_PER_TICK   5.0f  /* Max RPM change per tick */

/* ── Battery ADC (PB1, ADC1_IN9) ───────────────────────────── */
#define BATT_ADC_R1_KOHM          100.0f
#define BATT_ADC_R2_KOHM          33.0f
#define BATT_ADC_VREF             3.3f
#define BATT_ADC_FULL_SCALE       4095.0f
#define BATT_V_SCALE              3.727f  /* Calibrated: 12.29V measured / 14.19V displayed × 4.3 */

/* ── Current Sense A (PC3, ADC1_IN13, DBH-12V CT pin) ──────── */
#define CT_MV_PER_AMP             140.0f  /* ~140 mV/A from DBH-12V CT */
#define CT_OFFSET_MV              50.0f   /* Quiescent offset at idle */

/* ── Telemetry ─────────────────────────────────────────────── */
#define TELEM_RATE_HZ             50U
#define COMM_BAUD                 921600U
#define FRAME_SYNC                0xAAU
#define FRAME_VERSION             0x01U

/* ── IWDG Watchdog ─────────────────────────────────────────── */
#define IWDG_TIMEOUT_MS           200U

/* ── Stack Canary ──────────────────────────────────────────── */
#define STACK_CANARY_WORD         0xDEADBEEFU

/* ── IMU — MPU6050 on I2C1 (PB8=SCL, PB9=SDA) ────────────── */
/* PB8  = I2C1_SCL AF4              CN10-3  */
/* PB9  = I2C1_SDA AF4              CN10-5  */
#define IMU_I2C_SPEED_HZ          400000U
#define IMU_GYRO_CAL_SAMPLES      200U

/* ── Pin Assignments — Motor A (Nucleo-F401RE) ────────────── */
/* Encoder: TIM2, 32-bit, AF1 */
/* PA0  = TIM2_CH1 = ENC_A          CN7-28  */
/* PA1  = TIM2_CH2 = ENC_B          CN7-30  */
/* PWM:  TIM1, AF1 */
/* PA8  = TIM1_CH1 = IN1 (forward)  CN10-23 */
/* PA9  = TIM1_CH2 = IN2 (reverse)  CN10-21 */
/* GPIO */
/* PC10 = GPIO_OUT = EN             CN7-1   */
/* PC3  = ADC1_IN13 = CT (current)  CN7-37  */
/* PB1  = ADC1_IN9  = BATT_V        CN10-24 */
/* PA2  = USART2_TX (VCP, SB13)     CN10-35 */
/* PA3  = USART2_RX (VCP, SB14)     CN10-37 */
/* PA5  = LED green (heartbeat/LD2) CN10-11 */
/* PB2  = LED red   (fault)         CN10-22 */
/* PC13 = B1 user button (E-stop)   CN7-23  */

/* ── Pin Assignments — Motor B (Nucleo-F401RE) ────────────── */
/* Encoder: TIM3, 16-bit, AF2 */
/* PA6  = TIM3_CH1 = ENC_A          CN10-13 */
/* PA7  = TIM3_CH2 = ENC_B          CN10-15 */
/* PWM:  TIM4, APB1 84 MHz, AF2 */
/* PB6  = TIM4_CH1 = IN1_B (forward) CN10-17 */
/* PB7  = TIM4_CH2 = IN2_B (reverse) CN7-21  */
/* GPIO */
/* PC11 = GPIO_OUT = EN_B            CN7-2   */
/* PC4  = ADC1_IN14 = CT_B (current) CN10-34 */

// ─── Balance Controller ──────────────────────────────────────────────────────
// Angle PID: pitch error → g_diff_linear (±1.0 normalized)
// ISR converts: g_diff_linear * DIFF_MAX_RPM (150) = motor RPM
//
// g_imu.pitch_deg: complementary filter output, degrees, 200Hz
// Balance_Tick(): called at 50Hz from microros_task timestamp gate
//
// Setpoint -28.4° = hardware-measured upright position (HW-LIVE-02)
// Fall ±35° = cut motors, enter FAULT, require explicit re-enable

// ── Core ─────────────────────────────────────────────────────────────────────
#define BALANCE_SETPOINT_DEG        (-28.4f)   // HW-LIVE-02: do not change
#define BALANCE_FALL_THRESHOLD_DEG  (35.0f)    // deg from setpoint
#define BALANCE_LOOP_MS             (20U)      // 50Hz

// ── Angle PID gains ───────────────────────────────────────────────────────────
// Output: normalized [-1.0, +1.0] → * 150 RPM in ISR
// Kp=0.02: 1 deg error → 0.02 → 3 RPM (conservative, safe to observe)
#define BALANCE_ANGLE_KP            (0.02f)
#define BALANCE_ANGLE_KI            (0.0f)
#define BALANCE_ANGLE_KD            (0.002f)
#define BALANCE_ANGLE_IMAX          (0.3f)     // anti-windup clamp
#define BALANCE_ANGLE_OUT_MAX       (0.8f)     // max |g_diff_linear|

// ── Lean-to-drive ─────────────────────────────────────────────────────────────
// cmd_vel linear.x shifts setpoint; angular.z adds RPM differential
#define BALANCE_LEAN_SCALE          (2.0f)     // deg per m/s cmd_vel
#define BALANCE_LEAN_MAX_DEG        (5.0f)     // max setpoint shift
#define BALANCE_TURN_SCALE          (0.3f)     // normalized per rad/s
#define BALANCE_TURN_MAX            (0.5f)     // max |g_diff_angular|

// ── cmd_vel watchdog ──────────────────────────────────────────────────────────
#define BALANCE_CMDVEL_TIMEOUT_MS   (500U)

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
