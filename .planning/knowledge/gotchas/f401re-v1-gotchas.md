# NUCLEO-F401RE firmware/hardware gotchas (v1 planning)
**Category:** gotcha
**Tags:** nucleo, f401re, iwdg, i2c, hal, encoder, solder-bridge
**Date:** 2026-09-25
## Detail
- **SB62/SB63 OPEN** always (PA2/PA3 go to ST-Link VCP via SB13/SB14). **PB3 = SWO** (SB15), never GPIO. PA5 = LD2 (SB21). PC13 = B1 (SB17).
- **IWDG reset loop root cause** (`main.c:151` comment): `IWDG_TIMEOUT_MS 200` vs `vTaskDelay(500)` refresh in `heartbeat_task`, and the boot gyro calibration (500 ms settle + 500 × 1 ms samples) ran with no refresh. Also `hiwdg` was refreshed while uninitialised.
- **`HAL_I2C_Mem_Read_DMA` is not non-blocking**: it calls `I2C_RequestMemoryRead` with `HAL_GetTick` timeouts (`stm32f4xx_hal_i2c.c:3329`). In an ISR above SysTick, a stuck bus hangs forever. `HAL_I2C_Mem_Read_IT` has no blocking wait.
- **Command dispatch ran in the USART2 RX ISR** at prio 0 with a blocking 5 ms `HAL_UART_Transmit` (`main.c:753, 800-808`).
- **1320 CPR** (JGB37-520, 11 PPR × 4 × 30) gives 45 rpm per count at 1 kHz; estimate speed over 20 ms windows.
- **IMU axis pairing suspicious**: `atan2(ax, az)` fused with gyro X (`imu_mpu6050.c:212-226`). Verify with the B7b slow-tilt check.
- **RGB polarity contradiction**: `rgb_led.h:5` common-anode LOW=ON vs `rgb_led.c:17-19` SET=ON.
- **Timers**: TIM4 freed by v1 (PB6/PB7, only other encoder-capable pair); TIM5/TIM9 have no free channel pins on LQFP64 here.
