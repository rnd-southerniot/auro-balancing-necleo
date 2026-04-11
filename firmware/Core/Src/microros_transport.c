/**
 * @file microros_transport.c
 * @brief UART serial transport for micro-ROS (USART2, 921600 baud).
 *
 * Uses existing HW-validated VCP wiring: PA2=TX, PA3=RX.
 * USART2 NVIC priority = 0 (highest) — preserved from HW-BUG-02 fix.
 */

#include "main.h"
#include <uxr/client/transport.h>

extern UART_HandleTypeDef huart2;

bool cubemx_transport_open(struct uxrCustomTransport *transport)
{
    (void)transport;
    return true;  /* huart2 already initialized by MX_USART2_UART_Init */
}

bool cubemx_transport_close(struct uxrCustomTransport *transport)
{
    (void)transport;
    return true;
}

size_t cubemx_transport_write(struct uxrCustomTransport *transport,
                               const uint8_t *buf, size_t len,
                               uint8_t *errcode)
{
    (void)transport;
    HAL_StatusTypeDef status =
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, 100);
    *errcode = (status == HAL_OK) ? 0U : 1U;
    return (status == HAL_OK) ? len : 0;
}

size_t cubemx_transport_read(struct uxrCustomTransport *transport,
                              uint8_t *buf, size_t len,
                              int timeout_ms, uint8_t *errcode)
{
    (void)transport;
    HAL_StatusTypeDef status =
        HAL_UART_Receive(&huart2, buf, (uint16_t)len, (uint32_t)timeout_ms);
    *errcode = (status == HAL_OK) ? 0U : 1U;
    return (status == HAL_OK) ? len : 0;
}
