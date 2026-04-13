/**
 * @file ros_publishers.c
 * @brief ROS2 publisher tasks: IMU (50Hz), odometry (50Hz), diagnostics (1Hz).
 *
 * Reads existing global state (g_imu, g_pose, g_adc_dma_buf) — does NOT
 * call any driver functions directly. All sensor reads happen in TIM10 ISR.
 */

#include "FreeRTOS.h"
#include "task.h"

#include <rcl/rcl.h>
#include <rclc/rclc.h>
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

/* ── Shared micro-ROS objects (owned by microros_task in freertos_app.c) ── */
extern rcl_node_t      g_ros_node;
extern volatile uint8_t g_ros_ready;
extern rcl_publisher_t  g_imu_pub;
extern rcl_publisher_t  g_odom_pub;
extern rcl_publisher_t  g_diag_pub;

/* ── Conversions ─────────────────────────────────────────────── */
#define DPS_TO_RADS  0.017453293f
#define G_TO_MS2     9.80665f

/* ── IMU task: publish /auro/imu/data at 50Hz ────────────────── */

void ros_imu_task(void *arg)
{
    (void)arg;
    while (!g_ros_ready) vTaskDelay(pdMS_TO_TICKS(10));

    sensor_msgs__msg__Imu msg;
    memset(&msg, 0, sizeof(msg));

    /* Static frame ID */
    static char frame[] = FRAME_IMU;
    msg.header.frame_id.data = frame;
    msg.header.frame_id.size = sizeof(FRAME_IMU) - 1;
    msg.header.frame_id.capacity = sizeof(FRAME_IMU);

    /* Covariance: diagonal */
    msg.angular_velocity_covariance[0] = 2.5e-5;
    msg.angular_velocity_covariance[4] = 2.5e-5;
    msg.angular_velocity_covariance[8] = 2.5e-5;
    msg.linear_acceleration_covariance[0] = 8.5e-4;
    msg.linear_acceleration_covariance[4] = 8.5e-4;
    msg.linear_acceleration_covariance[8] = 8.5e-4;
    msg.orientation_covariance[0] = 1e-4;
    msg.orientation_covariance[4] = 1e-4;
    msg.orientation_covariance[8] = 1e-2; /* yaw unknown */

    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint32_t ms = HAL_GetTick();
        msg.header.stamp.sec     = (int32_t)(ms / 1000U);
        msg.header.stamp.nanosec = (ms % 1000U) * 1000000U;

        /* Read from global g_imu (updated in TIM10 ISR at 200Hz) */
        msg.linear_acceleration.x = (double)(g_imu.accel_g[0] * G_TO_MS2);
        msg.linear_acceleration.y = (double)(g_imu.accel_g[1] * G_TO_MS2);
        msg.linear_acceleration.z = (double)(g_imu.accel_g[2] * G_TO_MS2);
        msg.angular_velocity.x = (double)(g_imu.gyro_dps[0] * DPS_TO_RADS);
        msg.angular_velocity.y = (double)(g_imu.gyro_dps[1] * DPS_TO_RADS);
        msg.angular_velocity.z = (double)(g_imu.gyro_dps[2] * DPS_TO_RADS);

        /* Orientation from complementary filter (pitch only, yaw=0) */
        float pitch = g_imu.pitch_deg * DPS_TO_RADS;
        float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
        msg.orientation.w = (double)cp;
        msg.orientation.x = 0.0;
        msg.orientation.y = (double)sp;
        msg.orientation.z = 0.0;

        (void)!rcl_publish(&g_imu_pub, &msg, NULL);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20)); /* 50Hz */
    }
}

/* ── Odometry task: publish /auro/odom at 50Hz ───────────────── */

void ros_odom_task(void *arg)
{
    (void)arg;
    while (!g_ros_ready) vTaskDelay(pdMS_TO_TICKS(10));

    nav_msgs__msg__Odometry msg;
    memset(&msg, 0, sizeof(msg));

    static char frame_odom[] = FRAME_ODOM;
    static char frame_base[] = FRAME_BASE;
    msg.header.frame_id.data = frame_odom;
    msg.header.frame_id.size = sizeof(FRAME_ODOM) - 1;
    msg.header.frame_id.capacity = sizeof(FRAME_ODOM);
    msg.child_frame_id.data = frame_base;
    msg.child_frame_id.size = sizeof(FRAME_BASE) - 1;
    msg.child_frame_id.capacity = sizeof(FRAME_BASE);

    /* Pose covariance diagonal */
    msg.pose.covariance[0]  = 1e-3;
    msg.pose.covariance[7]  = 1e-3;
    msg.pose.covariance[35] = 1e-2;
    msg.twist.covariance[0] = 1e-3;
    msg.twist.covariance[35]= 1e-2;

    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint32_t ms = HAL_GetTick();
        msg.header.stamp.sec     = (int32_t)(ms / 1000U);
        msg.header.stamp.nanosec = (ms % 1000U) * 1000000U;

        /* Read from global g_pose (updated in TIM10 ISR) */
        Pose_t p = *(volatile Pose_t *)&g_pose; /* snapshot */

        msg.pose.pose.position.x = (double)p.x_m;
        msg.pose.pose.position.y = (double)p.y_m;
        msg.pose.pose.position.z = 0.0;

        float qw = cosf(p.theta_rad * 0.5f);
        float qz = sinf(p.theta_rad * 0.5f);
        msg.pose.pose.orientation.w = (double)qw;
        msg.pose.pose.orientation.z = (double)qz;

        msg.twist.twist.linear.x  = (double)p.v_m_s;
        msg.twist.twist.angular.z = (double)p.w_rad_s;

        (void)!rcl_publish(&g_odom_pub, &msg, NULL);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20)); /* 50Hz */
    }
}

/* ── Diagnostics task: publish /auro/diagnostics at 1Hz ──────── */

void ros_diag_task(void *arg)
{
    (void)arg;
    while (!g_ros_ready) vTaskDelay(pdMS_TO_TICKS(10));

    std_msgs__msg__String msg;
    memset(&msg, 0, sizeof(msg));

    /* Use a simple string message for diagnostics — avoids complex
     * DiagnosticArray allocation issues on constrained SRAM. */
    static char diag_buf[128];
    msg.data.data = diag_buf;
    msg.data.capacity = sizeof(diag_buf);

    TickType_t last = xTaskGetTickCount();
    for (;;) {
        float batt_v = ((float)g_adc_dma_buf[1] / BATT_ADC_FULL_SCALE)
                       * BATT_ADC_VREF * BATT_V_SCALE;
        uint8_t faults = Safety_GetFaults();

        int len = snprintf(diag_buf, sizeof(diag_buf),
            "batt=%.2fV fault=0x%02X pitch=%.1f imu=%s",
            batt_v, faults, g_imu.pitch_deg,
            g_imu.initialized ? "ok" : "fail");
        msg.data.size = (size_t)(len > 0 ? len : 0);

        (void)!rcl_publish(&g_diag_pub, &msg, NULL);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000)); /* 1Hz */
    }
}
