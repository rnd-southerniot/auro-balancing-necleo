# Bench Test Log

---

# MILESTONE 2 ACHIEVED — cmd_vel → motor control end-to-end
**Date:** 2026-04-15

## Stack confirmed working
  ROS2 → DDS → Humble agent → UDP → ESP32 WiFi bridge →
  UART USART6 → STM32 FreeRTOS → XRCE-DDS → PID → DBH-12V → motors

## Root cause of all previous failures
  static rcl_allocator_t — allocator freed before use
  (not the library version, not FastDDS, not SHM, not KEEP_LAST)

## Key fixes committed this session
  Humble libmicroros.a + headers (eliminates Jazzy format mismatch)
  static rcl_allocator_t (the actual crash root cause)
  Clean cmd_vel callback with geometry_msgs/Twist parsing
  Transport timeout caps (prevents agent thread stall)
  Diagnostic HardFault handler (captures stack trace to VCP)
  usleep() stub for Humble rclc compatibility

## Outstanding (tuning, not blockers)
  RGB not updating GREEN — timing/priority, cosmetic
  Motor B direction mirrored — chassis mounting, swap sign in firmware
  Stop command — Python node publishing zeros, use --times not pub

## MILESTONE 2: PASS
  17 entities created successfully
  cmd_vel → motor movement confirmed
  No HardFault under sustained load
  LD2 keeps blinking (session alive)
