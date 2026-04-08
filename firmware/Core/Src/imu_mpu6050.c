/**
 * @file imu_mpu6050.c
 * @brief MPU6050 I2C driver with complementary filter.
 *
 * Uses blocking HAL_I2C calls (< 500 µs per burst read at 400 kHz).
 * Safe to call from 1 kHz ISR since I2C priority is lower — but we
 * call from main context or a lower-priority tick to avoid jitter.
 */

#include "imu_mpu6050.h"
#include <math.h>

#define I2C_TIMEOUT_MS  10U

/**
 * @brief STM32F4 I2C BUSY flag workaround (errata 2.14.7).
 *
 * If the I2C bus is stuck with BUSY=1 after reset (e.g. slave holds SDA low),
 * we must bitbang SCL until SDA releases, then generate a STOP condition,
 * then re-init the peripheral.
 */
static void I2C1_BusReset(void)
{
    /* 1. Disable I2C peripheral */
    __HAL_RCC_I2C1_CLK_ENABLE();
    I2C1->CR1 &= ~I2C_CR1_PE;

    /* 2. Configure SCL (PB8) and SDA (PB9) as GPIO open-drain output */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* 3. Set both high */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);  /* SCL */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);  /* SDA */
    HAL_Delay(1);

    /* 4. Clock SCL until SDA goes high (max 16 clocks) */
    for (int i = 0; i < 16; i++) {
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_SET) {
            break;  /* SDA released */
        }
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_Delay(1);
    }

    /* 5. Generate STOP: SDA low while SCL high, then SDA high */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);  /* SDA low */
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);    /* SCL high */
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);    /* SDA high = STOP */
    HAL_Delay(1);

    /* 6. Reconfigure as AF4 (I2C1) — HAL_I2C_Init will handle this via MspInit */
}

/* ── Internal helpers ─────────────────────────────────────── */

static HAL_StatusTypeDef MPU_WriteReg(I2C_HandleTypeDef *hi2c,
                                       uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &val, 1, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef MPU_ReadReg(I2C_HandleTypeDef *hi2c,
                                      uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, len, I2C_TIMEOUT_MS);
}

/* ── Public API ───────────────────────────────────────────── */

IMU_Status_t IMU_Init(IMU_t *imu, I2C_HandleTypeDef *hi2c)
{
    imu->hi2c = hi2c;
    imu->initialized = 0U;
    imu->gyro_cal_done = 0U;
    imu->pitch_deg = 0.0f;
    imu->pitch_rate_dps = 0.0f;

    for (int i = 0; i < 3; i++) {
        imu->gyro_offset[i] = 0.0f;
    }

    /* STM32F4 I2C BUSY errata workaround — must run before first transaction */
    I2C1_BusReset();

    /* Re-init I2C1 peripheral after bus reset (MSP reconfigures GPIOs as AF) */
    HAL_I2C_DeInit(hi2c);
    HAL_I2C_Init(hi2c);

    /* Probe: is device present? 3 attempts, 50 ms each */
    if (HAL_I2C_IsDeviceReady(hi2c, MPU6050_ADDR, 3, 50) != HAL_OK) {
        return IMU_ERR_I2C;
    }

    /* WHO_AM_I check */
    uint8_t who = 0;
    if (MPU_ReadReg(hi2c, MPU6050_REG_WHO_AM_I, &who, 1) != HAL_OK) {
        return IMU_ERR_I2C;
    }
    if (who != MPU6050_WHO_AM_I_VAL) {
        return IMU_ERR_WHO_AM_I;
    }

    /* Wake up (clear SLEEP bit), use PLL with X-axis gyro ref */
    if (MPU_WriteReg(hi2c, MPU6050_REG_PWR_MGMT_1, 0x01U) != HAL_OK) {
        return IMU_ERR_I2C;
    }

    /* Sample rate = 1 kHz / (1 + SMPLRT_DIV) = 1 kHz */
    if (MPU_WriteReg(hi2c, MPU6050_REG_SMPLRT_DIV, 0x00U) != HAL_OK) {
        return IMU_ERR_I2C;
    }

    /* DLPF config: BW=44 Hz accel, 42 Hz gyro (CONFIG=3) */
    if (MPU_WriteReg(hi2c, MPU6050_REG_CONFIG, 0x03U) != HAL_OK) {
        return IMU_ERR_I2C;
    }

    /* Gyro: FS_SEL=0 → ±250 deg/s (best resolution for balancing) */
    if (MPU_WriteReg(hi2c, MPU6050_REG_GYRO_CONFIG, 0x00U) != HAL_OK) {
        return IMU_ERR_I2C;
    }

    /* Accel: AFS_SEL=0 → ±2g */
    if (MPU_WriteReg(hi2c, MPU6050_REG_ACCEL_CONFIG, 0x00U) != HAL_OK) {
        return IMU_ERR_I2C;
    }

    imu->initialized = 1U;
    return IMU_OK;
}

IMU_Status_t IMU_CalibrateGyro(IMU_t *imu, uint16_t samples)
{
    if (!imu->initialized) return IMU_ERR_NOT_INIT;

    float sum[3] = {0.0f, 0.0f, 0.0f};

    for (uint16_t i = 0; i < samples; i++) {
        IMU_Status_t st = IMU_ReadAll(imu);
        if (st != IMU_OK) return st;
        sum[0] += imu->gyro_dps[0];
        sum[1] += imu->gyro_dps[1];
        sum[2] += imu->gyro_dps[2];
        HAL_Delay(1);
    }

    imu->gyro_offset[0] = sum[0] / (float)samples;
    imu->gyro_offset[1] = sum[1] / (float)samples;
    imu->gyro_offset[2] = sum[2] / (float)samples;
    imu->gyro_cal_done = 1U;

    /* Seed pitch from accelerometer */
    float ax = imu->accel_g[0];
    float az = imu->accel_g[2];
    imu->pitch_deg = atan2f(ax, az) * (180.0f / 3.14159265f);

    return IMU_OK;
}

IMU_Status_t IMU_ReadAll(IMU_t *imu)
{
    if (!imu->initialized) return IMU_ERR_NOT_INIT;

    /* Burst read 14 bytes: accel(6) + temp(2) + gyro(6) */
    uint8_t buf[14];
    if (MPU_ReadReg(imu->hi2c, MPU6050_REG_ACCEL_XOUT_H, buf, 14) != HAL_OK) {
        return IMU_ERR_I2C;
    }

    /* Parse big-endian 16-bit values */
    imu->accel_raw[0] = (int16_t)((buf[0] << 8) | buf[1]);
    imu->accel_raw[1] = (int16_t)((buf[2] << 8) | buf[3]);
    imu->accel_raw[2] = (int16_t)((buf[4] << 8) | buf[5]);
    imu->temp_raw     = (int16_t)((buf[6] << 8) | buf[7]);
    imu->gyro_raw[0]  = (int16_t)((buf[8] << 8) | buf[9]);
    imu->gyro_raw[1]  = (int16_t)((buf[10] << 8) | buf[11]);
    imu->gyro_raw[2]  = (int16_t)((buf[12] << 8) | buf[13]);

    /* Scale */
    imu->accel_g[0] = (float)imu->accel_raw[0] * MPU6050_ACCEL_SCALE;
    imu->accel_g[1] = (float)imu->accel_raw[1] * MPU6050_ACCEL_SCALE;
    imu->accel_g[2] = (float)imu->accel_raw[2] * MPU6050_ACCEL_SCALE;

    imu->gyro_dps[0] = (float)imu->gyro_raw[0] * MPU6050_GYRO_SCALE;
    imu->gyro_dps[1] = (float)imu->gyro_raw[1] * MPU6050_GYRO_SCALE;
    imu->gyro_dps[2] = (float)imu->gyro_raw[2] * MPU6050_GYRO_SCALE;

    imu->temp_c = (float)imu->temp_raw / 340.0f + 36.53f;

    return IMU_OK;
}

void IMU_UpdateAngle(IMU_t *imu, float dt)
{
    /* Apply gyro calibration offset */
    float gx = imu->gyro_dps[0] - imu->gyro_offset[0];
    float gy = imu->gyro_dps[1] - imu->gyro_offset[1];

    /* Pitch rate — which axis depends on mounting.
     * Default: X-axis is pitch (rotation around the wheel axle).
     * If mounting differs, swap axes here. */
    imu->pitch_rate_dps = gx;

    /* Accel-based angle (atan2 of X vs Z for pitch) */
    float accel_pitch = atan2f(imu->accel_g[0], imu->accel_g[2])
                        * (180.0f / 3.14159265f);

    /* Complementary filter */
    imu->pitch_deg = IMU_COMP_ALPHA * (imu->pitch_deg + gx * dt)
                   + (1.0f - IMU_COMP_ALPHA) * accel_pitch;

    (void)gy; /* Y-axis gyro unused for now (roll) */
}
