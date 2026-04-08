# firmware/CMakeLists.txt
#
# Two build modes:
#
#   1. ARM cross-compile (firmware.elf):
#      cmake -S firmware -B firmware/build \
#            -DCMAKE_TOOLCHAIN_FILE=firmware/cmake/arm-gcc.cmake
#      cmake --build firmware/build
#
#   2. Host-compiled unit tests (no HAL, no ARM):
#      cmake -S firmware -B firmware/build-test -DBUILD_TESTS=ON
#      cmake --build firmware/build-test
#      ctest --test-dir firmware/build-test --output-on-failure

cmake_minimum_required(VERSION 3.22)

option(BUILD_TESTS "Build host-compiled unit tests instead of firmware" OFF)

if(BUILD_TESTS)
    # ── Host test build ─────────────────────────────────────
    project(stm32-pid-tests LANGUAGES C)

    include(FetchContent)
    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG        v2.6.0
    )
    FetchContent_MakeAvailable(unity)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)

    set(CORE_INC ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc)

    set(SRC_PID           ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/pid.c)
    set(SRC_COMM_PROTOCOL ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/comm_protocol.c)

    # ── test_pid ────────────────────────────────────────────
    add_executable(test_pid
        tests/test_pid.c
        ${SRC_PID}
    )
    target_include_directories(test_pid PRIVATE ${CORE_INC})
    target_link_libraries(test_pid PRIVATE unity m)

    # ── test_comm_protocol ──────────────────────────────────
    add_executable(test_comm_protocol
        tests/test_comm_protocol.c
        ${SRC_COMM_PROTOCOL}
    )
    target_include_directories(test_comm_protocol PRIVATE ${CORE_INC})
    target_link_libraries(test_comm_protocol PRIVATE unity m)

    # ── CTest registration ──────────────────────────────────
    enable_testing()
    add_test(NAME test_pid           COMMAND test_pid)
    add_test(NAME test_comm_protocol COMMAND test_comm_protocol)

else()
    # ── ARM cross-compile build ─────────────────────────────
    project(stm32-pid-firmware LANGUAGES C ASM)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)

    # HAL source files — only the modules we need
    set(HAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver)
    set(HAL_SRCS
        ${HAL_DIR}/Src/stm32f4xx_hal.c
        ${HAL_DIR}/Src/stm32f4xx_hal_adc.c
        ${HAL_DIR}/Src/stm32f4xx_hal_adc_ex.c
        ${HAL_DIR}/Src/stm32f4xx_hal_cortex.c
        ${HAL_DIR}/Src/stm32f4xx_hal_dma.c
        ${HAL_DIR}/Src/stm32f4xx_hal_dma_ex.c
        ${HAL_DIR}/Src/stm32f4xx_hal_flash.c
        ${HAL_DIR}/Src/stm32f4xx_hal_flash_ex.c
        ${HAL_DIR}/Src/stm32f4xx_hal_gpio.c
        ${HAL_DIR}/Src/stm32f4xx_hal_iwdg.c
        ${HAL_DIR}/Src/stm32f4xx_hal_pwr.c
        ${HAL_DIR}/Src/stm32f4xx_hal_pwr_ex.c
        ${HAL_DIR}/Src/stm32f4xx_hal_rcc.c
        ${HAL_DIR}/Src/stm32f4xx_hal_rcc_ex.c
        ${HAL_DIR}/Src/stm32f4xx_hal_tim.c
        ${HAL_DIR}/Src/stm32f4xx_hal_tim_ex.c
        ${HAL_DIR}/Src/stm32f4xx_hal_uart.c
        ${HAL_DIR}/Src/stm32f4xx_hal_i2c.c
        ${HAL_DIR}/Src/stm32f4xx_hal_i2c_ex.c
    )

    # CMSIS system init
    set(CMSIS_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
    )

    # Collect application sources
    file(GLOB CORE_SRCS Core/Src/*.c)

    add_executable(firmware.elf
        ${CORE_SRCS}
        ${HAL_SRCS}
        ${CMSIS_SRCS}
        startup_stm32f401retx.s
    )

    target_include_directories(firmware.elf PRIVATE
        Core/Inc
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${HAL_DIR}/Inc
        Drivers/CMSIS/Device/ST/STM32F4xx/Include
        Drivers/CMSIS/Include
    )

    target_compile_definitions(firmware.elf PRIVATE
        STM32F401xE
        USE_HAL_DRIVER
    )

    target_compile_options(firmware.elf PRIVATE
        -Wall -Wextra -Werror
        -Wno-unused-parameter
        -mthumb -mcpu=cortex-m4
        -mfpu=fpv4-sp-d16 -mfloat-abi=hard
        -ffunction-sections -fdata-sections
        -specs=nano.specs
    )

    target_link_options(firmware.elf PRIVATE
        -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F401RETX_FLASH.ld
        -mthumb -mcpu=cortex-m4
        -mfpu=fpv4-sp-d16 -mfloat-abi=hard
        -specs=nano.specs -specs=nosys.specs
        -Wl,--gc-sections
        -Wl,-Map=firmware.map
    )

    target_link_libraries(firmware.elf PRIVATE m)

    # Generate .bin from .elf
    add_custom_command(TARGET firmware.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary firmware.elf firmware.bin
        COMMENT "Generating firmware.bin"
    )

    # Print size after build
    add_custom_command(TARGET firmware.elf POST_BUILD
        COMMAND ${CMAKE_SIZE} firmware.elf
        COMMENT "Firmware size:"
    )

endif()
