/**
 * @file imu_mpu6050.h
 * @brief MPU6050 I2C driver with complementary filter for tilt angle.
 *
 * Polling-mode I2C reads at up to 1 kHz from the 1 kHz control ISR.
 * Provides fused pitch angle via complementary filter (accel + gyro).
 */

#ifndef IMU_MPU6050_H
#define IMU_MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ── MPU6050 I2C address (AD0 = GND) ──────────────────────── */
#define MPU6050_ADDR            (0x68U << 1)  /* HAL uses 8-bit address */

/* ── MPU6050 registers ────────────────────────────────────── */
#define MPU6050_REG_SMPLRT_DIV  0x19U
#define MPU6050_REG_CONFIG      0x1AU
#define MPU6050_REG_GYRO_CONFIG 0x1BU
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU  /* 14 bytes: accel(6)+temp(2)+gyro(6) */
#define MPU6050_REG_PWR_MGMT_1  0x6BU
#define MPU6050_REG_WHO_AM_I    0x75U

#define MPU6050_WHO_AM_I_VAL    0x68U
#define ICM20602_WHO_AM_I_VAL   0x12U
#define ICM20602_WHO_AM_I_ALT   0x72U   /* some ICM-20602 batches */
#define MPU6500_WHO_AM_I_VAL    0x70U

/* ── Gyro/Accel scale factors ─────────────────────────────── */
/* FS_SEL=0 → ±250 deg/s  → 131 LSB/deg/s */
#define MPU6050_GYRO_SCALE      (1.0f / 131.0f)
/* AFS_SEL=0 → ±2g → 16384 LSB/g */
#define MPU6050_ACCEL_SCALE     (1.0f / 16384.0f)

/* ── Complementary filter ─────────────────────────────────── */
#define IMU_COMP_ALPHA          0.98f   /* Gyro trust (0.95–0.99 typical) */

typedef enum {
    IMU_OK = 0,
    IMU_ERR_I2C,
    IMU_ERR_WHO_AM_I,
    IMU_ERR_NOT_INIT,
} IMU_Status_t;

typedef struct {
    I2C_HandleTypeDef *hi2c;

    /* Raw sensor data (latest read) */
    int16_t accel_raw[3];   /* X, Y, Z */
    int16_t gyro_raw[3];    /* X, Y, Z */
    int16_t temp_raw;

    /* Scaled values */
    float accel_g[3];       /* g */
    float gyro_dps[3];      /* deg/s */
    float temp_c;           /* Celsius */

    /* Fused angle (complementary filter output) */
    float pitch_deg;        /* Tilt angle used for balancing */
    float pitch_rate_dps;   /* Gyro rate on pitch axis */

    /* State */
    uint8_t initialized;
    uint8_t gyro_cal_done;
    float   gyro_offset[3]; /* Calibration offsets (deg/s) */
} IMU_t;

IMU_Status_t IMU_Init(IMU_t *imu, I2C_HandleTypeDef *hi2c);
IMU_Status_t IMU_CalibrateGyro(IMU_t *imu, uint16_t samples);
IMU_Status_t IMU_ReadAll(IMU_t *imu);
void         IMU_UpdateAngle(IMU_t *imu, float dt);

#ifdef __cplusplus
}
#endif

#endif /* IMU_MPU6050_H */
