/**
 * rgb_led.c — RGB LED state machine (common anode: LOW = ON)
 *   Red:   PB2  (CN10 pin 22)
 *   Green: PB14 (CN10 pin 28)
 *   Blue:  PB15 (CN10 pin 26)
 */

#include "rgb_led.h"

#define R_PORT GPIOB
#define R_PIN  GPIO_PIN_12  /* PB12 — CN10 pin 16 */
#define G_PORT GPIOB
#define G_PIN  GPIO_PIN_14
#define B_PORT GPIOB
#define B_PIN  GPIO_PIN_15

/* Common cathode: SET = ON, RESET = OFF */
#define LED_ON(p,n)  HAL_GPIO_WritePin(p, n, GPIO_PIN_SET)
#define LED_OFF(p,n) HAL_GPIO_WritePin(p, n, GPIO_PIN_RESET)

static volatile RGB_State_t g_state = RGB_OFF;
static volatile uint8_t     g_tick  = 0;

static void set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    r ? LED_ON(R_PORT, R_PIN) : LED_OFF(R_PORT, R_PIN);
    g ? LED_ON(G_PORT, G_PIN) : LED_OFF(G_PORT, G_PIN);
    b ? LED_ON(B_PORT, B_PIN) : LED_OFF(B_PORT, B_PIN);
}

void RGB_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin   = R_PIN | G_PIN | B_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    set_rgb(0, 0, 0);  /* all off */
    g_state = RGB_OFF;
}

void RGB_SetState(RGB_State_t state) { g_state = state; g_tick = 0; }
RGB_State_t RGB_GetState(void)       { return g_state; }

const char *RGB_StateName(RGB_State_t s)
{
    switch (s) {
        case RGB_OFF:         return "OFF";
        case RGB_WHITE_BLINK: return "WHITE_BLINK";
        case RGB_BLUE_BLINK:  return "BLUE_BLINK";
        case RGB_BLUE_SOLID:  return "BLUE_SOLID";
        case RGB_GREEN_BLINK: return "GREEN_BLINK";
        case RGB_GREEN_SOLID: return "GREEN_SOLID";
        case RGB_RED_SOLID:   return "RED_SOLID";
        default:              return "?";
    }
}

void RGB_Tick(void)  /* call every 50ms */
{
    g_tick++;
    uint8_t slow = (g_tick % 10) < 5;   /* 500ms on/off */
    uint8_t fast = (g_tick % 4)  < 2;   /* 200ms on/off */

    switch (g_state) {
    case RGB_OFF:         set_rgb(0,0,0); break;
    case RGB_WHITE_BLINK: set_rgb(fast,fast,fast); break;
    case RGB_BLUE_BLINK:  set_rgb(0,0,slow); break;
    case RGB_BLUE_SOLID:  set_rgb(0,0,1); break;
    case RGB_GREEN_BLINK: set_rgb(0,fast,0); break;
    case RGB_GREEN_SOLID: set_rgb(0,1,0); break;
    case RGB_RED_SOLID:   set_rgb(1,0,0); break;
    default:              set_rgb(0,0,0); break;
    }
}
