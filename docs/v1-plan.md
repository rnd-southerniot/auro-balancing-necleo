<!-- Source of record: approved in plan mode on 2026-09-25 (three review rounds + Gate 0 INA240 update).
     Copied from ~/.claude/plans/run-users-arif-mac-developer-projects-ro-witty-lovelace.md so it travels with the repo.
     Edit THIS file from now on; the ~/.claude copy is not maintained. -->

# Self-Balancing Robot v1 (JGB37-520 + TB6612FNG) — Plan for `auro-balancing-necleo`

Plan for the prompt in `balancing-robot-v1-cc-prompt.md` (repo root). Approved 2026-09-25.

Evidence labels used throughout: **PROVEN** (read in code/docs, cited `file:line`) · **ASSUMED** (reasoned, unverified) · **UNKNOWN** (nobody has checked).

**Review changes accepted (Arif, 2026-09-25) and folded in below:** H1 center-aligned PWM (TIM1 change in Phase 2 per N3 so B3/B4 are measured once; ADC injected trigger + scope check in Phase 3) · H2 IWDG armed in Phase 2 with an ISR-health-gated refresh · M1 I2C callback copies raw bytes + seq only, the filter runs in the angle step · M2 B10 gate = worst-case angle-tick ISR < 500 µs, angle and non-angle ticks reported separately · M3 B7b tilt-consistency check before any balance attempt · M4 complementary filter parameterised by τ · M5 B1 uses the measured CPR, pass = L/R within 1 % · minor: UM1724 E5V/USB sequencing quoted in the pin map, Vbat ≥ 84-cycle sampling + 100 nF at PB1, USART2 RX IRQ below TIM10 after F1.

**Second review (N1–N5) folded in:** N1 IWDG armed after the boot gyro calibration and before `Motor_Init` / STBY release, re-calibration refreshes every 50 samples, gate = 20 power cycles with 0 IWDGRST · N2 USART2 RX → circular DMA + IDLE-line IRQ in Phase 4 together with F1, only then USART2 IRQ prio 3 · N3 TIM1 center-aligned change moved to Phase 2 · N4 current samples at |duty| < ~5 % flagged low-confidence · N5 stale references fixed (§4.1 ARR, F7 phase, E-3 rationale).

**Gate 0 update (Arif): the INA240s are NOT fitted yet.** PC3/PC4 stay reserved. Phase 3 is split: **3a now** (battery monitor, B12 tooling, stall detection hardened) and **3b when the INA240s are fitted** (injected ADC trigger + scope check, `current_sense`, clamp, B6 stall pulse, overcurrent FAULT path). TIM1 center-aligned + TRGO = UPDATE stays in Phase 2 so 3b only adds the ADC side. Interim protection from Phase 2: encoder stall detect (|duty| > 50 % and |wheel speed| < threshold for > 300 ms → STBY LOW + latched STALL fault, param-driven, host-tested), `PWM_DUTY_CAP_PCT` = 60 until 3b passes then 98, and Phases 2–4 run from a bench PSU at 12.0 V with a 3 A limit (tether lead for the Phase 4 smoke test), recorded in `docs/v1-measurements.md`. 3b is a hard prerequisite for Phase 7 free-standing and for Phase 8 (B11 I_peak); the Phase 5 overcurrent gate moves to 3b. Fitted spec: 5 mΩ shunt in-line in each motor lead, REF1 → VS, REF2 → GND, ±6.6 A at VS = 3.3 V. Remaining Gate 0 answers to follow from Arif.

---

## Context

The prompt asks to take "an existing single-motor balance prototype" on a NUCLEO-F401RE to a complete two-wheel balancer on new hardware (JGB37-520 motors, TB6612FNG driver, INA240 current sense, 3S pack), with phase gates, measurements B1–B12, a cascaded controller, a supervisor FSM, binary telemetry and host tools.

The repo does not match several of the prompt's "existing state" claims. It is a two-motor robot on a DBH-12V H-bridge with MG513P30 motors, micro-ROS over an ESP32-S3 WiFi bridge, and a 50 Hz balance loop that only runs inside the micro-ROS task. A sibling repo, `auro-balancer` (v2 rewrite on the same old hardware, sustained balance achieved, CI + tests + safety FSM), is where the prompt's CHANGELOG / VCP monitor / `usbmodem1301` facts come from. Arif settled the direction (see Decisions).

---

## Decisions locked with Arif (2026-09-25)

| # | Decision | Consequence |
|---|---|---|
| D-A | **Base = this repo**, porting `auro-balancer`'s host-tested pure-logic modules where the spec needs an FSM / monitors / tests / CI guards. Port from **`auro-balancer` branch `main` @ `5d9bc9d`**, never from its dirty working tree (`config.h`, `balance*.c` are modified there on branch `model/bench-cmd-1khz-capture`). | No reimplementation of FSM/tests/CI from scratch; no work in the sibling repo. |
| D-B | **v1 default build is serial-only.** micro-ROS + FreeRTOS-hosted ROS task become an explicit `-DMICROROS=ON` CMake option; presence of `libmicroros.a` no longer flips the build. | USART6 PC6/PC7 released (kept reserved for the bridge when `MICROROS=ON`). Balance loop must leave `microros_task`. |
| D-C | **RGB LED stays on PB12 / PB14 / PB15** as coded. PB2 stays the separate fault LED. | Prompt's PB2/PC7/PA10 claim is dropped. Polarity (anode vs cathode) is contradictory in code → bench check in Phase 0. |
| D-D | **JGB37-520 + TB6612 are already assembled and wired.** None of that wiring is in the code. | The Phase 0 pin table is a *proposal* that Arif reconciles with the as-wired list (Gate 0 input, see §4.7). |

My own engineering calls (state them once, apply throughout):

| # | Call | Why |
|---|---|---|
| E-1 | **Keep FreeRTOS in both builds** (serial + micro-ROS); shrink heap when `MICROROS=OFF`. All hard-real-time work stays ISR-driven (TIM10, prio 1), which FreeRTOS cannot disturb. | One scheduler wrapper for both builds instead of two code paths; smaller diff ("extend, don't rewrite"). Alternative considered: bare-metal superloop for the serial build — rejected for divergence risk. |
| E-2 | **No inner per-motor RPM PID in the balance path.** Cascade = velocity PI (50 Hz) → tilt offset → angle PD (500 Hz target) → torque duty → yaw mix → deadband → Vbat comp → current clamp → PWM. | JGB37 gives 1320 counts/rev vs 60 000 today. At 1 kHz one count per tick = **45 rpm** quantisation (60000/1320), so the existing 1 kHz RPM PIDs (`main.c:364-405`) are unusable. Wheel speed is estimated over 20 ms windows. |
| E-3 | **Framing: keep the existing `0xAA` + VER + TYPE + CRC16-CCITT frame, add a LEN byte (VER 0x02). No COBS.** | Encoder/decoder (`comm_protocol.c`) and the host codec (`scripts/bench_monitor.py`) are bench-proven at 921600. The MCU↔ST-Link hop is a real UART (HW-BUG-02 byte-loss history, `BENCH_LOG.md`), not an error-free USB-CDC link, so the framing must resync after corruption: sync byte + LEN + CRC16 already gives bounded resync (≤ one max frame) and rejects damaged frames. COBS would add byte-exact resync only at the cost of rewriting both ends and the test vectors. Can be added later as an additive wrapper (VER 0x03) — flagged as a deviation from the prompt's wording; decision unchanged after N5. |
| E-4 | **IMU reads become interrupt-driven (`HAL_I2C_Mem_Read_IT`), kicked from the ISR, parsed in the completion callback.** Not DMA. | PROVEN: `HAL_I2C_Mem_Read_DMA` calls the blocking `I2C_RequestMemoryRead` with a `HAL_GetTick` timeout (`stm32f4xx_hal_i2c.c:3329`); inside the prio-1 TIM10 ISR SysTick (prio 15) cannot advance, so a stuck bus would hang forever. `Mem_Read_IT` has no blocking wait (`:2963-3040`). This also honours v2's rule R1 (no blocking I2C in the control ISR). |
| E-5 | **Current sense via ADC1 injected group triggered by TIM1 TRGO = UPDATE, with TIM1 in center-aligned PWM (ARR 2099 → 20 kHz) and RCR = 1**, so one update per period lands at the counter valley = centre of the on-pulse of both channels at any duty (Arif H1). No fixed sample offset. Regular group keeps Vbat on DMA. | PROVEN macros exist: `ADC_EXTERNALTRIGINJECCONV_T1_TRGO` (`stm32f4xx_hal_adc_ex.h:179`), `TIM_TRGO_UPDATE` (`stm32f4xx_hal_tim.h:868`). ASSUMED (RM0368 detail, verify on the scope): with RCR = 1 the surviving update event is the underflow; if it lands on the peak instead, fallback = TRGO = OC4REF with CCR4 = 1 (rising edge one clock before the valley). |

---

## 1. What the repo contains today (Discovery task 1 + inventory)

Build: CMake + `cmake/arm-gcc.cmake`, STM32 HAL, FreeRTOS, `-Werror`; `firmware/CMakeLists.txt`. Flash: `st-flash write firmware/build/firmware.bin 0x08000000` (`README.md:42`). Toolchain present on this Mac: arm-none-eabi-gcc 13.3.1 at `/opt/arm-gcc/bin`, `st-flash`, `openocd`. No board attached at plan time (`/dev/tty.usb*` empty).

Clock (PROVEN `main.c:843-881`): HSI → PLL M16 N336 P4 → SYSCLK/HCLK **84 MHz**, APB1 42 (timers 84), APB2 84 (timers 84).

**IMU (PROVEN):** GY-521 module whose silicon is **ICM-20602** (WHO_AM_I 0x72, `BENCH_LOG.md:75-78`; driver accepts 0x68/0x12/0x70-0x73, `imu_mpu6050.c:110-117`). I2C1 **PB8 SCL / PB9 SDA**, AF4, 400 kHz, addr 0x68 (`stm32f4xx_hal_msp.c:148-155`, `main.c:1072`). Config: ±250 dps, ±2 g, DLPF cfg 3 (~41 Hz), 1 kHz internal (`imu_mpu6050.c:119-142`). Read: **blocking** 14-byte `HAL_I2C_Mem_Read` inside the TIM10 ISR every 5th tick = 200 Hz (`main.c:302-312`). Fusion: complementary filter α = 0.98, dt = 5 ms → **τ ≈ 245 ms** (`imu_mpu6050.c:225`). Pitch = atan2(ax, az) fused with gyro **X** (`:212-226`) — axis pairing is the code's assumption, must be re-verified on the new chassis (B7/B8). Gyro bias: 500 samples at boot, no runtime tracking (`:148-171`).

**Motor direction today (PROVEN):** DBH-12V H-bridge, **sign-magnitude on two PWM channels per motor** plus an enable GPIO — no direction GPIOs: Motor A TIM1 CH1 PA8 (fwd) / CH2 PA9 (rev) + EN PC10; Motor B TIM4 CH1 PB6 / CH2 PB7 + EN PC11 (`motor_driver.c:43-53`, `main.c:891-902`). Brake = both channels at ARR; coast = both 0 + EN low. `PWM_DEADBAND_DUTY` is defined but unused (`config.h:29`).

**Timing (PROVEN):** TIM10 PSC 83 / ARR 999 → **1 kHz** update ISR (prio 1) → `App_ControlTick` (`main.c:975-986`, `:290-557`): encoders, IMU (÷5), ADC peak-hold, Vbat, odometry, safety, inner RPM PIDs. TIM11 PSC 83 / ARR 19999 → **50 Hz** (prio 2) → telemetry, serial build only (`main.c:988-999`, `:792-797`). **The balance loop is on neither timer:** `Balance_Tick` runs at a nominal 50 Hz inside `microros_task` with `HAL_GetTick` gating and a 10 ms `vTaskDelay` (±10 ms jitter, `freertos_app.c:218-227,247`), only after the ROS agent answers pings (`:109-112,184`), and the motors only drive while `/cmd_vel` keeps arriving (`cmd_vel_sub.c:114-119`). In the serial build no balance loop runs at all.

Other peripherals (PROVEN, `stm32f4xx_hal_msp.c`, `main.c`): TIM1 PWM 20 kHz ARR 4199 (`main.c:925-949`); TIM2 encoder A PA0/PA1 32-bit; TIM3 encoder B PA6/PA7 16-bit with wrap-corrected delta (`encoder.c:27-32`); ADC1 3-channel continuous DMA scan PC3 IN13 / PB1 IN9 (Vbat, 100k/33k, scale 3.727) / PC4 IN14, DMA2 Stream0 IRQ enabled at every sweep (`msp.c:135-136`); USART2 PA2/PA3 921600 VCP; USART6 PC6/PC7 → ESP32-S3 bridge (micro-ROS only); RGB PB12/PB14/PB15 (`rgb_led.c:10-15`); fault LED PB2; LD2 PA5; button PC13 configured but never read (`main.c:912-916`).

Balance today (PROVEN `balance.c`, `config.h:150-161`): angle PID Kp 0.12 / Ki 0 / Kd 0.05 (derivative on error, unfiltered), setpoint −5.0°, fall 35°, output ±0.8 → scaled to ±150 rpm RPM setpoints. Ku = 0.12 is recorded only host-side (`scripts/pitch_monitor.py:35`, `sim/params.py:43`); Tu 1.37 s is estimated, not measured.

Serial protocol (PROVEN `comm_protocol.h`): `[0xAA][0x01][type][payload][CRC16]`, CRC-CCITT init 0xFFFF; telemetry FAST/IMU/POSE; commands RPM/POSITION/GAINS/MODE/ESTOP/AUTOTUNE/DIFF_DRIVE; host decoder `scripts/bench_monitor.py` (pyserial, `--port` autodetect). On-MCU relay auto-tune is for **wheel RPM** (`autotune.c`), not balance. `scripts/pitch_monitor.py` (oscillation detection + Z-N) reads pitch over **SSH + `ros2 topic echo`**, not serial.

Hardware constraints (PROVEN `PORTING_NOTES.md:142-152`): SB13/SB14 ON (VCP), **SB62/SB63 OPEN**, SB15 ON (**PB3 = SWO, do not use**), SB21 ON (PA5 = LD2), SB17 ON (PC13). "21 GPIO remaps" in the prompt: the log has 15 remapped + 3 preserved rows (`PORTING_NOTES.md:46-70`); no list of 21 exists.

Docs on disk: `README.md` (stale: says dual-channel PID controller), `PORTING_NOTES.md`, `PIN_ASSIGNMENTS.md` (stale: PB8/PB9, PC6/PC7, PB12 listed AVAIL), `HARDWARE_BRINGUP.md`, `HARDWARE_STATUS.md`, `BENCH_LOG.md`, `bench_test_log.md`, `ARCHITECTURE.md`, `AUDIT.md`, `MIGRATION_PLAN.md`, `docs/hw_tune_01_plan.md`.

## 2. Gaps versus the v1 spec

| Spec item | State | Evidence |
|---|---|---|
| TB6612 driver (PWM + AIN/BIN + STBY) | Absent; DBH-12V sign-magnitude driver only | `motor_driver.c` |
| 1320 CPR encoders | 60 000 CPR constants | `config.h:17-21` |
| INA240 current sense synchronous with PWM | DBH CT peak-hold, free-running ADC | `main.c:314-341` |
| Angle loop 200–500 Hz | 50 Hz, task-context, ±10 ms jitter, micro-ROS only | `freertos_app.c:218-227` |
| Velocity / yaw outer loops, deadband / Vbat comp, current clamp | None (lean-to-drive only) | `balance.c:109-120` |
| Supervisor FSM BOOT…FAULT, auto re-arm | 3-state OFF/ON/FAULT, no re-arm path; `safety.c` checks Motor A only | `balance.h:22-26`, `main.c:354` |
| IWDG | Init commented out; `heartbeat_task` refreshes an **uninitialised** `hiwdg` | `main.c:151`, `freertos_app.c:70` |
| DWT loop timing | None | grep |
| Params struct + boot hash, flash persistence | None | grep |
| Binary telemetry incl. tilt/currents/state at loop rate | 50 Hz FAST/IMU/POSE, no balance frame, no gain command for balance (`Balance_SetGains` has no caller) | `comm_protocol.h`, `balance.c:78` |
| `host/balancer_cli.py`, `host/live_plot.py`, `host/measure/`, `host/logs/` | None (`scripts/` only, no requirements file) | tree |
| `docs/v1-*.md`, LED table | None | tree |
| Host tests, CI, CHANGELOG.md | `tests/` referenced by CMake but missing; no `.github/`; no CHANGELOG | `CMakeLists.txt:40-53` |

## 3. Findings that change the design

| # | Finding | Label | Impact |
|---|---|---|---|
| F1 | Command dispatch **and** a blocking 5 ms `HAL_UART_Transmit` run inside the USART2 RX ISR (prio 0, above the control ISR) | PROVEN `main.c:753, 800-808`, `msp.c:57` | Move command processing to a task in Phase 4; ACK/telemetry share one TX ring |
| F2 | ADC DMA HT/TC IRQ fires every 3-sample sweep (ADC clk 21 MHz, 96 cyc/ch → ≈73 k sweeps/s) | IRQ enabled PROVEN `msp.c:135-136`; rate ASSUMED (arithmetic) | Hidden CPU load; quantify in B10, fix in Phase 3 (larger buffer or mask DMA IRQ) |
| F3 | 1320 CPR makes the 1 kHz RPM estimate quantise at 45 rpm | PROVEN arithmetic on `encoder.c:41` | E-2 |
| F4 | Blocking I2C in the control ISR; DMA variant also blocks | PROVEN (E-4) | E-4 |
| F5 | ICM-20602 DLPF cfg 3 (~41 Hz, ~5.9 ms group delay) and ±250 dps are hard-coded | PROVEN `imu_mpu6050.c:130,135`; delay figure ASSUMED from datasheet | Make DLPF/FS params; default DLPF 2 / ±500 dps, verify in B7 |
| F6 | RGB polarity contradiction: header says common-anode LOW=ON, code drives SET=ON | PROVEN `rgb_led.h:5` vs `rgb_led.c:17-19` | Bench check in Phase 0; `bench_test_log.md:25` says green never showed |
| F7 | `hiwdg` refreshed without init (null instance) | PROVEN | Phase 2 (H2) |
| F8 | `libmicroros.a` ×3 (≈36 MB) and `include_jazzy/` are git-tracked despite `.gitignore` | PROVEN `git ls-files` | Leave alone in v1 (not in scope); CI can build `MICROROS=ON` compile-only |
| F9 | The old "IWDG reset loop" that got `MX_IWDG_Init` commented out (`main.c:151`) has two visible causes: timeout 200 ms but the only refresh ran every 500 ms, and the boot gyro calibration (500 ms settle + 500 samples at 1 ms, `main.c:268-272`, `imu_mpu6050.c:148-166`) ran with no refresh at all | PROVEN `config.h:100`, `freertos_app.c:70-71`, `main.c:268-272` | Phase 2 (H2, N1): IWDG armed after the boot calibration and before `Motor_Init` / STBY release; refresh 50 ms < 200 ms, gated on ISR health |

---

## 4. Phase 0 — Pin plan (draft for `docs/v1-pinmap.md`; Gate 0 = Arif confirms)

### 4.1 Fixed / reserved (unchanged, PROVEN from code)

| Pin | Function | Note |
|---|---|---|
| PA2 / PA3 | USART2 VCP TX/RX, 921600 | SB13/SB14 ON; **SB62/SB63 OPEN** (never route PA2/PA3 to D1/D0) |
| PA13 / PA14 | SWDIO / SWCLK | reserved |
| PB3 | SWO (SB15 ON) | **do not use** |
| PA5 | LD2 heartbeat | SB21 |
| PC13 | B1 button → v1: arm / disarm / fault-reset (long press) | SB17 |
| PA0 / PA1 | TIM2 CH1/CH2 encoder A (32-bit) | AF1 |
| PA6 / PA7 | TIM3 CH1/CH2 encoder B (16-bit) | AF2 — **no new timer needed for Motor B** |
| PA8 / PA9 | TIM1 CH1 = **PWMA**, CH2 = **PWMB** (20 kHz, center-aligned, ARR 2099 — today edge-aligned ARR 4199) | fixed by prompt; Motor B PWM moves off TIM4 |
| PB8 / PB9 | I2C1 SCL/SDA → GY-521 (ICM-20602) | AF4, 400 kHz |
| PB1 | ADC1_IN9 Vbat divider | 100k/33k → 13.0 V → 3.23 V (74 mV under 3.3 V; acceptable, tight) |
| PC3 / PC4 | ADC1_IN13 / IN14, **reserved for the INA240 A / B outputs (not fitted yet — Phase 3b)** | same pins the DBH CT used; nothing else may take them |
| PB12 / PB14 / PB15 | RGB R / G / B | D-C |
| PB2 | Fault LED | also driven by HardFault / hooks |

### 4.2 Released by v1

PC10, PC11 (DBH EN) · PB6, PB7 (TIM4 PWM B) · PC6, PC7 (USART6; reserved for the ESP32 bridge only when `MICROROS=ON`).

### 4.3 Proposed new assignments (proposal — the as-wired list from Arif overrides)

| Signal | Proposed pin | Header | Rationale | As-wired (Arif) |
|---|---|---|---|---|
| AIN1 | PC10 | CN7-1 | already a GPIO output that resets LOW (`main.c:891-897`) | ? |
| AIN2 | PC11 | CN7-2 | same (`:899-902`) | ? |
| BIN1 | PC12 | CN7-3 | adjacent, free | ? |
| BIN2 | PD2 | CN7-4 | adjacent, free → all four direction lines on CN7 pins 1–4 | ? |
| STBY | PC8 | CN10-2 | free; **external 10 k pull-down** so the driver is off before firmware runs; driven LOW first thing in `MX_GPIO_Init` | ? |
| IMU INT (optional) | PB10 | CN10-25 | EXTI15_10, only if the GY-521 INT pin is wired | ? |
| (TIM1 TRGO) | *no pin* | — | update event at the counter valley → ADC injected trigger; no CH4 needed, PA11 stays free | n/a |

Still free after this plan: PA4, PA10, PA11, PA12, PA15, PB0, PB4, PB5, PB6, PB7, PB13, PC0, PC1, PC2, PC5, PC9 (+ PC6/PC7 when micro-ROS is off).

### 4.4 Timer plan

| Timer | v1 role |
|---|---|
| TIM1 | **Center-aligned** PWM A/B, ARR 2099 → 20 kHz, RCR 1; TRGO = UPDATE at the counter valley = ADC injected trigger (E-5, H1). `PWM_MAX_DUTY` becomes 98 % of 2099 = 2057 |
| TIM2 | Encoder A (32-bit) |
| TIM3 | Encoder B (16-bit; wrap safe at ≤ 4 000 counts/s ≈ 8 s per wrap vs 1 kHz reads) |
| TIM10 | 1 kHz base ISR (prio 1): encoders, current, Vbat, supervisor; angle loop every `ANGLE_DIV` ticks (2 → 500 Hz, or 5 → 200 Hz, chosen from B10); outer loops every 20 ticks |
| TIM11 | 50 Hz telemetry decimation (as today) |
| TIM4 | **freed** — spare encoder-capable timer on PB6/PB7 (fallback if Arif wired encoder B there) |
| TIM5 / TIM9 | free, time-base only (their channel pins PA0–PA3 are taken) |

Conflict check (prompt §3): encoder B on TIM3 PA6/PA7 touches neither SB62/SB63 (PA2/PA3), the RGB pins, VCP, nor TIM10/11 (no pins). TIM4 on PB6/PB7 is the only other encoder-capable pair on this package (CH3/CH4 = PB8/PB9 are the IMU).

### 4.5 ADC plan

- Regular group: IN9 Vbat continuous DMA (existing) → shrink IRQ load (F2).
- Injected group: IN13 (INA240 A), IN14 (INA240 B); trigger `T1_TRGO` rising with TIM1 MMS = UPDATE, center-aligned, RCR = 1 → one sample per PWM period at the counter valley, i.e. the centre of the on-pulse for both channels regardless of duty (H1); 28-cycle sampling; read in the 1 kHz ISR (`HAL_ADCEx_InjectedGetValue`), JEOC IRQ only during the B6 stall-pulse burst capture. Scope check: trigger instant vs both PWM outputs high, at 10 / 50 / 90 % duty.
- Low-duty samples (N4): at |duty| < ~5 % the on-pulse is < 2.5 µs, shorter than the INA240's settling after a PWM edge (ASSUMED from its 400 kHz bandwidth) plus the 28-cycle sample window, so those samples are flagged **low-confidence** per motor (`CS_MIN_DUTY_PCT`, param, default 5 %): the flag travels in `TELEM_BAL`, such samples are excluded from the clamp and from the overcurrent debounce, and the behaviour is documented in `docs/v1-telemetry.md` and in the B6 section.
- Regular group Vbat: keep ≥ 84-cycle sampling (the 100k/33k divider has ~25 kΩ source impedance) **and** add 100 nF from PB1 to GND at the divider.
- INA240A2 = 50 V/V. **Build spec from Arif (parts not fitted yet, Phase 3b):** 5 mΩ shunt in-line in each motor lead, REF1 → VS, REF2 → GND (bidirectional, zero at VS/2 = 1.65 V) → 0.25 V/A, **±6.6 A** full scale at VS = 3.3 V; `INA240_SHUNT_MOHM = 5`, `INA240_REF_MV = 1650`; resolution 0.806 mV/LSB ÷ 0.25 V/A ≈ 3.2 mA/LSB. Verified in 3b against a clamp meter. Until then there is no current measurement on the robot at all — see the interim protection in Phase 2.

### 4.6 Power path (Mermaid in the doc)

Battery 3S (11.1 V nom / 12.6 V full, XT60) → blade fuse (ASSUMED 7.5 A fast; final from B6/B11 peak) → power switch → bulk low-ESR electrolytic ≥ 1000 µF 25 V + 100 nF at TB6612 VM + TVS as spike absorber (ASSUMED SMBJ13A; note any TVS clamps above the 15 V abs-max at high current, so the bulk cap does the real work — verify ringing on scope in B11) → **TB6612 VM** (≤ 13.5 V operating, PROVEN per prompt table / datasheet range). Buck → **5 V → Nucleo E5V (CN7-6) with JP5 in the E5V position** — not VIN (UM1724 gives VIN 7–12 V; a fresh 3S at 12.6 V exceeds it; ASSUMED from the user manual). TB6612 VCC = Nucleo 3V3; INA240 VS = 3.3 V; common ground star at the TB6612.

**UM1724 E5V/USB sequencing (Phase 0 doc task):** `docs/v1-pinmap.md` quotes ST's note on powering the board from E5V while the ST-Link USB is connected (JP5 position, required order of applying E5V vs USB) verbatim from UM1724 §6.3, citing the revision. Source = a local PDF if one is on disk, else st.com; not paraphrased from memory.

### 4.7 Gate 0 inputs I need from Arif (not in the code)

1. AIN1 / AIN2 / BIN1 / BIN2 / STBY: actual Nucleo pins (PXn or CN7/CN10 pin numbers).
2. PWMA = PA8 and PWMB = PA9 as stated? Encoder B still on PA6/PA7?
3. **Answered:** INA240 not fitted yet. When fitted: two, 5 mΩ shunt in-line per motor lead, REF1 → VS / REF2 → GND, VS 3.3 V, outputs on PC3 / PC4.
4. Battery divider: the same 100k/33k on PB1, or rebuilt?
5. Nucleo power: buck 5 V → E5V (JP5) or VIN?
6. STBY pull-down present? One TB6612 module or two (paralleled fallback)?
7. Same GY-521 on PB8/PB9? INT pin wired anywhere?
8. RGB: common anode or common cathode (F6).

---

## 5. Phases 1–8

Conventions for every phase: branch `phase/v1-<n>-<slug>`, conventional commits with the phase id, `CHANGELOG.md` entry, one PASS/FAIL gate with an exact expected outcome, rollback = `git revert` of that phase's commits.

### Phase 1 — Build / CI / test scaffolding + micro-ROS opt-in
- **Goal:** default build = serial-only firmware; `-DMICROROS=ON` = today's micro-ROS build; host tests exist and pass; CI green; bench behaviour unchanged.
- **Modify:** `firmware/CMakeLists.txt` (`:110-150` replace the `EXISTS libmicroros.a` autodetect with `option(MICROROS OFF)`, FATAL if ON and lib missing; replace `file(GLOB CORE_SRCS)` `:120` with an explicit list that excludes `microros_*.c`, `cmd_vel_sub.c`, `ros_publishers.c` when OFF; inject `FW_GIT_SHA`); `FreeRTOSConfig.h:27` heap 45 KB → 12 KB when `MICROROS=OFF` (E-1); `.gitignore` (`host/logs/*.csv`).
- **Add:** `firmware/tests/test_pid.c` (port of v2 `tests/test_pid.c` adapted to this repo's `PID_t`), `firmware/tests/test_comm_protocol.c` (CRC known-answer, encode→`Comm_ReceiveByte` round-trip per type, garbage-prefix resync, CRC-fail rejection); `.github/workflows/firmware-ci.yml` (lint → host-test → ARM build `MICROROS=OFF` and `ON` compile-only → size budget), `scripts/check_secrets.sh`, `scripts/check_size_budget.sh` (v2 verbatim / budgets adjusted), `CHANGELOG.md`, `docs/v1-build.md`.
- **Reuse:** existing `BUILD_TESTS` branch + Unity FetchContent (`CMakeLists.txt:19-58`); v2 `tests/CMakeLists.txt:22-56` macro pattern.
- **Gate:** `cmake -S firmware -B firmware/build -DCMAKE_TOOLCHAIN_FILE=$(pwd)/firmware/cmake/arm-gcc.cmake && cmake --build firmware/build` → `firmware.elf` with **no** `rclc_` symbols; `-DMICROROS=ON` links with them; `ctest --test-dir firmware/build-test --output-on-failure` → 2/2 pass; flashed default build: LD2 blinks, `scripts/bench_monitor.py` decodes TELEM_FAST at 50 Hz and `--cmd keepalive` returns ACK; CI green.

### Phase 2 — TB6612 driver + 1320 CPR + test modes for B1–B5
- **Goal:** both motors drive through the TB6612 with the confirmed pins; STBY low at boot; **IWDG armed before the TB6612 ever drives a motor (H2)**; **interim protection live before the first drive command: encoder stall detect + `PWM_DUTY_CAP_PCT` 60 (INA240 not fitted)**; all Phase 2–4 motion from a bench PSU at 12.0 V with a 3 A current limit; B1–B5 measured and logged.
- **Add:** `firmware/Core/Inc/pins.h` (single pin source; mirrors `docs/v1-pinmap.md`; checked by a ported `scripts/check_pin_consistency.py`); `Core/Src/motor_tb6612.c` + `Inc/motor_tb6612.h` (same `Motor_t` / `MotorDir_t` / `Motor_Set / Coast / Brake / EmergencyStop` shape as `motor_driver.h` so `main.c` call sites compile; adds IN1/IN2 pins and `Motor_StandbySet(bool)`; drops the one-tick zero-output on direction change (`motor_driver.c:34-40`) because the TB6612 has internal shoot-through protection and that tick is a deadband at upright; documents that PWM-low with IN1≠IN2 = short-brake on this chip — ASSUMED from datasheet, verified with the scope in the gate); `Core/Src/test_modes.c/.h` (`CTRL_TEST` mode: `TEST_DUTY`, `TEST_RAMP` to first motion, `TEST_BRAKE_HOLD`; results via new `MSG_RESP_TEST_RESULT`); `host/auro_proto.py` (codec + port autodetect factored out of `scripts/bench_monitor.py:49-176`; port from `--port` or `AURO_PORT`), `host/measure/b01_encoder_cpr.py … b05_backlash.py`, `host/logs/.gitkeep`, `docs/v1-measurements.md` skeleton (B1–B12 sections: method, data, fitted value, plot; plus a "Bench conditions" section: PSU 12.0 V / 3 A limit, battery disconnected, duty cap 60 %, tether lead for the Phase 4 smoke test); **`Core/Src/stall_detect.c` + `Inc/stall_detect.h`** (pure logic, per-motor instance, host-testable: trips when |duty| > `STALL_DUTY_PCT` (50) **and** the 20 ms encoder delta is below `STALL_SPEED_RPM` (5, converted to counts per 20 ms from the measured CPR at init) for longer than `STALL_MS` (300); extracted from the Motor-A-only check in `safety.c:85-93`, which uses `pwm_duty > SAFETY_STALL_PWM_THRESH && |rpm| < 1.0` for 500 ms (`config.h:73-74`) and stops only Motor A (`safety.c:99`); the 1 kHz IIR rpm is not usable as the speed input at 1320 CPR — F3), **`tests/test_stall_detect.c`** (trips at exactly 300 ms only while both conditions hold, timer resets when either clears, instances independent, threshold edge cases).
- **Modify:** `config.h:17-21` (PPR 11, CPR placeholder 1320 until B1 replaces it with the measured value, `ENC_A_SIGN/ENC_B_SIGN`), **`main.c` `MX_TIM1_Init:925-949` (N3/H1: `CounterMode = TIM_COUNTERMODE_CENTERALIGNED1`, `Period = 2099`, `RepetitionCounter = 1`, `HAL_TIMEx_MasterConfigSynchronization(TIM_TRGO_UPDATE)`, CCR preload on so duty changes latch once per period) and `config.h:26-27` (`PWM_ARR` → 2099, `PWM_MAX_DUTY` → 2057, both derived; `PWM_FREQ_HZ` stays 20 kHz)**, `main.c` (drop TIM4 init `:141-144,167-168,1108-1135`; `Motor_Init` with new pins `:204-205,233`; `MX_GPIO_Init` `:891-902` from `pins.h`, STBY LOW first), `stm32f4xx_hal_msp.c:211-223` (drop TIM4), `comm_protocol.h/.c` (`MSG_CMD_TEST 0x50`, `MSG_RESP_TEST_RESULT 0x51`, `MSG_CMD_FAULT_RESET 0x58`), dispatch `main.c:564-740`, CMake source list (drop `motor_driver.c`; keep the file until Phase 8); **interim protection:** `config.h` (`PWM_DUTY_CAP_PCT 60` — the effective ceiling inside `Motor_Set` becomes min(`PWM_MAX_DUTY`, `PWM_ARR` × cap / 100) = 1259 counts until the 3b gate passes, then 98 %; `STALL_DUTY_PCT 50`, `STALL_SPEED_RPM 5`, `STALL_MS 300`), `App_ControlTick` (stall detector ticked for both motors every 1 ms right after the encoder updates; on trip → `Motor_StandbySet(false)` so STBY is LOW, both motors coast, `FAULT_STALL_A/B` latched in the existing `s_fault_flags` (`safety.c:16`) with `Safety_Tick` extended to both motors as the interim safety until the Phase 5 supervisor absorbs it, PB2 on; cleared only by `MSG_CMD_FAULT_RESET` or the PC13 long press, never by time).
- **IWDG (H2, F9, N1):** enable `MX_IWDG_Init` (`main.c:137-146`, `:1137-1146`) at 200 ms, **armed after the boot gyro calibration (`main.c:268-272`, about 1 s with no refresh) and before `Motor_Init` / STBY release**, still before the scheduler starts; `__HAL_DBGMCU_FREEZE_IWDG()` so debugger halts do not reset; refresh from `heartbeat_task` every **50 ms** (was 500 ms — the reset-loop cause), and only if `g_tick_ms` advanced since the previous refresh (ISR-health gate: a dead TIM10 ISR lets the dog bite); any re-calibration while the dog is armed (Phase 5 CALIBRATE state) refreshes every 50 samples through one dedicated hook so the refresh call sites stay enumerable; from Phase 5 the refresh is additionally gated on the supervisor tick. Add `TEST_HANG` (spin with IRQs masked) to `test_modes.c`, and report the `RCC_FLAG_IWDGRST` flag in the boot banner then clear the reset flags.
- **Reuse:** `encoder.c` wrap logic, TIM2/TIM3 inits; TIM1 init modified in place (N3).
- **Gate (wheels off the ground, bench PSU 12.0 V with a 3 A current limit, battery disconnected):** DMM shows STBY LOW from power-on until the first command; scope: PWM = 20.0 kHz center-aligned after the TIM1 change, a 50 % duty pulse is centred on the counter valley (N3); IWDG: `TEST_HANG` → reset within ≤ 200 ms and the boot banner reports the IWDGRST flag, **20 power cycles → 0 IWDGRST (N1)**, no spurious reset over 10 min with motors running in `TEST_DUTY`; duty cap: a `TEST_DUTY` request of 90 % yields 60 % on the scope (1259 / 2099 counts) and the capped flag in telemetry; stall: a wheel held by hand at 55 % duty → STBY LOW on the DMM and `FAULT_STALL_x` within 300–320 ms, a free-running wheel at 55 % does not trip over 60 s, reset works via `MSG_CMD_FAULT_RESET` and via PC13; the PSU never enters current limit during B1–B5 (CC indicator stays off); B1 (M5): CPR measured on both wheels (10 hand turns each) and the measured value becomes `ENCODER_CPR_A/B` (1320 is the datasheet expectation, not the pass criterion), pass = left and right agree within 1 % with forward positive on both; B2 +duty → both wheels drive the robot forward; B3 deadband per motor per direction, 5 repeats, σ < 1 % duty; B4 10 steps × 2 directions with L/R gain mismatch reported (B3/B4 measured once, in the final center-aligned PWM mode — N3); B5 backlash in wheel degrees; CSVs in `host/logs/`, sections filled; ctest green.

### Phase 3a — Battery monitor, B12 tooling, stall protection hardened (now)
- **Goal:** battery monitor with the spec thresholds and PSU-calibrated Vbat; B12 test mode + script; the Phase 2 stall detector promoted to a host-tested module covering both motors; no current-sense code yet (INA240 not fitted).
- **Add (3a):** `batt_monitor.c/.h` (v2 verbatim; thresholds 9.9 V low / 12.8 V high / 0.2 V hysteresis / 100 ms debounce / 1 V implausible floor), `tests/test_batt_monitor.c` (v2), `tests/test_stall_detect.c`, `host/measure/b12_batt_sag.py`, a "Bench conditions" section in `docs/v1-measurements.md` (bench PSU 12.0 V / 3 A limit for Phases 2–4, battery only from 3b, duty cap 60 %).
- **Modify (3a):** `main.c` `MX_ADC1_Init:1034-1066` (regular DMA buffer → 20 sweeps, read last, or mask the DMA IRQ — F2; Vbat sampling stays ≥ 84 cycles, 100 nF added at PB1), `App_ControlTick:341` (Vbat → `batt_monitor`), `telemetry.c` (battery state + stall flags), `config.h` (battery thresholds, stall params).
- **Gate (3a):** Vbat within ±0.1 V of the DMM at PSU 10.0 / 11.1 / 12.5 V; LOW asserts ≤ 120 ms at 9.8 V and clears with hysteresis at 10.1 V; HIGH asserts at 12.9 V; B12 script validated on the PSU (log format, 100 Hz); a preliminary sag capture on the battery is allowed only with the duty cap at 60 %, stall detect armed and load pulses ≤ 250 ms — the definitive B12 curve is captured with B11 in 3b; `test_stall_detect` + `test_batt_monitor` green.

### Phase 3b — INA240 current sense (deferred until the INA240s are fitted; hard prerequisite for Phase 7 free-standing and Phase 8)
- **Goal:** per-motor current sampled at the PWM valley; software current clamp; stall-pulse test mode; overcurrent FAULT path. TIM1 center-aligned + TRGO = UPDATE is already in place from Phase 2, so 3b adds only the ADC side.
- **Add:** `current_sense.c/.h` (raw→A with boot zero-cal at STBY low, 1 ms mean + peak-hold, per-motor **low-confidence flag when |duty| < `CS_MIN_DUTY_PCT` (N4)** that excludes the sample from the clamp and the overcurrent debounce, `cs_clamp_duty()` proportional reduction + trip-ms counter; adapted from v2 `ct_sense.c:35-43,71-90`), `tests/test_current_sense.c`, `host/measure/b06_stall_pulse.py`, `b11_stress_log.py`.
- **Modify (3b):** `main.c` `MX_ADC1_Init` (+ `HAL_ADCEx_InjectedConfigChannel` ×2, trigger T1_TRGO rising, 28-cycle), `App_ControlTick:314-341` (replace CT peak-hold), `config.h:89-91` (→ `INA240_GAIN 50`, `INA240_SHUNT_MOHM 5`, `INA240_REF_MV 1650`, `CURRENT_LIMIT_A 1.5`, `CURRENT_TRIP_MS 100`, `CS_MIN_DUTY_PCT 5`; no sample-offset parameter), `telemetry.c` (low-confidence flags in the current fields), `test_modes.c` (`TEST_STALL_PULSE(motor, duty, ms ≤ 200)` with a 4000-sample RAM burst at 20 kHz → `MSG_TELEM_DUMP`); the supervisor's OC_A / OC_B LATCH inputs (Phase 5) get wired to `current_sense` here; `PWM_DUTY_CAP_PCT` 60 → 98 only after the 3b gate passes.
- **Gate (3b):** the injected trigger instant (a GPIO toggled in the JEOC ISR during the check) sits at the centre of the on-pulse of both PWMA and PWMB at 10 / 50 / 90 % duty — if it sits at the peak instead, apply the OC4REF fallback from E-5 and re-check; at 3 % duty the low-confidence flag is set and at 8 % it is clear (N4); zero-offset within ±20 mA; free-running current vs clamp meter within ±10 %; B6 stall pulse ≤ 200 ms → CSV + fitted τ_e and I_stall; `CURRENT_LIMIT_A = 0.3` visibly limits duty with the clamp flag set; **overcurrent FAULT path (moved here from Phase 5): `CURRENT_LIMIT_A = 0.2` with a loaded wheel → FAULT in 100–110 ms and STBY LOW on the DMM, logged in `docs/v1-fault-paths.md`**; ctest green. Then raise `PWM_DUTY_CAP_PCT` to 98 and re-run B4 at full duty.

### Phase 4 — Loop restructure: ISR angle loop at 200–500 Hz, IRQ-driven IMU, DWT timing, B7/B8/B10
- **Goal:** angle loop runs from the TIM10 ISR at `CONTROL_ANGLE_HZ` with non-blocking IMU reads; command processing out of the RX ISR (F1); loop timing measured.
- **Add:** `imu_filter.c/.h` (from v2, **parameterised by τ (M4)**: `IMU_FILTER_TAU_S` default 0.245 s, α = τ/(τ + dt) computed at init from the actual angle-loop dt) + `tests/test_imu_filter.c` (α for dt = 2 ms and 5 ms, step response reaches 63 % at τ); `loop_timing.c/.h` (DWT CYCCNT: per-section max/mean/last, tick-to-tick jitter, 16-bin histogram, overrun flag, **angle ticks and non-angle ticks accumulated separately (M2)**); `host/measure/b07_imu_static.py`, **`b07b_tilt_consistency.py` (M3)**, `b08_balance_point.py`, `b09_com.py`, `b10_loop_timing.py`.
- **Modify:** `imu_mpu6050.c` (split `IMU_ReadAll:176-207` into `IMU_StartReadIT()` [BUSY pre-check, `HAL_I2C_Mem_Read_IT`] + `HAL_I2C_MemRxCpltCallback` [**M1: copies the 14 raw bytes into a double buffer and increments `seq` — nothing else**]; the TIM10 angle step converts the latest buffer and runs the filter; **an unchanged `seq` since the previous angle step counts as an IMU miss**, N consecutive misses → IMU_COMMS fault; `HAL_I2C_ErrorCallback` → same counter; keep the blocking read only for boot calibration `:148-174`; params `IMU_DLPF_CFG` (default 2), `IMU_GYRO_FS` (±500 dps), axis/sign selects `:212-222`); `stm32f4xx_hal_msp.c:157-160` (I2C1 EV/ER prio 1 → 2, below TIM10); **N2: USART2 RX → circular DMA (DMA1 Stream5 Channel 4 — ASSUMED from the RM0368 request map, verify) + IDLE-line IRQ**, modelled on the USART6 ring in `microros_transport.c:17-33` (`HAL_UARTEx_ReceiveToIdle_DMA` if this HAL revision provides it — UNKNOWN, else a manual `UART_IT_IDLE` handler); the IRQ only notifies the command task, which drains the ring through `Comm_ReceiveByte`; **only after that** `:57` USART2 IRQ prio 0 → 3 below TIM10 (a per-byte RX interrupt under a ≤ 500 µs control ISR would overrun at 921600, one byte every 10.9 µs); `main.c App_ControlTick:290-557` (1 kHz housekeeping; `tick % ANGLE_DIV == 0` → angle step; `tick % 20 == 0` → outer step; IMU kick at the end of the angle step; `loop_timing` wrap); `main.c:800-808` (the per-byte `HAL_UART_RxCpltCallback` path is retired; `App_ProcessCommand` runs in the FreeRTOS command task fed from the DMA ring; ACK via the TX ring, blocking `:753` removed); `balance.c` (`Balance_Tick(dt)` from ISR, output = duty in [−1, 1]; keep setpoint/fall logic `:98-107`); `pid.c/.h` (add derivative-on-measurement + `ki == 0` guard, adapted from v2 `pid.c:128-135`); `config.h` (`CONTROL_ANGLE_HZ 500`, drop `BALANCE_LOOP_MS`); telemetry `MSG_TELEM_TIMING 0x13` + raw-IMU stream mode.
- **Gate:** **B10 (M2)** at 500 Hz: worst-case ISR time on an **angle tick < 500 µs** (50 % of the 1 ms TIM10 period), angle ticks and non-angle ticks reported separately (max / mean / histogram), 0 overruns in 60 s (if it fails, `ANGLE_DIV = 5` → 200 Hz and re-measure); **byte-loss regression:** 10 000 KEEPALIVE frames at max rate → 10 000 ACKs, run with the RX DMA ring active and USART2 at prio 3 (the HW-BUG-02 class, `BENCH_LOG.md`; N2); B7: 60 s static log → gyro bias, accel σ, post-bias drift < 0.5°/min; **B7b (M3), before any balance attempt:** slow ±20° hand tilt about the wheel axle → accel-derived angle and integrated gyro agree in sign and magnitude within ~1° over the sweep; if not, the axis pairing in `IMU_UpdateAngle` (`imu_mpu6050.c:212-226`, atan2(ax, az) with gyro X) is wrong and gets fixed before proceeding; B8: balance point measured, setpoint updated from data, sign convention documented; smoke (only after B7b passes): tethered angle-only hold ≥ 10 s.

### Phase 5 — Supervisor FSM + IWDG + fault paths + LED map
- **Goal:** the supervisor owns STBY and motor authority; every FAULT path demonstrated; IWDG armed.
- **Add:** `supervisor.c/.h` (pure logic, host-testable: BOOT → IDLE → CALIBRATE → READY → BALANCING → FALLEN → FAULT; inputs {tilt, imu_ok, batt_state, oc_ms, overrun, wdg_reset, arm/disarm/reset/calibrate}; outputs {motors_allowed, stby, led_state, faults}; LATCH mask = OC_A/OC_B (inputs wired in 3b)/STALL_A/STALL_B (from the Phase 2 stall detector)/BATT_LOW/BATT_HIGH/IMU_COMMS/LOOP_OVERRUN/WDG_RESET/ESTOP; FALLEN → auto re-arm after |tilt − sp| < 5° for 1000 ms; saturating ms counters — modelled on v2 `safety_fsm.c` masks `:150-189`, `add_ms_saturating` `:62-70`, `Tick` `:233-269`, `RequestClearLatch` `:183-197`); `monitors.c` (glue; PC13 2 s long-press arm/disarm/reset; IWDG-reset detection via `RCC_FLAG_IWDGRST` → FAULT then clear flags); `tests/test_supervisor.c` (from v2 `test_safety_fsm.c` pattern); `docs/v1-led-states.md`, `docs/v1-fault-paths.md`.
- **Modify:** IWDG refresh (armed in Phase 2, H2) additionally gated on the supervisor tick having run; `App_ControlTick` motor write gated by `Supervisor_MotorsAllowed()`; STBY from the supervisor; `safety.c/.h` removed from the build (fault bits re-exported for telemetry compat); `rgb_led.c/.h` (+ yellow, white, blink variants; BOOT = white, IDLE = blue, CALIBRATE = white blink, READY = green blink, BALANCING = green, FALLEN = yellow, FAULT = red 5 Hz; polarity per Phase 0); `MSG_CMD_SUPERVISOR 0x52`; `config.h` thresholds.
- **Gate (each logged with timestamps in `docs/v1-fault-paths.md`):** stall (interim detector): wheel held at 55 % duty → STALL fault within 300–320 ms and STBY LOW on the DMM; overcurrent path is tested in 3b once the INA240s are fitted; 9.8 V → FAULT ≤ 120 ms; 12.9 V → FAULT; SDA pulled → FAULT ≤ 50 ms; `TEST_BUSY_LOOP` (1.5 ms in ISR) → LOOP_OVERRUN FAULT; `TEST_HANG` → IWDG reset ≤ 200 ms, boots into FAULT with the WDG bit; tilt > 35° → FALLEN with PWM = 0 within one angle tick, upright 1 s → READY → BALANCING; reset via CLI and via button; LED table matches; `test_supervisor` ≥ 95 % line coverage.

### Phase 6 — Telemetry v2 + versioned params + host tools
- **Goal:** loop-rate binary telemetry with every spec field; one versioned `Params_t` with a boot hash; `balancer_cli.py` + `live_plot.py`.
- **Add:** `params.c/.h` (`Params_t` v1: angle kp/ki/kd/d_fc, vel kp/ki/tilt_limit, yaw kp/ki, setpoint, deadband ×4, vbat_nom, current_limit, angle_hz, imu_dlpf, crc32; X-macro `params.def` shared with `host/gen_param_ids.py`; boot prints `PARAMS v<n> hash=<crc32>`), `telem_ring.c` (2 KB TX ring + DMA chunks replacing the single buffer in `telemetry.c:118-121`; fixes the ACK/telemetry collision), `tests/test_params.c`, `host/balancer_cli.py` (get/set/mode/arm/disarm/reset/test/dump; `--port` / `AURO_PORT`), `host/live_plot.py` (pyqtgraph + CSV record), `host/requirements.txt` (pyserial, numpy, pyqtgraph), `docs/v1-telemetry.md`.
- **Modify:** `comm_protocol.h/.c` (LEN byte, VER 0x02, `MSG_TELEM_BAL 0x14` {t_us, tilt, rate, tilt_sp, wl_cps, wr_cps, v_cmd, yaw_cmd, pwm_l, pwm_r, i_a, i_b, vbat, state, faults, loop_us, seq}, `PARAM_SET/GET/RESP 0x53–0x55`, `CMD_SETPOINT 0x56`, `TELEM_RATE 0x57`), `telemetry.c` (TELEM_BAL enqueued from the ISR every `TELEM_DIV` angle ticks, default 5 → 100 Hz; legacy 50 Hz frames kept behind a `CMD_TELEM_MODE` switch = "existing monitor behind a mode switch"), `scripts/bench_monitor.py` (VER 0x02 LEN).
- **Optional 6b (flash persistence):** `params_flash.c`, sector 7 (0x08060000, linker `LENGTH` → 384 K), A/B slots + seq + CRC32; save only in IDLE/FAULT with STBY LOW; IWDG reload stretched to 5 s around the 1–2 s sector erase, then restored.
- **Gate:** `get angle_kp` returns the compiled default; `set` → ACK and the next TELEM_BAL reflects it; hash changes when a default changes (ctest); 5 min at 100 Hz: 0 `seq` gaps, 0 CRC errors; 60 s at full 500 Hz: 0 drops, UART utilisation < 40 %; legacy mode still decodes in `bench_monitor.py`.

### Phase 7 — Cascade control + compensations
- **Goal:** spec §5 cascade, tethered → free-standing. **Tethered steps may run on the interim protection (duty cap 60 %, stall detect, PSU 3 A limit); free-standing requires Phase 3b complete (INA240 clamp + overcurrent fault) — hard prerequisite.**
- **Add:** `cascade.c/.h` (pure: `outer_50hz(avg_cps, v_cmd, yaw_meas, yaw_cmd)` → tilt offset ± `TILT_LIMIT` and yaw duty, clamp-and-freeze anti-windup from v2 `control_logic.c:82-102`; `mix(torque, yaw)` → L/R normalised; `deadband_comp()` per motor/direction from B3; `vbat_comp()` from B12; 20 ms delta ring for wheel speed), `tests/test_cascade.c` (deadband symmetry, Vbat comp at 10 / 11.1 / 12.6 V, tilt clamp, mix normalisation, yaw sign).
- **Modify:** `balance.c` (angle PD with derivative on measurement; setpoint = `BALANCE_SETPOINT_DEG` + tilt offset; lean-to-drive `:109-120` becomes `v_cmd` into the velocity loop), `main.c` tick (outer → angle → mix → comp chain → clamp → `Motor_Set`; `g_diff_linear/angular` keep their meaning as `v_cmd/yaw_cmd` so `cmd_vel_sub.c` and `MSG_CMD_DIFF_DRIVE` still work; CI verifies the `MICROROS=ON` build), params (VEL_KP/KI, TILT_LIMIT 8°, YAW_KP/KI, VBAT_NOM 11.1, deadband table).
- **Gate (spec §7 steps 3–5):** tethered angle-only 60 s; + velocity loop: free-standing drift < 20 cm over 60 s, no limit cycle > ±3°; + yaw loop: heading drift < 10°/60 s, ±0.5 rad/s command tracks within 15 %; deadband: encoder responds for any |duty| > 0.5 %; Vbat comp: same settling ±10 % at 12.4 V vs 10.5 V; clamp engages during a hard recovery without tripping FAULT; `test_cascade` green.

### Phase 8 — Re-tune, acceptance, closeout
- **Prerequisite:** Phase 3b complete (B11 I_peak and the clamp need the INA240s); duty cap at 98 %.
- **Add:** `host/tune/relay_tune.py` (Ku/Tu at the new rate: Kd = 0, raise Kp to sustained oscillation; reuses `pitch_monitor.py:39-111` peak/Tu/Z-N logic fed from serial instead of ROS/SSH), `docs/v1-tuning.md`, `docs/v1-acceptance.md`, CHANGELOG `v1.0.0`; optional LQR export (stretch, only after acceptance).
- **Modify:** param defaults; `git rm motor_driver.c safety.c` (tag `v1.0.0-rc` first).
- **Gate = spec §8:** ≥ 10 min balance on a hard floor, drift < 20 cm; push recovery ≥ 9/10 with a documented tap procedure; B11: 20 recoveries, I_peak < 3.2 A, T_chip < 80 °C, B12 sag curve; fall cut ≤ 50 ms (telemetry: tilt crossing → PWM 0 within one tick); all FAULT paths documented; CI green; docs complete (pin map, measurements, tuning, LED table).

---

## 6. Port matrix from `auro-balancer` @ `5d9bc9d` (read via `git show 5d9bc9d:<path>`)

| v2 source | Destination here | Mode |
|---|---|---|
| `firmware/safety/batt_monitor.{c,h}`, `tests/test_batt_monitor.c` | `Core/Src|Inc`, `tests/` | verbatim (thresholds from this `config.h`) |
| `firmware/safety/imu_filter.{c,h}`, `tests/test_imu_filter.c` | same | adapted: τ-parameterised, α computed at init (M4) + sign cases |
| `firmware/safety/safety_fsm.{c,h}`, `tests/test_safety_fsm.c` | `supervisor.{c,h}`, `tests/test_supervisor.c` | pattern (spec's 7 states, not v2's 6) |
| `firmware/safety/safety.c:105-135` (long-press), `safety_monitors.c:87-120` (IMU staleness) | `monitors.c` | adapted |
| `firmware/safety/ct_sense.c:35-43,71-90`, `tests/test_ct_sense.c` | `current_sense.c`, `tests/test_current_sense.c` | adapted (INA240 math) |
| `firmware/safety/control_logic.c:82-125` (anti-windup, feed-forward form) | `cascade.c` | adapted |
| `firmware/safety/encoder_logic.c:22-40`, `encoder.c:140-166` (self-test drive) | `test_modes.c` (B2) | pattern |
| `firmware/control/pid.c:128-135` (D on measurement) | `pid.c` extension | adapted |
| `tests/test_pid.c`, `tests/CMakeLists.txt:22-56` | `tests/` | adapted to `PID_t` |
| `scripts/check_secrets.sh`, `check_coverage.sh`, `check_size_budget.sh`, `check_pin_consistency.py` | `scripts/` | verbatim / budgets + paths changed |
| `.github/workflows/firmware-ci.yml` (lint, host-test, firmware-build jobs) | `.github/workflows/firmware-ci.yml` | adapted; add `MICROROS=ON` leg |
| **Not ported:** `fault_log.c` (needs the RTC backup domain; RTC HAL status here UNKNOWN — optional later), `hal_ws2812`, `tasks.c`, `bench_vcp/cmd/rx` (this repo's binary protocol is kept) | — | — |

## 7. Risks

| # | Risk | Mitigation |
|---|---|---|
| 1 | Blocking I2C in the control ISR / IMU latency (F4) | E-4; BUSY pre-check; miss counter → IMU_COMMS fault; I2C IRQ prio below TIM10; errata bus-reset only from task context; byte-loss regression gate; fallback = today's 200 Hz blocking read |
| 2 | 1320 CPR too coarse (F3); TIM3 16-bit wrap | E-2 (20 ms windows, IIR); if still noisy, 40 ms window or TIM5 input-capture period as 7b; wrap arithmetic gets a ctest |
| 3 | INA240 unknowns (shunt, REF) + which update event survives RCR = 1 (E-5) + hidden ADC IRQ load (F2) | Gate 0 questions; valley-synchronous trigger verified on the scope with the OC4REF fallback ready; larger DMA buffer / masked IRQ; B10 quantifies |
| 4 | Two build configs bit-rot; uninitialised IWDG (F7) and the old reset loop (F9) | CI builds both; shared `App_*` bodies; IWDG armed in Phase 2 with a 50 ms refresh against a 200 ms timeout, ISR-health-gated |
| 5 | TB6612 stall margin (spec risk 1) | clamp default 1.5 A, B6 sets it; paralleled-channel fallback per spec; fuse sized from B6/B11 |
| 6 | Framing deviation (E-3) and telemetry bandwidth at 500 Hz | VER byte keeps host tools dual-compatible; TX ring + DMA; `TELEM_DIV`; utilisation measured |
| 7 | INA240 not fitted: no current clamp and no overcurrent fault until 3b; the TB6612's 3.2 A peak is unprotected by firmware | Interim from Phase 2: encoder stall detect (STBY LOW + latched fault after 300 ms), `PWM_DUTY_CAP_PCT` 60, bench PSU 12.0 V with a 3 A current limit for Phases 2–4, battery + fuse only from 3b; 3b gates Phase 7 free-standing and Phase 8 |

## 8. Verification (end to end)

- Every phase gate above is the verification for that phase; phases do not advance on a red gate (per the prompt: I report data, Arif approves).
- Build: `cmake -S firmware -B firmware/build -DCMAKE_TOOLCHAIN_FILE=$(pwd)/firmware/cmake/arm-gcc.cmake && cmake --build firmware/build` (both `MICROROS` settings).
- Host tests: `cmake -S firmware -B firmware/build-test -DBUILD_TESTS=ON && cmake --build firmware/build-test && ctest --test-dir firmware/build-test --output-on-failure`.
- Flash: `ls /dev/tty.usb* /Volumes/NODE_*` first (auto-detect the port; never hard-code `usbmodem1301`), then `st-flash write firmware/build/firmware.bin 0x08000000`.
- Hardware safety before any motion: STBY low confirmed on the DMM, wheels off the ground for Phases 2–4, **Phases 2–4 powered from a bench PSU at 12.0 V with a 3 A current limit (battery + blade fuse only from Phase 3b), Phase 4 smoke test on a tether lead, duty cap 60 % and stall detect armed until the 3b gate passes**, tether for the first Phase 7 runs.
- Acceptance = Phase 8 gate (spec §8).

## 9. Out of scope / deferred

- Cleaning the tracked `libmicroros.a*` / `include_jazzy/` (F8) and the committed `esp32_bridge/.pio` artifacts.
- The WiFi credentials that remain in git history before `3167b03` (rotation status UNKNOWN from the repo) — flag to Arif, not part of v1.
- LQR (spec stretch) — only after Phase 8 acceptance.
- Knowledge capture: after approval, record D-A…D-D and E-1…E-5 via `/learn` (project memory + `.planning/knowledge/`), since none of it is derivable from the code.
