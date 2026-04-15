/**
 * @file ros_publishers.c
 * @brief ROS2 message init + fill helpers for IMU, odometry, diagnostics.
 *
 * All publishing happens in microros_task (freertos_app.c) to ensure
 * single-threaded XRCE-DDS session access.  These helpers only prepare
 * message data — they do NOT call rcl_publish().
 */

#include <rcl/rcl.h>
#include <sensor_msgs/msg/imu.h>
#include <nav_msgs/msg/odometry.h>
#include <std_msgs/msg/string.h>

#include "main.h"
#include "config.h"
#include "ros_topics.h"
#include "imu_mpu6050.h"
#include "odometry.h"
#include "safety.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

/* ── Conversions ─────────────────────────────────────────────── */
#define DPS_TO_RADS  0.017453293f
#define G_TO_MS2     9.80665f

/* ── IMU message init + fill ─────────────────────────────────── */

static char imu_frame[] = FRAME_IMU;

void ros_imu_msg_init(sensor_msgs__msg__Imu *m)
{
    memset(m, 0, sizeof(*m));
    m->header.frame_id.data     = imu_frame;
    m->header.frame_id.size     = sizeof(FRAME_IMU) - 1;
    m->header.frame_id.capacity = sizeof(FRAME_IMU);

    m->angular_velocity_covariance[0] = 2.5e-5;
    m->angular_velocity_covariance[4] = 2.5e-5;
    m->angular_velocity_covariance[8] = 2.5e-5;
    m->linear_acceleration_covariance[0] = 8.5e-4;
    m->linear_acceleration_covariance[4] = 8.5e-4;
    m->linear_acceleration_covariance[8] = 8.5e-4;
    m->orientation_covariance[0] = 1e-4;
    m->orientation_covariance[4] = 1e-4;
    m->orientation_covariance[8] = 1e-2;
}

void ros_imu_fill(sensor_msgs__msg__Imu *m)
{
    uint32_t ms = HAL_GetTick();
    m->header.stamp.sec     = (int32_t)(ms / 1000U);
    m->header.stamp.nanosec = (ms % 1000U) * 1000000U;

    m->linear_acceleration.x = (double)(g_imu.accel_g[0] * G_TO_MS2);
    m->linear_acceleration.y = (double)(g_imu.accel_g[1] * G_TO_MS2);
    m->linear_acceleration.z = (double)(g_imu.accel_g[2] * G_TO_MS2);
    m->angular_velocity.x = (double)(g_imu.gyro_dps[0] * DPS_TO_RADS);
    m->angular_velocity.y = (double)(g_imu.gyro_dps[1] * DPS_TO_RADS);
    m->angular_velocity.z = (double)(g_imu.gyro_dps[2] * DPS_TO_RADS);

    float pitch = g_imu.pitch_deg * DPS_TO_RADS;
    float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
    m->orientation.w = (double)cp;
    m->orientation.x = 0.0;
    m->orientation.y = (double)sp;
    m->orientation.z = 0.0;
}

/* ── Odometry message init + fill ────────────────────────────── */

static char odom_frame[] = FRAME_ODOM;
static char base_frame[] = FRAME_BASE;

void ros_odom_msg_init(nav_msgs__msg__Odometry *m)
{
    memset(m, 0, sizeof(*m));
    m->header.frame_id.data     = odom_frame;
    m->header.frame_id.size     = sizeof(FRAME_ODOM) - 1;
    m->header.frame_id.capacity = sizeof(FRAME_ODOM);
    m->child_frame_id.data      = base_frame;
    m->child_frame_id.size      = sizeof(FRAME_BASE) - 1;
    m->child_frame_id.capacity  = sizeof(FRAME_BASE);

    m->pose.covariance[0]  = 1e-3;
    m->pose.covariance[7]  = 1e-3;
    m->pose.covariance[35] = 1e-2;
    m->twist.covariance[0] = 1e-3;
    m->twist.covariance[35]= 1e-2;
}

void ros_odom_fill(nav_msgs__msg__Odometry *m)
{
    uint32_t ms = HAL_GetTick();
    m->header.stamp.sec     = (int32_t)(ms / 1000U);
    m->header.stamp.nanosec = (ms % 1000U) * 1000000U;

    Pose_t p = *(volatile Pose_t *)&g_pose;

    m->pose.pose.position.x = (double)p.x_m;
    m->pose.pose.position.y = (double)p.y_m;
    m->pose.pose.position.z = 0.0;

    float qw = cosf(p.theta_rad * 0.5f);
    float qz = sinf(p.theta_rad * 0.5f);
    m->pose.pose.orientation.w = (double)qw;
    m->pose.pose.orientation.z = (double)qz;

    m->twist.twist.linear.x  = (double)p.v_m_s;
    m->twist.twist.angular.z = (double)p.w_rad_s;
}

/* ── Diagnostics message init + fill ─────────────────────────── */

void ros_diag_msg_init(std_msgs__msg__String *m)
{
    memset(m, 0, sizeof(*m));
}

void ros_diag_fill(std_msgs__msg__String *m)
{
    float batt_v = ((float)g_adc_dma_buf[1] / BATT_ADC_FULL_SCALE)
                   * BATT_ADC_VREF * BATT_V_SCALE;
    uint8_t faults = Safety_GetFaults();

    int len = snprintf(m->data.data, m->data.capacity,
        "batt=%.2fV fault=0x%02X pitch=%.1f imu=%s",
        batt_v, faults, g_imu.pitch_deg,
        g_imu.initialized ? "ok" : "fail");
    m->data.size = (size_t)(len > 0 ? len : 0);
}
