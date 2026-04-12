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

#if defined(MICROROS_ENABLED)
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
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
rcl_node_t      g_ros_node;
volatile uint8_t g_ros_ready = 0U;
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

/* Blink fault LED (PB2) N times to indicate error stage */
static void blink_error(uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS(200));
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void microros_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Stage 1: blink = task alive */
    blink_error(1);

    /* Stage 2: UART transport (skip custom allocator — use malloc/free
     * overrides in microros_allocators.c instead) */
    if (rmw_uros_set_custom_transport(
            true,
            (void *)&huart6,  /* USART6 → ESP32-S3 WiFi bridge */
            cubemx_transport_open,
            cubemx_transport_close,
            cubemx_transport_write,
            cubemx_transport_read) != RMW_RET_OK) {
        for (;;) blink_error(3);
    }
    blink_error(2);  /* 2 blinks = transport OK */

    /* Stage 4: Wait for agent — LD2 rapid blink */
    while (rmw_uros_ping_agent(1000, 5) != RCL_RET_OK) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    blink_error(4);  /* 4 blinks = agent connected */

    /* Stage 5: Init micro-ROS node (shared with publisher tasks) */
    rcl_allocator_t allocator = rcl_get_default_allocator();
    static rclc_support_t support;
    rcl_publisher_t pub;
    std_msgs__msg__Int32 msg;

    if (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK) {
        for (;;) blink_error(5);
    }
    if (rclc_node_init_default(&g_ros_node, "auro_stm32", "auro", &support) != RCL_RET_OK) {
        for (;;) blink_error(6);
    }
    if (rclc_publisher_init_best_effort(&pub, &g_ros_node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
            "/auro/heartbeat") != RCL_RET_OK) {
        for (;;) blink_error(7);
    }

    /* Signal publisher tasks that node is ready */
    g_ros_ready = 1U;

    extern void ros_imu_task(void *arg);
    extern void ros_odom_task(void *arg);
    extern void ros_diag_task(void *arg);
    xTaskCreate(ros_imu_task,  "imu",  1536, NULL, 3, NULL);
    xTaskCreate(ros_odom_task, "odom", 1536, NULL, 2, NULL);
    xTaskCreate(ros_diag_task, "diag", 1024, NULL, 1, NULL);

    blink_error(5);  /* 5 blinks = all publishers ready */

    msg.data = 0;
    for (;;) {
        msg.data++;
        rcl_ret_t pub_rc __attribute__((unused)) =
            rcl_publish(&pub, &msg, NULL);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif /* MICROROS_ENABLED */

/* ── App init — called from main() ──────────────────────────── */

void app_freertos_init(void)
{
    /* Heartbeat MUST start first — provides visual feedback even if
     * micro-ROS task crashes during init. */
    xTaskCreate(heartbeat_task, "heartbeat", 256, NULL, 1, NULL);

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
