# Build, host tests, flash — measured facts
**Category:** devops
**Tags:** cmake, arm-gcc, micro-ros, unity, st-flash, nucleo-f401re
**Date:** 2026-09-27
## Context
The v1 plan's Phase 1 changes the build (micro-ROS opt-in, host tests, CI). These are the facts
of the build as it stands before Phase 1, so a regression can be told from a pre-existing state.
## Detail (PROVEN, `siot-dev-m5`, 2026-09-27, scratch build dir)
| Fact | Evidence |
|---|---|
| Compiler used = `/opt/arm-gcc` GCC 13.3.1 | configure output; `cmake/arm-gcc.cmake` hard-codes a CubeIDE **13.3** plugin path, only **14.3** is installed, so PATH fallback |
| cmake = 3.31.10 (`~/.local/bin`) | `cmake --version` |
| Default build = micro-ROS | configure prints "micro-ROS library found"; auto-enable at `CMakeLists.txt:110-150` |
| Size: text 186 560 · data 3 100 · bss 73 616 | post-build `arm-none-eabi-size` |
| Benign link warnings | `_lseek/_read/_write is not implemented`, `LOAD segment with RWX permissions` |
| Host tests do not configure | `No SOURCES given to target: test_comm_protocol` — `firmware/tests/` never committed |
## Usage
Commands and flash preconditions: skill `nucleo-f401-build-flash`. Re-measure after Phase 1 and
record the serial-only (`MICROROS=OFF`) size next to these numbers.
## Related
`CLAUDE.md` Build section; `docs/v1-plan.md` Phase 1.
