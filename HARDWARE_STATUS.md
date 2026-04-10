# Hardware Validation Status — auro-balancing-necleo

## STATUS: ALL 7 PHASES PASS — HARDWARE VALIDATED

**Validated:** 2026-04-11
**Firmware:** 99b608b
**Tester:** Arif, Southern IoT Limited R&D
**Board:** NUCLEO-F401RE (MB1136)
**Project:** auro-balancing-necleo (port of auro-balancing-bot STM32F429ZI)

---

## Phase Results — Final

| Phase | Description | Result | Key measurement |
|-------|-------------|--------|-----------------|
| 1 | VCP + power | PASS | 268 frames, 0 CRC errors |
| 2 | IMU | PASS | Pitch -28.4 deg, accel 1.013g, gyro <0.3 dps |
| 3 | Encoders A+B | PASS | Both bidirectional, same polarity |
| 4 | ADC | PASS | 12.30V vs 12.27V (0.24% error) |
| 5 | PWM | PASS | Both motors FWD/REV |
| 6 | Motors + CT | PASS | CT_A = 46.8 mA avg at 60 RPM no-load |
| 7 | Timing | PASS | TIM11 = 20.000ms, 0ms jitter |

---

## Open Items — All Closed

| ID | Description | Root cause | Fix | Status |
|----|-------------|------------|-----|--------|
| OPEN-01 | CT reads 0mA under load | CT wires swapped on H-bridge | Swapped DMA buffer indices + 20ms peak-hold | CLOSED |
| OPEN-02 | Level shift voltage not measured | Not measured in HW-LIVE-01 | 3.28V native encoder output, no divider needed | CLOSED |

---

## Firmware Bugs Found and Fixed

### HW-BUG-02: Blocking I2C in priority-0 ISR killed UART RX (HW-LIVE-02)

- **Files:** main.c, stm32f4xx_hal_msp.c
- **Symptom:** UART RX dropped frames during IMU reads, commands not received
- **Root cause:** Blocking HAL_I2C_Mem_Read (~300us) called from TIM10 ISR (priority 0) preempted USART2 RX ISR (priority 1)
- **Fix:** IMU read rate reduced to 200Hz (5x divider), USART2 priority raised to 0, TIM10 lowered to 1

### HW-BUG-03: UART RxState death after error (HW-LIVE-02)

- **File:** main.c (ErrorCallback)
- **Symptom:** UART RX permanently stops after first HAL overrun error
- **Root cause:** HAL sets huart->RxState = BUSY, never resets on error
- **Fix:** Reset huart->RxState = READY in HAL_UART_ErrorCallback before re-arming IT

### HW-BUG-04: CT reads 0mA — pulsed output requires peak-hold (HW-LIVE-03)

- **File:** main.c
- **Symptom:** CT average reads 0mA despite motor running at 60 RPM
- **Root cause:** DBH-12V CT output is pulsed (only during PWM ON phase), 1ms ISR sample mostly catches OFF period
- **Fix:** 20ms peak-hold window in 1kHz ISR captures maximum CT value per window

### HW-BUG-05: CT DMA buffer channels swapped (HW-LIVE-03)

- **File:** main.c
- **Symptom:** CT_A reading appeared on CT_B channel and vice versa
- **Root cause:** H-bridge CT wires physically swapped during chassis assembly
- **Fix:** Swapped DMA buffer indices in firmware (buf[2] for Motor A, buf[0] for Motor B)

---

## Hardware Configuration (All Verified)

### MCU

- STM32F401RE, LQFP64, 84MHz, 512KB flash, 96KB SRAM
- SWD: PA13/PA14 (do not use as GPIO)
- VCP: USART2 PA2/PA3, 921600 baud, SB13/SB14 ON

### IMU

- Module: GY-521 (labelled MPU6050, actual chip: ICM-20602)
- WHO_AM_I: 0x72 (fix applied, accepts 0x70-0x73)
- Bus: I2C1, PB8 (SCL), PB9 (SDA), 400kHz
- Update rate: 200Hz (reduced from 1kHz to fix HW-BUG-02)
- Chassis resting pitch: **-28.4 deg** (balance setpoint)

### Encoders

- Motor A: PA0 (TIM2_CH1), PA1 (TIM2_CH2), 500PPR x4 x30:1 = 60,000 CPR
- Motor B: PA6 (TIM3_CH1), PA7 (TIM3_CH2), same spec
- Output voltage: 3.28V native, no level shift needed
- Polarity: CW rotation = count INCREASES (both A and B)

### Motor Driver

- Motor A PWM: PA8 (TIM1_CH1, IN1), PA9 (TIM1_CH2, IN2), 20kHz ARR=4199
- Motor B PWM: PB6 (TIM4_CH1, IN1), PB7 (TIM4_CH2, IN2), 20kHz ARR=4199
- Motor A EN: PC10, Motor B EN: PC11
- Current sense A: PC3 (ADC1_IN13), via DMA buf[2] (swapped)
- Current sense B: PC4 (ADC1_IN14), via DMA buf[0] (swapped)
- CT reading at 60 RPM no-load: 46.8 mA avg (Motor A, peak-hold verified)

### Battery ADC

- Pin: PB1 (ADC1_IN9)
- Divider: 100k / 33k, 0.248x attenuation
- Accuracy: 0.24% error (12.30V reported vs 12.27V measured)

### Control Loop

- PID ISR: TIM10 at 1kHz (priority 1)
- Telemetry ISR: TIM11 at 50Hz (priority 2)
- IMU: 200Hz (every 5th control tick)
- Measured period: 20.000ms, 0ms jitter

### NVIC Priority Map

| Priority | Peripheral | Reason |
|----------|-----------|--------|
| 0 (highest) | USART2 (VCP RX) | Must not be blocked during I2C |
| 1 | TIM10 (1kHz control ISR) | Control loop with blocking I2C |
| 2 | TIM11 (50Hz telemetry ISR) | Telemetry build + DMA TX |
| 3 | DMA1_Stream6 (UART TX) | Background UART transmit |

---

## PID Parameters (Ported — Not Yet Hardware-Tuned)

| Parameter | Ported value | Hardware-tuned | Notes |
|-----------|-------------|----------------|-------|
| RPM Kp (A) | 9.5 | -- | Next: auto-tune |
| RPM Ki (A) | 4.2 | -- | |
| RPM Kd (A) | 1.5 | -- | |
| RPM Kp (B) | 7.70 | -- | |
| RPM Ki (B) | 3.45 | -- | |
| RPM Kd (B) | 1.24 | -- | |
| Balance angle | -- | **-28.4 deg** | Measured chassis pitch |

---

## Next Development Actions (Ordered)

1. **HW-TUNE-01** — RPM PID auto-tune both axes (relay feedback, 60 RPM)
2. **HW-TUNE-02** — Set balance angle -28.4 deg, enable balance control loop
3. **HW-TUNE-03** — Balance PID tuning (manual, then relay auto-tune)
4. **HW-TUNE-04** — Disturbance rejection test (push test on chassis)
