# v1 design decisions (JGB37-520 + TB6612FNG)
**Category:** architecture
**Tags:** v1, tb6612, ina240, tim1, iwdg, imu, framing, freertos
**Date:** 2026-09-25
## Status (2026-09-27)
Plan approved and copied into the repo as `docs/v1-plan.md` (source of record from now on). Phase 0 draft pin map committed; Gate 0 open. Nothing implemented yet.

## Context
Arif's v1 prompt (`balancing-robot-v1-cc-prompt.md`) described an "existing state" that partly belonged to the sibling repo `auro-balancer`. Decisions below were locked with Arif in plan review.
## Detail
| Topic | Decision | Why |
|---|---|---|
| Base | This repo; port pure-logic modules from `auro-balancer` @ `5d9bc9d` via `git show` | v2 has host-tested FSM/monitors/CI |
| Build | Serial-only default; `-DMICROROS=ON` opt-in | spec §6 is serial; frees PC6/PC7 |
| RTOS | Keep FreeRTOS in both builds, smaller heap when serial | one code path; hard RT is ISR-driven |
| RGB | PB12/PB14/PB15 (polarity to confirm) | as wired |
| PWM | TIM1 center-aligned, ARR 2099 (20 kHz), RCR 1, TRGO = UPDATE; done in Phase 2 so B3/B4 are measured once | valley = on-pulse centre at any duty |
| Current | ADC1 injected IN13/IN14 on T1_TRGO; samples flagged low-confidence below ~5 % duty; INA240 NOT fitted yet (Phase 3b) | switching-noise-free sampling |
| INA240 build spec | 5 mΩ, REF1→VS, REF2→GND, VS 3.3 V → 0.25 V/A, ±6.6 A | Arif |
| Interim protection | stall detect (>50 % duty, <5 rpm, 300 ms → STBY LOW latched), duty cap 60 %, PSU 12.0 V / 3 A | no current sense until 3b |
| IWDG | Armed in Phase 2 after boot calibration, before STBY release; 50 ms refresh gated on TIM10 tick | fixes old reset loop |
| IMU | `HAL_I2C_Mem_Read_IT` kicked from ISR; callback copies raw + seq only; filter in angle step; τ-parameterised complementary filter | DMA variant blocks |
| UART RX | Circular DMA + IDLE IRQ, then USART2 prio 3 below TIM10 | per-byte IRQ overruns at 921600 |
| Control | No inner RPM PID; velocity PI 50 Hz → angle PD 500 Hz → mix → deadband → Vbat comp → clamp | 1320 CPR = 45 rpm/count at 1 kHz |
| Framing | Keep `0xAA` VER TYPE … CRC16, add LEN (VER 0x02); no COBS | proven codec; ST-Link hop is a real UART, CRC+sync resync is enough |
| B10 gate | Angle-tick ISR worst case < 500 µs; angle/non-angle ticks reported separately | |
| B1 | Use measured CPR; pass = L/R within 1 % | |
## Usage
Plan: `docs/v1-plan.md` (the `~/.claude/plans/` copy is no longer maintained). Pin map: `docs/v1-pinmap.md`.
## Related
`.planning/knowledge/gotchas/f401re-v1-gotchas.md`
