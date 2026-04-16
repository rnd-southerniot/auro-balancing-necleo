/**
 * @file freertos_app.c
 * @brief FreeRTOS task init and heartbeat.
 *
 * ENV-01: Minimal FreeRTOS integration.
 * - Heartbeat task: toggles LD2 at 1Hz + feeds IWDG
 * - micro-ROS task: publishes /auro/heartbeat if MICROROS_ENABLED
 *
 * All ISR-driven control (TIM10 PID, TIM11 telemetry, UART RX command
 * dispatch) continues unchanged in interrupt context.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "config.h"
#include "rgb_led.h"
#include "balance.h"

volatile uint32_t g_stack_hwm = 0xFFFF;

#if defined(MICROROS_ENABLED)
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/string.h>
#include <sensor_msgs/msg/imu.h>
#include <nav_msgs/msg/odometry.h>
#include <rmw_microros/rmw_microros.h>
#include <rmw/ret_types.h>

/* Transport functions (microros_transport.c) */
extern bool cubemx_transport_open(struct uxrCustomTransport *transport);
extern bool cubemx_transport_close(struct uxrCustomTransport *transport);
extern size_t cubemx_transport_write(struct uxrCustomTransport *transport,
                                      const uint8_t *buf, size_t len,
                                      uint8_t *errcode);
extern size_t cubemx_transport_read(struct uxrCustomTransport *transport,
                                     uint8_t *buf, size_t len,
                                     int timeout, uint8_t *errcode);

/* Allocator functions (microros_allocators.c) */
extern void *microros_allocate(size_t size, void *state);
extern void  microros_deallocate(void *pointer, void *state);
extern void *microros_reallocate(void *pointer, size_t size, void *state);
extern void *microros_zero_allocate(size_t num, size_t size, void *state);

/* Shared node for all publisher tasks */
rcl_node_t       g_ros_node;
rclc_support_t   g_ros_support;
volatile uint8_t g_ros_ready = 0U;

/* Publishers created in microros_task, used by sensor tasks.
 * XRCE-DDS client is NOT thread-safe — all entity creation
 * MUST happen in a single task to avoid transport corruption. */
rcl_publisher_t  g_imu_pub;
rcl_publisher_t  g_odom_pub;
rcl_publisher_t  g_diag_pub;
#endif /* MICROROS_ENABLED */

/* ── Heartbeat task ──────────────────────────────────────────── */

static void heartbeat_task(void *arg)
{
    (void)arg;
    for (;;) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  /* LD2 */
        HAL_IWDG_Refresh(&hiwdg);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ── micro-ROS task (only if library is linked) ──────────────── */

#if defined(MICROROS_ENABLED)

/* blink_error removed — PB2 now driven by RGB LED driver.
 * Error stages shown via RGB color transitions instead. */

static void microros_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));

    RGB_SetState(RGB_WHITE_BLINK);  /* FreeRTOS started, micro-ROS init */

    /* Stage 1: task alive */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Stage 2: UART transport (skip custom allocator — use malloc/free
     * overrides in microros_allocators.c instead) */
    if (rmw_uros_set_custom_transport(
            true,
            (void *)&huart6,  /* USART6 → ESP32-S3 WiFi bridge */
            cubemx_transport_open,
            cubemx_transport_close,
            cubemx_transport_write,
            cubemx_transport_read) != RMW_RET_OK) {
        for (;;) { RGB_SetState(RGB_RED_SOLID); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    vTaskDelay(pdMS_TO_TICKS(500));  /* transport OK */

    Balance_Init();

    /* Stage 4: Wait for agent — RGB blue blink */
    RGB_SetState(RGB_BLUE_BLINK);
    while (rmw_uros_ping_agent(1000, 5) != RCL_RET_OK) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    RGB_SetState(RGB_BLUE_SOLID);  /* agent connected */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Stage 5: Init micro-ROS node (shared with publisher tasks) */
    rcl_allocator_t allocator = rcl_get_default_allocator();
    /* Note: g_ros_support is global — used by cmd_vel executor */
    rcl_publisher_t pub;
    std_msgs__msg__Int32 msg;

    if (rclc_support_init(&g_ros_support, 0, NULL, &allocator) != RCL_RET_OK) {
        for (;;) { RGB_SetState(RGB_RED_SOLID); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    if (rclc_node_init_default(&g_ros_node, "auro_stm32", "auro", &g_ros_support) != RCL_RET_OK) {
        for (;;) { RGB_SetState(RGB_RED_SOLID); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    if (rclc_publisher_init_best_effort(&pub, &g_ros_node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
            "/auro/heartbeat") != RCL_RET_OK) {
        for (;;) { RGB_SetState(RGB_RED_SOLID); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    /* Create ALL publishers here in microros_task — XRCE-DDS client
     * is NOT thread-safe.  Concurrent entity creation from multiple
     * FreeRTOS tasks corrupts the transport buffer, causing agent
     * deserialization errors and failed session setup. */
    if (rclc_publisher_init_best_effort(&g_imu_pub, &g_ros_node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
            "/auro/imu/data") != RCL_RET_OK) {
        for (;;) { RGB_SetState(RGB_RED_SOLID); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    if (rclc_publisher_init_best_effort(&g_odom_pub, &g_ros_node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry),
            "/auro/odom") != RCL_RET_OK) {
        for (;;) { RGB_SetState(RGB_RED_SOLID); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    if (rclc_publisher_init_best_effort(&g_diag_pub, &g_ros_node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
            "/auro/diagnostics") != RCL_RET_OK) {
        for (;;) { RGB_SetState(RGB_RED_SOLID); vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    /* cmd_vel subscriber — runs in THIS task, shares transport context */
    extern void cmd_vel_init(void);
    extern void cmd_vel_spin(void);
    cmd_vel_init();

    /* XRCE-DDS client is NOT thread-safe.  ALL session operations
     * (publish, spin, etc.) must happen in this single task.
     * Sensor tasks are NOT created — publishing happens here. */
    g_ros_ready = 1U;  /* Sensor globals (g_imu, g_pose) are still updated by ISR */

    /* Prepare sensor messages (one-time init) */
    extern void ros_imu_msg_init(sensor_msgs__msg__Imu *m);
    extern void ros_odom_msg_init(nav_msgs__msg__Odometry *m);
    extern void ros_diag_msg_init(std_msgs__msg__String *m);

    sensor_msgs__msg__Imu imu_msg;
    nav_msgs__msg__Odometry odom_msg;
    std_msgs__msg__String diag_msg;
    static char diag_buf[128];

    ros_imu_msg_init(&imu_msg);
    ros_odom_msg_init(&odom_msg);
    ros_diag_msg_init(&diag_msg);
    diag_msg.data.data = diag_buf;
    diag_msg.data.capacity = sizeof(diag_buf);

    RGB_SetState(RGB_GREEN_BLINK);  /* topics publishing */

    /* Enable balance controller — fall detection (±35°) active as safety */
    Balance_Enable();
    /* Set CTRL_DIFF so ISR drives motors from g_diff_linear */
    {
        extern volatile ControlMode_t g_mode;
        extern volatile ControlMode_t g_mode_b;
        extern volatile uint32_t      g_last_cmd_ms;
        g_mode   = CTRL_DIFF;
        g_mode_b = CTRL_DIFF;
        g_last_cmd_ms = HAL_GetTick();
    }

    g_stack_hwm = uxTaskGetStackHighWaterMark(NULL);

    msg.data = 0;

    uint32_t imu_next  = HAL_GetTick();
    uint32_t odom_next = imu_next;
    uint32_t diag_next = imu_next;
    uint32_t hb_next   = imu_next;
    uint32_t bal_next  = imu_next;

    for (;;) {
        uint32_t now = HAL_GetTick();

        /* Spin cmd_vel executor — processes incoming /cmd_vel */
        cmd_vel_spin();

        /* Publish sensor data (single-threaded — XRCE-DDS not thread-safe) */
        if (now >= imu_next) {
            imu_next = now + 20U;   /* 50Hz */
            extern void ros_imu_fill(sensor_msgs__msg__Imu *m);
            ros_imu_fill(&imu_msg);
            (void)!rcl_publish(&g_imu_pub, &imu_msg, NULL);
        }
        /* Balance controller tick at 50Hz (aligned with IMU rate) */
        if (now >= bal_next) {
            bal_next = now + 20U;
            if (Balance_IsOn()) {
                if (!Balance_Tick()) {
                    Balance_Disable();  /* fall detected */
                    RGB_SetState(RGB_RED_SOLID);
                }
            }
        }

        if (now >= odom_next) {
            odom_next = now + 20U;  /* 50Hz */
            extern void ros_odom_fill(nav_msgs__msg__Odometry *m);
            ros_odom_fill(&odom_msg);
            (void)!rcl_publish(&g_odom_pub, &odom_msg, NULL);
        }
        if (now >= diag_next) {
            diag_next = now + 1000U;  /* 1Hz */
            extern void ros_diag_fill(std_msgs__msg__String *m);
            ros_diag_fill(&diag_msg);
            (void)!rcl_publish(&g_diag_pub, &diag_msg, NULL);
        }
        if (now >= hb_next) {
            hb_next = now + 1000U;  /* 1Hz */
            msg.data++;
            (void)!rcl_publish(&pub, &msg, NULL);
        }

        vTaskDelay(pdMS_TO_TICKS(10));  /* 100Hz spin rate */
    }
}
#endif /* MICROROS_ENABLED */

/* ── App init — called from main() ──────────────────────────── */

/* ── RGB tick task — drives blink timing at 20Hz ────────────── */
static void rgb_tick_task(void *arg)
{
    (void)arg;
    for (;;) {
        RGB_Tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_freertos_init(void)
{
    RGB_Init();  /* init RGB LED pins before any task starts */

    /* Heartbeat MUST start first — provides visual feedback even if
     * micro-ROS task crashes during init. */
    xTaskCreate(heartbeat_task, "heartbeat", 256, NULL, 1, NULL);
    xTaskCreate(rgb_tick_task, "rgb", 128, NULL, 1, NULL);

#if defined(MICROROS_ENABLED)
    /* micro-ROS task — 2KB stack to leave heap for micro-ROS allocs. */
    BaseType_t rc = xTaskCreate(microros_task, "microros", 2048, NULL, 2, NULL);
    if (rc != pdPASS) {
        /* Task creation failed — blink fault LED rapidly forever */
        for (;;) {
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
            HAL_Delay(100);
        }
    }
#endif

    /* Start scheduler — does not return */
    vTaskStartScheduler();
}
