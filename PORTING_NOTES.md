# Porting Notes — auro-balancing-necleo
## STM32F429ZIT6 Discovery → STM32F401RET6 NUCLEO-F401RE

**Source repo:** https://github.com/rnd-southerniot/auro-balancing-bot
**Target repo:** https://github.com/rnd-southerniot/auro-balancing-necleo
**Original MCU:** STM32F429ZIT6 · 168MHz · 2MB Flash · 256KB SRAM · LQFP176
**Target MCU:**   STM32F401RET6 · 84MHz  · 512KB Flash · 96KB SRAM · LQFP64

---

## 1. MCU Capability Comparison

| Parameter | F429ZI Discovery | F401RE NUCLEO |
|-----------|-----------------|---------------|
| CPU | Cortex-M4 168MHz | Cortex-M4 84MHz |
| Flash | 2MB | 512KB |
| SRAM | 256KB (192K+64K CCM) | 96KB |
| GPIO ports | PA-PI (176-pin) | PA, PB, PC only (64-pin) |
| FPU | Yes | Yes |
| TIM6/TIM7 | Yes | **No** → TIM10/TIM11 |
| ADC3 | Yes | **No** → ADC1 only |
| LTDC/DMA2D | Yes | No (not used) |
| USB FS | Yes | Yes |

---

## 2. Clock Change

| Parameter | F429ZI | F401RE |
|-----------|--------|--------|
| SYSCLK | 168 MHz | 84 MHz |
| PLL source | HSI 16MHz | HSI 16MHz |
| PLL config | M=16 N=336 P=2 | M=16 N=336 P=4 |
| AHB | 168 MHz | 84 MHz |
| APB1 | 42 MHz (timers ×2 = 84 MHz) | 42 MHz (timers ×2 = 84 MHz) |
| APB2 | 84 MHz (timers ×2 = 168 MHz) | 84 MHz (timers ×1 = 84 MHz) |
| Flash latency | 5 WS | 2 WS |
| Voltage scale | Scale 1 | Scale 2 (84MHz max) |

---

## 3. Complete Pin Remap Decision Log

### 3.1 Pins Remapped (physical rewire required)

| Peripheral | F429ZI Pin | Port | F401RE Pin | Port | Connector | Reason |
|-----------|-----------|------|-----------|------|-----------|--------|
| Motor A PWM CH1 (TIM1_CH1) | PE9 | GPIOE | PA8 | GPIOA | CN10 pin 23 | GPIOE not in LQFP64 |
| Motor A PWM CH2 (TIM1_CH2) | PE11 | GPIOE | PA9 | GPIOA | CN10 pin 21 | GPIOE not in LQFP64 |
| Motor A EN | PD2 | GPIOD | PC10 | GPIOC | CN7 pin 1 | GPIOD not in LQFP64 |
| Motor B PWM CH1 (TIM4_CH1) | PD12 | GPIOD | PB6 | GPIOB | CN10 pin 17 | GPIOD not in LQFP64 |
| Motor B EN | PG9 | GPIOG | PC11 | GPIOC | CN7 pin 2 | GPIOG not in LQFP64 |
| Current sense B (ADC) | PF6 (ADC3_IN4) | GPIOF | PC4 (ADC1_IN14) | GPIOC | CN10 pin 34 | GPIOF not in LQFP64 + no ADC3 |
| Status LED | PG13 | GPIOG | PA5 (LD2) | GPIOA | CN10 pin 11 | GPIOG not in LQFP64 → NUCLEO LD2 |
| Fault LED | PG14 | GPIOG | PB2 | GPIOB | CN10 pin 22 | GPIOG not in LQFP64 |
| Encoder A CH1 (TIM2_CH1) | PA5 | GPIOA | PA0 | GPIOA | CN7 pin 28 | PA5 = LD2 on NUCLEO (SB21) |
| Battery ADC | PA4 (ADC12_IN4) | GPIOA | PB1 (ADC1_IN9) | GPIOB | CN10 pin 24 | New divider R1=100k R2=33k |
| Encoder B CH1 (TIM3_CH1) | PC6 | GPIOC | PA6 | GPIOA | CN10 pin 13 | Free up PC6/7 |
| Encoder B CH2 (TIM3_CH2) | PC7 | GPIOC | PA7 | GPIOA | CN10 pin 15 | Consistent with CH1 |
| UART TX | PA9 (USART1) | GPIOA | PA2 (USART2) | GPIOA | VCP SB13 | NUCLEO VCP = USART2 |
| UART RX | PA10 (USART1) | GPIOA | PA3 (USART2) | GPIOA | VCP SB14 | NUCLEO VCP = USART2 |
| E-stop button | PA0 (active HIGH) | GPIOA | PC13 (active LOW) | GPIOC | B1 on-board | Polarity inverted in code |

### 3.2 Pins Preserved Identical (no rewire)

| Peripheral | Pin | Timer/AF | NUCLEO Connector | Notes |
|-----------|-----|---------|-----------------|-------|
| Encoder A CH2 (TIM2_CH2) | PA1 | AF1 | CN7 pin 30 | Same on both boards |
| Motor A Current (ADC1_IN13) | PC3 | analog | CN7 pin 37 | Same on both boards |
| Motor B PWM CH2 (TIM4_CH2) | PB7 | AF2 | CN7 pin 21 | Same on both boards |

### 3.3 PB3 / SWO Note

PB3 is **not used** in this port. On the F429ZI source, Encoder A used PA5/PA1
(not PB3). PB3 carries SWO debug trace via SB15 on NUCLEO MB1136 (default ON).
SB15 should remain ON to preserve SWD/SWO debug capability.

### 3.4 Timer Changes

| Timer | F429ZI use | F401RE replacement | Reason |
|-------|-----------|-------------------|--------|
| TIM6 | PID loop ISR (1 kHz) | **TIM10** | F401RE has no TIM6 |
| TIM7 | Telemetry ISR (50 Hz) | **TIM11** | F401RE has no TIM7 |
| TIM1 | Motor A PWM | TIM1 (same) | PA8/PA9 alternate mapping |
| TIM2 | Encoder A (32-bit) | TIM2 (same) | PA0/PA1 mapping |
| TIM3 | Encoder B | TIM3 (same) | PA6/PA7 alternate mapping |
| TIM4 | Motor B PWM | TIM4 (same) | PB6/PB7 alternate mapping |

Timer prescaler/ARR values unchanged for PID/telem (both see 84 MHz timer clock).

### 3.5 ADC Changes

| Channel | F429ZI | F401RE |
|---------|--------|--------|
| Motor A current | ADC1_IN13 (PC3) | ADC1_IN13 (PC3) — identical |
| Battery monitor | ADC1_IN4 (PA4) | **ADC1_IN9 (PB1)** — new divider |
| Motor B current | ADC3_IN4 (PF6) | **ADC1_IN14 (PC4)** — ADC3 not available |
| Scan mode | ADC1 + ADC3 separate | ADC1 3-ch DMA scan (merged) |
| DMA | DMA2_S0 + polled ADC3 | DMA2_S0 Ch0 (ADC1 only) |

DMA buffer: `g_adc_dma_buf[0]`=CT_A, `[1]`=battery, `[2]`=CT_B.

### 3.6 UART DMA Reassignment

- F429ZI: USART1 TX DMA = DMA2 Stream7 Channel4
- F401RE: USART2 TX DMA = DMA1 Stream6 Channel4

### 3.7 Peripherals Removed (F429-only)

| Peripheral | Status |
|-----------|--------|
| LTDC / DMA2D | Not used in project — removed from build |
| FMC / SDRAM | Not used in project — removed from build |
| ADC3 | Merged into ADC1 3-ch DMA scan |

---

## 4. PWM Timer ARR Recalculation

```
F429ZI TIM1 (APB2 timer = 168MHz): ARR = (168,000,000 / 200,000) - 1 = 839  → 200 kHz
F401RE TIM1 (APB2 timer = 84MHz):  ARR = (84,000,000  / 20,000)  - 1 = 4199 → 20 kHz

F429ZI TIM4 (APB1 timer = 84MHz):  ARR = 839 → 100 kHz
F401RE TIM4 (APB1 timer = 84MHz):  ARR = 4199 → 20 kHz

Both motors now at identical 20 kHz PWM (above audible range).
PWM_MAX_DUTY: 820 → 4115 (98% of ARR).
```

---

## 5. Control Loop Timer

```
TIM10 on APB2 (84MHz): PSC=83, ARR=999 → 84,000,000 / 84 / 1000 = 1000 Hz
dt = 0.001 s (unchanged) — PID gains valid as-is.
```

---

## 6. Solder Bridge State (NUCLEO-F401RE MB1136)

| Bridge | Required state | Effect |
|--------|---------------|--------|
| SB13 | ON (default) | PA2 = USART2_TX → ST-LINK VCP |
| SB14 | ON (default) | PA3 = USART2_RX → ST-LINK VCP |
| SB62 | OPEN (default) | PA2 NOT on Arduino D1 |
| SB63 | OPEN (default) | PA3 NOT on Arduino D0 |
| SB15 | ON (default) | PB3 = SWO — do not use PB3 for GPIO |
| SB21 | ON (default) | PA5 = LD2 LED (shared with ENC_A_CH1) |
| SB17 | ON (default) | PC13 = USER button |

---

## 7. Known Hardware Risks

| Risk | Severity | Mitigation |
|------|----------|-----------|
| 12.6V LiPo into DBH-12V (rated 12V) | HIGH | Add TVS diode or limit charge to 11.1V (3S nominal) |
| Encoder lines at 5V into 3.3V GPIO | HIGH | 74LVC245 or 1k/2k resistor divider on all 4 lines |
| PA5 dual-use: LD2 + ENC_A_CH1 | LOW | LED blinks with encoder — cosmetic only |
| User button polarity inverted | LOW | Handled in firmware: GPIO_PIN_RESET = pressed |
| PID gains may need re-tuning at 84MHz | MEDIUM | dt preserved; gains identical as starting point |
| SRAM: 96KB vs 256KB | LOW | 37KB used — 59KB headroom |

---

## 8. Memory Layout

| Region | F429ZI | F401RE |
|--------|--------|--------|
| FLASH | 0x08000000, 2048K | 0x08000000, 512K |
| RAM | 0x20000000, 192K | 0x20000000, 96K |
| CCMRAM | 0x10000000, 64K | N/A |

Firmware size: ~37 KB text, ~4 KB data+bss — fits comfortably.

---

## 9. Build System Changes

- Linker script: `STM32F429ZITX_FLASH.ld` → `STM32F401RETX_FLASH.ld`
- Startup: `startup_stm32f429zitx.s` → `startup_stm32f401retx.s`
- Define: `STM32F429xx` → `STM32F401xE`
- Toolchain comment: F429ZI → F401RE

---

*RM0368 Rev5 · UM1724 Rev17 · rnd-southerniot/auro-balancing-necleo*
