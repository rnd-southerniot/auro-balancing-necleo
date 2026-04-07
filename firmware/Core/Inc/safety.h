/**
 * @file safety.h
 * @brief Fault monitoring and emergency stop logic (Motor A).
 */

#ifndef SAFETY_H
#define SAFETY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    FAULT_NONE        = 0x00,
    FAULT_OVERCURRENT = 0x01,
    FAULT_OVERSPEED   = 0x02,
    FAULT_LOW_BATT    = 0x04,
    FAULT_OVERVOLT    = 0x08,
    FAULT_COMMS_LOSS  = 0x10,
    FAULT_STALL       = 0x20,
} FaultFlags_t;

void    Safety_Init(void);
void    Safety_Tick(float rpm, float current_ma,
                    float batt_v, uint16_t pwm_duty,
                    uint32_t last_cmd_ms, uint32_t now_ms);
uint8_t Safety_GetFaults(void);
void    Safety_ClearFaults(void);
uint8_t Safety_IsSafe(void);   /* returns 1 if FAULT_NONE */

#ifdef __cplusplus
}
#endif

#endif /* SAFETY_H */
