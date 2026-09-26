# Claude Code Prompt — Self-Balancing Robot v1 (JGB37-520 + TB6612FNG)

> Run inside the existing **Auro-balancing-necleo** repo (NUCLEO-F401RE port).
> Start in **plan mode**. First task: read the repo and report what exists before changing anything.

---

## 0. Role and working rules

You are a senior embedded/control engineer taking an existing single-motor balance prototype to a **complete two-wheel self-balancing robot (v1)** on production-quality firmware.

Rules:
- **Extend, don't rewrite.** Reuse the working drivers, clock setup, VCP monitor and auto-tune. Justify any refactor.
- Work in **small, reversible phases** with a gate at each one: you report measured data, I approve, you continue.
- Never guess hardware facts. Read the code, or ask me.
- Every new feature ships with a test mode and a log format.
- Commit per phase (conventional commits) and update `CHANGELOG.md`.

---

## 1. Existing state (verify against the repo)

| Item | Current value |
|---|---|
| MCU | STM32 NUCLEO-F401RE, 84 MHz (ported from F429ZI; 21 GPIO remaps; TIM6/7 → TIM10/11) |
| **Hard constraint** | **Solder bridges SB62/SB63 must stay OPEN.** Never propose a pin that requires closing them |
| Encoder (motor A) | TIM2 encoder mode, PA0 / PA1 |
| PWM | TIM1, PA8 (CH1) / PA9 (CH2), ARR = 4199 → 20 kHz |
| Balance loop | 50 Hz angle PID, setpoint −5.0°, Ku = 0.12 confirmed |
| Tooling | VCP monitor with oscillation detection and auto-tune |
| Debug LED | RGB common-anode: PB2 = R, PC7 = G, PA10 = B |
| Serial | ST-Link VCP, 921600 baud (port path passed as arg; don't hard-code `/dev/tty.usbmodem1301`) |

**Discovery task 1:** report which IMU the repo uses (part, bus, pins, filter type), how the motor direction pins are driven today, and where timing is generated (TIM10/TIM11).

---

## 2. Target hardware (v1 — fixed)

| Role | Part | Key facts |
|---|---|---|
| Motors ×2 | JGB37-520 12 V gearmotor + hall encoder | ~178 rpm output, 30:1, 11 PPR → **1320 counts/rev** (4×) |
| Driver | TB6612FNG module | 1.2 A continuous / 3.2 A peak per channel, VM ≤ 13.5 V |
| Wheels | 65 mm rubber tyre, 6 mm D-bore hub | |
| Mounts | JGB37 L-brackets + couplers | |
| Current sense | INA240A2 + shunt (≥ 1 motor, ideally 2) | |
| Battery | 3S Li-ion/LiPo (11.1 V nominal, 12.6 V full) | Charged with the balance charger. **Fused (blade fuse) + XT60** |
| Safety | Power switch + blade fuse; firmware tilt cut-off | |
| **Do not use** | DRV8833 | 10.8 V max, will fail on 3S |

### Known risks
1. **TB6612 current margin.** JGB37-520 stall current at 12 V can exceed the TB6612 peak during hard recovery pulses. Mitigate with INA240 current clamping. Fallback: **one TB6612 per motor with channels paralleled.**
2. **Gearbox backlash** (spur 30:1) creates deadband near upright. It must be measured and compensated.
3. **3S full charge (12.6 V) is close to the TB6612 VM limit (13.5 V).** Add bulk capacitance (low-ESR electrolytic at the driver) and a TVS, and check ringing on the scope.
4. The second encoder needs a free 16/32-bit timer that doesn't disturb existing pins.

---

## 3. Phase 0 — Pin plan and wiring (no code)

Produce `docs/v1-pinmap.md`:
- **Motor B encoder:** propose a timer in encoder mode (e.g. TIM3 or TIM4). Check conflicts with SB62/SB63, the RGB LED pins, VCP (PA2/PA3) and existing TIM10/11 usage.
- **TB6612 control:** AIN1, AIN2, BIN1, BIN2, STBY GPIOs. PWMA = PA8, PWMB = PA9 (existing TIM1). STBY defaults low (driver off) at boot.
- **INA240 outputs** to ADC channels, sampled synchronously with the PWM (TIM1 TRGO) to avoid switching noise.
- **Battery voltage** divider to ADC (sized for 13 V max).
- Wiring diagram in Mermaid, plus a power-path diagram: battery → fuse → switch → bulk caps/TVS → TB6612 VM; buck → 5 V → Nucleo.

Gate 0: I confirm the pin map.

---

## 4. Required measurements

Each gets a firmware test mode, a Python script in `host/measure/`, a CSV in `host/logs/`, and a section in `docs/v1-measurements.md` (method, data, fitted value, plot).

| # | Measurement | Method | Pass / use |
|---|---|---|---|
| B1 | Encoder counts/rev, both wheels | 10 hand turns against a mark | ≈ 1320; sign convention: forward = positive on both |
| B2 | Motor direction mapping | +PWM on each motor | Both drive the robot forward |
| B3 | **PWM deadband** per motor, per direction | Ramp duty slowly until the wheel moves (wheels off ground) | Feeds deadband compensation |
| B4 | Steady-state speed vs duty curve | 10 duty steps × 2 directions, at fixed battery voltage | Gives the motor gain; check left/right mismatch |
| B5 | **Gearbox backlash** at the wheel | Hold the motor shaft (motor braked), rock the wheel, read the encoder | Degrees at the wheel; > 2° → note in the control design |
| B6 | Motor electrical time constant and stall current | Short locked-rotor pulse, INA240 log (≤ 200 ms!) | Sets the current clamp threshold |
| B7 | IMU bias and noise | 60 s static log, flat and still | Gyro bias, accel noise σ |
| B8 | **Mechanical balance point** | Find the angle where the robot balances by hand; compare with the −5.0° setpoint | Update the setpoint from data |
| B9 | CoM height and total mass | Balance-point and scale | Used in the model and LQR option |
| B10 | Loop timing | DWT cycle counter per loop iteration | Max < 50% of the budget; report jitter |
| B11 | TB6612 temperature and current under stress | 20 hard push-recoveries, INA240 log + thermocouple/IR on the chip | Peak current < 3.2 A, temperature < 80 °C |
| B12 | Battery sag under load | Log Vbat during B11 | Justifies voltage compensation of PWM |

Gate 1: I review `v1-measurements.md`.

---

## 5. Control architecture

Upgrade from the 50 Hz single-loop PID to a cascaded controller:

```
 position/velocity target ─► [Velocity loop, 50 Hz] ─► tilt setpoint
                                                         │
 IMU ─► [Complementary/Kalman filter, 500 Hz] ─► tilt ─► [Angle loop, 200–500 Hz] ─► torque cmd
                                                                                       │
                              steering (yaw) cmd ─► (+/−) mixing ─► deadband comp ─► Vbat comp ─► current clamp ─► PWM A/B
```

Requirements:
1. **Raise the angle loop to 200–500 Hz** (choose from B10). Re-verify Ku with the existing auto-tune at the new rate. Don't carry over 0.12 blindly.
2. **Sensor fusion:** keep the existing filter if it's sound. Document the time constant, and use the gyro bias from B7.
3. **Velocity outer loop:** use the average of both wheel speeds to stop drift. Output is limited to ±N° of tilt setpoint.
4. **Yaw loop:** the wheel speed difference is held to a commanded yaw rate (straight-line hold at 0).
5. **Deadband compensation** from B3, **battery voltage compensation** from B12, and **current clamp** from B6/B11 (duty reduced when INA240 exceeds the threshold, default 1.5 A, configurable).
6. Optional, as a stretch: a discrete LQR from the measured model (B4, B9) for comparison with the PID cascade, with gains exported to a generated header.

### Supervisor FSM
`BOOT → IDLE(driver STBY low) → CALIBRATE(gyro bias) → READY → BALANCING → FALLEN → FAULT`
- `FALLEN`: |tilt| > 35° → motors off immediately. Auto re-arm only after the robot is held upright within ±5° for 1 s.
- `FAULT`: overcurrent sustained > 100 ms, Vbat < 9.9 V (3.3 V/cell) or > 12.8 V, IMU comms error, loop overrun, or IWDG. Needs an explicit reset.
- RGB LED shows the state: e.g. blue = IDLE, green = BALANCING, yellow = FALLEN, red = FAULT. Document the mapping.

---

## 6. Telemetry and host tools

- Binary framed telemetry (COBS + CRC16) at the loop rate or decimated: tilt, gyro, wheel speeds, commands, PWM, currents, Vbat and state. Keep the existing text VCP monitor working behind a mode switch.
- `host/balancer_cli.py`: set gains and setpoint, change modes, reset fault.
- `host/live_plot.py`: pyqtgraph live view.
- Parameters in one versioned struct; print a hash at boot. Optionally persist to flash with CRC.
- Serial port from a CLI arg or env var.

---

## 7. Bring-up sequence

1. Wheels off the ground: B1–B4, direction and encoder signs, deadband.
2. Current clamp and FAULT paths tested (simulate overcurrent by threshold, pull the IMU, low-Vbat via adjustable supply).
3. Tethered balance (string or hand spotter) with the angle loop only.
4. Add the velocity loop, then check drift over 60 s.
5. Add the yaw loop.
6. Stress test B11, then tune.

---

## 8. Acceptance criteria (v1 done)

- [ ] Balances on a hard floor for **≥ 10 minutes**, drift < 20 cm without a position loop, or < 5 cm with position hold.
- [ ] Recovers from a moderate push (reproducible: pendulum-weight tap or ruler flick) ≥ 9/10 times.
- [ ] TB6612 peak current < 3.2 A and chip temperature < 80 °C in the B11 stress test.
- [ ] Falls are detected and motors cut within 50 ms. Safe re-arm works.
- [ ] Every FAULT path tested and documented.
- [ ] CI builds cleanly. `docs/` covers the pin map, measurements, tuning and the LED state table.

## 9. First response I expect from you

1. A summary of what the repo already contains (drivers, IMU, loop structure, tests), plus gaps versus this spec.
2. The Phase 0 pin plan with conflict checks.
3. Any questions blocking you. **Ask only what you can't find in the code.**
