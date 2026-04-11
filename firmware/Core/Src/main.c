/**
 * @file main.c
 * @brief Application entry point — peripheral init, control loop, command dispatch.
 *
 * Architecture (bare-metal, interrupt-driven, Motor A only):
 *   TIM10 ISR @ 1 kHz -> encoder, ADC, safety, PID, motor, telem accum
 *   TIM11 ISR @ 50 Hz -> telemetry build + DMA TX
 *   USART2 RxISR       -> byte-level frame parser
 *   main loop          -> WFI, watchdog, heartbeat
 */

#include "main.h"
#include <string.h>

/* ================================================================
 * Global state
 * ================================================================ */

/* ── Motor A state ─────────────────────────────────────────── */
Autotune_t             g_autotune_a;
Encoder_t              g_enc_a;
Motor_t                g_motor_a;
PID_t                  g_pid_rpm;
PID_t                  g_pid_pos;
volatile ControlMode_t g_mode = CTRL_IDLE;
volatile uint32_t      g_last_cmd_ms;
volatile uint16_t      g_applied_duty;

/* ── Motor B state ─────────────────────────────────────────── */
Autotune_t             g_autotune_b;
Encoder_t              g_enc_b;
Motor_t                g_motor_b;
PID_t                  g_pid_rpm_b;
PID_t                  g_pid_pos_b;
volatile ControlMode_t g_mode_b = CTRL_IDLE;
volatile uint32_t      g_last_cmd_ms_b;
volatile uint16_t      g_applied_duty_b;

/* ── Differential drive ────────────────────────────────────── */
static volatile float  g_diff_linear;
static volatile float  g_diff_angular;

/* ── Shared state ──────────────────────────────────────────── */
Telemetry_t            g_telem;
CommRx_t               g_comm_rx;
volatile uint32_t      g_tick_ms;

/* ── Peripheral handles ────────────────────────────────────── */
TIM_HandleTypeDef      htim1;          /* PWM motor A (PA8/PA9)        */
TIM_HandleTypeDef      htim2;          /* Encoder motor A (PA0/PA1)    */
TIM_HandleTypeDef      htim3;          /* Encoder motor B (PA6/PA7)    */
TIM_HandleTypeDef      htim4;          /* PWM motor B (PB6/PB7)        */
TIM_HandleTypeDef      htim10;         /* 1 kHz PID tick               */
TIM_HandleTypeDef      htim11;         /* 50 Hz telem tick             */
UART_HandleTypeDef     huart2;         /* ST-LINK VCP                  */
ADC_HandleTypeDef      hadc1;          /* Motor A current + battery + Motor B current */
DMA_HandleTypeDef      hdma_adc1;
DMA_HandleTypeDef      hdma_usart2_tx;
IWDG_HandleTypeDef     hiwdg;
I2C_HandleTypeDef      hi2c1;          /* MPU6050 IMU (PB8/PB9)        */

/* ── IMU state ─────────────────────────────────────────────── */
IMU_t                  g_imu;
volatile uint8_t       g_imu_init_err;   /* debug: IMU_Init return code */

/* ── ADC DMA buffers ───────────────────────────────────────── */
volatile uint16_t      g_adc_dma_buf[ADC_NUM_CHANNELS];
volatile uint16_t      g_ct_a_peak;    /* Peak-held CT_A raw ADC value */
volatile uint16_t      g_ct_b_peak;    /* Peak-held CT_B raw ADC value */

/* ── UART single-byte receive buffer ───────────────────────── */
volatile uint8_t       g_uart_rx_byte;

/* ── Linker-defined symbols ────────────────────────────────── */
extern uint32_t _estack;
extern uint32_t _Min_Stack_Size;
static volatile uint32_t *stack_canary_ptr;

#if !defined(MICROROS_ENABLED)
/* ── TX buffer for response frames ─────────────────────────── */
static uint8_t tx_resp_buf[COMM_MAX_FRAME_SIZE];
#endif

/* ================================================================
 * Forward declarations
 * ================================================================ */

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM10_Init(void);
static void MX_TIM11_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
/* static void MX_IWDG_Init(void); — disabled during bench debug */

static void App_InitModules(void);
#if !defined(MICROROS_ENABLED)
static void App_ProcessCommand(void);
static void App_SendAck(uint8_t msg_type);
static void App_SendNack(uint8_t msg_type, NackCode_t code);
static void App_SendAutotuneResult(const Autotune_t *at);
#endif

/* ================================================================
 * Helpers
 * ================================================================ */

static float ADC_RawToCurrentMa(uint16_t raw)
{
    float v_mv = (float)raw * (BATT_ADC_VREF * 1000.0f) / BATT_ADC_FULL_SCALE;
    float v_corrected = v_mv - CT_OFFSET_MV;
    if (v_corrected < 0.0f) v_corrected = 0.0f;
    return v_corrected / CT_MV_PER_AMP * 1000.0f;
}

static float ADC_RawToBattV(uint16_t raw)
{
    return ((float)raw / BATT_ADC_FULL_SCALE) * BATT_ADC_VREF * BATT_V_SCALE;
}

/* ================================================================
 * main()
 * ================================================================ */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_DMA_Init();
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_TIM1_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_TIM10_Init();
    MX_TIM11_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    MX_I2C1_Init();
    /* MX_IWDG_Init(); — disabled during bench debug: IWDG reset loop */

    App_InitModules();

    /* Stack canary */
    stack_canary_ptr  = (volatile uint32_t *)((uint32_t)&_estack
                        - (uint32_t)&_Min_Stack_Size);
    *stack_canary_ptr = STACK_CANARY_WORD;

    /* Start encoders */
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

    /* Start PWM channels */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);

    /* Start ADC DMA (circular) */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_dma_buf, ADC_NUM_CHANNELS);

#if !defined(MICROROS_ENABLED)
    /* Start UART receive (interrupt, single byte at a time).
     * Disabled when micro-ROS is enabled — transport owns USART2. */
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&g_uart_rx_byte, 1U);
#endif

    /* Start tick timers */
    HAL_TIM_Base_Start_IT(&htim10);
    HAL_TIM_Base_Start_IT(&htim11);

    /* ── Start FreeRTOS scheduler ─────────────────────────── */
    /* All ISR-driven control (TIM10 PID, TIM11 telemetry, UART RX)
     * continues to work unchanged. FreeRTOS runs alongside them.
     * The heartbeat + watchdog move to the FreeRTOS idle hook or
     * a dedicated low-priority task. */
    extern void app_freertos_init(void);
    app_freertos_init();  /* Creates tasks and starts scheduler — does not return */

    /* Should never reach here */
    for (;;) {}
}

/* ================================================================
 * Application module initialisation
 * ================================================================ */

static void App_InitModules(void)
{
    /* Encoder */
    Encoder_Init(&g_enc_a, &htim2, 0U, ENCODER_CPR_X4_A);

    /* Motor — TIM1 CH1/CH2, EN=PC10 */
    Motor_Init(&g_motor_a, &htim1, GPIOC, GPIO_PIN_10);

    /* RPM PID */
    PID_Reset(&g_pid_rpm);
    g_pid_rpm.kp             = PID_RPM_KP;
    g_pid_rpm.ki             = PID_RPM_KI;
    g_pid_rpm.kd             = PID_RPM_KD;
    g_pid_rpm.integral_limit = PID_DEFAULT_INTEGRAL_LIM;
    g_pid_rpm.d_filter_alpha = PID_DEFAULT_D_ALPHA;
    g_pid_rpm.out_min        = -(float)PWM_MAX_DUTY;
    g_pid_rpm.out_max        = (float)PWM_MAX_DUTY;
    g_pid_rpm.slew_limit     = PID_RPM_SLEW_RPM_PER_TICK;
    g_pid_rpm.enabled        = 0U;

    /* Position PID (cascade -> RPM setpoint) */
    PID_Reset(&g_pid_pos);
    g_pid_pos.kp             = PID_POS_KP;
    g_pid_pos.ki             = PID_POS_KI;
    g_pid_pos.kd             = PID_POS_KD;
    g_pid_pos.integral_limit = PID_DEFAULT_INTEGRAL_LIM;
    g_pid_pos.d_filter_alpha = PID_DEFAULT_D_ALPHA;
    g_pid_pos.out_min        = -SAFETY_MAX_RPM;
    g_pid_pos.out_max        = SAFETY_MAX_RPM;
    g_pid_pos.slew_limit     = PID_POS_SLEW_DEG_PER_TICK;
    g_pid_pos.enabled        = 0U;

    /* ── Motor B ────────────────────────────────────────────── */
    Encoder_Init(&g_enc_b, &htim3, 1U, ENCODER_CPR_X4_B);
    Motor_Init(&g_motor_b, &htim4, GPIOC, GPIO_PIN_11);

    PID_Reset(&g_pid_rpm_b);
    g_pid_rpm_b.kp             = PID_RPM_KP_B;
    g_pid_rpm_b.ki             = PID_RPM_KI_B;
    g_pid_rpm_b.kd             = PID_RPM_KD_B;
    g_pid_rpm_b.integral_limit = PID_DEFAULT_INTEGRAL_LIM;
    g_pid_rpm_b.d_filter_alpha = PID_DEFAULT_D_ALPHA;
    g_pid_rpm_b.out_min        = -(float)PWM_MAX_DUTY;
    g_pid_rpm_b.out_max        = (float)PWM_MAX_DUTY;
    g_pid_rpm_b.slew_limit     = PID_RPM_SLEW_RPM_PER_TICK;
    g_pid_rpm_b.enabled        = 0U;

    PID_Reset(&g_pid_pos_b);
    g_pid_pos_b.kp             = PID_POS_KP;
    g_pid_pos_b.ki             = PID_POS_KI;
    g_pid_pos_b.kd             = PID_POS_KD;
    g_pid_pos_b.integral_limit = PID_DEFAULT_INTEGRAL_LIM;
    g_pid_pos_b.d_filter_alpha = PID_DEFAULT_D_ALPHA;
    g_pid_pos_b.out_min        = -SAFETY_MAX_RPM;
    g_pid_pos_b.out_max        = SAFETY_MAX_RPM;
    g_pid_pos_b.slew_limit     = PID_POS_SLEW_DEG_PER_TICK;
    g_pid_pos_b.enabled        = 0U;

    /* Odometry — Motor A=left, Motor B=right */
    Odometry_Init(CHASSIS_WHEEL_DIAM_M, CHASSIS_TRACK_M,
                  ENCODER_CPR_X4_A, ENCODER_CPR_X4_B);

    /* Safety */
    Safety_Init();

    /* Telemetry */
    Telemetry_Init(&g_telem, &huart2);

    /* IMU — MPU6050 on I2C1 (PB8/PB9). Gracefully skipped if not present. */
    g_imu_init_err = (uint8_t)IMU_Init(&g_imu, &hi2c1);
    if (g_imu_init_err == IMU_OK) {
        HAL_Delay(500);   /* thermal settle before gyro calibration */
        g_imu_init_err = (uint8_t)IMU_CalibrateGyro(&g_imu, 500);
    }

    /* Comm RX state machine */
    Comm_RxInit(&g_comm_rx);

    /* Default modes */
    g_mode          = CTRL_IDLE;
    g_last_cmd_ms   = 0U;
    g_applied_duty  = 0U;
    g_mode_b        = CTRL_IDLE;
    g_last_cmd_ms_b = 0U;
    g_applied_duty_b = 0U;
}

/* ================================================================
 * 1 kHz control loop — called from TIM10 ISR
 * ================================================================ */

static void App_ControlTick(void)
{
    uint32_t now = HAL_GetTick();
    g_tick_ms = now;

    /* 1. Encoder update */
    Encoder_Update(&g_enc_a);

    /* 1b. IMU read + complementary filter update.
     * Blocking I2C read (~300 µs at 400 kHz) must not run every tick —
     * it preempts USART2 RX (priority 1) and causes byte loss.
     * 200 Hz (every 5th tick) is sufficient for the complementary filter. */
    {
        static uint8_t imu_div = 0U;
        imu_div++;
        if (imu_div >= 5U) {
            imu_div = 0U;
            if (g_imu.initialized) {
                IMU_ReadAll(&g_imu);
                IMU_UpdateAngle(&g_imu, 5.0f * PID_DT_S);
            }
        }
    }

    /* 2. ADC readings.
     * CT sense is pulsed — DBH-12V only outputs during PWM ON phase.
     * Track peak over 20 ticks (20ms), then latch to g_ct_x_peak.
     * The latch holds the previous window's peak until a new one is ready,
     * so telemetry always reads a valid (non-zero) value when motor runs. */
    {
        static uint16_t ct_a_acc = 0U;
        static uint16_t ct_b_acc = 0U;
        static uint8_t  ct_div = 0U;

        /* CT wires swapped on H-bridge: Motor A CT → PC4 (buf[2]),
         * Motor B CT → PC3 (buf[0]). Swap in firmware. */
        uint16_t raw_a = (uint16_t)g_adc_dma_buf[2];
        uint16_t raw_b = (uint16_t)g_adc_dma_buf[0];
        if (raw_a > ct_a_acc) ct_a_acc = raw_a;
        if (raw_b > ct_b_acc) ct_b_acc = raw_b;

        ct_div++;
        if (ct_div >= 20U) {
            ct_div = 0U;
            g_ct_a_peak = ct_a_acc;
            g_ct_b_peak = ct_b_acc;
            ct_a_acc = 0U;
            ct_b_acc = 0U;
        }
    }
    float current_ma = ADC_RawToCurrentMa(g_ct_a_peak);
    float batt_v     = ADC_RawToBattV((uint16_t)g_adc_dma_buf[1]);

    float rpm     = Encoder_GetRPM(&g_enc_a);
    float pos_deg = Encoder_GetPositionDeg(&g_enc_a);

    /* 2b. Odometry — uses raw deltas from both encoders */
    Odometry_Update(g_enc_a.delta, g_enc_b.delta, PID_DT_S);

    /* 3. Safety check — pass 0 for last_cmd_ms during autotune to skip COMMS watchdog */
    uint32_t cmd_ts = (g_mode == CTRL_AUTOTUNE) ? 0U : g_last_cmd_ms;
    Safety_Tick(rpm, current_ma, batt_v, g_applied_duty,
                cmd_ts, now);

    if (!Safety_IsSafe()) {
        g_applied_duty = 0U;
        /* Don't return — still need to run Motor B */
        goto motor_b_tick;
    }

    /* 4. Differential drive — handles both motors atomically */
    if (g_mode == CTRL_DIFF && g_mode_b == CTRL_DIFF) {
        float lin = g_diff_linear * DIFF_MAX_RPM;
        float ang = g_diff_angular * DIFF_MAX_RPM * DIFF_TRACK_FACTOR;
        float rpm_l = lin - ang;
        float rpm_r = lin + ang;

        /* Normalise if either exceeds max */
        float abs_l = (rpm_l < 0.0f) ? -rpm_l : rpm_l;
        float abs_r = (rpm_r < 0.0f) ? -rpm_r : rpm_r;
        float max_v = (abs_l > abs_r) ? abs_l : abs_r;
        if (max_v > DIFF_MAX_RPM) {
            float s = DIFF_MAX_RPM / max_v;
            rpm_l *= s;
            rpm_r *= s;
        }

        /* Motor A = left, Motor B = right */
        g_pid_rpm.enabled = 1U;
        g_pid_rpm_b.enabled = 1U;
        PID_SetSetpoint(&g_pid_rpm, rpm_l);
        PID_SetSetpoint(&g_pid_rpm_b, rpm_r);

        float out_a = PID_Compute(&g_pid_rpm, rpm, PID_DT_S);
        MotorDir_t da = (out_a >= 0.0f) ? MOTOR_FWD : MOTOR_REV;
        uint16_t du_a = (out_a >= 0.0f) ? (uint16_t)out_a : (uint16_t)(-out_a);
        if (du_a > PWM_MAX_DUTY) du_a = PWM_MAX_DUTY;
        Motor_Set(&g_motor_a, da, du_a);
        g_applied_duty = du_a;

        /* Motor B tick */
        Encoder_Update(&g_enc_b);
        float rpm_b_val = Encoder_GetRPM(&g_enc_b);

        float out_b = PID_Compute(&g_pid_rpm_b, rpm_b_val, PID_DT_S);
        MotorDir_t db = (out_b >= 0.0f) ? MOTOR_FWD : MOTOR_REV;
        uint16_t du_b = (out_b >= 0.0f) ? (uint16_t)out_b : (uint16_t)(-out_b);
        if (du_b > PWM_MAX_DUTY) du_b = PWM_MAX_DUTY;
        Motor_Set(&g_motor_b, db, du_b);
        g_applied_duty_b = du_b;
        return;
    }

    /* 5. Motor A PID compute + motor set */
    {
        float duty_f = 0.0f;
        uint8_t motor_a_active = 1U;

        switch (g_mode) {
        case CTRL_RPM:
            g_pid_rpm.enabled = 1U;
            g_pid_pos.enabled = 0U;
            duty_f = PID_Compute(&g_pid_rpm, rpm, PID_DT_S);
            break;

        case CTRL_POSITION:
            g_pid_pos.enabled = 1U;
            g_pid_rpm.enabled = 1U;
            {
                float rpm_sp = PID_Compute(&g_pid_pos, pos_deg, PID_DT_S);
                PID_SetSetpoint(&g_pid_rpm, rpm_sp);
                duty_f = PID_Compute(&g_pid_rpm, rpm, PID_DT_S);
            }
            break;

        case CTRL_AUTOTUNE:
            g_pid_rpm.enabled = 0U;
            g_pid_pos.enabled = 0U;
            duty_f = Autotune_Tick(&g_autotune_a, rpm, now);
            if (Autotune_GetState(&g_autotune_a) == AT_COMPLETE) {
                float kp, ki, kd;
                Autotune_GetResults(&g_autotune_a, &kp, &ki, &kd);
                PID_SetGains(&g_pid_rpm, kp, ki, kd);
                g_mode = CTRL_IDLE;
                Motor_Coast(&g_motor_a);
#if !defined(MICROROS_ENABLED)
                App_SendAutotuneResult(&g_autotune_a);
#endif
                g_applied_duty = 0U;
                motor_a_active = 0U;
            } else if (Autotune_GetState(&g_autotune_a) == AT_FAILED) {
                g_mode = CTRL_IDLE;
                Motor_Coast(&g_motor_a);
                g_applied_duty = 0U;
                motor_a_active = 0U;
            }
            break;

        case CTRL_IDLE:
        default:
            g_pid_rpm.enabled = 0U;
            g_pid_pos.enabled = 0U;
            Motor_Coast(&g_motor_a);
            g_applied_duty = 0U;
            motor_a_active = 0U;
            break;
        }

        if (motor_a_active) {
            MotorDir_t dir;
            uint16_t duty_u;
            if (duty_f >= 0.0f) {
                dir    = MOTOR_FWD;
                duty_u = (uint16_t)duty_f;
            } else {
                dir    = MOTOR_REV;
                duty_u = (uint16_t)(-duty_f);
            }
            if (duty_u > PWM_MAX_DUTY) {
                duty_u = PWM_MAX_DUTY;
            }
            Motor_Set(&g_motor_a, dir, duty_u);
            g_applied_duty = duty_u;
        }
    }

motor_b_tick:
    /* ── Motor B PID tick ──────────────────────────────────── */
    Encoder_Update(&g_enc_b);
    float rpm_b = Encoder_GetRPM(&g_enc_b);
    float pos_b = Encoder_GetPositionDeg(&g_enc_b);
    float cur_b = ADC_RawToCurrentMa(g_ct_b_peak);
    (void)cur_b;  /* TODO: per-motor safety in Phase 9 */

    float duty_b = 0.0f;
    uint8_t motor_b_active = 1U;

    switch (g_mode_b) {
    case CTRL_RPM:
        g_pid_rpm_b.enabled = 1U;
        g_pid_pos_b.enabled = 0U;
        duty_b = PID_Compute(&g_pid_rpm_b, rpm_b, PID_DT_S);
        break;

    case CTRL_POSITION:
        g_pid_pos_b.enabled = 1U;
        g_pid_rpm_b.enabled = 1U;
        {
            float rpm_sp_b = PID_Compute(&g_pid_pos_b, pos_b, PID_DT_S);
            PID_SetSetpoint(&g_pid_rpm_b, rpm_sp_b);
            duty_b = PID_Compute(&g_pid_rpm_b, rpm_b, PID_DT_S);
        }
        break;

    case CTRL_AUTOTUNE:
        g_pid_rpm_b.enabled = 0U;
        g_pid_pos_b.enabled = 0U;
        duty_b = Autotune_Tick(&g_autotune_b, rpm_b, now);
        if (Autotune_GetState(&g_autotune_b) == AT_COMPLETE) {
            float kpb, kib, kdb;
            Autotune_GetResults(&g_autotune_b, &kpb, &kib, &kdb);
            PID_SetGains(&g_pid_rpm_b, kpb, kib, kdb);
            g_mode_b = CTRL_IDLE;
            Motor_Coast(&g_motor_b);
#if !defined(MICROROS_ENABLED)
            App_SendAutotuneResult(&g_autotune_b);
#endif
            g_applied_duty_b = 0U;
            motor_b_active = 0U;
        } else if (Autotune_GetState(&g_autotune_b) == AT_FAILED) {
            g_mode_b = CTRL_IDLE;
            Motor_Coast(&g_motor_b);
            g_applied_duty_b = 0U;
            motor_b_active = 0U;
        }
        break;

    case CTRL_IDLE:
    default:
        g_pid_rpm_b.enabled = 0U;
        g_pid_pos_b.enabled = 0U;
        Motor_Coast(&g_motor_b);
        g_applied_duty_b = 0U;
        motor_b_active = 0U;
        break;
    }

    if (motor_b_active) {
        MotorDir_t dir_b;
        uint16_t duty_ub;
        if (duty_b >= 0.0f) {
            dir_b   = MOTOR_FWD;
            duty_ub = (uint16_t)duty_b;
        } else {
            dir_b   = MOTOR_REV;
            duty_ub = (uint16_t)(-duty_b);
        }
        if (duty_ub > PWM_MAX_DUTY) {
            duty_ub = PWM_MAX_DUTY;
        }
        Motor_Set(&g_motor_b, dir_b, duty_ub);
        g_applied_duty_b = duty_ub;
    }
}

#if !defined(MICROROS_ENABLED)
/* ================================================================
 * Command dispatch (disabled when micro-ROS owns USART2)
 * ================================================================ */

static void App_ProcessCommand(void)
{
    uint8_t msg = g_comm_rx.msg_type;
    const uint8_t *p = g_comm_rx.payload;

    g_last_cmd_ms = HAL_GetTick();

    switch (msg) {

    case MSG_CMD_RPM: {
        const PayloadCmdRpm_t *cmd = (const PayloadCmdRpm_t *)p;
        if (!Safety_IsSafe()) {
            App_SendNack(msg, NACK_FAULT_ACTIVE);
            return;
        }
        PID_SetSetpoint(&g_pid_rpm, cmd->setpoint_rpm);
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_POSITION: {
        const PayloadCmdPosition_t *cmd = (const PayloadCmdPosition_t *)p;
        if (!Safety_IsSafe()) {
            App_SendNack(msg, NACK_FAULT_ACTIVE);
            return;
        }
        PID_SetSetpoint(&g_pid_pos, cmd->setpoint_deg);
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_GAINS: {
        const PayloadCmdGains_t *cmd = (const PayloadCmdGains_t *)p;
        PID_SetGains(&g_pid_rpm, cmd->kp, cmd->ki, cmd->kd);
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_MODE: {
        const PayloadCmdMode_t *cmd = (const PayloadCmdMode_t *)p;
        ControlMode_t new_mode = (ControlMode_t)cmd->mode;

        if (new_mode == CTRL_IDLE) {
            g_mode = CTRL_IDLE;
            Safety_ClearFaults();
            Motor_Coast(&g_motor_a);
            PID_Reset(&g_pid_rpm);
            PID_Reset(&g_pid_pos);
            Encoder_ZeroPosition(&g_enc_a);
            g_applied_duty = 0U;
        } else {
            /* Clear COMMS fault before entering new mode */
            Safety_ClearFaults();
            if (new_mode == CTRL_POSITION) {
                Encoder_ZeroPosition(&g_enc_a);
                PID_Reset(&g_pid_rpm);
                PID_Reset(&g_pid_pos);
            }
            g_mode = new_mode;
        }
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_ESTOP:
        g_mode = CTRL_IDLE;
        g_mode_b = CTRL_IDLE;
        g_autotune_a.state = AT_IDLE;
        g_autotune_b.state = AT_IDLE;
        Motor_EmergencyStop(&g_motor_a);
        Motor_EmergencyStop(&g_motor_b);
        PID_Reset(&g_pid_rpm);
        PID_Reset(&g_pid_pos);
        PID_Reset(&g_pid_rpm_b);
        PID_Reset(&g_pid_pos_b);
        g_applied_duty = 0U;
        g_applied_duty_b = 0U;
        App_SendAck(msg);
        break;

    case MSG_CMD_KEEPALIVE:
        App_SendAck(msg);
        break;

    case MSG_CMD_DIFF_DRIVE: {
        const PayloadCmdDiffDrive_t *cmd = (const PayloadCmdDiffDrive_t *)p;
        g_diff_linear  = cmd->linear;
        g_diff_angular = cmd->angular;
        /* Auto-enter diff mode if not already */
        if (g_mode != CTRL_DIFF || g_mode_b != CTRL_DIFF) {
            Safety_ClearFaults();
            g_mode = CTRL_DIFF;
            g_mode_b = CTRL_DIFF;
            g_pid_rpm.enabled = 1U;
            g_pid_rpm_b.enabled = 1U;
        }
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_AUTOTUNE_START: {
        const PayloadCmdAutotuneStart_t *cmd = (const PayloadCmdAutotuneStart_t *)p;
        if (!Safety_IsSafe()) {
            App_SendNack(msg, NACK_FAULT_ACTIVE);
            return;
        }
        g_mode = CTRL_AUTOTUNE;
        Autotune_Start(&g_autotune_a, cmd->setpoint_rpm, 250.0f, 45000U);
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_AUTOTUNE_ABORT:
        g_autotune_a.state = AT_IDLE;
        g_mode = CTRL_IDLE;
        Motor_Coast(&g_motor_a);
        g_applied_duty = 0U;
        App_SendAck(msg);
        break;

    /* ── Motor B commands ───────────────────────────────────── */
    case MSG_CMD_RPM_B: {
        const PayloadCmdRpm_t *cmd = (const PayloadCmdRpm_t *)p;
        PID_SetSetpoint(&g_pid_rpm_b, cmd->setpoint_rpm);
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_POSITION_B: {
        const PayloadCmdPosition_t *cmd = (const PayloadCmdPosition_t *)p;
        PID_SetSetpoint(&g_pid_pos_b, cmd->setpoint_deg);
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_GAINS_B: {
        const PayloadCmdGains_t *cmd = (const PayloadCmdGains_t *)p;
        PID_SetGains(&g_pid_rpm_b, cmd->kp, cmd->ki, cmd->kd);
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_MODE_B: {
        const PayloadCmdMode_t *cmd = (const PayloadCmdMode_t *)p;
        ControlMode_t new_mode = (ControlMode_t)cmd->mode;
        if (new_mode == CTRL_IDLE) {
            g_mode_b = CTRL_IDLE;
            Motor_Coast(&g_motor_b);
            PID_Reset(&g_pid_rpm_b);
            PID_Reset(&g_pid_pos_b);
            Encoder_ZeroPosition(&g_enc_b);
            g_applied_duty_b = 0U;
        } else {
            if (new_mode == CTRL_POSITION) {
                Encoder_ZeroPosition(&g_enc_b);
                PID_Reset(&g_pid_rpm_b);
                PID_Reset(&g_pid_pos_b);
            }
            g_mode_b = new_mode;
        }
        App_SendAck(msg);
        break;
    }

    case MSG_CMD_AUTOTUNE_START_B: {
        const PayloadCmdAutotuneStart_t *cmd = (const PayloadCmdAutotuneStart_t *)p;
        g_mode_b = CTRL_AUTOTUNE;
        Autotune_Start(&g_autotune_b, cmd->setpoint_rpm, 250.0f, 45000U);
        App_SendAck(msg);
        break;
    }

    default:
        App_SendNack(msg, NACK_UNKNOWN_MSG);
        break;
    }
}

/* ================================================================
 * Response helpers
 * ================================================================ */

static void App_SendAck(uint8_t msg_type)
{
    PayloadRespAck_t ack;
    ack.acked_msg_type = msg_type;
    uint16_t len = Comm_EncodeFrame(tx_resp_buf, MSG_RESP_ACK,
                                    (const uint8_t *)&ack,
                                    PAYLOAD_LEN_RESP_ACK);
    HAL_UART_Transmit(&huart2, tx_resp_buf, len, 5U);
}

static void App_SendNack(uint8_t msg_type, NackCode_t code)
{
    PayloadRespNack_t nack;
    nack.nacked_msg_type = msg_type;
    nack.error_code      = (uint8_t)code;
    uint16_t len = Comm_EncodeFrame(tx_resp_buf, MSG_RESP_NACK,
                                    (const uint8_t *)&nack,
                                    PAYLOAD_LEN_RESP_NACK);
    HAL_UART_Transmit(&huart2, tx_resp_buf, len, 5U);
}

static void App_SendAutotuneResult(const Autotune_t *at)
{
    PayloadRespAutotuneResult_t res;
    res.ku  = at->ku;
    res.pu_s = at->pu_s;
    res.kp  = at->kp_result;
    res.ki  = at->ki_result;
    res.kd  = at->kd_result;
    uint16_t len = Comm_EncodeFrame(tx_resp_buf, MSG_RESP_AUTOTUNE_RESULT,
                                    (const uint8_t *)&res,
                                    PAYLOAD_LEN_RESP_AUTOTUNE_RESULT);
    HAL_UART_Transmit(&huart2, tx_resp_buf, len, 10U);
}

#endif /* !MICROROS_ENABLED — end command dispatch block */

/* ================================================================
 * HAL callbacks
 * ================================================================ */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM10) {
        App_ControlTick();
    }
#if !defined(MICROROS_ENABLED)
    else if (htim->Instance == TIM11) {
        /* Telemetry uses UART DMA TX — disabled when micro-ROS owns USART2 */
        Telemetry_BuildAndSend(&g_telem);
    }
#endif
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
#if !defined(MICROROS_ENABLED)
    if (huart->Instance == USART2) {
        if (Comm_ReceiveByte(&g_comm_rx, g_uart_rx_byte)) {
            App_ProcessCommand();
        }
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&g_uart_rx_byte, 1U);
    }
#else
    (void)huart;
#endif
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
#if !defined(MICROROS_ENABLED)
    if (huart->Instance == USART2) {
        Telemetry_TxCompleteCallback(&g_telem);
    }
#else
    (void)huart;
#endif
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
#if !defined(MICROROS_ENABLED)
        /* Re-arm IT receive — micro-ROS uses blocking receive instead */
        huart->RxState = HAL_UART_STATE_READY;
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&g_uart_rx_byte, 1U);
#endif
    }
}

/* ================================================================
 * Peripheral init
 * ================================================================ */

static void SystemClock_Config(void)
{
    /*
     * HSI (16 MHz) + PLL → 84 MHz SYSCLK (F401RE max).
     * PLLM=16 → 1 MHz VCO input, PLLN=336 → 336 MHz VCO, PLLP=/4 → 84 MHz.
     * APB1 = 42 MHz, APB2 = 84 MHz.
     * Timer clocks: APB1 timers = 84 MHz, APB2 timers = 84 MHz.
     *
     * PORTED FROM: STM32F429ZI @ 168 MHz (PLLP=/2, APB1=/4, APB2=/2)
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    osc.HSIState       = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSIState       = RCC_LSI_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM       = 16U;
    osc.PLL.PLLN       = 336U;
    osc.PLL.PLLP       = RCC_PLLP_DIV4;   /* 336/4 = 84 MHz */
    osc.PLL.PLLQ       = 7U;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* AHB  = 84 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* APB1 = 42 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2 = 84 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    /* Motor A enable pin: PC10 */
    gpio.Pin   = GPIO_PIN_10;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET);

    /* Motor B enable pin: PC11 */
    gpio.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOC, &gpio);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET);

    /* Heartbeat LED: PA5 (NUCLEO LD2) */
    gpio.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* Fault LED: PB2 */
    gpio.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* E-stop button: PC13 (NUCLEO B1, active LOW with pull-up) */
    gpio.Pin  = GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
}

static void MX_TIM1_Init(void)
{
    /* TIM1: PWM for Motor A. GPIO/clock configured in HAL_TIM_PWM_MspInit. */
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0U;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = PWM_ARR;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0U;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_PWM_Init(&htim1);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 0U;
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_2);
}

static void MX_TIM2_Init(void)
{
    /* TIM2: encoder for Motor A. GPIO/clock configured in HAL_TIM_Encoder_MspInit. */
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 0U;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 0xFFFFFFFFU;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef enc_cfg = {0};
    enc_cfg.EncoderMode  = TIM_ENCODERMODE_TI12;
    enc_cfg.IC1Polarity  = TIM_ICPOLARITY_RISING;
    enc_cfg.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    enc_cfg.IC1Prescaler = TIM_ICPSC_DIV1;
    enc_cfg.IC1Filter    = 0x0FU;
    enc_cfg.IC2Polarity  = TIM_ICPOLARITY_RISING;
    enc_cfg.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    enc_cfg.IC2Prescaler = TIM_ICPSC_DIV1;
    enc_cfg.IC2Filter    = 0x0FU;

    HAL_TIM_Encoder_Init(&htim2, &enc_cfg);
}

static void MX_TIM10_Init(void)
{
    /* TIM10: 1 kHz PID. Clock/NVIC in HAL_TIM_Base_MspInit. */
    htim10.Instance               = TIM10;
    htim10.Init.Prescaler         = 83U;
    htim10.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim10.Init.Period            = 999U;
    htim10.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_Base_Init(&htim10);
}

static void MX_TIM11_Init(void)
{
    /* TIM11: 50 Hz telem. Clock/NVIC in HAL_TIM_Base_MspInit. */
    htim11.Instance               = TIM11;
    htim11.Init.Prescaler         = 83U;
    htim11.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim11.Init.Period            = 19999U;
    htim11.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_Base_Init(&htim11);
}

static void MX_USART2_UART_Init(void)
{
    /* USART2: 921600 baud VCP. GPIO/DMA/NVIC in HAL_UART_MspInit. */
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = COMM_BAUD;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&huart2);
}

static void MX_ADC1_Init(void)
{
    /* ADC1: 3-ch scan + DMA circular. GPIO/DMA in HAL_ADC_MspInit. */
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion       = ADC_NUM_CHANNELS;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;

    HAL_ADC_Init(&hadc1);

    ADC_ChannelConfTypeDef ch = {0};
    ch.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    ch.Channel = ADC_CHANNEL_13;    /* PC3 - motor A current */
    ch.Rank    = 1U;
    HAL_ADC_ConfigChannel(&hadc1, &ch);

    ch.Channel = ADC_CHANNEL_9;     /* PB1 - battery voltage */
    ch.Rank    = 2U;
    HAL_ADC_ConfigChannel(&hadc1, &ch);

    ch.Channel = ADC_CHANNEL_14;    /* PC4 - motor B current */
    ch.Rank    = 3U;
    HAL_ADC_ConfigChannel(&hadc1, &ch);
}

static void MX_I2C1_Init(void)
{
    /* I2C1: 400 kHz Fast Mode for MPU6050. GPIO/NVIC in HAL_I2C_MspInit. */
    hi2c1.Instance              = I2C1;
    hi2c1.Init.ClockSpeed       = 400000U;
    hi2c1.Init.DutyCycle        = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1      = 0U;
    hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2      = 0U;
    hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_Init(&hi2c1);
}

static void MX_TIM3_Init(void)
{
    /* TIM3: encoder for Motor B (16-bit). GPIO in HAL_TIM_Encoder_MspInit. */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0U;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 0xFFFFU;  /* 16-bit full range */
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef enc_cfg = {0};
    enc_cfg.EncoderMode  = TIM_ENCODERMODE_TI12;
    enc_cfg.IC1Polarity  = TIM_ICPOLARITY_RISING;
    enc_cfg.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    enc_cfg.IC1Prescaler = TIM_ICPSC_DIV1;
    enc_cfg.IC1Filter    = 0x0FU;
    enc_cfg.IC2Polarity  = TIM_ICPOLARITY_RISING;
    enc_cfg.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    enc_cfg.IC2Prescaler = TIM_ICPSC_DIV1;
    enc_cfg.IC2Filter    = 0x0FU;

    HAL_TIM_Encoder_Init(&htim3, &enc_cfg);
}

static void MX_TIM4_Init(void)
{
    /*
     * TIM4: PWM for Motor B.
     * APB1 timer clock = 84 MHz.
     * ARR = 839 (same as Motor A TIM1 on APB2 84 MHz — but TIM4 on APB1
     * has 84 MHz timer clock, so effective PWM = 84 MHz / 840 = 100 kHz).
     * PB6  = TIM4_CH1 = IN1_B (forward)
     * PB7  = TIM4_CH2 = IN2_B (reverse)
     */
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 0U;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = PWM_ARR;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_PWM_Init(&htim4);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0U;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim4, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim4, &oc, TIM_CHANNEL_2);
}

/* MX_IWDG_Init — disabled during bench debug: IWDG reset loop
static void MX_IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    hiwdg.Init.Reload    = IWDG_TIMEOUT_MS;

    HAL_IWDG_Init(&hiwdg);
}
*/

/* ================================================================
 * Error handler
 * ================================================================ */

void Error_Handler(void)
{
    __disable_irq();

    Motor_EmergencyStop(&g_motor_a);
    Motor_EmergencyStop(&g_motor_b);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);

    while (1) {
    }
}
