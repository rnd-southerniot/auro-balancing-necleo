/**
 * @file microros_transport.c
 * @brief USART6 transport for micro-ROS via ESP32-S3 WiFi bridge.
 *
 * USART6 pins (AF8):
 *   PC6 = TX (CN10 pin 4)  → ESP32-S3 GPIO17 (RX)
 *   PC7 = RX (CN10 pin 19) ← ESP32-S3 GPIO18 (TX)
 *   921600 baud
 *
 * ESP32-S3 bridges UART ↔ WiFi UDP to agent at 10.10.8.110:8888.
 * USART2 (PA2/PA3) remains as ST-LINK VCP — completely unchanged.
 */

#include "main.h"
#include <uxr/client/transport.h>

extern UART_HandleTypeDef huart6;

bool cubemx_transport_open(struct uxrCustomTransport *transport)
{
    (void)transport;
    return true;
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
        HAL_UART_Transmit(&huart6, (uint8_t *)buf, (uint16_t)len, 100);
    *errcode = (status == HAL_OK) ? 0U : 1U;
    return (status == HAL_OK) ? len : 0;
}

size_t cubemx_transport_read(struct uxrCustomTransport *transport,
                              uint8_t *buf, size_t len,
                              int timeout_ms, uint8_t *errcode)
{
    (void)transport;
    /* The stream framing layer calls read with len=1 to read byte-by-byte.
     * Use blocking HAL_UART_Receive for the full requested length. */
    HAL_StatusTypeDef status =
        HAL_UART_Receive(&huart6, buf, (uint16_t)len, (uint32_t)timeout_ms);
    *errcode = (status == HAL_OK) ? 0U : 1U;
    return (status == HAL_OK) ? len : 0;
}
