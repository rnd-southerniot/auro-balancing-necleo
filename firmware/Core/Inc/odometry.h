/**
 * @file odometry.h
 * @brief Differential drive odometry — pose estimation from encoder deltas.
 */

#ifndef ODOMETRY_H
#define ODOMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    float x_m;
    float y_m;
    float theta_rad;
    float v_m_s;
    float w_rad_s;
} Pose_t;

extern volatile Pose_t g_pose;

void Odometry_Init(float wheel_diam_m, float track_m,
                   uint32_t cpr_left, uint32_t cpr_right);
void Odometry_Update(int32_t delta_left, int32_t delta_right, float dt_s);
void Odometry_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ODOMETRY_H */
