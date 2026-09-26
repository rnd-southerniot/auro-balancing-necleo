# auro-balancing-necleo — execution contract

Two-wheel self-balancing robot on a **NUCLEO-F401RE** (STM32F401RET6, 84 MHz), being taken to **v1**:
2× JGB37-520 gearmotors + **TB6612FNG** driver + ICM-20602 IMU (GY-521 module) + 3S pack.
Inherits the global profile (`~/.claude/CLAUDE.md`): evidence discipline, phase gates, hardware-safe
defaults, small reversible commits. This file adds what is specific here.

**Start every session with [docs/NEXT_SESSION.md](docs/NEXT_SESSION.md).**

## Source documents

| Document | Role |
|---|---|
| `balancing-robot-v1-cc-prompt.md` | Arif's v1 spec (targets, measurements B1–B12, controller, FSM, acceptance) |
| [docs/v1-plan.md](docs/v1-plan.md) | **Approved plan** (2026-09-25): decisions D-A…D-D, E-1…E-5, phases 0–8 with gates |
| [docs/v1-pinmap.md](docs/v1-pinmap.md) | Phase 0 pin map — DRAFT until Gate 0 |
| `.planning/knowledge/` | Decisions, gotchas, devops, session logs (`$KDIR`) |
| Root `*.md` (PORTING_NOTES, BENCH_LOG, HARDWARE_STATUS, …) | Pre-v1 history (DBH-12V + MG513P30 era). Several are stale — trust the code and the v1 docs over them |

## Locked decisions (Arif, 2026-09-25) — do not re-open without him

- **Base = this repo.** Pure-logic modules are ported from sibling `../auro-balancer` **at commit
  `5d9bc9d` only** (`git -C ../auro-balancer show 5d9bc9d:<path>`). Its working tree is dirty on another
  branch — never copy from it. No work happens in that repo from here.
- **Default build = serial-only.** micro-ROS + ESP32-S3 bridge become opt-in `-DMICROROS=ON` (Phase 1).
- **RGB LED = PB12 / PB14 / PB15** as coded. PB2 = fault LED. (The spec's PB2/PC7/PA10 is wrong.)
- **JGB37-520 + TB6612 are wired; the INA240s are NOT fitted.** PC3/PC4 stay reserved. Fitted spec:
  5 mΩ shunt per motor lead, REF1 → VS, REF2 → GND, VS 3.3 V → ±6.6 A.
- TIM1 **center-aligned** PWM (ARR 2099, RCR 1, TRGO = UPDATE) lands in **Phase 2**; ADC injected
  valley sampling in **Phase 3b**. No inner RPM PID (1320 CPR ⇒ 45 rpm/count at 1 kHz).
- Framing: keep `0xAA` + VER + TYPE + CRC16, add LEN (VER 0x02). **No COBS** (deviation from the spec, agreed).

## 🔒 Hardware safety gates

1. **SB62 / SB63 stay OPEN.** PA2/PA3 are the ST-Link VCP via SB13/SB14. Never propose a pin needing them closed.
2. **PB3 = SWO** (SB15) — never GPIO. PA13/PA14 = SWD. PA5 = LD2.
3. **TB6612 STBY LOW from power-on** until firmware deliberately releases it (external pull-down + first GPIO write).
4. **Until Phase 3b passes (no current sense on the robot):** bench PSU **12.0 V, 3 A limit**, battery
   disconnected, wheels off the ground for Phases 2–4, tether lead for the Phase 4 smoke test,
   `PWM_DUTY_CAP_PCT` = 60, encoder stall detect armed. Free-standing balance (Phase 7) and Phase 8
   **require Phase 3b**.
5. **Never command motor motion unless Arif has confirmed, in the same session, wheels-off-ground (or
   tether) and PSU limits.** Build, host tests, flashing a non-driving image and reading registers need no go-ahead.
6. **Flashing happens only inside a phase gate.** List devices first (`ls /dev/tty.usb* /Volumes/NODE_*`);
   never hard-code `/dev/tty.usbmodem1301`.
7. VM on TB6612 ≤ 13.5 V: a full 3S (12.6 V) is close — bulk capacitance + TVS at VM, ringing checked on a scope in B11.

## Build / test / flash (verified on `siot-dev-m5` 2026-09-27 unless marked)

```bash
cmake -S firmware -B firmware/build -DCMAKE_TOOLCHAIN_FILE=$(pwd)/firmware/cmake/arm-gcc.cmake
cmake --build firmware/build -j8                     # -> firmware/build/firmware.{elf,bin}
st-flash write firmware/build/firmware.bin 0x08000000 # inside a phase gate only (not re-run 2026-09-27)
```

- **PROVEN:** configure picks `/opt/arm-gcc` GCC 13.3.1 — the STM32CubeIDE 13.3 path hard-coded in
  `cmake/arm-gcc.cmake` does not exist (only the 14.3 bundle is installed), so the PATH fallback wins.
  `cmake` on PATH = 3.31.10.
- **PROVEN:** today's default build is the **micro-ROS** build (auto-enabled because
  `firmware/micro_ros/libmicroros.a` exists): text 186 560 B, data 3 100 B, bss 73 616 B. Expected link
  warnings: `_lseek/_read/_write not implemented`, `LOAD segment with RWX permissions`.
- **PROVEN:** `cmake -S firmware -B firmware/build-test -DBUILD_TESTS=ON` **fails** — `firmware/tests/`
  does not exist. Phase 1 creates it.
- Details and traps: skill `nucleo-f401-build-flash`.

## Phases (full detail + gates in docs/v1-plan.md)

| Phase | Content | Status |
|---|---|---|
| 0 | Pin map `docs/v1-pinmap.md` | **Draft committed; Gate 0 waiting on Arif's as-wired pins** |
| 1 | Build/CI/tests, `-DMICROROS` opt-in | not started |
| 2 | TB6612 driver, center-aligned TIM1, IWDG, stall detect + duty cap, B1–B5 | not started |
| 3a | Battery monitor, B12 tooling | not started |
| 3b | INA240 current sense, clamp, B6, overcurrent FAULT | **blocked: INA240 not fitted** |
| 4 | ISR angle loop 200–500 Hz, IT-driven IMU, USART2 RX DMA, B7/B7b/B8/B10 | not started |
| 5 | Supervisor FSM, fault paths, LED map | not started |
| 6 | Telemetry v2 (LEN), params + hash, host CLI/plot | not started |
| 7 | Cascade (velocity, yaw), deadband/Vbat comp; free-standing needs 3b | not started |
| 8 | Re-tune, acceptance (spec §8); needs 3b | not started |

Per phase: branch `phase/v1-<n>-<slug>`, Conventional Commits with the phase id in the body,
`CHANGELOG.md` entry, one PASS/FAIL gate with an exact expected outcome, Arif approves before the next phase.

## Knowledge, skills, MCP upstream

- Project skills (`.claude/skills/`): `nucleo-f401-build-flash`, `v1-bench-measurement`, `port-from-auro-balancer`.
- `$KDIR` = `.planning/knowledge/` — read before planning, write when a durable fact or decision
  emerges, add a dated `sessions/` file at stopping points.
- Gateway upstream **`auro-balancing-knowledge`** = `127.0.0.1:8022` on `10.10.8.113`, prefix `aurobal`,
  unit `mcp-auro-balancing-knowledge.service`, mirror `/home/mcp/knowledge/auro-balancing-necleo/`.
  Server source of record: `tools/mcp-server/`. **Run `tools/sync-knowledge-mcp.sh` after changing
  docs, skills or knowledge**, or the gateway serves stale content.
- GitHub: `rnd-southerniot/auro-balancing-necleo`.

## Conventions

- Evidence labels on every hardware/behaviour claim: **PROVEN** (cite file:line, log, or instrument) ·
  **ASSUMED** (say why) · **UNKNOWN** (say what would settle it).
- `docs/v1-pinmap.md` is canonical for pins; `firmware/Core/Inc/pins.h` (Phase 2) mirrors it.
- `firmware/Core/Inc/config.h` holds every tunable; no magic numbers in drivers.
- Vendored code (`firmware/Drivers/`, `firmware/Middlewares/`, `firmware/micro_ros/`) is not edited.
- "Extend, don't rewrite": keep `Motor_Set`-shaped APIs so call sites survive; justify any refactor.

## Known defects in the current code (fix in the phase named)

| Defect | Evidence | Phase |
|---|---|---|
| `hiwdg` refreshed while never initialised; old reset loop = 200 ms timeout vs 500 ms refresh + ~1 s unrefreshed boot calibration | `freertos_app.c:70-71`, `main.c:151,268-272`, `config.h:100` | 2 |
| Blocking I2C IMU read inside the 1 kHz TIM10 ISR | `main.c:302-312` | 4 |
| Command dispatch + blocking 5 ms UART TX inside the USART2 RX ISR (prio 0) | `main.c:753,800-808` | 4 |
| Pitch fuses atan2(ax, az) with gyro **X** — suspected axis mismatch | `imu_mpu6050.c:212-226` | 4 (B7b) |
| RGB polarity contradiction (header: common-anode LOW=ON; code: SET=ON) | `rgb_led.h:5`, `rgb_led.c:17-19` | 0/5 |
| `safety.c` checks and stops Motor A only | `safety.c:99`, `main.c:354` | 2 (interim), 5 |
| Balance loop only runs inside the micro-ROS task, 50 Hz, ±10 ms jitter | `freertos_app.c:218-227` | 4 |

## State

```
2026-09-25  Discovery vs the v1 spec; four decisions locked with Arif; plan approved after two review
            rounds (H1-M5, N1-N5) and the Gate 0 INA240-not-fitted update.
2026-09-25  Phase 0 draft pin map committed (3447636) on phase/v1-0-pinmap. UM1724 E5V/USB note NOT
            quoted: st.com unreachable from this Mac (HTTP/2 stream error, then timeouts) -> UNKNOWN.
2026-09-27  Session close: CLAUDE.md, 3 project skills, knowledge, MCP upstream auro-balancing-knowledge
            (:8022), docs/NEXT_SESSION.md. Build re-verified (micro-ROS default, 186 560 B text).
            Upstream verified via gateway: 22/22 healthy, 29 docs. Branch phase/v1-0-pinmap committed
            but NOT pushed, and the final mirror resync NOT run: both blocked by the permission check.
            NEXT: Gate 0 answers from Arif -> finalize pin map -> Phase 1 (docs/NEXT_SESSION.md).
2026-09-27  Follow-up: phase/v1-0-pinmap pushed to origin (gitleaks clean on all 8 commits); mirror
            resynced, gateway serves 32 docs incl. the port-8022 note. No PR opened.
```

## Guardrails

- Do not "fix" things the plan schedules for a later phase while working on an earlier one.
- Do not copy from `../auro-balancer`'s working tree; `5d9bc9d` only, provenance comment in each ported file.
- Do not delete `motor_driver.c` / `safety.c` before Phase 8 (tag `v1.0.0-rc` first).
- Do not commit captures in `host/logs/` other than bench CSVs referenced from `docs/v1-measurements.md`.
- Git history before `3167b03` holds WiFi credentials and an internal VM IP (already on GitHub);
  rotation status UNKNOWN from the repo. Never re-introduce secrets; `esp32_bridge/include/secrets.h` stays ignored.
