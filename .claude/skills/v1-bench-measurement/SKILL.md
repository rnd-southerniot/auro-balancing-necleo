---
name: v1-bench-measurement
description: How to add or run one of the v1 bench measurements B1–B12 (encoder CPR, direction, PWM deadband, speed curve, backlash, stall current, IMU bias/noise, tilt consistency, balance point, CoM, loop timing, stress, battery sag) — the four required artefacts, the CSV and doc conventions, bench conditions, and who triggers motion. Use when writing a firmware test mode, a host/measure script, logging bench data, or filling docs/v1-measurements.md.
---

# v1 bench measurements (B1–B12)

Spec: `balancing-robot-v1-cc-prompt.md` §4. Phase placement and pass criteria: `docs/v1-plan.md`.

## Every measurement ships four artefacts

| Artefact | Where | Rule |
|---|---|---|
| Firmware test mode | `firmware/Core/Src/test_modes.c` (`CTRL_TEST`, `MSG_CMD_TEST 0x50`) | Bounded duration and duty; refuses to run unless STBY release is allowed |
| Host script | `host/measure/bNN_<slug>.py` | `--port` or `AURO_PORT`, never a hard-coded device; writes one CSV per run |
| CSV | `host/logs/bNN_<slug>_<YYYYMMDD-HHMMSS>.csv` | Header row with units; first comment lines record firmware SHA, PSU voltage, duty cap |
| Doc section | `docs/v1-measurements.md` | Method · data (CSV path) · fitted value · plot · PASS/FAIL against the plan's criterion |

## Measurement map

| # | What | Phase | Pass / use (from the plan) |
|---|---|---|---|
| B1 | Encoder counts/rev, both wheels, 10 hand turns | 2 | Use the **measured** CPR; L/R agree within 1 %; forward positive on both |
| B2 | Direction mapping | 2 | +duty drives the robot forward on both motors |
| B3 | PWM deadband per motor per direction | 2 | 5 repeats, σ < 1 % duty; measured once, in center-aligned PWM |
| B4 | Speed vs duty, 10 steps × 2 directions | 2 (re-run at 98 % cap after 3b) | Motor gain + L/R mismatch |
| B5 | Gearbox backlash at the wheel | 2 | Degrees; > 2° goes into the control design |
| B6 | Electrical time constant + stall current, ≤ 200 ms pulse | **3b** | Sets the clamp threshold |
| B7 | IMU bias and noise, 60 s static | 4 | Gyro bias, accel σ, drift < 0.5°/min |
| B7b | Slow ±20° hand tilt: accel angle vs integrated gyro | 4 | Agree in sign and magnitude within ~1° **before any balance attempt** |
| B8 | Mechanical balance point | 4 | Setpoint from data (replaces −5.0°) |
| B9 | CoM height and mass | 4 | Model / LQR input |
| B10 | Loop timing (DWT) | 4 | Angle-tick ISR worst case < 500 µs; angle and non-angle ticks reported separately |
| B11 | TB6612 current and temperature, 20 push recoveries | **3b / 8** | I_peak < 3.2 A, T < 80 °C |
| B12 | Battery sag under load | 3a tooling, curve with B11 | Justifies Vbat compensation |

## Bench conditions (until Phase 3b passes)

- Bench PSU **12.0 V, 3 A limit**, battery disconnected; the PSU's CC indicator must stay off during B1–B5.
- Wheels off the ground; `PWM_DUTY_CAP_PCT` = 60; stall detect armed.
- Record these in the "Bench conditions" section of `docs/v1-measurements.md`.

## Who triggers motion

Claude writes the test mode and the script. **Arif (or the operator at the bench) confirms the
conditions above in the session before any motion command is sent.** Hand-turn and static
measurements (B1, B5, B7, B7b, B8) need no motion command.

## Evidence

A value goes into `config.h` or the doc as PROVEN only with its CSV path. A number read off a
screen without a log is ASSUMED until re-captured.
