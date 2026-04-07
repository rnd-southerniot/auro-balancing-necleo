/**
 * @file telemetry.c
 * @brief Telemetry packet building and UART DMA transmission (Motor A).
 *
 * BuildAndSend is called from TIM11 ISR at 50 Hz.
 */

#include "telemetry.h"
#include "comm_protocol.h"
#include "config.h"
#include "main.h"
#include <string.h>

void Telemetry_Init(Telemetry_t *telem, UART_HandleTypeDef *huart)
{
    memset(telem, 0, sizeof(Telemetry_t));
    telem->huart = huart;
}

void Telemetry_BuildAndSend(Telemetry_t *telem)
{
    if (telem->tx_busy) return;

    /* Snapshot Motor A */
    telem->timestamp_ms    = g_tick_ms;
    telem->m1_rpm          = Encoder_GetRPM(&g_enc_a);
    telem->m1_position_deg = Encoder_GetPositionDeg(&g_enc_a);
    {
        float v_mv = (float)g_adc_dma_buf[0] * (BATT_ADC_VREF * 1000.0f) / BATT_ADC_FULL_SCALE;
        float v_corr = v_mv - CT_OFFSET_MV;
        if (v_corr < 0.0f) v_corr = 0.0f;
        telem->m1_current_ma = v_corr / CT_MV_PER_AMP * 1000.0f;
    }
    telem->m1_pid_output   = (float)g_applied_duty;
    telem->m1_fault_flags  = Safety_GetFaults();
    telem->m1_mode         = (uint8_t)g_mode;

    /* Motor B */
    telem->m2_rpm          = Encoder_GetRPM(&g_enc_b);
    telem->m2_position_deg = Encoder_GetPositionDeg(&g_enc_b);
    {
        float v_mv_b = (float)g_adc_dma_buf[2] * (BATT_ADC_VREF * 1000.0f) / BATT_ADC_FULL_SCALE;
        float v_corr_b = v_mv_b - CT_OFFSET_MV;
        if (v_corr_b < 0.0f) v_corr_b = 0.0f;
        telem->m2_current_ma = v_corr_b / CT_MV_PER_AMP * 1000.0f;
    }
    telem->m2_pid_output   = (float)g_applied_duty_b;
    telem->m2_fault_flags  = 0U;  /* TODO: per-motor safety in Phase 9 */
    telem->m2_mode         = (uint8_t)g_mode_b;

    telem->batt_voltage    = (float)g_adc_dma_buf[1] * BATT_ADC_VREF * BATT_V_SCALE
                             / BATT_ADC_FULL_SCALE;

    /* Build dual-channel TELEM_FAST payload */
    PayloadTelemFast_t fast;
    fast.timestamp_ms    = telem->timestamp_ms;
    fast.m1_rpm          = telem->m1_rpm;
    fast.m1_position_deg = telem->m1_position_deg;
    fast.m1_current_ma   = telem->m1_current_ma;
    fast.m1_pid_output   = telem->m1_pid_output;
    fast.m2_rpm          = telem->m2_rpm;
    fast.m2_position_deg = telem->m2_position_deg;
    fast.m2_current_ma   = telem->m2_current_ma;
    fast.m2_pid_output   = telem->m2_pid_output;
    fast.batt_voltage    = telem->batt_voltage;
    fast.m1_fault_flags  = telem->m1_fault_flags;
    fast.m2_fault_flags  = telem->m2_fault_flags;
    fast.m1_mode         = telem->m1_mode;
    fast.m2_mode         = telem->m2_mode;

    uint16_t frame_len;

    /* Every 5th call (10 Hz), send pose instead of telem_fast */
    telem->pose_divider++;
    if (telem->pose_divider >= 5U) {
        telem->pose_divider = 0U;

        PayloadTelemPose_t pose;
        pose.timestamp_ms = telem->timestamp_ms;
        pose.x_m          = g_pose.x_m;
        pose.y_m          = g_pose.y_m;
        pose.theta_deg    = g_pose.theta_rad * (180.0f / 3.14159265f);
        pose.v_m_s        = g_pose.v_m_s;
        pose.w_deg_s      = g_pose.w_rad_s * (180.0f / 3.14159265f);

        frame_len = Comm_EncodeFrame(telem->tx_buf, MSG_TELEM_POSE,
                                     (const uint8_t *)&pose,
                                     PAYLOAD_LEN_TELEM_POSE);
    } else {
        frame_len = Comm_EncodeFrame(telem->tx_buf, MSG_TELEM_FAST,
                                     (const uint8_t *)&fast,
                                     PAYLOAD_LEN_TELEM_FAST);
    }

    if (frame_len > 0U) {
        telem->tx_busy = 1U;
        HAL_UART_Transmit_DMA(telem->huart, telem->tx_buf, frame_len);
    }
}

void Telemetry_TxCompleteCallback(Telemetry_t *telem)
{
    telem->tx_busy = 0U;
}
