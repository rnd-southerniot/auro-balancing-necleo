# Architecture -- MicroROS Self-Balancing Robot

## System Overview

```
 +----------------------------------------------------------+
 |  Ubuntu 22.04 Host (VM)                                  |
 |                                                          |
 |  +------------------+  +-----------------------------+   |
 |  | micro-ROS Agent  |  |  ROS2 Humble Nodes          |   |
 |  | (serial transport)|  |                             |   |
 |  |  /dev/ttyACM0    |  |  balance_node.py            |   |
 |  |  921600 baud     |  |  rqt dashboard              |   |
 |  +--------+---------+  +-----------------------------+   |
 |           |                                              |
 +-----------|----------------------------------------------+
             | UART 921600 (USB VCP)
             |
 +-----------|----------------------------------------------+
 |  STM32F401RE (Nucleo)                                    |
 |           |                                              |
 |  +--------+---------+                                    |
 |  | micro-ROS Client |                                    |
 |  | (XRCE-DDS)       |                                    |
 |  +--------+---------+                                    |
 |           |                                              |
 |  +--------+---------+                                    |
 |  |   FreeRTOS        |                                   |
 |  |                   |                                   |
 |  |  balance_task     |  10ms  (100Hz)  prio 3            |
 |  |  encoder_task     |  1ms   (1kHz)   prio 3            |
 |  |  microros_task    |  event-driven   prio 4            |
 |  |  safety_task      |  100ms (10Hz)   prio 5 (highest)  |
 |  +-------------------+                                   |
 |           |                                              |
 |  +--------+----------+----------+-----------+            |
 |  |        |          |          |           |            |
 |  v        v          v          v           v            |
 | ICM-20602  Encoder A  Encoder B  Motor A    Motor B     |
 | (I2C1)     (TIM2)     (TIM3)    (TIM1 PWM) (TIM4 PWM)  |
 | 200Hz      1kHz       1kHz      20kHz       20kHz       |
 +----------------------------------------------------------+
              |          |          |           |
              v          v          v           v
         +--------+  +--------+  +---------------------+
         | MG513  |  | MG513  |  | DBH-12V H-Bridge    |
         | Motor A|  | Motor B|  | (DIR + PWM per ch)  |
         +--------+  +--------+  +---------------------+
```

## ROS2 Topic Map

### Published by STM32

| Topic                | Message Type                          | Rate   |
|----------------------|---------------------------------------|--------|
| `/imu/data`          | `sensor_msgs/Imu`                     | 200 Hz |
| `/odom`              | `nav_msgs/Odometry`                   | 50 Hz  |
| `/diagnostics`       | `diagnostic_msgs/DiagnosticArray`     | 1 Hz   |
| `/balance/status`    | `auro_msgs/BalanceStatus`             | 50 Hz  |

### Subscribed by STM32

| Topic                | Message Type                          | Notes                     |
|----------------------|---------------------------------------|---------------------------|
| `/cmd_vel`           | `geometry_msgs/Twist`                 | linear.x + angular.z      |
| `/balance/setpoint`  | `std_msgs/Float32`                    | Pitch angle override (deg)|

## Balance Control Architecture -- Cascade PID

```
                       Outer Loop (100 Hz)
  target_pitch ---->(+)--->[Balance PID]----> velocity_setpoint
  (-28.4 deg)   -   |                              |
                    |                              |
              measured_pitch                       |
              (from IMU)                           v
                                          +-------+-------+
                                          |               |
                                Inner Loop A (1kHz)  Inner Loop B (1kHz)
                                          |               |
                 rpm_setpoint_a ---->(+)-->[RPM PID A]    rpm_setpoint_b ---->(+)-->[RPM PID B]
                                 -   |         |                          -   |         |
                                    |         v                             |         v
                              encoder_rpm_a  duty_a                   encoder_rpm_b  duty_b
                                             |                                       |
                                             v                                       v
                                          Motor A                                 Motor B
```

### Outer Loop: Balance PID (100 Hz, 10ms period)

- **Input**: target pitch angle (-28.4 deg) vs measured pitch from IMU
- **Output**: velocity setpoint (RPM) fed to both inner loops
- **Controller**: PID with anti-windup, output clamped to max RPM

### Inner Loop: RPM PID (1 kHz, 1ms period) -- one per motor

- **Input**: RPM setpoint (from outer loop + differential steering) vs encoder RPM
- **Output**: motor duty cycle (-100% to +100%)
- **Controller**: existing `pid.c` implementation, already tuned

### Differential Steering

`/cmd_vel` angular.z is mapped to a differential RPM offset:

```
rpm_setpoint_a = balance_velocity_setpoint + cmd_vel_linear - cmd_vel_angular
rpm_setpoint_b = balance_velocity_setpoint + cmd_vel_linear + cmd_vel_angular
```

## FreeRTOS Task Structure

| Task            | Priority | Stack  | Period      | Responsibilities                              |
|-----------------|----------|--------|-------------|-----------------------------------------------|
| `safety_task`   | 5 (max)  | 1 KB   | 100ms (10Hz)| Battery monitor, watchdog kick, e-stop check  |
| `microros_task` | 4        | 8 KB   | event-driven| Spin micro-ROS executor, pub/sub callbacks    |
| `balance_task`  | 3        | 4 KB   | 10ms (100Hz)| Read IMU, run balance PID, drive RPM PID      |
| `encoder_task`  | 3        | 2 KB   | 1ms (1kHz)  | Read TIM2/TIM3 encoder counts, compute RPM    |

### Task Interactions

- `encoder_task` writes RPM values to shared variables (atomic or mutex-protected)
- `balance_task` reads RPM, reads IMU, computes cascade PID, writes motor duty
- `microros_task` reads all sensor data for publishing, writes cmd_vel/setpoint from subscriptions
- `safety_task` can override motor duty to zero (e-stop), highest priority

## Firmware Directory Structure

### KEEP (existing, proven)

| File              | Purpose                                |
|-------------------|----------------------------------------|
| `encoder.c/h`    | TIM2/TIM3 encoder driver, RPM calc     |
| `motor_driver.c/h`| DBH-12V H-bridge PWM control          |
| `pid.c/h`        | Generic PID controller with anti-windup|
| `imu_mpu6050.c/h`| I2C IMU driver (adapt for ICM-20602)   |
| `safety.c/h`     | Battery, watchdog, fault management    |

### ADD (new files)

| File                   | Purpose                                     |
|------------------------|---------------------------------------------|
| `freertos_tasks.c/h`   | Task creation, stack allocation, priorities |
| `balance_controller.c/h`| Cascade PID logic, setpoint management    |
| `ros_interface.c/h`    | micro-ROS publishers, subscribers, timer callbacks |

### REPLACE

| File      | Reason                                              |
|-----------|-----------------------------------------------------|
| `main.c`  | Replace bare-metal super-loop with FreeRTOS init    |

## micro-ROS Transport

- **Physical**: USB VCP (existing Nucleo ST-Link wiring)
- **Protocol**: UART serial transport (XRCE-DDS over UART)
- **Baud rate**: 921600
- **Peripheral**: USART2
- **Agent command**: `ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 921600`

## Flash / SRAM Budget

| Component              | Flash (est.) | SRAM (est.) |
|------------------------|-------------|-------------|
| FreeRTOS kernel        | 10 KB       | 2 KB        |
| FreeRTOS task stacks   | --          | 15 KB       |
| micro-ROS client       | 80 KB       | 25 KB       |
| Application code       | 30 KB       | 5 KB        |
| HAL + drivers          | 50 KB       | 4 KB        |
| Message buffers        | 15 KB       | 8 KB        |
| Heap + misc            | 40 KB       | 4 KB        |
| **Total (estimated)**  | **~225 KB** | **~63 KB**  |
| **Available**          | **512 KB**  | **96 KB**   |
| **Margin**             | **287 KB**  | **33 KB**   |

## NVIC Priority Map

| Priority | Peripheral | Reason                                 |
|----------|-----------|----------------------------------------|
| 0        | USART2    | micro-ROS transport (HW-BUG-02 fix)   |
| 1        | TIM10     | Encoder A sampling                     |
| 2        | TIM11     | Encoder B sampling                     |
| 3        | DMA       | General DMA transfers                  |

**CRITICAL**: USART2 MUST remain at priority 0 (highest). Lowering it causes
dropped bytes at 921600 baud, which corrupts the XRCE-DDS session. This was
identified as HW-BUG-02 during initial bring-up and applies to both the
current bare-metal firmware and the FreeRTOS migration.

Note: FreeRTOS `configMAX_SYSCALL_INTERRUPT_PRIORITY` must be set so that
USART2 at priority 0 is above it (not managed by FreeRTOS), ensuring
zero-latency UART servicing.
