---
name: port-from-auro-balancer
description: Rules for porting pure-logic modules (batt_monitor, imu_filter, safety_fsm pattern, ct_sense math, control_logic anti-windup, pid D-on-measurement, test files, CI guard scripts) from the sibling v2 repo ../auro-balancer into this repo — pinned commit, git show extraction, provenance header, adaptation rules and the port matrix. Use whenever v1 work needs code or tests that already exist in auro-balancer.
---

# Porting from `../auro-balancer` (v2)

`auro-balancer` is the v2 rewrite of the same robot on the **old** hardware (DBH-12V, MG513P30).
Its pure-logic modules are host-tested and bench-proven; v1 reuses them instead of rewriting.

## Hard rules

1. **Commit `5d9bc9d` only.** Its working tree is dirty on branch `model/bench-cmd-1khz-capture`
   (`config.h`, `balance*.c` modified). Never `cp` from the working tree.
   ```bash
   git -C ../auro-balancer show 5d9bc9d:firmware/safety/batt_monitor.c > firmware/Core/Src/batt_monitor.c
   git -C ../auro-balancer show 5d9bc9d:firmware/tests/test_batt_monitor.c > firmware/tests/test_batt_monitor.c
   ```
2. **Read-only.** Never commit, branch or edit in `../auro-balancer` from this repo (one repo per session).
3. **Provenance header** at the top of every ported file:
   `/* Ported from rnd-southerniot/auro-balancer @ 5d9bc9d:<path> (<verbatim|adapted: what changed>). */`
4. **Tests come with the module.** A ported module without its host test is not ported.
5. **Constants come from this repo's `config.h`**, never from v2's (different hardware, different thresholds).

## Port matrix (from docs/v1-plan.md §6)

| v2 source @ 5d9bc9d | Here | Mode |
|---|---|---|
| `firmware/safety/batt_monitor.{c,h}` + test | `Core/Src|Inc`, `tests/` | verbatim; v1 thresholds 9.9 / 12.8 V |
| `firmware/safety/imu_filter.{c,h}` + test | same | adapted: parameterised by τ, α = τ/(τ+dt) at init |
| `firmware/safety/safety_fsm.{c,h}` + test | `supervisor.{c,h}` | pattern only (spec's 7 states, not v2's 6) |
| `safety.c:105-135` long-press, `safety_monitors.c:87-120` IMU staleness | `monitors.c` | adapted |
| `firmware/safety/ct_sense.c:35-43,71-90` + test | `current_sense.c` | adapted to INA240 math (Phase 3b) |
| `firmware/safety/control_logic.c:82-125` | `cascade.c` | anti-windup + FF form |
| `firmware/control/pid.c:128-135` | extend this repo's `pid.c` | D on measurement |
| `tests/test_pid.c`, `tests/CMakeLists.txt:22-56` | `firmware/tests/` | adapted to this repo's `PID_t` |
| `scripts/check_secrets.sh`, `check_size_budget.sh`, `check_coverage.sh`, `check_pin_consistency.py` | `scripts/` | paths and budgets changed |
| `.github/workflows/firmware-ci.yml` | `.github/workflows/` | adapted; add a `MICROROS=ON` leg |

**Not ported:** `fault_log.c` (needs the RTC backup domain; not enabled here), `hal_ws2812`,
`tasks.c`, `bench_vcp/cmd/rx` (this repo keeps its binary protocol).

## Traps

- v2's `BALANCE_*` and `SAFETY_*` values are tuned for MG513P30 at 60 000 CPR. v1 is 1320 CPR — re-derive.
- v2's safety FSM states differ from the spec's (BOOT/SELF_TEST/IDLE/ARMED/DEGRADED/FAULT_LATCHED vs
  BOOT/IDLE/CALIBRATE/READY/BALANCING/FALLEN/FAULT). Port the masks and saturating timers, not the states.
- v2 includes `config.h` from `firmware/config/`; fix include paths to `Core/Inc`.
