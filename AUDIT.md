# Reference Audit — auro-balancing-necleo MicroROS Migration

Generated: 2026-04-11

---

## 1. Reference materials audited

### 1.1 Yahboom MicroROS-Self-balancing-Robot (GitHub)

URL: https://github.com/YahboomTechnology/MicroROS-Self-balancing-Robot

The GitHub repo contains **only documentation (PDFs)** — no source code.
All source code and firmware are distributed via Google Drive or the A9 SD card.

Key documentation reviewed:
- 12.STM32 balancing case / 04.Robot car PID control course
- 12.STM32 balancing case / 03.STM32 expansion course
- 01.Introduction / 4. Balance car interface description
- 03.Preparation / 4. Connect to microROS agent
- 08.Robot basic course / 1. Robot information release

### 1.2 A9 Local Reference

Path: /Users/robotics/Downloads/A9_MicroROS_Self-balancing car/

Contents:
- 1.Instruction_Manual/ — 19 assembly images (JPG)
- 2.3D_Model_File/ — STEP file of chassis
- 3.Code/Factory-Firmware/ — compiled .bin + .hex (no source), config_Balance_Car.py
- 3.Code/ROS_Source_Code/ — 4 zipped ROS workspaces + shell scripts
- 4.Hareware_Info/ — chip datasheets, motor specs, driver specs
- 5.Software_Tools/ — Windows tools (VMware, STM32CubeIDE, ST-LINK drivers)

**No STM32 source code available** — only compiled factory firmware binary.
The config_Balance_Car.py is a Python host tool for configuring WiFi/ROS params
via a custom serial protocol (0xFF header, checksum, 115200 baud to ESP32).

### 1.3 VM — Ubuntu 22.04

Path: /Users/robotics/Downloads/VM22.04/
Type: **VMware Fusion** (.vmx, .vmdk split files)
Display name: "Ubuntu22.04_For_microROS"
Guest OS: ubuntu-64
RAM: 4096 MB
vCPUs: 4
Disk: ~42 GB across 11 VMDK splits
USB: Enabled (HID + hub)
Network: e1000 NAT
Pre-installed: ROS2 Humble + micro-ROS agent (per Yahboom documentation)

### 1.4 Google Drive

Folder: 1m3GHZrfJSu7rhe_7XuWua1PhHlxmdK5K
Status: Not accessible from CLI (requires browser auth).
Contents (per repo reference): STM32 source code, additional ROS packages.
**Action needed:** Download STM32 source code manually from Drive if deeper
firmware audit is required. For architecture planning, the PDFs provide enough detail.

---

## 2. Hardware comparison — Yahboom vs our hardware

| Component | Yahboom A9 | Our Hardware | Delta | Impact |
|-----------|-----------|-------------|-------|--------|
| **MCU** | STM32F103RCT6 (Cortex-M3, 72MHz, 256KB flash, 48KB SRAM) | STM32F401RET6 (Cortex-M4F, 84MHz, 512KB flash, 96KB SRAM) | **Better** — more flash/SRAM, FPU | Easier to fit micro-ROS |
| **Comms MCU** | ESP32-S3 (WiFi UDP to host) | None — single MCU | **Missing** | Must use UART serial transport |
| **IMU** | MPU6050 (DMP firmware, software I2C on PB10/PB11) | ICM-20602 (WHO_AM_I=0x72, hardware I2C1 PB8/PB9) | Different chip | Our driver works, skip DMP |
| **Motor driver** | AT8236 (2-ch H-bridge, TIM8 PWM) | DBH-12V (2-ch H-bridge, TIM1+TIM4 PWM) | Different driver | Our driver works, CT peak-hold needed |
| **Motors** | 520 DC motor, 11-line Hall encoder, 1:30 | MG513P30, 500PPR GMR encoder, 1:30 | Different encoder type | Much higher CPR (60k vs 1320) |
| **Encoder CPR** | 1320 (11 x 4 x 30) | 60,000 (500 x 4 x 30) | **45x higher resolution** | Better velocity estimation |
| **Battery** | 12V (assumed) | 3S 18650 12.3V | Similar | OK |
| **Transport** | WiFi UDP (ESP32 bridges to host) | UART VCP (direct ST-LINK) | **Simpler** | Serial micro-ROS agent instead of UDP |
| **LiDAR** | YDLIDAR T-mini Plus | None | Not needed for balancing | Skip for now |
| **Camera** | ESP32-S3 WiFi camera module | None | Not needed for balancing | Skip |
| **OLED** | SSD1306 I2C display | None | Not needed | Skip |
| **Ultrasonic** | HC-SR04 | None | Not needed for balancing | Skip |
| **Balance angle** | Not documented | -28.4 deg (measured) | Must use ours | -- |

### Key architectural difference

Yahboom uses a **dual-MCU design**: STM32F103 for motor/sensor control,
ESP32-S3 for WiFi/micro-ROS communication. The ESP32 runs micro-ROS client
and bridges to the host via WiFi UDP.

We have a **single MCU** (STM32F401RE). micro-ROS client must run directly
on the STM32, communicating over UART serial to the host. This is simpler
but requires FreeRTOS + micro-ROS to fit in our flash/SRAM budget.

---

## 3. Firmware architecture comparison

| Layer | Yahboom | Our current | Migration action |
|-------|---------|------------|-----------------|
| RTOS | Bare metal (STM32F103) | Bare metal | ADD FreeRTOS |
| micro-ROS | On ESP32-S3 (separate chip) | None | ADD to STM32 directly |
| Transport | WiFi UDP (ESP32 to host) | COBS UART custom | REPLACE with micro-ROS serial |
| IMU driver | MPU6050 DMP (software I2C) | ICM-20602 complementary filter (hardware I2C) | KEEP ours |
| Angle estimation | DMP quaternion to Euler | Complementary filter (0.98 alpha) | KEEP ours |
| Encoder driver | TIM3/TIM4 encoder mode | TIM2/TIM3 encoder mode | KEEP ours |
| Motor driver | AT8236 via TIM8 PWM | DBH-12V via TIM1/TIM4 PWM | KEEP ours |
| Balance PID | Cascade: position PID + incremental speed PI | Not implemented | PORT concept, adapt gains |
| RPM PID | Incremental PID | ISA discrete PID (anti-windup) | KEEP ours |
| Current sense | Not documented | ADC DMA + 20ms peak-hold | KEEP ours |
| Safety | Not documented | Overcurrent, overspeed, battery, watchdog, stall | KEEP ours |

---

## 4. ROS2 topic map (from Yahboom reference)

### Published by robot (STM32/ESP32 via micro-ROS)

| Topic | Type | Rate | Notes |
|-------|------|------|-------|
| /imu | sensor_msgs/Imu | ~50Hz | IMU data from MPU6050 DMP |
| /mpuimu | sensor_msgs/Imu | ~50Hz | Raw MPU6050 data |
| /odom_raw | nav_msgs/Odometry | ~50Hz | Encoder odometry |
| /scan | sensor_msgs/LaserScan | ~10Hz | LiDAR (optional) |

### Subscribed by robot

| Topic | Type | Notes |
|-------|------|-------|
| /cmd_vel_bl | geometry_msgs/Twist | linear.x (0-60 units/s), angular.z (0-1000 rad/s) |
| /beep | std_msgs/UInt16 | Buzzer control |

### Our adaptation

| Yahboom topic | Our topic | Change |
|---------------|-----------|--------|
| /imu | /imu/data | Rename to ROS convention |
| /mpuimu | Skip | Redundant with /imu/data |
| /odom_raw | /odom | Standard naming |
| /scan | Skip | No LiDAR |
| /cmd_vel_bl | /cmd_vel | Standard naming |
| /beep | Skip | No buzzer |
| -- | /diagnostics | ADD: battery, faults |
| -- | /balance/status | ADD: pitch, PID output |

---

## 5. Balance PID structure (from Yahboom docs)

Yahboom uses cascade PID:

```
                        Outer loop (Position PID)
                        ├── Input: target angle - measured pitch
                        ├── Output: velocity setpoint
                        └── Type: Standard PID (Kp, Ki, Kd)
                                │
                                ▼
                        Inner loop (Speed PI)
                        ├── Input: velocity setpoint - measured velocity
                        ├── Output: motor PWM duty
                        └── Type: Incremental PI (delta output per tick)
```

Key parameters from Yahboom:
- PWM output limits: +/-2500
- Encoder counts per cycle: 1320 max (11-line Hall x4 x 30:1)
- Control rate: TIM6 interrupt (rate not explicitly documented, ~100-200Hz inferred)

### Our adaptation

We will use the same cascade structure but:
- Replace DMP angle with our complementary filter (already working, 200Hz)
- Use our ISA PID (already tuned) for inner RPM loop at 1kHz
- Add outer balance PID at 100-200Hz
- Our encoder resolution is 45x higher (60k vs 1320) — better velocity estimate
- Balance setpoint: -28.4 deg (hardware-measured, Yahboom value unknown)

---

## 6. Critical constraints for micro-ROS on STM32F401RE

### Flash budget

| Component | Estimated flash | Source |
|-----------|----------------|--------|
| Current firmware | 51 KB | Measured (linker map) |
| FreeRTOS kernel | 10-15 KB | ST CubeMX config |
| micro-ROS library | 80-150 KB | Depends on topic count, STATIC alloc |
| micro-ROS transport (serial) | 5-10 KB | UART transport layer |
| **Total estimate** | **146-226 KB** | |
| **Budget** | **512 KB** | STM32F401RE |
| **Headroom** | **286-366 KB** | Comfortable |

### SRAM budget

| Component | Estimated SRAM | Notes |
|-----------|---------------|-------|
| Current firmware BSS+stack | 4 KB + 2 KB stack | Measured |
| FreeRTOS heap + task stacks | 15-25 KB | 5 tasks x 2-4KB stack each |
| micro-ROS buffers | 15-25 KB | RMEM, publisher/subscriber buffers |
| **Total estimate** | **36-56 KB** | |
| **Budget** | **96 KB** | STM32F401RE |
| **Headroom** | **40-60 KB** | Adequate |

### NVIC priority constraints (from HW-BUG-02/03)

- USART2 must be priority 0 (cannot be preempted during I2C blocking)
- TIM10 (control ISR) at priority 1
- FreeRTOS SysTick must be lower priority than both
- micro-ROS serial transport must not conflict with telemetry DMA

---

## 7. Transport decision: UART serial (not WiFi UDP)

Yahboom uses WiFi UDP via a separate ESP32. We don't have an ESP32.

**Decision: micro-ROS serial transport over existing UART VCP**

Advantages:
- Already wired and HW-validated (921600 baud, 0 CRC errors)
- No additional hardware needed
- Simpler than WiFi (no AP config, no IP addressing)
- Lower latency than WiFi

Disadvantages:
- Requires physical USB cable to host
- Can't do wireless teleoperation

Future upgrade path: Add ESP32-S3 module for WiFi transport later.

---

## 8. What we reuse from current project (HW-validated)

All of these are production-grade and must NOT be regressed:

| Module | File | Status | Bug fixes applied |
|--------|------|--------|------------------|
| IMU driver | imu_mpu6050.c | HW-validated | WHO_AM_I 0x72, 200Hz rate limit |
| Encoder driver | encoder.c | HW-validated | Both A+B bidirectional |
| Motor driver | motor_driver.c | HW-validated | EN pin gate, direction dead-time |
| PID algorithm | pid.c | HW-validated | ISA discrete, anti-windup |
| Safety monitor | safety.c | HW-validated | Overcurrent debounce, stall detect |
| Battery ADC | main.c (ADC helpers) | HW-validated | 0.24% accuracy |
| CT peak-hold | main.c | HW-validated | 20ms window, DMA index swap |
| NVIC priorities | stm32f4xx_hal_msp.c | HW-validated | USART2=0, TIM10=1 |
| UART error recovery | main.c | HW-validated | RxState reset in ErrorCallback |

---

## 9. What we add

| Component | Source | Priority |
|-----------|--------|----------|
| FreeRTOS | STM32CubeMX | Milestone 0 |
| micro-ROS client library | colcon build or pre-built | Milestone 0 |
| micro-ROS serial transport | micro-ROS UART transport | Milestone 0 |
| /imu/data publisher | New: sensor_msgs/Imu | Milestone 1 |
| /odom publisher | New: nav_msgs/Odometry | Milestone 1 |
| /diagnostics publisher | New: diagnostic_msgs | Milestone 1 |
| /cmd_vel subscriber | New: geometry_msgs/Twist | Milestone 2 |
| Balance controller | New: cascade PID | Milestone 3 |
| /balance/status publisher | New: custom or Float32MultiArray | Milestone 3 |
| ROS2 launch file | New: balance.launch.py | Milestone 4 |

---

## 10. Key risks

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| micro-ROS too large for F401RE | Low | High | F401RE has 512KB (vs F103 256KB); use STATIC alloc, minimize topics |
| FreeRTOS + micro-ROS SRAM overflow | Medium | High | Profile in Milestone 0; reduce task stacks; use static allocation |
| I2C blocking in ISR conflicts with FreeRTOS | High | High | Keep I2C in dedicated task (not ISR); maintain NVIC priorities |
| Balance PID unstable | High | Medium | Start from Yahboom cascade structure; tune iteratively |
| VM USB passthrough unreliable | Medium | Medium | Test in Milestone 0; fallback: native Ubuntu on spare laptop |
| No STM32 source code from Yahboom | -- | Low | We have our own drivers; only need the PID structure (documented in PDFs) |
