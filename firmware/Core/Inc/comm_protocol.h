/**
 * @file comm_protocol.h
 * @brief Binary frame definitions for STM32 <-> host communication.
 *
 * Frame structure (little-endian, on the wire):
 *
 *   [0xAA][VERSION=0x01][MSG_TYPE][PAYLOAD...][CRC16_LO][CRC16_HI]
 *
 * CRC16-CCITT (init = 0xFFFF, poly = 0x1021, no reflect, no final XOR)
 * computed over VERSION + MSG_TYPE + PAYLOAD (excludes sync byte).
 */

#ifndef COMM_PROTOCOL_H
#define COMM_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/* ── Message type codes ────────────────────────────────────── */
#define MSG_TELEM_FAST           0x10U  /* STM32 -> host, 50 Hz       */
#define MSG_TELEM_IMU            0x11U  /* STM32 -> host, 50 Hz       */
#define MSG_TELEM_POSE           0x12U  /* STM32 -> host, 10 Hz       */
#define MSG_CMD_RPM              0x20U  /* host -> STM32               */
#define MSG_CMD_POSITION         0x21U  /* host -> STM32               */
#define MSG_CMD_GAINS            0x22U  /* host -> STM32               */
#define MSG_CMD_MODE             0x23U  /* host -> STM32               */
#define MSG_CMD_ESTOP            0x24U  /* host -> STM32               */
#define MSG_CMD_AUTOTUNE_START   0x25U  /* host -> STM32               */
#define MSG_CMD_AUTOTUNE_ABORT   0x26U  /* host -> STM32               */
#define MSG_CMD_KEEPALIVE        0x29U  /* host -> STM32, no payload   */
#define MSG_CMD_DIFF_DRIVE       0x2AU  /* host -> STM32, 8 bytes      */
/* Motor B commands — same payload format as Motor A */
#define MSG_CMD_RPM_B            0x40U
#define MSG_CMD_POSITION_B       0x41U
#define MSG_CMD_GAINS_B          0x42U
#define MSG_CMD_MODE_B           0x43U
#define MSG_CMD_AUTOTUNE_START_B 0x45U
#define MSG_RESP_ACK             0x30U  /* STM32 -> host               */
#define MSG_RESP_NACK            0x31U  /* STM32 -> host               */
#define MSG_RESP_AUTOTUNE_RESULT 0x32U  /* STM32 -> host               */

/* ── Max payload size ──────────────────────────────────────── */
#define COMM_MAX_PAYLOAD         48U

/* ── Frame overhead: sync(1) + version(1) + type(1) + crc(2)  */
#define COMM_FRAME_OVERHEAD      5U

/* ── Max total frame size ──────────────────────────────────── */
#define COMM_MAX_FRAME_SIZE      (COMM_MAX_PAYLOAD + COMM_FRAME_OVERHEAD)

/* ── Control mode enum ─────────────────────────────────────── */
typedef enum {
    CTRL_IDLE      = 0x00,
    CTRL_RPM       = 0x01,
    CTRL_POSITION  = 0x02,
    CTRL_AUTOTUNE  = 0x03,
    CTRL_DIFF      = 0x04,
} ControlMode_t;

/* ── NACK error codes ──────────────────────────────────────── */
typedef enum {
    NACK_UNKNOWN_MSG    = 0x01,
    NACK_BAD_PAYLOAD    = 0x02,
    NACK_FAULT_ACTIVE   = 0x03,
} NackCode_t;

/* ── Payload structures — all fields little-endian, packed ─── */
#pragma pack(push, 1)

/**
 * @brief TELEM_FAST payload — 44 bytes, sent at 50 Hz (dual channel).
 */
typedef struct {
    uint32_t timestamp_ms;      /*  4 */
    float    m1_rpm;            /*  4 */
    float    m1_position_deg;   /*  4 */
    float    m1_current_ma;     /*  4 */
    float    m1_pid_output;     /*  4 */
    float    m2_rpm;            /*  4 */
    float    m2_position_deg;   /*  4 */
    float    m2_current_ma;     /*  4 */
    float    m2_pid_output;     /*  4 */
    float    batt_voltage;      /*  4 */
    uint8_t  m1_fault_flags;    /*  1 */
    uint8_t  m2_fault_flags;    /*  1 */
    uint8_t  m1_mode;           /*  1 */
    uint8_t  m2_mode;           /*  1 */
} PayloadTelemFast_t;           /* 44 bytes */

typedef struct {
    uint32_t timestamp_ms;      /*  4 */
    float    pitch_deg;         /*  4  complementary filter output */
    float    pitch_rate_dps;    /*  4  gyro rate on pitch axis */
    float    accel_x_g;         /*  4 */
    float    accel_y_g;         /*  4 */
    float    accel_z_g;         /*  4 */
    float    gyro_x_dps;        /*  4 */
    float    gyro_y_dps;        /*  4 */
    float    gyro_z_dps;        /*  4 */
    float    temp_c;            /*  4 */
    uint8_t  imu_status;        /*  1  IMU_Status_t */
} PayloadTelemImu_t;            /* 41 bytes */

typedef struct {
    float setpoint_rpm;
} PayloadCmdRpm_t;            /* 4 bytes */

typedef struct {
    float setpoint_deg;
} PayloadCmdPosition_t;       /* 4 bytes */

typedef struct {
    float kp;
    float ki;
    float kd;
} PayloadCmdGains_t;          /* 12 bytes */

typedef struct {
    uint8_t mode;              /* ControlMode_t */
} PayloadCmdMode_t;           /* 1 byte */

typedef struct {
    uint32_t timestamp_ms;
    float    x_m;
    float    y_m;
    float    theta_deg;
    float    v_m_s;
    float    w_deg_s;
} PayloadTelemPose_t;      /* 24 bytes */

typedef struct {
    float linear;      /* -1.0 to +1.0 */
    float angular;     /* -1.0 to +1.0, positive = turn right */
} PayloadCmdDiffDrive_t;  /* 8 bytes */

/* CMD_ESTOP has no payload (0 bytes). */
/* CMD_KEEPALIVE has no payload (0 bytes). */
/* CMD_AUTOTUNE_ABORT has no payload (0 bytes). */

typedef struct {
    float setpoint_rpm;
} PayloadCmdAutotuneStart_t;  /* 4 bytes */

typedef struct {
    uint8_t acked_msg_type;
} PayloadRespAck_t;           /* 1 byte */

typedef struct {
    uint8_t nacked_msg_type;
    uint8_t error_code;        /* NackCode_t */
} PayloadRespNack_t;          /* 2 bytes */

typedef struct {
    float ku;
    float pu_s;
    float kp;
    float ki;
    float kd;
} PayloadRespAutotuneResult_t; /* 20 bytes */

#pragma pack(pop)

/* ── Compile-time payload size verification ────────────────── */
_Static_assert(sizeof(PayloadTelemFast_t) == 44,
               "TELEM_FAST payload must be 44 bytes");
_Static_assert(sizeof(PayloadTelemImu_t) == 41,
               "TELEM_IMU payload must be 41 bytes");
_Static_assert(sizeof(PayloadCmdRpm_t) == 4,
               "CMD_RPM payload must be 4 bytes");
_Static_assert(sizeof(PayloadCmdPosition_t) == 4,
               "CMD_POSITION payload must be 4 bytes");
_Static_assert(sizeof(PayloadCmdGains_t) == 12,
               "CMD_GAINS payload must be 12 bytes");
_Static_assert(sizeof(PayloadCmdMode_t) == 1,
               "CMD_MODE payload must be 1 byte");
_Static_assert(sizeof(PayloadRespAck_t) == 1,
               "RESP_ACK payload must be 1 byte");
_Static_assert(sizeof(PayloadRespNack_t) == 2,
               "RESP_NACK payload must be 2 bytes");

/* ── Payload length constants ──────────────────────────────── */
#define PAYLOAD_LEN_TELEM_FAST          44U
#define PAYLOAD_LEN_TELEM_IMU           41U
#define PAYLOAD_LEN_TELEM_POSE          24U
#define PAYLOAD_LEN_CMD_RPM              4U
#define PAYLOAD_LEN_CMD_POSITION         4U
#define PAYLOAD_LEN_CMD_GAINS           12U
#define PAYLOAD_LEN_CMD_MODE             1U
#define PAYLOAD_LEN_CMD_ESTOP            0U
#define PAYLOAD_LEN_CMD_AUTOTUNE_START   4U
#define PAYLOAD_LEN_CMD_AUTOTUNE_ABORT   0U
#define PAYLOAD_LEN_CMD_KEEPALIVE        0U
#define PAYLOAD_LEN_CMD_DIFF_DRIVE       8U
#define PAYLOAD_LEN_CMD_RPM_B            4U
#define PAYLOAD_LEN_CMD_POSITION_B       4U
#define PAYLOAD_LEN_CMD_GAINS_B         12U
#define PAYLOAD_LEN_CMD_MODE_B           1U
#define PAYLOAD_LEN_CMD_AUTOTUNE_START_B 4U
#define PAYLOAD_LEN_RESP_ACK             1U
#define PAYLOAD_LEN_RESP_NACK            2U
#define PAYLOAD_LEN_RESP_AUTOTUNE_RESULT 20U

/* ── Receive state machine ─────────────────────────────────── */
typedef enum {
    RX_WAIT_SYNC = 0,
    RX_WAIT_VERSION,
    RX_WAIT_TYPE,
    RX_WAIT_PAYLOAD,
    RX_WAIT_CRC_LO,
    RX_WAIT_CRC_HI
} CommRxState_t;

typedef struct {
    CommRxState_t state;
    uint8_t  version;
    uint8_t  msg_type;
    uint8_t  payload[COMM_MAX_PAYLOAD];
    uint8_t  payload_len;
    uint8_t  payload_idx;
    uint16_t crc_received;
} CommRx_t;

/* ── Public API ────────────────────────────────────────────── */

uint16_t Comm_CRC16(const uint8_t *data, uint16_t len);
void     Comm_RxInit(CommRx_t *rx);
bool     Comm_ReceiveByte(CommRx_t *rx, uint8_t byte);
uint16_t Comm_EncodeFrame(uint8_t *buf, uint8_t msg_type,
                          const uint8_t *payload, uint8_t payload_len);
uint8_t  Comm_GetPayloadLength(uint8_t msg_type);

#ifdef __cplusplus
}
#endif

#endif /* COMM_PROTOCOL_H */
