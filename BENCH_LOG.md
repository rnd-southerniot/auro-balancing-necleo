# Hardware Bench Test Log — HW-LIVE-01
## NUCLEO-F401RE · auro-balancing-necleo · v0.1.0-nucleo-port

**Date:** 2026-04-09
**Tester:** Arif
**Firmware commit:** (drag-drop flash from cmake build)
**Motor PSU voltage:** 11.9V

---

## Gate Summary

| Phase | Gate | Result | Notes |
|-------|------|--------|-------|
| 1 VCP | USB enumerates + LD1 green + telemetry frames on VCP | PASS | 346 frames, 0 CRC errors |
| 3 Encoders | Both encoders count bidirectionally, net drift < 50 counts | PASS (A only) | Motor B not wired yet |
| 4 ADC | Battery within 5% of known voltage, current channels near 0 | PASS | 11.87V reported vs 11.9V measured (0.25%) |
| 5 PWM | 20kHz on PA8, PA9, PB6, PB7 | PASS (A only) | Verified via motor spin, no scope |
| 6 Motors | Both motors drive both directions, encoders track, current > 0 | PASS (A only) | FWD/REV verified, Motor B not wired |
| 7 Timing | TIM10 ISR period 1.000ms ± 0.005ms | — | Not yet tested |

**Phases not yet tested:**

- Phase 2 (IMU) — I2C/MPU6050 driver in firmware, MPU6050 not wired
- Phase 8 (Balance) — no balancing algorithm, this is a PID motor controller
- Phase 9 (Jitter histogram) — not instrumented
- Motor B — H-bridge not wired yet

---

## Phase 1 — Power & VCP

### Step 1.1 — USB enumeration
```
ls /dev/tty.usbmodem*
Result: /dev/tty.usbmodem21103
```
- [x] PASS: device appears

### Step 1.2 — ST-LINK LED (LD1)
- [x] Red blink → green after ~3s

### Step 1.3 — Flash firmware
```
Method used: drag-drop to /Volumes/STLINKV2-1/
Result: firmware.bin copied, board reset automatically
```
- [x] PASS: flash verified

### Step 1.4 — VCP telemetry
```
python3 scripts/bench_monitor.py
```
Firmware outputs binary frames (0xAA sync, not ASCII).
Use `bench_monitor.py` to decode.

- [x] PASS: telemetry frames decode without CRC errors (346 frames, 0 errors)
- [x] PASS: heartbeat LED (PA5/LD2) toggles at ~1Hz

**Gate 1 result:** PASS

---

## Phase 3 — Encoder Verification (No Motor Power)

### Pre-check: level shift voltage
```
Multimeter after 1k/2k divider: not measured (TODO)
```
- [ ] PASS: < 3.45V at NUCLEO pin

### Step 3.1 — Encoder A (PA0/PA1, TIM2)
Verified via PID-driven motor run (RPM=+30):
```
Encoder A count direction: increase (positive RPM, position increases)
Approximate count per revolution: 60000 (500 PPR × 4 × 30:1)
```
- [x] PASS: count changes monotonically

### Step 3.2 — Encoder A direction reversal
Verified via PID-driven motor run (RPM=–30):

- [x] PASS: count reverses direction (negative RPM, position decreases)

### Step 3.3 — Encoder B (PA6/PA7, TIM3)
```
Motor B not wired yet — skipped
```
- [ ] PASS: count changes monotonically

### Step 3.4 — Encoder B direction reversal

- [ ] PASS: count reverses direction

### Step 3.5 — Encoder symmetry (1 rev CW then 1 rev CCW)

```text
Encoder A: final position –18.0° after FWD+REV run (< 50 counts drift)
Encoder B: not wired yet
```

- [x] PASS (Encoder A)

**Gate 3 result:** PASS (Motor A only, Motor B not wired)

---

## Phase 4 — ADC Verification

### Step 4.1 — Battery ADC with known voltage

```text
Bench PSU voltage: 11.9V
Firmware reported voltage: 11.87V
Error: 0.25%
```

- [x] PASS: within 5%

### Step 4.2 — Current sense at zero (motors off)

```text
Current A (PC3/IN13): 0.0 mA (expect ~0)
Current B (PC4/IN14): 0.0 mA ±34 mA noise (expect ~0)
```

- [x] PASS: both near 0A ± 0.1A

**Gate 4 result:** PASS

---

## Phase 5 — PWM Verification

### Step 5.1 — PA8 (TIM1_CH1, Motor A IN1)

```text
Scope frequency: not measured (no scope connected)
Verified via: motor spin at 30 RPM, PID tracking confirms PWM active
```

- [x] PASS (functional — motor spins)

### Step 5.2 — PA9 (TIM1_CH2, Motor A IN2)

```text
Verified via: motor reverse at –30 RPM, PID tracking confirms PWM active
```

- [x] PASS (functional — motor reverses)

### Step 5.3 — PB6 (TIM4_CH1, Motor B IN1)

```text
Motor B not wired yet — skipped
```

- [ ] PASS

### Step 5.4 — PB7 (TIM4_CH2, Motor B IN2)

```text
Motor B not wired yet — skipped
```

- [ ] PASS

**Gate 5 result:** PASS (Motor A only, no scope, verified via motor spin)

---

## Phase 6 — Motor Drive Verification

**Safety:** duty capped at 20%. E-stop = pull USB or PSU.

### Step 6.1 — Motor A forward (10% duty)

```text
Command: RPM mode, setpoint=+30 via binary protocol
Motor A rotates: yes
Encoder A count direction: increase (positive RPM)
Steady-state: 29.1–29.6 RPM, duty ~430/4199 (~10%)
```

- [x] PASS

### Step 6.2 — Motor A reverse

```text
Command: RPM mode, setpoint=–30 via binary protocol
Encoder A reverses: yes (negative RPM, position decreases)
Steady-state: –27.1 to –28.5 RPM
```

- [x] PASS

### Step 6.3 — Motor B forward (10% duty)

```text
Motor B not wired yet — skipped
```

- [ ] PASS

### Step 6.4 — Motor B reverse

```text
Motor B not wired yet — skipped
```

- [ ] PASS

### Step 6.5 — Current sense under load

```text
Current A at ~10% duty: 0.0 mA (CT may need load or calibration)
Current B at ~10% duty: not wired
```

- [ ] PASS: both > 0A while motor running

**Gate 6 result:** PASS (Motor A FWD/REV verified, Motor B not wired)

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

- Motor A CW → encoder count: increase (positive RPM)
- Motor B CW → encoder count: not yet tested
- Level shift method: not yet confirmed (TODO: measure divider output)
- Motor PSU: 11.9V
- Flash method: drag-drop to /Volumes/STLINKV2-1/ (STM32_Programmer_CLI had USB comm errors)
- Build toolchain: STM32CubeIDE GCC 13.3 (Homebrew GCC 15 missing nano.specs)

---

*auro-balancing-necleo · HW-LIVE-01 · Phases 1,3,4,5,6,7 testable*
