/**
 * @file microros_transport.c
 * @brief USART6 transport for micro-ROS via ESP32-S3 WiFi bridge.
 *
 * Uses a circular DMA RX buffer on USART6 so incoming bytes from the
 * ESP32 bridge are never lost during WiFi round-trip latency (~50ms).
 */

#include "main.h"
#include <uxr/client/transport.h>
#include <string.h>

extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef  hdma_usart6_rx;

/* ── Circular DMA RX buffer ──────────────────────────────────── */
#define RX_BUF_SIZE 512
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_rd_pos = 0;
static volatile uint8_t dma_started = 0;

static void start_dma_rx(void)
{
    if (!dma_started) {
        HAL_UART_Receive_DMA(&huart6, (uint8_t *)rx_buf, RX_BUF_SIZE);
        dma_started = 1;
    }
}

static uint16_t dma_write_pos(void)
{
    return RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
}

static uint16_t available(void)
{
    uint16_t wp = dma_write_pos();
    uint16_t rp = rx_rd_pos;
    if (wp >= rp) return wp - rp;
    return RX_BUF_SIZE - rp + wp;
}

static uint8_t read_byte(void)
{
    uint8_t b = rx_buf[rx_rd_pos];
    rx_rd_pos = (rx_rd_pos + 1) % RX_BUF_SIZE;
    return b;
}

/* ── Transport API ───────────────────────────────────────────── */

bool cubemx_transport_open(struct uxrCustomTransport *transport)
{
    (void)transport;
    start_dma_rx();
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
        HAL_UART_Transmit(&huart6, (uint8_t *)buf, (uint16_t)len, 10);
    *errcode = (status == HAL_OK) ? 0U : 1U;
    return (status == HAL_OK) ? len : 0;
}

size_t cubemx_transport_read(struct uxrCustomTransport *transport,
                              uint8_t *buf, size_t len,
                              int timeout_ms, uint8_t *errcode)
{
    (void)transport;
    size_t got = 0;

    /* Cap timeout: XRCE-DDS may pass large or negative values which
     * cause the busy-wait loop to hang the FreeRTOS task.
     * Keep it short (max 5ms) to avoid starving motor control. */
    if (timeout_ms <= 0 || timeout_ms > 5) {
        timeout_ms = 5;
    }
    uint32_t deadline = HAL_GetTick() + (uint32_t)timeout_ms;

    while (got < len) {
        if (HAL_GetTick() >= deadline) break;
        if (available() > 0) {
            buf[got++] = read_byte();
        }
    }

    *errcode = (got > 0) ? 0U : 1U;
    return got;
}
