# HW-TUNE-01 — RPM PID Auto-Tune Session Plan

## auro-balancing-necleo

**Prerequisite:** All 7 HW-LIVE phases PASS (tag: v0.1.0-hw-validated)
**Goal:** Auto-tune RPM PID for both Motor A and Motor B using relay feedback
**Estimated duration:** 30-45 minutes per axis

---

## Pre-session checklist

- [ ] Board flashed with latest firmware (drag-drop to /Volumes/STLINKV2-1/)
- [ ] Motor PSU connected (12V battery)
- [ ] Chassis elevated (wheels off ground) — motors must spin freely
- [ ] VCP connected (/dev/tty.usbmodem*)
- [ ] bench_monitor.py running for telemetry

---

## Auto-tune parameters (starting values)

| Parameter | Motor A | Motor B | Notes |
|-----------|---------|---------|-------|
| Setpoint (RPM) | 60 | 60 | No-load operating point |
| Relay amplitude | 250 | 250 | PWM duty swing |
| Min cycles | 3 | 3 | Oscillation cycles before accepting |
| Timeout | 60s | 60s | Abort if no oscillation |

---

## Expected results (from HW-LIVE-01 auto-tune)

| Metric | Motor A (ported) | Motor B (ported) | Notes |
|--------|-----------------|-----------------|-------|
| Ku | 60.63 | 49.29 | Ultimate gain |
| Pu | 1.017s | 1.013s | Ultimate period |
| Kp (50% TL) | 9.5 | 7.70 | Tyreus-Luyben |
| Ki | 4.2 | 3.45 | |
| Kd | 1.5 | 1.24 | |

These were tuned on the F429ZI board. The F401RE port should produce
similar values, but motor mounting and chassis inertia may differ.

---

## Procedure

### Motor A auto-tune

1. Set mode to IDLE
2. Command auto-tune: setpoint=60 RPM
3. Motor will oscillate (relay feedback) — this is normal
4. Wait for AUTOTUNE_COMPLETE in telemetry (20-60 seconds)
5. Record Ku, Tu, computed Kp/Ki/Kd
6. Accept gains

### Motor B auto-tune

7. Set Motor A to IDLE
8. Command Motor B auto-tune: setpoint=60 RPM
9. Wait for AUTOTUNE_COMPLETE
10. Record Ku, Tu, computed Kp/Ki/Kd

### Step response validation

11. After auto-tune each axis:
    - Step 0 to 60 RPM: measure overshoot (<15%), settling (<2s), SS error (<2 RPM)
    - Step 60 to 30 RPM: measure settling (<1.5s)
    - Apply hand load briefly: measure recovery (<2s)

---

## Pass criteria

| Test | Criterion |
|------|-----------|
| Auto-tune completes | < 60 seconds |
| Ku in range | 1.0 - 100.0 |
| Tu in range | 0.2 - 3.0s |
| Step overshoot | < 15% |
| Settling time | < 2.0s |
| Steady-state error | < 2 RPM |

---

## Claude Code prompt for HW-TUNE-01 session

Start next session with:

```
Resume auro-balancing-necleo hardware tuning.
All 7 HW-LIVE phases passed. Tag: v0.1.0-hw-validated.
Board is wired, flashed, and responding on /dev/tty.usbmodem*.
Chassis is elevated (wheels off ground).

Run RPM auto-tune on Motor A then Motor B:
  - Setpoint: 60 RPM, relay amplitude: 250
  - Accept gains if Ku in [1,100] and Tu in [0.2,3.0]s
  - Run step response validation after each axis
  - Update HARDWARE_STATUS.md with tuned parameters
```
