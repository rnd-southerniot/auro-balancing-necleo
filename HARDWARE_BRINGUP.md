# Hardware Bring-Up Checklist — auro-balancing-necleo
## NUCLEO-F401RE + DBH-12V + MG513 Encoders

---

## 1. Required Wiring (from F429ZI Discovery → NUCLEO-F401RE)

| Signal | H/W Source | NUCLEO Pin | Connector | Verified |
|--------|-----------|-----------|-----------|---------|
| Motor A PWM IN1 | DBH-12V IN1 | PA8 (TIM1_CH1) | CN10 pin 23 | ☐ |
| Motor A PWM IN2 | DBH-12V IN2 | PA9 (TIM1_CH2) | CN10 pin 21 | ☐ |
| Motor A EN | DBH-12V ENA | PC10 (GPIO) | CN7 pin 1 | ☐ |
| Motor B PWM IN3 | DBH-12V IN3 | PB6 (TIM4_CH1) | CN10 pin 17 | ☐ |
| Motor B PWM IN4 | DBH-12V IN4 | PB7 (TIM4_CH2) | CN7 pin 21  | ☐ |
| Motor B EN | DBH-12V ENB | PC11 (GPIO) | CN7 pin 2 | ☐ |
| Encoder A CH1 | MG513 Enc A A+ | PA0 (TIM2_CH1) | CN7 pin 28 | ☐ |
| Encoder A CH2 | MG513 Enc A B+ | PA1 (TIM2_CH2) | CN7 pin 30 | ☐ |
| Encoder B CH1 | MG513 Enc B A+ | PA6 (TIM3_CH1) | CN10 pin 13 | ☐ |
| Encoder B CH2 | MG513 Enc B B+ | PA7 (TIM3_CH2) | CN10 pin 15 | ☐ |
| Current sense A | Divider output | PC3 (ADC1_IN13) | CN7 pin 37 | ☐ |
| Current sense B | Divider output | PC4 (ADC1_IN14) | CN10 pin 34 | ☐ |
| Battery monitor | Divider output | PB1 (ADC1_IN9) | CN10 pin 24 | ☐ |
| GND | Common GND | GND | CN7 pin 8 | ☐ |
| VCP USB | Host PC | USB mini-B | CN1 | ☐ |

---

## 2. Level Shifting — MANDATORY

All 4 encoder lines output 5V. F401RE GPIO is 3.3V. **Do not connect directly.**

Option A: 74LVC245 (8-bit bus transceiver, 5V→3.3V direction)

Option B: Resistor divider per line:
```
MG513 5V output ──[1kΩ]──┬── NUCLEO 3.3V GPIO
                          └──[2kΩ]── GND
```

| Encoder line | 5V source | NUCLEO pin |
|-------------|-----------|-----------|
| Enc A CH1 | MG513-A A+ | PA0 CN7-28 |
| Enc A CH2 | MG513-A B+ | PA1 CN7-30 |
| Enc B CH1 | MG513-B A+ | PA6 CN10-13 |
| Enc B CH2 | MG513-B B+ | PA7 CN10-15 |

---

## 3. Solder Bridge Verification

Check the bottom of the NUCLEO board before powering on:

| Bridge | Required | Check |
|--------|----------|-------|
| SB13 | CLOSED | ☐ |
| SB14 | CLOSED | ☐ |
| SB62 | OPEN | ☐ |
| SB63 | OPEN | ☐ |
| SB15 | CLOSED (do not cut) | ☐ |
| SB21 | CLOSED | ☐ |
| SB17 | CLOSED | ☐ |

---

## 4. Power Sequence

> **Warning:** 12.6V full-charge LiPo exceeds DBH-12V 12V rating.
> Use 11.1V nominal (3S) or add TVS/Schottky clamp before VIN.

1. ☐ Verify battery voltage <= 12V before connecting
2. ☐ Load firmware via USB ST-LINK (board powered from USB only)
3. ☐ Open serial monitor: `screen /dev/tty.usbmodem* 921600`
4. ☐ Confirm UART output — telemetry frames should appear
5. ☐ Connect motor power supply (separate from USB)
6. ☐ Rotate wheels by hand — verify encoder counts change in UART telemetry
7. ☐ Scope PA8/PA9 and PB6/PB7 — expect 20kHz PWM at configured duty
8. ☐ Enable motor control only after all 7 steps pass

---

## 5. First-Power Smoke Tests

### Test 1 — VCP alive
```bash
screen /dev/tty.usbmodem* 921600
# Expect: telemetry frames within 1s (binary, 0xAA sync byte)
```

### Test 2 — Encoder counting
Rotate Motor A by hand → encoder count changes in expected direction.
Rotate Motor B by hand → encoder count changes in expected direction.
Verify quadrature decodes correctly (count direction reverses when reversed).

### Test 3 — PWM present
Oscilloscope on PA8 → 20kHz square wave at configured duty cycle.
Oscilloscope on PB6 → same.

### Test 4 — ADC readings sane
Battery ADC (PB1): voltage matches multimeter on VIN rail (x divider ratio).
Current sense A (PC3) / B (PC4): near zero with motors disabled.

### Test 5 — Heartbeat LED
PA5 (LD2) toggles at ~1 Hz — confirms main loop is running.

---

*rnd-southerniot/auro-balancing-necleo*
