# auro-balancing-necleo

Dual-channel PID motor controller firmware ported from STM32F429ZI Discovery to NUCLEO-F401RE.

## Source project
- Original: [auro-balancing-bot](https://github.com/rnd-southerniot/auro-balancing-bot) (branch `stm32f429zi`)
- Original MCU: STM32F429ZIT6 (168MHz, 2MB Flash, 256KB SRAM, LQFP176)
- This MCU: STM32F401RET6 (84MHz, 512KB Flash, 96KB SRAM, LQFP64)

## Hardware constraints vs original

| Parameter        | F429ZI Discovery   | F401RE NUCLEO      |
|-----------------|--------------------|--------------------|
| CPU speed        | 168 MHz            | 84 MHz             |
| Flash            | 2 MB               | 512 KB             |
| SRAM             | 256 KB (192+64CCM) | 96 KB              |
| GPIO ports       | PA-PI              | PA, PB, PC only    |
| FPU              | Yes (Cortex-M4)    | Yes (Cortex-M4)    |
| Encoder timer    | TIM2 (32-bit)      | TIM2 (32-bit)      |
| PWM timer        | TIM1/TIM4          | TIM1/TIM4          |
| PID tick timer   | TIM6               | TIM10              |
| Telem timer      | TIM7               | TIM11              |
| UART VCP         | USART1 (PA9/PA10)  | USART2 (PA2/PA3)   |
| ADC              | ADC1 + ADC3        | ADC1 (3-ch DMA)    |

See `PORTING_NOTES.md` for full pin remap decision log.

## Build

```bash
# Configure
cmake -S firmware -B firmware/build \
      -DCMAKE_TOOLCHAIN_FILE=$(pwd)/firmware/cmake/arm-gcc.cmake

# Build
cmake --build firmware/build

# Check size
arm-none-eabi-size firmware/build/firmware.elf

# Flash
st-flash write firmware/build/firmware.bin 0x08000000
```

## Unit Tests (host-compiled)

```bash
cmake -S firmware -B firmware/build-test -DBUILD_TESTS=ON
cmake --build firmware/build-test
ctest --test-dir firmware/build-test --output-on-failure
```
