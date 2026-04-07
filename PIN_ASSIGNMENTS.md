# Pin Assignments — auro-balancing-necleo
## NUCLEO-F401RE (STM32F401RET6) - Complete Reference

> Single source of truth for all GPIO assignments.
> All values cross-checked against RM0368 Rev5 and UM1724 Rev17.

---

## CN7 — ST Morpho Left Connector

| Pin | Signal | STM32 | Timer/AF | Project Use | Status |
|-----|--------|-------|---------|-------------|--------|
| 1  | PC10 | PC10 | GPIO | **Motor A EN** | PROJ |
| 2  | PC11 | PC11 | GPIO | **Motor B EN** | PROJ |
| 3  | PC12 | PC12 | UART5_TX/SPI3_MOSI | — | AVAIL |
| 4  | PD2  | PD2  | UART5_RX | — | AVAIL |
| 5  | VDD  | —    | 3.3V MCU | Power | PWR |
| 6  | E5V  | —    | 5V ext input | Power | PWR |
| 7  | BOOT0 | —   | Boot mode | LOW default | WARN |
| 8  | GND  | —    | Ground | GND | PWR |
| 13 | PA13 | PA13 | SWDIO | ST-LINK debug | SYS |
| 14 | RESET | NRST | MCU reset | — | SYS |
| 15 | PA14 | PA14 | SWCLK | ST-LINK clock | SYS |
| 16 | +3.3V | — | 3.3V out | — | PWR |
| 18 | +5V  | —  | 5V out | — | PWR |
| 21 | PB7  | PB7 | TIM4_CH2 AF2 | **Motor B PWM CH2** | PROJ |
| 23 | PC13 | PC13 | GPIO | **USER Button B1 (active LOW)** | PROJ |
| 24 | VIN  | —   | 7-12V in | — | PWR |
| 28 | PA0  | PA0 | TIM2_CH1 AF1 | **Encoder A CH1** | PROJ |
| 30 | PA1  | PA1 | TIM2_CH2 AF1 | **Encoder A CH2** | PROJ |
| 32 | PA4  | PA4 | ADC1_IN4 | — | AVAIL |
| 37 | PC3  | PC3 | ADC1_IN13 | **Current sense A** | PROJ |

---

## CN10 — ST Morpho Right Connector

| Pin | Signal | STM32 | Timer/AF | Project Use | Status |
|-----|--------|-------|---------|-------------|--------|
| 3  | PB8  | PB8 | I2C1_SCL AF4 | — | AVAIL |
| 4  | PC6  | PC6 | TIM3_CH1 AF2 | — | AVAIL |
| 5  | PB9  | PB9 | I2C1_SDA AF4 | — | AVAIL |
| 6  | PC5  | PC5 | ADC1_IN15 | — | AVAIL |
| 8  | U5V  | —   | ST-LINK 5V | — | PWR |
| 11 | PA5  | PA5 | TIM2_CH1 AF1 | **Heartbeat LED (LD2)** | PROJ |
| 12 | PA12 | PA12 | USB_DP | — | AVAIL |
| 13 | PA6  | PA6 | TIM3_CH1 AF2 | **Encoder B CH1** | PROJ |
| 14 | PA11 | PA11 | USB_DM | — | AVAIL |
| 15 | PA7  | PA7 | TIM3_CH2 AF2 | **Encoder B CH2** | PROJ |
| 16 | PB12 | PB12 | SPI2_NSS | — | AVAIL |
| 17 | PB6  | PB6 | TIM4_CH1 AF2 | **Motor B PWM CH1** | PROJ |
| 19 | PC7  | PC7 | TIM3_CH2 AF2 | — | AVAIL |
| 21 | PA9  | PA9 | TIM1_CH2 AF1 | **Motor A PWM CH2** | PROJ |
| 22 | PB2  | PB2 | GPIO | **Fault LED** | PROJ |
| 23 | PA8  | PA8 | TIM1_CH1 AF1 | **Motor A PWM CH1** | PROJ |
| 24 | PB1  | PB1 | ADC1_IN9 | **Battery monitor ADC** | PROJ |
| 25 | PB10 | PB10 | TIM2_CH3/I2C2_SCL | — | AVAIL |
| 27 | PB4  | PB4 | TIM3_CH1/SPI1_MISO | — | AVAIL |
| 29 | PB5  | PB5 | TIM3_CH2/SPI1_MOSI | — | AVAIL |
| 31 | PB3  | PB3 | **SWO (SB15 ON)** | **DO NOT USE — SWO active** | SYS |
| 33 | PA10 | PA10 | USART1_RX/TIM1_CH3 | — | AVAIL |
| 34 | PC4  | PC4 | ADC1_IN14 | **Current sense B** | PROJ |
| 35 | PA2  | PA2 | USART2_TX AF7 | **VCP TX (SB13)** | SYS |
| 37 | PA3  | PA3 | USART2_RX AF7 | **VCP RX (SB14)** | SYS |

---

## Peripheral Assignment Summary

| Peripheral | Pins | Timer | Function |
|-----------|------|-------|---------|
| Motor A PWM | PA8, PA9 | TIM1 CH1/CH2 AF1 | 20kHz H-bridge |
| Motor B PWM | PB6, PB7 | TIM4 CH1/CH2 AF2 | 20kHz H-bridge |
| Motor A EN | PC10 | GPIO output | H-bridge enable |
| Motor B EN | PC11 | GPIO output | H-bridge enable |
| Encoder A | PA0, PA1 | TIM2 CH1/CH2 AF1 | MG513 quadrature |
| Encoder B | PA6, PA7 | TIM3 CH1/CH2 AF2 | MG513 quadrature |
| Control loop ISR | — | TIM10 APB2 84MHz | 1kHz PID tick |
| Telemetry ISR | — | TIM11 APB2 84MHz | 50Hz telem tick |
| Battery ADC | PB1 | ADC1_IN9 | Analog DMA scan |
| Current A ADC | PC3 | ADC1_IN13 | Analog DMA scan |
| Current B ADC | PC4 | ADC1_IN14 | Analog DMA scan |
| VCP UART | PA2, PA3 | USART2 AF7 | 921600 baud debug |
| User Button | PC13 | GPIO input | Active LOW (B1) |
| Heartbeat LED | PA5 | GPIO output | LD2 green, 1Hz toggle |
| Fault LED | PB2 | GPIO output | Red, set on fault |

---

## Legend

- **PROJ** — Active in this project
- **SYS** — System reserved, do not use
- **WARN** — Conditional / solder bridge dependent
- **AVAIL** — Free for expansion
- **PWR** — Power / GND rail

---

## Key Constraint: PB3

PB3 is **permanently reserved** on this board configuration.
SB15 routes SWO debug trace to PB3. SB15 is ON by default and must remain ON
to preserve SWD/SWO debug capability.

Encoder A CH2 uses **PA1 (TIM2_CH2 AF1)** — same timer/channel/AF as PB3 would
provide, no functional difference.

---

*UM1724 Rev17 - RM0368 Rev5 - rnd-southerniot/auro-balancing-necleo*
