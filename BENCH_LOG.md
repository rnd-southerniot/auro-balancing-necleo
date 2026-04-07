# Hardware Bench Test Log — HW-LIVE-01
## NUCLEO-F401RE · auro-balancing-necleo · v0.1.0-nucleo-port

**Date:** ___________
**Tester:** ___________
**Firmware commit:** 975cea0
**Motor PSU voltage:** ___________V

---

## Gate Summary

| Phase | Gate | Result | Notes |
|-------|------|--------|-------|
| 1 VCP | USB enumerates + LD1 green + telemetry frames on VCP | | |
| 3 Encoders | Both encoders count bidirectionally, net drift < 50 counts | | |
| 4 ADC | Battery within 5% of known voltage, current channels near 0 | | |
| 5 PWM | 20kHz on PA8, PA9, PB6, PB7 | | |
| 6 Motors | Both motors drive both directions, encoders track, current > 0 | | |
| 7 Timing | TIM10 ISR period 1.000ms ± 0.005ms | | |

**Phases not testable with current firmware:**
- Phase 2 (IMU) — no I2C/MPU6050 driver in firmware
- Phase 8 (Balance) — no balancing algorithm, this is a PID motor controller
- Phase 9 (Jitter histogram) — not instrumented

---

## Phase 1 — Power & VCP

### Step 1.1 — USB enumeration
```
ls /dev/tty.usbmodem*
Result: ___________
```
- [ ] PASS: device appears

### Step 1.2 — ST-LINK LED (LD1)
- [ ] Red blink → green after ~3s

### Step 1.3 — Flash firmware
```
Method used: st-flash / CubeProgrammer / drag-drop
Result: ___________
```
- [ ] PASS: flash verified

### Step 1.4 — VCP telemetry
```
python3 scripts/bench_monitor.py
```
Firmware outputs binary frames (0xAA sync, not ASCII).
Use `bench_monitor.py` to decode.
- [ ] PASS: telemetry frames decode without CRC errors
- [ ] PASS: heartbeat LED (PA5/LD2) toggles at ~1Hz

**Gate 1 result:** ___________

---

## Phase 3 — Encoder Verification (No Motor Power)

### Pre-check: level shift voltage
```
Multimeter after 1k/2k divider: ___________V (expect 3.30V ± 0.15V)
```
- [ ] PASS: < 3.45V at NUCLEO pin

### Step 3.1 — Encoder A (PA0/PA1, TIM2)
Rotate Motor A shaft ~5 turns CW by hand:
```
Encoder A count direction: increase / decrease (circle one)
Approximate count per revolution: ___________
```
- [ ] PASS: count changes monotonically

### Step 3.2 — Encoder A direction reversal
Rotate Motor A ~5 turns CCW:
- [ ] PASS: count reverses direction

### Step 3.3 — Encoder B (PA6/PA7, TIM3)
Rotate Motor B shaft ~5 turns CW by hand:
```
Encoder B count direction: increase / decrease (circle one)
Approximate count per revolution: ___________
```
- [ ] PASS: count changes monotonically

### Step 3.4 — Encoder B direction reversal
Rotate Motor B ~5 turns CCW:
- [ ] PASS: count reverses direction

### Step 3.5 — Encoder symmetry (1 rev CW then 1 rev CCW)
```
Net count drift: ___________ counts (expect < 50)
```
- [ ] PASS

**Gate 3 result:** ___________

---

## Phase 4 — ADC Verification

### Step 4.1 — Battery ADC with known voltage
```
Bench PSU voltage: ___________V
Firmware reported voltage: ___________V
Error: ___________%
```
- [ ] PASS: within 5%

### Step 4.2 — Current sense at zero (motors off)
```
Current A (PC3/IN13): ___________A (expect ~0)
Current B (PC4/IN14): ___________A (expect ~0)
```
- [ ] PASS: both near 0A ± 0.1A

**Gate 4 result:** ___________

---

## Phase 5 — PWM Verification

### Step 5.1 — PA8 (TIM1_CH1, Motor A IN1)
```
Scope frequency: ___________kHz (expect 20.0 ± 0.5)
Amplitude: ___________V
```
- [ ] PASS

### Step 5.2 — PA9 (TIM1_CH2, Motor A IN2)
```
Scope frequency: ___________kHz
```
- [ ] PASS

### Step 5.3 — PB6 (TIM4_CH1, Motor B IN1)
```
Scope frequency: ___________kHz
```
- [ ] PASS

### Step 5.4 — PB7 (TIM4_CH2, Motor B IN2)
```
Scope frequency: ___________kHz
```
- [ ] PASS

**Gate 5 result:** ___________

---

## Phase 6 — Motor Drive Verification

**Safety:** duty capped at 20%. E-stop = pull USB or PSU.

### Step 6.1 — Motor A forward (10% duty)
```
Command: python3 scripts/bench_monitor.py --cmd rpm_a 30
Motor A rotates: yes / no
Encoder A count direction: increase / decrease
```
- [ ] PASS

### Step 6.2 — Motor A reverse
```
Command: python3 scripts/bench_monitor.py --cmd rpm_a -30
Encoder A reverses: yes / no
```
- [ ] PASS

### Step 6.3 — Motor B forward (10% duty)
```
Command: python3 scripts/bench_monitor.py --cmd rpm_b 30
Motor B rotates: yes / no
Encoder B count direction: increase / decrease
```
- [ ] PASS

### Step 6.4 — Motor B reverse
```
Command: python3 scripts/bench_monitor.py --cmd rpm_b -30
Encoder B reverses: yes / no
```
- [ ] PASS

### Step 6.5 — Current sense under load
```
Current A at 10% duty: ___________A
Current B at 10% duty: ___________A
```
- [ ] PASS: both > 0A while motor running

**Gate 6 result:** ___________

---

## Phase 7 — Control Loop Timing

### Step 7.1 — TIM10 ISR rate
```
Measurement method: scope on spare GPIO / VCP timestamp delta
Measured ISR period: ___________ms (expect 1.000 ± 0.005)
```
- [ ] PASS

**Gate 7 result:** ___________

---

## Failure Records

### Failure #1
**Phase:** ___  **Step:** ___
**Symptom:**
**Root cause:**
**Fix applied:**
**Re-test result:**

### Failure #2
**Phase:** ___  **Step:** ___
**Symptom:**
**Root cause:**
**Fix applied:**
**Re-test result:**

---

## Tuned Parameters (if changed from ported values)

| Parameter | Original | Final | Reason |
|-----------|----------|-------|--------|
| PID_RPM_KP | 9.5 | | |
| PID_RPM_KI | 4.2 | | |
| PID_RPM_KD | 1.5 | | |
| PID_RPM_KP_B | 7.70 | | |
| PID_RPM_KI_B | 3.45 | | |
| PID_RPM_KD_B | 1.24 | | |

## Hardware Notes
- Motor A CW → encoder count: increase / decrease
- Motor B CW → encoder count: increase / decrease
- Level shift method: 1k/2k divider / 74LVC245
- Motor PSU: ___________V @ ___________A

---

*auro-balancing-necleo · HW-LIVE-01 · Phases 1,3,4,5,6,7 testable*
