# Migration Plan -- MicroROS Self-Balancing Robot

## Overview

Incremental migration from bare-metal STM32F401RE firmware to FreeRTOS +
micro-ROS, organized as 5 milestones (M0--M4). Each milestone has explicit
pass/fail criteria tested on real hardware before proceeding.

Total estimated effort: 10--14 sessions.

## Hardware Constraints (from bring-up)

These constraints are non-negotiable and must be respected throughout all
milestones:

| ID         | Constraint                                             |
|------------|--------------------------------------------------------|
| HW-BUG-02 | IMU capped at 200 Hz due to I2C/UART priority conflict |
| HW-BUG-02 | USART2 NVIC priority must be 0 (highest)               |
| HW-BUG-03 | USART2 priority 0 required for stable 921600 baud      |
| HW-BUG-04 | CT peak-hold window is 20ms                            |
| HW-BUG-05 | CT DMA buffer indices are swapped (A<->B)              |
| BUILD      | Flash via drag-drop to Nucleo mass storage only        |
| BUILD      | Compile with STM32CubeIDE GCC 13.3 only                |
| BALANCE    | Hardware-measured balance angle: -28.4 deg              |

---

## Milestone 0 -- Environment Setup

**Effort**: 2 sessions
**Goal**: FreeRTOS + micro-ROS running on STM32, visible from ROS2 host.

### Tasks

| # | Task                                          | Session |
|---|-----------------------------------------------|---------|
| 1 | Boot Ubuntu 22.04 VM (VMware Fusion)          | 1       |
|   | Image location: `/Users/robotics/Downloads/VM22.04/` | |
| 2 | Verify ROS2 Humble is installed (`ros2 doctor`)| 1      |
| 3 | USB passthrough: STM32 VCP to VM              | 1       |
|   | Verify: `ls /dev/ttyACM0` inside VM           |         |
| 4 | Enable FreeRTOS in STM32CubeMX project        | 1       |
|   | CMSIS-RTOS v2 interface, 4 tasks stubbed      |         |
| 5 | Add micro-ROS static library to firmware      | 2       |
|   | Use `micro_ros_stm32cubemx_utils`             |         |
| 6 | Create minimal firmware:                      | 2       |
|   | - FreeRTOS scheduler starts                   |         |
|   | - One task publishes `std_msgs/Int32` at 1 Hz |         |
|   | - micro-ROS serial transport on USART2        |         |
| 7 | Run micro-ROS agent on VM                     | 2       |
|   | `ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 921600` | |
| 8 | Flash firmware (drag-drop .bin)                | 2       |

### Pass Criteria

```
ros2 topic list
```
Shows the STM32 Int32 topic. `ros2 topic echo` prints incrementing values.

---

## Milestone 1 -- Sensor Topics

**Effort**: 2--3 sessions
**Goal**: All sensor data available as ROS2 topics at correct rates.

### Tasks

| # | Task                                          | Session |
|---|-----------------------------------------------|---------|
| 1 | IMU publisher: `sensor_msgs/Imu`              | 1       |
|   | Topic: `/imu/data`, rate: 200 Hz              |         |
|   | Populate: orientation (quaternion from comp.   |         |
|   | filter), angular_vel, linear_accel             |         |
|   | Respect HW-BUG-02: USART2 prio 0              |         |
| 2 | Odometry publisher: `nav_msgs/Odometry`       | 1--2    |
|   | Topic: `/odom`, rate: 50 Hz                   |         |
|   | Source: encoder counts from TIM2/TIM3          |         |
|   | Populate: twist (linear + angular velocity)   |         |
| 3 | Diagnostics publisher                         | 2       |
|   | Topic: `/diagnostics`, rate: 1 Hz             |         |
|   | Report: battery voltage, RTOS stack watermarks,|        |
|   | IMU health, motor fault flags                  |         |
| 4 | Balance status publisher                      | 2--3    |
|   | Topic: `/balance/status`                       |         |
|   | Type: `auro_msgs/BalanceStatus`, rate: 50 Hz  |         |

### Pass Criteria

```bash
# Verify topics exist
ros2 topic list | grep -E "imu|odom|diagnostics|balance"

# Verify rates
ros2 topic hz /imu/data          # expect ~200 Hz
ros2 topic hz /odom              # expect ~50 Hz
ros2 topic hz /diagnostics       # expect ~1 Hz

# Verify data content
ros2 topic echo /imu/data --once
# Confirm: non-zero accel, gyro values; valid quaternion
```

---

## Milestone 2 -- Motor Control via ROS2

**Effort**: 1--2 sessions
**Goal**: Wheels respond to `/cmd_vel` commands from the host.

### Tasks

| # | Task                                          | Session |
|---|-----------------------------------------------|---------|
| 1 | `/cmd_vel` subscriber (`geometry_msgs/Twist`) | 1       |
|   | Map `linear.x` to forward velocity (RPM)     |         |
|   | Map `angular.z` to differential steering      |         |
| 2 | Convert velocity to RPM setpoints             | 1       |
|   | Feed into existing RPM PID (pid.c)            |         |
|   | `rpm_a = linear_rpm - angular_rpm`            |         |
|   | `rpm_b = linear_rpm + angular_rpm`            |         |
| 3 | Command watchdog: 500ms timeout               | 1       |
|   | If no `/cmd_vel` received for 500ms,          |         |
|   | set RPM setpoints to zero (stop motors)       |         |
| 4 | Safety integration                            | 2       |
|   | Respect e-stop from safety_task               |         |
|   | Clamp RPM to safe maximum                     |         |

### Pass Criteria

```bash
# Spin both wheels forward
ros2 topic pub /cmd_vel geometry_msgs/Twist \
  "{linear: {x: 0.5}, angular: {z: 0.0}}" --rate 10

# Differential turn
ros2 topic pub /cmd_vel geometry_msgs/Twist \
  "{linear: {x: 0.0}, angular: {z: 1.0}}" --rate 10

# Watchdog: stop publishing, wheels stop within 500ms
```

---

## Milestone 3 -- Balance Control

**Effort**: 3--5 sessions
**Goal**: Robot self-balances using cascade PID, controllable via ROS2.

### Tasks

| # | Task                                          | Session |
|---|-----------------------------------------------|---------|
| 1 | Implement `balance_controller.c`              | 1       |
|   | Cascade PID: outer (pitch) + inner (RPM)      |         |
|   | Outer loop: 100 Hz (in balance_task)          |         |
|   | Inner loop: 1 kHz (in encoder_task or         |         |
|   | balance_task fast path)                        |         |
| 2 | Balance setpoint: -28.4 deg default           | 1       |
|   | Hardware-measured center of gravity angle      |         |
| 3 | `/balance/setpoint` subscriber                | 2       |
|   | `std_msgs/Float32` -- overrides balance angle |         |
|   | Clamp to safe range (-35 to -20 deg)          |         |
| 4 | `/cmd_vel` integration with balance           | 2--3    |
|   | `linear.x` shifts balance setpoint slightly   |         |
|   | to induce forward/backward motion              |         |
|   | `angular.z` adds differential RPM offset      |         |
| 5 | Tune outer PID gains                          | 3--4    |
|   | Start with Kp=5.0, Ki=0.1, Kd=0.5            |         |
|   | Use `/balance/status` for real-time tuning    |         |
| 6 | Recovery and fault handling                   | 4--5    |
|   | Detect fallen state (pitch > 45 deg offset)   |         |
|   | Disable motors when fallen                    |         |
|   | Re-engage when uprighted                       |         |

### Pass Criteria

1. Robot balances autonomously for >10 seconds
2. Robot recovers from gentle push within <2 seconds
3. `/cmd_vel` causes controlled forward/backward motion while balancing
4. `/balance/setpoint` adjusts balance angle in real-time
5. Robot safely stops motors when fallen

---

## Milestone 4 -- Production Polish

**Effort**: 2 sessions
**Goal**: Complete system ready for demonstration and ongoing development.

### Tasks

| # | Task                                          | Session |
|---|-----------------------------------------------|---------|
| 1 | ROS2 launch file                              | 1       |
|   | `ros2 launch auro_balance balance.launch.py`  |         |
|   | Starts: micro-ROS agent + balance_node        |         |
|   | Configurable: serial port, baud, PID gains    |         |
| 2 | rqt dashboard                                 | 1       |
|   | Pitch angle plot (real-time)                   |         |
|   | RPM A/B visualization                          |         |
|   | PID gain sliders (dynamic reconfigure)        |         |
| 3 | Persistent PID gains                          | 2       |
|   | Store tuned gains in STM32 flash              |         |
|   | Load on boot, override via ROS2 parameter     |         |
| 4 | Documentation and demo video                  | 2       |
|   | Update README with ROS2 usage                 |         |
|   | Record balancing demo                         |         |

### Pass Criteria

```bash
# Single command launches entire system
ros2 launch auro_balance balance.launch.py

# Dashboard shows live data
# PID gains survive power cycle
# Robot balances on launch without manual intervention
```

---

## Risk Register

| Risk                                    | Impact | Likelihood | Mitigation                                   |
|-----------------------------------------|--------|------------|----------------------------------------------|
| micro-ROS SRAM exceeds 96 KB            | High   | Medium     | Profile early in M0; reduce publisher count   |
| UART priority conflict with FreeRTOS    | High   | High       | USART2 above `configMAX_SYSCALL_INTERRUPT_PRIORITY` |
| 200 Hz IMU + micro-ROS overloads CPU    | Medium | Medium     | Profile CPU load; reduce IMU to 100 Hz if needed |
| FreeRTOS stack overflow in micro-ROS    | High   | Medium     | Enable `configCHECK_FOR_STACK_OVERFLOW = 2`  |
| USB VCP passthrough unstable in VM      | Medium | Low        | Use dedicated USB-UART adapter as fallback   |
| Balance tuning takes longer than planned| Medium | High       | Budget extra sessions in M3; use rqt for tuning |
| CubeMX regeneration breaks micro-ROS    | Medium | Medium     | Lock CubeMX-generated sections; use USER CODE blocks |

## Session Plan

| Session | Milestone | Focus                                | Deliverable                    |
|---------|-----------|--------------------------------------|--------------------------------|
| 1       | M0        | VM setup, USB passthrough            | ROS2 host ready                |
| 2       | M0        | FreeRTOS + micro-ROS minimal         | Int32 topic visible            |
| 3       | M1        | IMU publisher                        | /imu/data at 200 Hz            |
| 4       | M1        | Odometry + diagnostics               | /odom at 50 Hz                 |
| 5       | M1        | Balance status publisher             | /balance/status at 50 Hz       |
| 6       | M2        | /cmd_vel subscriber + motor control  | Wheels respond to commands     |
| 7       | M2        | Watchdog, safety integration         | Safe motor control             |
| 8       | M3        | Cascade PID implementation           | Balance controller compiles    |
| 9       | M3        | Initial balance tuning               | First balance attempt          |
| 10      | M3        | PID tuning iteration                 | Stable balance >10s            |
| 11      | M3        | /cmd_vel integration, recovery       | Full balance + mobility        |
| 12      | M4        | Launch file, dashboard               | One-command launch             |
| 13      | M4        | Flash storage, documentation         | Production-ready system        |
