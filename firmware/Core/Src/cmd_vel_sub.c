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
#include "safety.h"
#include "rgb_led.h"

/* ── Extern globals from main.c ──────────────────────────────── */
extern volatile float       g_diff_linear;
extern volatile float       g_diff_angular;
extern volatile ControlMode_t g_mode;
extern volatile ControlMode_t g_mode_b;
extern volatile uint32_t      g_last_cmd_ms;

/* ── Extern micro-ROS node (from freertos_app.c) ─────────────── */
extern rcl_node_t       g_ros_node;
extern volatile uint8_t  g_ros_ready;

/* ── Velocity limits (m/s and rad/s → normalized -1..+1) ─────── */
#define MAX_LINEAR_VEL   0.5f   /* m/s at which g_diff_linear = 1.0 */
#define MAX_ANGULAR_VEL  3.0f   /* rad/s at which g_diff_angular = 1.0 */
#define CMD_VEL_TIMEOUT  500U   /* ms — stop motors 500ms after last cmd_vel */

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

    g_diff_linear  = clampf((float)t->linear.x  / MAX_LINEAR_VEL,  -1.0f, 1.0f);
    g_diff_angular = clampf((float)t->angular.z / MAX_ANGULAR_VEL, -1.0f, 1.0f);

    Safety_ClearFaults();
    g_mode       = CTRL_DIFF;
    g_mode_b     = CTRL_DIFF;
    g_last_cmd_ms = HAL_GetTick();
    last_cmd_tick = HAL_GetTick();   /* MUST update — prevents watchdog zero */

    RGB_SetState(RGB_GREEN_SOLID);
}

/* ── Init (call once from microros_task after node ready) ─────── */
void cmd_vel_init(void)
{
    static rcl_allocator_t alloc;
    alloc = rcl_get_default_allocator();

    rclc_subscription_init_best_effort(
        &cmd_vel_sub, &g_ros_node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "/cmd_vel");

    extern rclc_support_t g_ros_support;
    rclc_executor_init(&cmd_vel_exec,
        &g_ros_support.context, 1, &alloc);
    rclc_executor_add_subscription(&cmd_vel_exec,
        &cmd_vel_sub, &cmd_vel_msg,
        cmd_vel_cb, ON_NEW_DATA);

    last_cmd_tick = HAL_GetTick();
}

/* ── Spin (call from microros_task main loop at 100Hz) ─────── */
void cmd_vel_spin(void)
{
    rclc_executor_spin_some(&cmd_vel_exec, 0);

    /* Watchdog: idle motors if no cmd_vel recently */
    if ((HAL_GetTick() - last_cmd_tick) > CMD_VEL_TIMEOUT) {
        g_diff_linear  = 0.0f;
        g_diff_angular = 0.0f;
        g_mode         = CTRL_IDLE;
        g_mode_b       = CTRL_IDLE;
        g_last_cmd_ms  = HAL_GetTick();
        if (RGB_GetState() == RGB_GREEN_SOLID)
            RGB_SetState(RGB_GREEN_BLINK);
    }
}
