/**
 * @file cmd_vel_sub.c
 * @brief micro-ROS /cmd_vel subscriber → differential drive motor control.
 *
 * Uses the existing CTRL_DIFF mode in the control ISR:
 *   g_diff_linear  = linear.x  / MAX_LINEAR_VEL  (clamped -1..+1)
 *   g_diff_angular = angular.z / MAX_ANGULAR_VEL  (clamped -1..+1)
 *   g_mode = g_mode_b = CTRL_DIFF
 *
 * Watchdog: if no /cmd_vel for 500ms, sets both to CTRL_IDLE.
 *
 * Stack: 1536 words
 * Priority: 3 (above normal — responsive to commands)
 */

#include "FreeRTOS.h"
#include "task.h"

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>

#include "main.h"
#include "config.h"
#include "ros_topics.h"

/* ── Extern globals from main.c ──────────────────────────────── */
extern volatile float       g_diff_linear;
extern volatile float       g_diff_angular;
extern volatile ControlMode_t g_mode;
extern volatile ControlMode_t g_mode_b;

/* ── Extern micro-ROS node (from freertos_app.c) ─────────────── */
extern rcl_node_t       g_ros_node;
extern volatile uint8_t  g_ros_ready;

/* ── Velocity limits (m/s and rad/s → normalized -1..+1) ─────── */
#define MAX_LINEAR_VEL   0.5f   /* m/s at which g_diff_linear = 1.0 */
#define MAX_ANGULAR_VEL  3.0f   /* rad/s at which g_diff_angular = 1.0 */
#define CMD_VEL_TIMEOUT  500U   /* ms — stop motors if no msg */

/* ── State ───────────────────────────────────────────────────── */
static rcl_subscription_t         cmd_vel_sub;
static geometry_msgs__msg__Twist  cmd_vel_msg;
static rclc_executor_t            cmd_vel_exec;
static volatile uint32_t          last_cmd_tick;

/* ── Clamp helper ────────────────────────────────────────────── */
static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── Callback ────────────────────────────────────────────────── */
static void cmd_vel_cb(const void *msg)
{
    const geometry_msgs__msg__Twist *t =
        (const geometry_msgs__msg__Twist *)msg;

    float lin = clampf((float)t->linear.x  / MAX_LINEAR_VEL,  -1.0f, 1.0f);
    float ang = clampf((float)t->angular.z / MAX_ANGULAR_VEL, -1.0f, 1.0f);

    g_diff_linear  = lin;
    g_diff_angular = ang;

    /* Enter differential drive mode if not already */
    if (g_mode != CTRL_DIFF || g_mode_b != CTRL_DIFF) {
        g_mode   = CTRL_DIFF;
        g_mode_b = CTRL_DIFF;
    }

    last_cmd_tick = HAL_GetTick();
}

/* ── Task ────────────────────────────────────────────────────── */
void cmd_vel_task(void *arg)
{
    (void)arg;

    while (!g_ros_ready) vTaskDelay(pdMS_TO_TICKS(10));

    rcl_allocator_t alloc = rcl_get_default_allocator();

    rcl_ret_t rc = rclc_subscription_init_best_effort(
        &cmd_vel_sub, &g_ros_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "/cmd_vel");
    if (rc != RCL_RET_OK) {
        for (;;) { HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
                   vTaskDelay(pdMS_TO_TICKS(200)); }
    }

    extern rclc_support_t g_ros_support;
    rclc_executor_init(&cmd_vel_exec,
        &g_ros_support.context, 1, &alloc);
    rclc_executor_add_subscription(&cmd_vel_exec,
        &cmd_vel_sub, &cmd_vel_msg,
        cmd_vel_cb, ON_NEW_DATA);

    last_cmd_tick = HAL_GetTick();

    for (;;) {
        rclc_executor_spin_some(&cmd_vel_exec, RCL_MS_TO_NS(10));

        /* Watchdog: stop if no cmd_vel received recently */
        if ((HAL_GetTick() - last_cmd_tick) > CMD_VEL_TIMEOUT) {
            if (g_mode == CTRL_DIFF) {
                g_diff_linear  = 0.0f;
                g_diff_angular = 0.0f;
                g_mode   = CTRL_IDLE;
                g_mode_b = CTRL_IDLE;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));  /* 100Hz spin */
    }
}
