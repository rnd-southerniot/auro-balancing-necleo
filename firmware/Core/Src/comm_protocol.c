/**
 * @file comm_protocol.c
 * @brief Binary frame encode/decode and CRC16-CCITT implementation.
 */

#include "comm_protocol.h"
#include <string.h>

/* ── CRC16-CCITT ───────────────────────────────────────────── */

uint16_t Comm_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0U; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8U;
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (crc << 1U) ^ 0x1021U;
            } else {
                crc = crc << 1U;
            }
        }
    }

    return crc;
}

/* ── Receive state machine ─────────────────────────────────── */

void Comm_RxInit(CommRx_t *rx)
{
    memset(rx, 0, sizeof(CommRx_t));
    rx->state = RX_WAIT_SYNC;
}

bool Comm_ReceiveByte(CommRx_t *rx, uint8_t byte)
{
    switch (rx->state) {

    case RX_WAIT_SYNC:
        if (byte == FRAME_SYNC) {
            rx->state = RX_WAIT_VERSION;
        }
        break;

    case RX_WAIT_VERSION:
        rx->version = byte;
        rx->state = RX_WAIT_TYPE;
        break;

    case RX_WAIT_TYPE:
        rx->msg_type    = byte;
        rx->payload_len = Comm_GetPayloadLength(byte);
        rx->payload_idx = 0U;

        if (rx->payload_len > COMM_MAX_PAYLOAD) {
            rx->state = RX_WAIT_SYNC;
        } else if (rx->payload_len == 0U) {
            rx->state = RX_WAIT_CRC_LO;
        } else {
            rx->state = RX_WAIT_PAYLOAD;
        }
        break;

    case RX_WAIT_PAYLOAD:
        rx->payload[rx->payload_idx] = byte;
        rx->payload_idx++;
        if (rx->payload_idx >= rx->payload_len) {
            rx->state = RX_WAIT_CRC_LO;
        }
        break;

    case RX_WAIT_CRC_LO:
        rx->crc_received = (uint16_t)byte;
        rx->state = RX_WAIT_CRC_HI;
        break;

    case RX_WAIT_CRC_HI: {
        rx->crc_received |= (uint16_t)byte << 8U;

        uint8_t crc_buf[2U + COMM_MAX_PAYLOAD];
        crc_buf[0] = rx->version;
        crc_buf[1] = rx->msg_type;
        if (rx->payload_len > 0U) {
            memcpy(&crc_buf[2], rx->payload, rx->payload_len);
        }

        uint16_t crc_calc = Comm_CRC16(crc_buf,
                                        (uint16_t)(2U + rx->payload_len));

        rx->state = RX_WAIT_SYNC;

        if (crc_calc == rx->crc_received) {
            return true;
        }
        break;
    }

    default:
        rx->state = RX_WAIT_SYNC;
        break;
    }

    return false;
}

/* ── Frame encoder ─────────────────────────────────────────── */

uint16_t Comm_EncodeFrame(uint8_t *buf, uint8_t msg_type,
                          const uint8_t *payload, uint8_t payload_len)
{
    uint16_t idx = 0U;

    buf[idx++] = FRAME_SYNC;
    buf[idx++] = FRAME_VERSION;
    buf[idx++] = msg_type;

    if (payload_len > 0U && payload != NULL) {
        memcpy(&buf[idx], payload, payload_len);
        idx += payload_len;
    }

    /* CRC over version + msg_type + payload */
    uint16_t crc = Comm_CRC16(&buf[1], (uint16_t)(idx - 1U));

    buf[idx++] = (uint8_t)(crc & 0xFFU);
    buf[idx++] = (uint8_t)((crc >> 8U) & 0xFFU);

    return idx;
}

/* ── Payload length lookup ─────────────────────────────────── */

uint8_t Comm_GetPayloadLength(uint8_t msg_type)
{
    switch (msg_type) {
    case MSG_TELEM_FAST:         return PAYLOAD_LEN_TELEM_FAST;
    case MSG_TELEM_IMU:          return PAYLOAD_LEN_TELEM_IMU;
    case MSG_TELEM_POSE:         return PAYLOAD_LEN_TELEM_POSE;
    case MSG_CMD_RPM:            return PAYLOAD_LEN_CMD_RPM;
    case MSG_CMD_POSITION:       return PAYLOAD_LEN_CMD_POSITION;
    case MSG_CMD_GAINS:          return PAYLOAD_LEN_CMD_GAINS;
    case MSG_CMD_MODE:           return PAYLOAD_LEN_CMD_MODE;
    case MSG_CMD_ESTOP:          return PAYLOAD_LEN_CMD_ESTOP;
    case MSG_CMD_AUTOTUNE_START: return PAYLOAD_LEN_CMD_AUTOTUNE_START;
    case MSG_CMD_AUTOTUNE_ABORT: return PAYLOAD_LEN_CMD_AUTOTUNE_ABORT;
    case MSG_CMD_KEEPALIVE:      return PAYLOAD_LEN_CMD_KEEPALIVE;
    case MSG_CMD_DIFF_DRIVE:     return PAYLOAD_LEN_CMD_DIFF_DRIVE;
    case MSG_CMD_RPM_B:            return PAYLOAD_LEN_CMD_RPM_B;
    case MSG_CMD_POSITION_B:       return PAYLOAD_LEN_CMD_POSITION_B;
    case MSG_CMD_GAINS_B:          return PAYLOAD_LEN_CMD_GAINS_B;
    case MSG_CMD_MODE_B:           return PAYLOAD_LEN_CMD_MODE_B;
    case MSG_CMD_AUTOTUNE_START_B: return PAYLOAD_LEN_CMD_AUTOTUNE_START_B;
    case MSG_RESP_ACK:           return PAYLOAD_LEN_RESP_ACK;
    case MSG_RESP_NACK:          return PAYLOAD_LEN_RESP_NACK;
    case MSG_RESP_AUTOTUNE_RESULT: return PAYLOAD_LEN_RESP_AUTOTUNE_RESULT;
    default:                     return 0U;
    }
}
