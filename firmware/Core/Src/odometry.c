/**
 * @file odometry.c
 * @brief Differential drive odometry using midpoint integration.
 *
 * Called from TIM10 ISR at 1 kHz with raw encoder count deltas.
 * Chassis: wheel=65mm, track=180mm, Motor A=left, Motor B=right.
 */

#include "odometry.h"
#include <math.h>

static float s_track_m;
static float s_m_per_count_l;
static float s_m_per_count_r;

volatile Pose_t g_pose = {0};

void Odometry_Init(float wheel_diam_m, float track_m,
                   uint32_t cpr_left, uint32_t cpr_right)
{
    float circ = 3.14159265f * wheel_diam_m;
    s_track_m       = track_m;
    s_m_per_count_l = circ / (float)cpr_left;
    s_m_per_count_r = circ / (float)cpr_right;
}

void Odometry_Update(int32_t delta_left, int32_t delta_right, float dt_s)
{
    float dl = (float)delta_left  * s_m_per_count_l;
    float dr = (float)delta_right * s_m_per_count_r;
    float dc = (dl + dr) * 0.5f;
    float dtheta = (dr - dl) / s_track_m;

    /* Midpoint integration — more accurate than Euler at 1 kHz */
    float theta_mid    = g_pose.theta_rad + dtheta * 0.5f;
    g_pose.x_m        += dc * cosf(theta_mid);
    g_pose.y_m        += dc * sinf(theta_mid);
    g_pose.theta_rad  += dtheta;

    /* Wrap heading to [-pi, pi] */
    while (g_pose.theta_rad >  3.14159265f) g_pose.theta_rad -= 6.28318530f;
    while (g_pose.theta_rad < -3.14159265f) g_pose.theta_rad += 6.28318530f;

    if (dt_s > 0.0f) {
        g_pose.v_m_s   = dc / dt_s;
        g_pose.w_rad_s = dtheta / dt_s;
    }
}

void Odometry_Reset(void)
{
    g_pose.x_m        = 0.0f;
    g_pose.y_m        = 0.0f;
    g_pose.theta_rad  = 0.0f;
    g_pose.v_m_s      = 0.0f;
    g_pose.w_rad_s    = 0.0f;
}
