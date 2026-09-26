---
name: nucleo-f401-build-flash
description: Build, host-test and flash the auro-balancing-necleo firmware on the NUCLEO-F401RE — the CMake + arm-gcc invocation, which toolchain actually gets picked, the micro-ROS auto-enable trap, expected link warnings, device listing before flash, and the bench preconditions for any image that can drive the TB6612. Use when building, flashing, adding a build option, or when a build or flash behaves unexpectedly.
---

# NUCLEO-F401RE build and flash (auro-balancing-necleo)

## Build

```bash
cmake -S firmware -B firmware/build -DCMAKE_TOOLCHAIN_FILE=$(pwd)/firmware/cmake/arm-gcc.cmake
cmake --build firmware/build -j8
arm-none-eabi-size firmware/build/firmware.elf
```

Measured 2026-09-27 on `siot-dev-m5` (PROVEN):

| Fact | Detail |
|---|---|
| Compiler | `/opt/arm-gcc` GCC **13.3.1**. `cmake/arm-gcc.cmake` hard-codes a STM32CubeIDE **13.3** plugin path that is not installed (only 14.3 is), so it falls back to PATH. |
| cmake | 3.31.10 from `~/.local/bin`. Homebrew also ships 4.x; the PATH one is what was used. |
| Build flavour | **micro-ROS**, because `firmware/micro_ros/libmicroros.a` exists and `CMakeLists.txt:110-150` auto-enables it. Phase 1 replaces this with `option(MICROROS OFF)`. |
| Size | text 186 560 · data 3 100 · bss 73 616 (RAM ≈ 76.7 KB of 96 KB) |
| Benign link warnings | `_lseek/_read/_write is not implemented`, `LOAD segment with RWX permissions` |

Flags are `-Wall -Wextra -Werror`: a new warning fails the build.

## Host tests

```bash
cmake -S firmware -B firmware/build-test -DBUILD_TESTS=ON
cmake --build firmware/build-test && ctest --test-dir firmware/build-test --output-on-failure
```

**PROVEN 2026-09-27: configure fails** (`No SOURCES given to target: test_comm_protocol`) because
`firmware/tests/` was never committed. Phase 1 adds `test_pid.c` and `test_comm_protocol.c`.
Unity v2.6.0 comes via FetchContent (network needed on first configure).

## Flash (inside a phase gate only)

```bash
ls /dev/tty.usb* /dev/cu.usbmodem* /Volumes/NODE_* 2>/dev/null   # identify the board first
st-info --probe                                                  # on-board ST-LINK V2-1
st-flash write firmware/build/firmware.bin 0x08000000
```

- Never hard-code `/dev/tty.usbmodem1301`; the path changes with the USB port. Host tools take `--port` or `AURO_PORT`.
- Drag-and-drop to `/Volumes/NODE_F401RE` also works (BENCH_LOG notes it was the only method at one point).
- The VCP runs at **921600** on USART2 PA2/PA3. SB62/SB63 must stay OPEN.

## Before flashing an image that can drive the TB6612

Confirm in the same session, with Arif:

1. Wheels off the ground (Phases 2–4) or tether (Phase 4 smoke test, first Phase 7 runs).
2. Bench PSU at **12.0 V, 3 A limit**, battery disconnected — until Phase 3b (INA240) passes.
3. STBY is LOW at power-on (DMM on the TB6612 STBY pin).
4. Stall detect armed and `PWM_DUTY_CAP_PCT` = 60 in the image (from Phase 2).

A reset re-runs init; an unexpected reset can look like "motor didn't move". Check the boot banner
and the IWDG reset flag (from Phase 2) before concluding a hardware fault.
