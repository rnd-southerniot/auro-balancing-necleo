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

static void microros_task(void *arg)
{
    (void)arg;

    /* Custom FreeRTOS allocator for micro-ROS */
    rcl_allocator_t freertos_alloc = rcutils_get_zero_initialized_allocator();
    freertos_alloc.allocate      = microros_allocate;
    freertos_alloc.deallocate    = microros_deallocate;
    freertos_alloc.reallocate    = microros_reallocate;
    freertos_alloc.zero_allocate = microros_zero_allocate;
    bool alloc_ok __attribute__((unused)) =
        rcutils_set_default_allocator(&freertos_alloc);

    /* UART transport over USART2 (existing VCP, 921600 baud) */
    rmw_uros_set_custom_transport(
        true,
        (void *)&huart2,
        cubemx_transport_open,
        cubemx_transport_close,
        cubemx_transport_write,
        cubemx_transport_read
    );

    /* Wait for micro-ROS agent (LD2 blink = waiting) */
    while (rmw_uros_ping_agent(500, 3) != RCL_RET_OK) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* Agent connected — init micro-ROS */
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rcl_node_t node;
    rcl_publisher_t pub;
    std_msgs__msg__Int32 msg;

    rclc_support_init(&support, 0, NULL, &allocator);
    rclc_node_init_default(&node, "auro_stm32", "auro", &support);
    rclc_publisher_init_default(&pub, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "/auro/heartbeat");

    msg.data = 0;

    /* Publish heartbeat at 1Hz */
    for (;;) {
        msg.data++;
        rcl_ret_t pub_rc __attribute__((unused)) =
            rcl_publish(&pub, &msg, NULL);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif /* MICROROS_ENABLED */

/* ── App init — called from main() ──────────────────────────── */

void app_freertos_init(void)
{
    /* Heartbeat + watchdog task (low priority) */
    xTaskCreate(heartbeat_task, "heartbeat", 256, NULL, 1, NULL);

#if defined(MICROROS_ENABLED)
    /* micro-ROS task (needs large stack for RMW + transport buffers) */
    xTaskCreate(microros_task, "microros", 4096, NULL, 2, NULL);
#endif

    /* Start scheduler — does not return */
    vTaskStartScheduler();
}
