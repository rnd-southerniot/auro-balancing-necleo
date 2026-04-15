#pragma once
/**
 * rgb_led.h — RGB LED state machine for micro-ROS debug
 *
 * Common anode RGB LED: 3.3V on common, LOW = ON per channel.
 *   Red:   PB12 (CN10 pin 16) — 220R
 *   Green: PB14 (CN10 pin 28) — 220R
 *   Blue:  PB15 (CN10 pin 26) — 220R
 */

#include "stm32f4xx_hal.h"

typedef enum {
    RGB_OFF         = 0,
    RGB_WHITE_BLINK = 1,  /* FreeRTOS started, micro-ROS not init */
    RGB_BLUE_BLINK  = 2,  /* waiting for agent (ping loop) */
    RGB_BLUE_SOLID  = 3,  /* agent connected, session established */
    RGB_GREEN_BLINK = 4,  /* topics publishing, DDS data flowing */
    RGB_GREEN_SOLID = 5,  /* /cmd_vel received, motors active */
    RGB_RED_SOLID   = 6,  /* FAULT */
} RGB_State_t;

void        RGB_Init(void);
void        RGB_SetState(RGB_State_t state);
void        RGB_Tick(void);   /* call every 50ms */
RGB_State_t RGB_GetState(void);
const char *RGB_StateName(RGB_State_t s);
