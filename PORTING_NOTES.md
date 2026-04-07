# Porting Notes: STM32F429ZI Discovery → NUCLEO-F401RE

## Pin Remap Decision Log

### Incompatible pins (GPIOD/E/F/G not available on F401RE LQFP64)

| # | Peripheral | F429ZI Pin | F401RE Pin | Reason | Hardware Note |
|---|-----------|-----------|-----------|--------|---------------|
| 1 | Motor A PWM CH1 | PE9 (TIM1_CH1, AF1) | PA8 (TIM1_CH1, AF1) | GPIOE N/A | Rewire H-bridge IN1 → CN10-23 |
| 2 | Motor A PWM CH2 | PE11 (TIM1_CH2, AF1) | PA9 (TIM1_CH2, AF1) | GPIOE N/A; PA9 freed by UART move | Rewire IN2 → CN10-21 |
| 3 | Motor A EN | PD2 (GPIO) | PC10 (GPIO) | GPIOD N/A | Rewire EN → CN7-1 |
| 4 | Motor B PWM CH1 | PD12 (TIM4_CH1, AF2) | PB6 (TIM4_CH1, AF2) | GPIOD N/A | Rewire IN1_B → CN10-17 |
| 5 | Motor B EN | PG9 (GPIO) | PC11 (GPIO) | GPIOG N/A | Rewire EN_B → CN7-2 |
| 6 | Motor B Current | PF6 (ADC3_IN4) | PC4 (ADC1_IN14) | GPIOF N/A + no ADC3 on F401RE | Rewire CT_B → CN10-34 |
| 7 | Heartbeat LED | PG13 (GPIO) | PA5 (LD2, SB21) | GPIOG N/A | NUCLEO built-in green LED |
| 8 | Fault LED | PG14 (GPIO) | PB2 (GPIO) | GPIOG N/A | Wire external red LED → CN10-22 |

### Board-specific remaps (functional on F401RE but different on NUCLEO)

| # | Peripheral | F429ZI Pin | F401RE Pin | Reason | Hardware Note |
|---|-----------|-----------|-----------|--------|---------------|
| 9 | Encoder A CH1 | PA5 (TIM2_CH1, AF1) | PA0 (TIM2_CH1, AF1) | PA5 = LD2 on NUCLEO | Rewire encoder → CN7-28 |
| 10 | Battery ADC | PA4 (ADC12_IN4) | PB1 (ADC1_IN9) | Avoid DAC conflict | New divider: R1=100k R2=33k → CN10-24 |
| 11 | UART TX | PA9 (USART1, AF7) | PA2 (USART2, AF7) | NUCLEO VCP = USART2 | ST-LINK USB (SB13) |
| 12 | UART RX | PA10 (USART1, AF7) | PA3 (USART2, AF7) | NUCLEO VCP = USART2 | ST-LINK USB (SB14) |
| 13 | E-stop button | PA0 (active HIGH) | PC13 (B1, active LOW) | NUCLEO B1 standard | Logic inverted in code |
| 14 | Encoder B CH1 | PC6 (TIM3_CH1, AF2) | PA6 (TIM3_CH1, AF2) | Free up PC6/7 | Rewire encoder → CN10-13 |
| 15 | Encoder B CH2 | PC7 (TIM3_CH2, AF2) | PA7 (TIM3_CH2, AF2) | Consistent with CH1 | Rewire encoder → CN10-15 |

### Pins unchanged

| Peripheral | Pin | Notes |
|-----------|-----|-------|
| Encoder A CH2 | PA1 (TIM2_CH2, AF1) | Same on both boards |
| Motor A Current | PC3 (ADC1_IN13) | Same on both boards |
| Motor B PWM CH2 | PB7 (TIM4_CH2, AF2) | Same on both boards |

## Peripheral Changes

### Timers not available on F401RE
- **TIM6** → **TIM10** (1 kHz PID tick). IRQ: `TIM1_UP_TIM10_IRQn` (shared with TIM1 update)
- **TIM7** → **TIM11** (50 Hz telemetry tick). IRQ: `TIM1_TRG_COM_TIM11_IRQn` (shared with TIM1 trigger)

Timer prescaler/ARR values unchanged (both TIM6 and TIM10 see 84 MHz APB timer clock on their respective MCUs).

### ADC consolidation
- F429ZI: ADC1 (2-ch DMA: IN13+IN4) + ADC3 (1-ch polled: IN4 on PF6)
- F401RE: ADC1 (3-ch DMA: IN13+IN9+IN14) — no ADC3 on F401RE

DMA buffer expanded from 2 to 3 entries:
- `g_adc_dma_buf[0]` = Motor A current (PC3, IN13) — unchanged
- `g_adc_dma_buf[1]` = Battery voltage (PB1, IN9) — was PA4/IN4
- `g_adc_dma_buf[2]` = Motor B current (PC4, IN14) — was polled ADC3

### UART DMA reassignment
- F429ZI: USART1 TX DMA = DMA2 Stream7 Channel4
- F401RE: USART2 TX DMA = DMA1 Stream6 Channel4

## Clock Changes

| Parameter | F429ZI | F401RE | Formula |
|-----------|--------|--------|---------|
| SYSCLK | 168 MHz | 84 MHz | HSI/PLLM*PLLN/PLLP = 16/16*336/4 |
| AHB | 168 MHz | 84 MHz | SYSCLK/1 |
| APB1 | 42 MHz | 42 MHz | AHB/4 → AHB/2 |
| APB2 | 84 MHz | 84 MHz | AHB/2 → AHB/1 |
| APB1 timer | 84 MHz | 84 MHz | APB1×2 (prescale>1) |
| APB2 timer | 168 MHz | 84 MHz | APB2×2 → APB2×1 (prescale=1) |
| Flash latency | 5 WS | 2 WS | Per RM0368 Table 6 |
| Voltage scale | Scale 1 | Scale 2 | 84 MHz ≤ Scale 2 max |

### PWM frequency change
- F429ZI: TIM1 (APB2=168MHz), ARR=839 → 200 kHz
- F401RE: TIM1 (APB2=84MHz), ARR=4199 → 20 kHz
- F429ZI: TIM4 (APB1=84MHz), ARR=839 → 100 kHz
- F401RE: TIM4 (APB1=84MHz), ARR=4199 → 20 kHz
- Both motors now run at identical 20 kHz PWM (above audible range)
- PWM_MAX_DUTY: 820 → 4115 (98% of ARR)

### PID gains
All PID gains preserved identically. `PID_DT_S = 0.001f` unchanged (1 kHz loop).
Re-tune on hardware only if motor response differs.

## Memory Layout

| Region | F429ZI | F401RE |
|--------|--------|--------|
| FLASH | 0x08000000, 2048K | 0x08000000, 512K |
| RAM | 0x20000000, 192K | 0x20000000, 96K |
| CCMRAM | 0x10000000, 64K | N/A |

Firmware size: ~37 KB text, ~4 KB data+bss — fits comfortably.

## Build System Changes
- Linker script: `STM32F429ZITX_FLASH.ld` → `STM32F401RETX_FLASH.ld`
- Startup: `startup_stm32f429zitx.s` → `startup_stm32f401retx.s`
- Define: `STM32F429xx` → `STM32F401xE`
- Toolchain comment: F429ZI → F401RE
